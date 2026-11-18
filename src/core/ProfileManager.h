#pragma once

#include <QObject>
#include <QString>

#include <memory>
#include <optional>
#include <vector>

namespace kmedia {

class DatabaseManager;
class SettingsStore;

struct ProfileMetadata
{
    QString id;
    QString name;
    QString mediaLanguage{QStringLiteral("E")};
    QString uiLanguage{QStringLiteral("en")};
    QString storagePath;
    QString midweekSchedule{QStringLiteral("19:30")};
    QString weekendSchedule{QStringLiteral("09:30")};
    QString obsHost{QStringLiteral("127.0.0.1")};
    int obsPort{4455};
    QString zoomMeetingId;
    QString createdAt;
    QString lastUsedAt;
};

class ProfileManager : public QObject
{
    Q_OBJECT

public:
    explicit ProfileManager(QObject* parent = nullptr);
    ~ProfileManager() override;

    Q_DISABLE_COPY_MOVE(ProfileManager)

    [[nodiscard]] bool initialize(const QString& baseDataDir);

    [[nodiscard]] QString baseDataDir() const;
    [[nodiscard]] QString activeProfileId() const;
    [[nodiscard]] std::optional<ProfileMetadata> activeProfile() const;
    [[nodiscard]] std::vector<ProfileMetadata> profiles() const;
    [[nodiscard]] std::optional<ProfileMetadata> findProfile(const QString& profileId) const;

    [[nodiscard]] QString createProfile(const QString& name,
                                        const QString& mediaLanguage = QStringLiteral("E"),
                                        const QString& customStoragePath = {});
    [[nodiscard]] bool updateProfile(const ProfileMetadata& updated);
    [[nodiscard]] bool renameProfile(const QString& profileId, const QString& newName);
    [[nodiscard]] bool deleteProfile(const QString& profileId);
    [[nodiscard]] bool switchProfile(const QString& profileId);

    [[nodiscard]] DatabaseManager* activeDatabase();
    [[nodiscard]] const DatabaseManager* activeDatabase() const;
    [[nodiscard]] SettingsStore* activeSettings();
    [[nodiscard]] const SettingsStore* activeSettings() const;

    [[nodiscard]] qint64 lastSwitchDurationMs() const;
    [[nodiscard]] QString lastError() const;

signals:
    void activeProfileChanged(const QString& profileId);
    void profilesChanged();

private:
    [[nodiscard]] bool loadRegistry();
    [[nodiscard]] bool saveRegistry() const;
    [[nodiscard]] QString profileRootDir(const QString& profileId) const;
    [[nodiscard]] QString uniqueProfileId(const QString& baseName) const;

    QString m_baseDataDir;
    QString m_activeProfileId;
    std::vector<ProfileMetadata> m_profiles;
    std::unique_ptr<DatabaseManager> m_database;
    std::unique_ptr<SettingsStore> m_settings;
    qint64 m_lastSwitchDurationMs{0};
    QString m_lastError;
};

} // namespace kmedia
