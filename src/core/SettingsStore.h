#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariant>

namespace kmedia {

class DatabaseManager;

namespace settings_keys {

inline constexpr auto kMediaLanguage = "media/language";
inline constexpr auto kUiLanguage = "ui/language";
inline constexpr auto kBackgroundMusicFadeMs = "audio/background_fade_ms";
inline constexpr auto kAudioOutputDevice = "audio/output_device";
inline constexpr auto kHardwareDecoder = "video/hwdec";
inline constexpr auto kMaxConcurrentDownloads = "network/max_concurrent_downloads";
inline constexpr auto kThumbnailCacheMaxMb = "cache/thumbnail_max_mb";
inline constexpr auto kStandbyBlackoutMode = "presentation/standby_mode";
inline constexpr auto kObsHost = "obs/host";
inline constexpr auto kObsPort = "obs/port";
inline constexpr auto kZoomAutoShare = "zoom/auto_share";
inline constexpr auto kCloudSyncFolders = "cloud/watched_folders";

} // namespace settings_keys

class SettingsStore : public QObject
{
    Q_OBJECT

public:
    explicit SettingsStore(QObject* parent = nullptr);

    Q_DISABLE_COPY_MOVE(SettingsStore)

    void bindDatabase(DatabaseManager* database);

    [[nodiscard]] QVariant value(const QString& key, const QVariant& defaultValue = {}) const;
    [[nodiscard]] bool setValue(const QString& key, const QVariant& value);
    [[nodiscard]] bool remove(const QString& key);
    [[nodiscard]] bool contains(const QString& key) const;

    [[nodiscard]] QString stringValue(const QString& key, const QString& defaultValue = {}) const;
    [[nodiscard]] int intValue(const QString& key, int defaultValue = 0) const;
    [[nodiscard]] bool boolValue(const QString& key, bool defaultValue = false) const;

    [[nodiscard]] QString mediaLanguage() const;
    [[nodiscard]] QString uiLanguage() const;
    [[nodiscard]] int backgroundMusicFadeMs() const;
    [[nodiscard]] int maxConcurrentDownloads() const;
    [[nodiscard]] QString hardwareDecoder() const;
    [[nodiscard]] int thumbnailCacheMaxMb() const;

signals:
    void settingChanged(const QString& key, const QVariant& value);
    void storeReloaded();

private:
    void reloadFromDatabase();
    [[nodiscard]] static QString encodeVariant(const QVariant& value);

    DatabaseManager* m_database{nullptr};
    QHash<QString, QString> m_cache;
};

} // namespace kmedia
