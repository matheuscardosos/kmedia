#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

namespace kmedia {

/**
 * Coordinates Zoom booth workflows: standardized `zoommtg://` deep-link meeting launch,
 * dedicated audio output device isolation, live Zoom status tracking, and share shortcut hints.
 */
class ZoomBridge : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool zoomRunning READ isZoomRunning NOTIFY zoomRunningChanged FINAL)
    Q_PROPERTY(QString meetingId READ meetingId WRITE setMeetingId NOTIFY configChanged FINAL)
    Q_PROPERTY(QString meetingPassword READ meetingPassword WRITE setMeetingPassword NOTIFY
                   configChanged FINAL)
    Q_PROPERTY(QString displayName READ displayName WRITE setDisplayName NOTIFY configChanged FINAL)
    Q_PROPERTY(QString dedicatedAudioDevice READ dedicatedAudioDevice WRITE setDedicatedAudioDevice
                   NOTIFY audioDeviceChanged FINAL)
    Q_PROPERTY(bool autoShareOnPlayback READ isAutoShareOnPlayback WRITE setAutoShareOnPlayback
                   NOTIFY configChanged FINAL)
    Q_PROPERTY(QString presentationWindowTitle READ presentationWindowTitle CONSTANT FINAL)
    Q_PROPERTY(QString shareShortcutHint READ shareShortcutHint CONSTANT FINAL)

public:
    using UrlLauncher = std::function<bool(const QUrl&)>;

    explicit ZoomBridge(QObject* parent = nullptr);
    ~ZoomBridge() override = default;

    Q_DISABLE_COPY_MOVE(ZoomBridge)

    void setUrlLauncher(UrlLauncher launcher);

    [[nodiscard]] bool isZoomRunning() const;
    void setZoomRunning(bool running);

    [[nodiscard]] QString meetingId() const;
    void setMeetingId(const QString& meetingId);

    [[nodiscard]] QString meetingPassword() const;
    void setMeetingPassword(const QString& password);

    [[nodiscard]] QString displayName() const;
    void setDisplayName(const QString& displayName);

    [[nodiscard]] QString dedicatedAudioDevice() const;
    void setDedicatedAudioDevice(const QString& deviceId);

    [[nodiscard]] bool isAutoShareOnPlayback() const;
    void setAutoShareOnPlayback(bool enabled);

    [[nodiscard]] static QString presentationWindowTitle();
    [[nodiscard]] static QString shareShortcutHint();
    [[nodiscard]] static QString sanitizeMeetingId(const QString& rawMeetingId);
    [[nodiscard]] static QUrl buildJoinUrl(const QString& meetingId, const QString& password = {},
                                           const QString& displayName = {});

    Q_INVOKABLE bool joinMeeting();
    Q_INVOKABLE void notifyPresentationPlaybackStarted();

signals:
    void zoomRunningChanged();
    void configChanged();
    void audioDeviceChanged();
    void joinLaunched(const QUrl& url);
    void shareTriggerRequested(const QString& shortcutHint, const QString& targetWindowTitle);

private:
    UrlLauncher m_launcher;
    bool m_zoomRunning{false};
    QString m_meetingId;
    QString m_meetingPassword;
    QString m_displayName;
    QString m_dedicatedAudioDevice;
    bool m_autoShareOnPlayback{false};
};

} // namespace kmedia
