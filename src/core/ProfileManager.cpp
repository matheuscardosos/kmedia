#include "core/ProfileManager.h"

#include "core/DatabaseManager.h"
#include "core/SettingsStore.h"
#include "core/SystemUtils.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

namespace kmedia {

namespace {

QString currentUtcIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QJsonObject profileToJson(const ProfileMetadata& profile)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), profile.id);
    obj.insert(QStringLiteral("name"), profile.name);
    obj.insert(QStringLiteral("mediaLanguage"), profile.mediaLanguage);
    obj.insert(QStringLiteral("uiLanguage"), profile.uiLanguage);
    obj.insert(QStringLiteral("storagePath"), profile.storagePath);
    obj.insert(QStringLiteral("midweekSchedule"), profile.midweekSchedule);
    obj.insert(QStringLiteral("weekendSchedule"), profile.weekendSchedule);
    obj.insert(QStringLiteral("obsHost"), profile.obsHost);
    obj.insert(QStringLiteral("obsPort"), profile.obsPort);
    obj.insert(QStringLiteral("zoomMeetingId"), profile.zoomMeetingId);
    obj.insert(QStringLiteral("createdAt"), profile.createdAt);
    obj.insert(QStringLiteral("lastUsedAt"), profile.lastUsedAt);
    return obj;
}

ProfileMetadata profileFromJson(const QJsonObject& obj)
{
    ProfileMetadata profile;
    profile.id = obj.value(QStringLiteral("id")).toString();
    profile.name = obj.value(QStringLiteral("name")).toString();
    profile.mediaLanguage =
        obj.value(QStringLiteral("mediaLanguage")).toString(QStringLiteral("E"));
    profile.uiLanguage = obj.value(QStringLiteral("uiLanguage")).toString(QStringLiteral("en"));
    profile.storagePath = obj.value(QStringLiteral("storagePath")).toString();
    profile.midweekSchedule =
        obj.value(QStringLiteral("midweekSchedule")).toString(QStringLiteral("19:30"));
    profile.weekendSchedule =
        obj.value(QStringLiteral("weekendSchedule")).toString(QStringLiteral("09:30"));
    profile.obsHost = obj.value(QStringLiteral("obsHost")).toString(QStringLiteral("127.0.0.1"));
    profile.obsPort = obj.value(QStringLiteral("obsPort")).toInt(4455);
    profile.zoomMeetingId = obj.value(QStringLiteral("zoomMeetingId")).toString();
    profile.createdAt = obj.value(QStringLiteral("createdAt")).toString();
    profile.lastUsedAt = obj.value(QStringLiteral("lastUsedAt")).toString();
    return profile;
}

} // namespace

ProfileManager::ProfileManager(QObject* parent)
    : QObject(parent), m_database(std::make_unique<DatabaseManager>()),
      m_settings(std::make_unique<SettingsStore>())
{
}

ProfileManager::~ProfileManager() = default;

bool ProfileManager::initialize(const QString& baseDataDir)
{
    if (baseDataDir.trimmed().isEmpty()) {
        m_lastError = QStringLiteral("Base data directory cannot be empty");
        return false;
    }

    m_baseDataDir = QDir::cleanPath(baseDataDir);
    if (!QDir().mkpath(m_baseDataDir) ||
        !QDir().mkpath(m_baseDataDir + QStringLiteral("/profiles"))) {
        m_lastError = QStringLiteral("Failed to create profile base directories");
        return false;
    }

    if (!loadRegistry()) {
        return false;
    }

    if (m_profiles.empty()) {
        const QString defaultId = createProfile(QStringLiteral("Default Congregation"));
        if (defaultId.isEmpty()) {
            return false;
        }
        m_activeProfileId = defaultId;
    }

    if (m_activeProfileId.isEmpty() || !findProfile(m_activeProfileId).has_value()) {
        m_activeProfileId = m_profiles.front().id;
    }

    return switchProfile(m_activeProfileId);
}

QString ProfileManager::baseDataDir() const
{
    return m_baseDataDir;
}

QString ProfileManager::activeProfileId() const
{
    return m_activeProfileId;
}

std::optional<ProfileMetadata> ProfileManager::activeProfile() const
{
    return findProfile(m_activeProfileId);
}

std::vector<ProfileMetadata> ProfileManager::profiles() const
{
    return m_profiles;
}

