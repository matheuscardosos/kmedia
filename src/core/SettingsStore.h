#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariant>

namespace kmedia {

class DatabaseManager;

namespace settings_keys {

inline constexpr auto kMediaLanguage = "media/language";
inline constexpr auto kMediaFallbackLanguage = "media/fallback_language";
inline constexpr auto kUiLanguage = "ui/language";
inline constexpr auto kUiDarkTheme = "ui/dark_theme";
inline constexpr auto kUiFloatingPreview = "ui/floating_preview";
inline constexpr auto kMeetingMidweekDay = "meeting/midweek_day";
inline constexpr auto kMeetingMidweekTime = "meeting/midweek_time";
inline constexpr auto kMeetingWeekendDay = "meeting/weekend_day";
inline constexpr auto kMeetingWeekendTime = "meeting/weekend_time";
inline constexpr auto kMeetingSpecialEvents = "meeting/special_events_json";
inline constexpr auto kMeetingTempScheduleChanges = "meeting/temp_schedule_changes_json";
inline constexpr auto kFilterIncludePrinted = "catalog/include_printed";
inline constexpr auto kFilterExcludeFootnotes = "catalog/exclude_footnotes";
inline constexpr auto kFilterExcludeWtParagraphVideos = "catalog/exclude_wt_paragraph_videos";
inline constexpr auto kFilterExcludeTh = "catalog/exclude_th";
inline constexpr auto kFilterExcludeCbsPubs = "catalog/exclude_cbs_pubs";
inline constexpr auto kBackgroundMusicFadeMs = "audio/background_fade_ms";
inline constexpr auto kMusicAutoStart = "audio/music_auto_start";
inline constexpr auto kAudioOutputDevice = "audio/output_device";
inline constexpr auto kHardwareDecoder = "video/hwdec";
inline constexpr auto kMaxConcurrentDownloads = "network/max_concurrent_downloads";
inline constexpr auto kThumbnailCacheMaxMb = "cache/thumbnail_max_mb";
inline constexpr auto kStandbyBlackoutMode = "presentation/standby_mode";
inline constexpr auto kStandbyImage = "presentation/standby_image";
inline constexpr auto kTimerCountDown = "timer/count_down";
inline constexpr auto kTimerOvertimeAmountOnly = "timer/overtime_amount_only";
inline constexpr auto kTimerDayStates = "timer/day_states";
inline constexpr auto kPresentationCamera = "presentation/camera";
inline constexpr auto kObsEnabled = "obs/enabled";
inline constexpr auto kObsHost = "obs/host";
inline constexpr auto kObsPort = "obs/port";
inline constexpr auto kObsPassword = "obs/password";
inline constexpr auto kObsSceneStage = "obs/scene_stage";
inline constexpr auto kObsSceneMedia = "obs/scene_media";
inline constexpr auto kObsSceneImage = "obs/scene_image";
inline constexpr auto kZoomMeetingId = "zoom/meeting_id";
inline constexpr auto kZoomMeetingPass = "zoom/meeting_pass";
inline constexpr auto kZoomDisplayName = "zoom/display_name";
inline constexpr auto kZoomAudioDevice = "zoom/audio_device";
inline constexpr auto kZoomAutoShare = "zoom/auto_share";
inline constexpr auto kCloudSyncFolders = "cloud/watched_folders";
inline constexpr auto kWatchedFolderEnabled = "watch/enabled";
inline constexpr auto kWatchedFolder = "watch/folder";
inline constexpr auto kWatchedFolderDeletePast = "watch/delete_past";
inline constexpr auto kAutoExportEnabled = "export/enabled";
inline constexpr auto kAutoExportFolder = "export/folder";
inline constexpr auto kAutoExportConvertMp4 = "export/convert_mp4";
inline constexpr auto kSecurityPinHash = "security/booth_pin_hash";
inline constexpr auto kSecurityPinSalt = "security/booth_pin_salt";

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
