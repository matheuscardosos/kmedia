#include "integrations/ZoomBridge.h"

#include <QDesktopServices>
#include <QUrlQuery>

namespace kmedia {

ZoomBridge::ZoomBridge(QObject* parent)
    : QObject(parent), m_launcher([](const QUrl& url) { return QDesktopServices::openUrl(url); })
{
}

void ZoomBridge::setUrlLauncher(UrlLauncher launcher)
{
    if (launcher) {
        m_launcher = std::move(launcher);
    }
}

bool ZoomBridge::isZoomRunning() const
{
    return m_zoomRunning;
}

void ZoomBridge::setZoomRunning(bool running)
{
    if (m_zoomRunning == running) {
        return;
    }
    m_zoomRunning = running;
    emit zoomRunningChanged();
}

QString ZoomBridge::meetingId() const
{
    return m_meetingId;
}

void ZoomBridge::setMeetingId(const QString& meetingId)
{
    const QString cleaned = meetingId.trimmed();
    if (m_meetingId == cleaned) {
        return;
    }
    m_meetingId = cleaned;
    emit configChanged();
}

QString ZoomBridge::meetingPassword() const
{
    return m_meetingPassword;
}

void ZoomBridge::setMeetingPassword(const QString& password)
{
    if (m_meetingPassword == password) {
        return;
    }
    m_meetingPassword = password;
    emit configChanged();
}

QString ZoomBridge::displayName() const
{
    return m_displayName;
}

void ZoomBridge::setDisplayName(const QString& displayName)
{
    const QString trimmed = displayName.trimmed();
    if (m_displayName == trimmed) {
        return;
    }
    m_displayName = trimmed;
    emit configChanged();
}

QString ZoomBridge::dedicatedAudioDevice() const
{
    return m_dedicatedAudioDevice;
}

void ZoomBridge::setDedicatedAudioDevice(const QString& deviceId)
{
    const QString trimmed = deviceId.trimmed();
    if (m_dedicatedAudioDevice == trimmed) {
        return;
    }
    m_dedicatedAudioDevice = trimmed;
    emit audioDeviceChanged();
}

bool ZoomBridge::isAutoShareOnPlayback() const
{
    return m_autoShareOnPlayback;
}

void ZoomBridge::setAutoShareOnPlayback(bool enabled)
{
    if (m_autoShareOnPlayback == enabled) {
        return;
    }
    m_autoShareOnPlayback = enabled;
    emit configChanged();
}

QString ZoomBridge::presentationWindowTitle()
{
    return QStringLiteral("KMedia - Presentation");
}

QString ZoomBridge::shareShortcutHint()
{
#if defined(Q_OS_MAC)
    return QStringLiteral("Cmd+Shift+S");
#else
    return QStringLiteral("Alt+S");
#endif
}

QString ZoomBridge::sanitizeMeetingId(const QString& rawMeetingId)
{
    QString clean;
    clean.reserve(rawMeetingId.size());
    for (const QChar ch : rawMeetingId) {
        if (ch.isDigit()) {
            clean.append(ch);
        }
    }
    return clean;
}

QUrl ZoomBridge::buildJoinUrl(const QString& meetingId, const QString& password,
                              const QString& displayName)
{
    const QString confno = sanitizeMeetingId(meetingId);
    if (confno.isEmpty()) {
        return {};
    }

    QUrl url(QStringLiteral("zoommtg://zoom.us/join"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("action"), QStringLiteral("join"));
    query.addQueryItem(QStringLiteral("confno"), confno);
    if (!password.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("pwd"), password.trimmed());
    }
    if (!displayName.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("uname"), displayName.trimmed());
    }
    url.setQuery(query);
    return url;
}

bool ZoomBridge::joinMeeting()
{
    const QUrl url = buildJoinUrl(m_meetingId, m_meetingPassword, m_displayName);
    if (!url.isValid()) {
        return false;
    }
    if (m_launcher && m_launcher(url)) {
        emit joinLaunched(url);
        return true;
    }
    return false;
}

void ZoomBridge::notifyPresentationPlaybackStarted()
{
    if (m_autoShareOnPlayback && m_zoomRunning) {
        emit shareTriggerRequested(shareShortcutHint(), presentationWindowTitle());
    }
}

} // namespace kmedia