std::optional<ProfileMetadata> ProfileManager::findProfile(const QString& profileId) const
{
    for (const auto& profile : m_profiles) {
        if (profile.id == profileId) {
            return profile;
        }
    }
    return std::nullopt;
}

QString ProfileManager::createProfile(const QString& name, const QString& mediaLanguage,
                                      const QString& customStoragePath)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty() || m_baseDataDir.isEmpty()) {
        m_lastError = QStringLiteral("Profile name and base directory must be valid");
        return {};
    }

    const QString profileId = uniqueProfileId(trimmedName);
    const QString rootDir = profileRootDir(profileId);
    const QString storageDir = customStoragePath.trimmed().isEmpty()
                                   ? QDir::cleanPath(rootDir + QStringLiteral("/media"))
                                   : QDir::cleanPath(customStoragePath);

    if (!QDir().mkpath(rootDir) || !QDir().mkpath(storageDir)) {
        m_lastError = QStringLiteral("Failed to create directories for profile");
        return {};
    }

    // Pre-initialize schema once at creation time so subsequent switches stay fast.
    {
        DatabaseManager seedDb;
        if (!seedDb.open(QDir::cleanPath(rootDir + QStringLiteral("/profile.db")))) {
            m_lastError = seedDb.lastError();
            return {};
        }
    }

    const QString now = currentUtcIso();
    ProfileMetadata profile;
    profile.id = profileId;
    profile.name = trimmedName;
    profile.mediaLanguage =
        mediaLanguage.trimmed().isEmpty() ? QStringLiteral("E") : mediaLanguage.trimmed();
    profile.storagePath = storageDir;
    profile.createdAt = now;
    profile.lastUsedAt = now;

    m_profiles.push_back(profile);
    if (!saveRegistry()) {
        m_profiles.pop_back();
        return {};
    }

    emit profilesChanged();
    return profileId;
}

bool ProfileManager::updateProfile(const ProfileMetadata& updated)
{
    if (updated.id.isEmpty() || updated.name.trimmed().isEmpty()) {
        return false;
    }

    for (auto& existing : m_profiles) {
        if (existing.id == updated.id) {
            existing.name = updated.name.trimmed();
            existing.mediaLanguage = updated.mediaLanguage;
            existing.uiLanguage = updated.uiLanguage;
            if (!updated.storagePath.trimmed().isEmpty()) {
                existing.storagePath = QDir::cleanPath(updated.storagePath);
                if (!QDir().mkpath(existing.storagePath)) {
                    m_lastError = QStringLiteral("Failed to create updated media directory");
                    return false;
                }
            }
            existing.midweekSchedule = updated.midweekSchedule;
            existing.weekendSchedule = updated.weekendSchedule;
            existing.obsHost = updated.obsHost;
            existing.obsPort = updated.obsPort;
            existing.zoomMeetingId = updated.zoomMeetingId;

            if (!saveRegistry()) {
                return false;
            }
            emit profilesChanged();
            return true;
        }
    }
    return false;
}

bool ProfileManager::renameProfile(const QString& profileId, const QString& newName)
{
    auto existing = findProfile(profileId);
    if (!existing || newName.trimmed().isEmpty()) {
        return false;
    }
    existing->name = newName.trimmed();
    return updateProfile(*existing);
}

bool ProfileManager::deleteProfile(const QString& profileId)
{
    if (m_profiles.size() <= 1) {
        m_lastError = QStringLiteral("Cannot delete the last remaining congregation profile");
        return false;
    }

    const auto it = std::find_if(m_profiles.begin(), m_profiles.end(),
                                 [&](const ProfileMetadata& p) { return p.id == profileId; });
    if (it == m_profiles.end()) {
        return false;
    }

    const bool deletingActive = (profileId == m_activeProfileId);
    if (deletingActive) {
        m_settings->bindDatabase(nullptr);
        m_database->close();
    }

    m_profiles.erase(it);
    QDir(profileRootDir(profileId)).removeRecursively();

    if (deletingActive) {
        m_activeProfileId = m_profiles.front().id;
        if (!switchProfile(m_activeProfileId)) {
            return false;
        }
    } else if (!saveRegistry()) {
        return false;
    }

    emit profilesChanged();
    return true;
}

