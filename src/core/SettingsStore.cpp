#include "core/SettingsStore.h"

#include "core/DatabaseManager.h"

namespace kmedia {

SettingsStore::SettingsStore(QObject* parent) : QObject(parent) {}

void SettingsStore::bindDatabase(DatabaseManager* database)
{
    m_database = database;
    reloadFromDatabase();
    emit storeReloaded();
}

QVariant SettingsStore::value(const QString& key, const QVariant& defaultValue) const
{
    const auto it = m_cache.constFind(key);
    if (it == m_cache.constEnd()) {
        return defaultValue;
    }
    return QVariant(it.value());
}

bool SettingsStore::setValue(const QString& key, const QVariant& value)
{
    if (key.isEmpty()) {
        return false;
    }

    const QString encoded = encodeVariant(value);
    if (m_database != nullptr && m_database->isOpen()) {
        if (!m_database->setSetting(key, encoded)) {
            return false;
        }
    }

    const auto it = m_cache.constFind(key);
    if (it != m_cache.constEnd() && it.value() == encoded) {
        return true;
    }

    m_cache.insert(key, encoded);
    emit settingChanged(key, value);
    return true;
}

bool SettingsStore::remove(const QString& key)
{
    if (key.isEmpty()) {
        return false;
    }
    if (m_database != nullptr && m_database->isOpen()) {
        if (!m_database->removeSetting(key)) {
            return false;
        }
    }
    if (m_cache.remove(key) > 0) {
        emit settingChanged(key, {});
    }
    return true;
}

bool SettingsStore::contains(const QString& key) const
{
    return m_cache.contains(key);
}

QString SettingsStore::stringValue(const QString& key, const QString& defaultValue) const
{
    const auto it = m_cache.constFind(key);
    return it == m_cache.constEnd() ? defaultValue : it.value();
}

int SettingsStore::intValue(const QString& key, int defaultValue) const
{
    const auto it = m_cache.constFind(key);
    if (it == m_cache.constEnd()) {
        return defaultValue;
    }
    bool ok = false;
    const int parsed = it.value().toInt(&ok);
    return ok ? parsed : defaultValue;
}

bool SettingsStore::boolValue(const QString& key, bool defaultValue) const
{
    const auto it = m_cache.constFind(key);
    if (it == m_cache.constEnd()) {
        return defaultValue;
    }
    const QString lower = it.value().trimmed().toLower();
    if (lower == QStringLiteral("true") || lower == QStringLiteral("1")) {
        return true;
    }
    if (lower == QStringLiteral("false") || lower == QStringLiteral("0")) {
        return false;
    }
    return defaultValue;
}

QString SettingsStore::mediaLanguage() const
{
    return stringValue(QLatin1StringView(settings_keys::kMediaLanguage), QStringLiteral("E"));
}

QString SettingsStore::uiLanguage() const
{
    return stringValue(QLatin1StringView(settings_keys::kUiLanguage), QStringLiteral("en"));
}

int SettingsStore::backgroundMusicFadeMs() const
{
    return qBound(0, intValue(QLatin1StringView(settings_keys::kBackgroundMusicFadeMs), 5000),
                  30000);
}

int SettingsStore::maxConcurrentDownloads() const
{
    return qBound(1, intValue(QLatin1StringView(settings_keys::kMaxConcurrentDownloads), 3), 6);
}

QString SettingsStore::hardwareDecoder() const
{
    return stringValue(QLatin1StringView(settings_keys::kHardwareDecoder),
                       QStringLiteral("auto-safe"));
}

int SettingsStore::thumbnailCacheMaxMb() const
{
    return qBound(16, intValue(QLatin1StringView(settings_keys::kThumbnailCacheMaxMb), 128), 4096);
}

void SettingsStore::reloadFromDatabase()
{
    m_cache.clear();
    if (m_database == nullptr || !m_database->isOpen()) {
        return;
    }
    for (const auto& [key, val] : m_database->allSettings()) {
        m_cache.insert(key, val);
    }
}

QString SettingsStore::encodeVariant(const QVariant& value)
{
    if (value.metaType().id() == QMetaType::Bool) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    return value.toString();
}

} // namespace kmedia