bool ProfileManager::switchProfile(const QString& profileId)
{
    QElapsedTimer timer;
    timer.start();

    auto targetIt = std::find_if(m_profiles.begin(), m_profiles.end(),
                                 [&](const ProfileMetadata& p) { return p.id == profileId; });
    if (targetIt == m_profiles.end()) {
        m_lastError = QStringLiteral("Target profile does not exist");
        return false;
    }

    const QString rootDir = profileRootDir(profileId);
    if (!QDir().mkpath(rootDir) || !QDir().mkpath(targetIt->storagePath)) {
        m_lastError = QStringLiteral("Failed to ensure target profile directories");
        return false;
    }

    m_settings->bindDatabase(nullptr);
    m_database->close();

    const QString dbPath = QDir::cleanPath(rootDir + QStringLiteral("/profile.db"));
    if (!m_database->open(dbPath)) {
        m_lastError = m_database->lastError();
        return false;
    }

    m_activeProfileId = profileId;
    targetIt->lastUsedAt = currentUtcIso();
    m_settings->bindDatabase(m_database.get());

    if (!saveRegistry()) {
        return false;
    }

    m_lastSwitchDurationMs = timer.elapsed();
    emit activeProfileChanged(m_activeProfileId);
    return true;
}

DatabaseManager* ProfileManager::activeDatabase()
{
    return m_database.get();
}

const DatabaseManager* ProfileManager::activeDatabase() const
{
    return m_database.get();
}

SettingsStore* ProfileManager::activeSettings()
{
    return m_settings.get();
}

const SettingsStore* ProfileManager::activeSettings() const
{
    return m_settings.get();
}

qint64 ProfileManager::lastSwitchDurationMs() const
{
    return m_lastSwitchDurationMs;
}

QString ProfileManager::lastError() const
{
    return m_lastError;
}

bool ProfileManager::loadRegistry()
{
    m_profiles.clear();
    m_activeProfileId.clear();

    const QString registryPath = m_baseDataDir + QStringLiteral("/profiles.json");
    QFile file(registryPath);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("Failed to read profiles registry");
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        m_lastError = QStringLiteral("Invalid profiles registry JSON");
        return false;
    }

    const QJsonObject rootObj = doc.object();
    m_activeProfileId = rootObj.value(QStringLiteral("activeProfileId")).toString();
    const QJsonArray array = rootObj.value(QStringLiteral("profiles")).toArray();
    for (const QJsonValue& val : array) {
        if (!val.isObject()) {
            continue;
        }
        ProfileMetadata p = profileFromJson(val.toObject());
        if (!p.id.isEmpty() && !p.name.isEmpty()) {
            if (p.storagePath.isEmpty()) {
                p.storagePath = QDir::cleanPath(profileRootDir(p.id) + QStringLiteral("/media"));
            }
            m_profiles.push_back(std::move(p));
        }
    }
    return true;
}

bool ProfileManager::saveRegistry() const
{
    QJsonArray array;
    for (const auto& profile : m_profiles) {
        array.append(profileToJson(profile));
    }

    QJsonObject rootObj;
    rootObj.insert(QStringLiteral("version"), 1);
    rootObj.insert(QStringLiteral("activeProfileId"), m_activeProfileId);
    rootObj.insert(QStringLiteral("profiles"), array);

    const QString registryPath = m_baseDataDir + QStringLiteral("/profiles.json");
    QSaveFile saveFile(registryPath);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    saveFile.write(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
    return saveFile.commit();
}

QString ProfileManager::profileRootDir(const QString& profileId) const
{
    return QDir::cleanPath(m_baseDataDir + QStringLiteral("/profiles/") + profileId);
}

QString ProfileManager::uniqueProfileId(const QString& baseName) const
{
    static const QRegularExpression kNonAlnum(QStringLiteral("[^a-z0-9]+"));
    QString slug = baseName.toLower();
    slug.replace(kNonAlnum, QStringLiteral("-"));
    while (slug.startsWith(QLatin1Char('-'))) {
        slug.remove(0, 1);
    }
    while (slug.endsWith(QLatin1Char('-'))) {
        slug.chop(1);
    }
    if (slug.isEmpty()) {
        slug = QStringLiteral("congregation");
    }

    QString candidate = slug;
    int suffix = 2;
    while (findProfile(candidate).has_value()) {
        candidate = QStringLiteral("%1-%2").arg(slug).arg(suffix);
        ++suffix;
    }
    return candidate;
}

} // namespace kmedia
