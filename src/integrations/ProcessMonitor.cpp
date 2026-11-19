#include "integrations/ProcessMonitor.h"

#include <QFileInfo>

#if defined(Q_OS_WIN)
// clang-format off
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
// clang-format on
#elif defined(Q_OS_MAC)
#include <libproc.h>
#include <sys/proc_info.h>
#elif defined(Q_OS_LINUX)
#include <QDir>
#include <QFile>
#endif

#include <vector>

namespace kmedia {

namespace {

bool matchesObsExecutable(const QString& lowerName)
{
    return lowerName == QStringLiteral("obs64.exe") || lowerName == QStringLiteral("obs32.exe") ||
           lowerName == QStringLiteral("obs.exe") || lowerName == QStringLiteral("obs") ||
           lowerName == QStringLiteral("obs-studio");
}

bool matchesZoomExecutable(const QString& lowerName)
{
    return lowerName == QStringLiteral("zoom.exe") || lowerName == QStringLiteral("cpthost.exe") ||
           lowerName == QStringLiteral("zoom.us") || lowerName == QStringLiteral("zoom");
}

} // namespace

ProcessMonitor::ProcessMonitor(QObject* parent) : QObject(parent)
{
    m_timer.setInterval(m_intervalMs);
    connect(&m_timer, &QTimer::timeout, this, &ProcessMonitor::refreshNow);
}

bool ProcessMonitor::isObsRunning() const
{
    return m_obsRunning;
}

bool ProcessMonitor::isZoomRunning() const
{
    return m_zoomRunning;
}

bool ProcessMonitor::isMonitoring() const
{
    return m_timer.isActive();
}

int ProcessMonitor::intervalMs() const
{
    return m_intervalMs;
}

void ProcessMonitor::setIntervalMs(int intervalMs)
{
    const int clamped = qBound(1000, intervalMs, 30000);
    if (m_intervalMs == clamped) {
        return;
    }
    m_intervalMs = clamped;
    m_timer.setInterval(m_intervalMs);
    emit intervalMsChanged();
}

void ProcessMonitor::startMonitoring()
{
    if (!m_timer.isActive()) {
        refreshNow();
        m_timer.start();
        emit monitoringChanged();
    }
}

void ProcessMonitor::stopMonitoring()
{
    if (m_timer.isActive()) {
        m_timer.stop();
        emit monitoringChanged();
    }
}

void ProcessMonitor::refreshNow()
{
    const auto [obsNow, zoomNow] = classifyProcessNames(queryRunningProcessNames());
    applySimulatedState(obsNow, zoomNow);
}

void ProcessMonitor::applySimulatedState(bool obsRunning, bool zoomRunning)
{
    if (m_obsRunning == obsRunning && m_zoomRunning == zoomRunning) {
        return;
    }
    m_obsRunning = obsRunning;
    m_zoomRunning = zoomRunning;
    emit statusChanged();
}

std::pair<bool, bool> ProcessMonitor::classifyProcessNames(const QStringList& processNames)
{
    bool obsFound = false;
    bool zoomFound = false;

    for (const QString& rawName : processNames) {
        const QString base = QFileInfo(rawName.trimmed()).fileName().toLower();
        if (base.isEmpty()) {
            continue;
        }
        if (matchesObsExecutable(base)) {
            obsFound = true;
        }
        if (matchesZoomExecutable(base)) {
            zoomFound = true;
        }
        if (obsFound && zoomFound) {
            break;
        }
    }
    return {obsFound, zoomFound};
}

QStringList ProcessMonitor::queryRunningProcessNames()
{
    QStringList names;

#if defined(Q_OS_WIN)
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return names;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            names.append(QString::fromWCharArray(entry.szExeFile));
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

#elif defined(Q_OS_MAC)
    const int byteCount = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (byteCount <= 0) {
        return names;
    }
    std::vector<pid_t> pids(static_cast<size_t>(byteCount) / sizeof(pid_t) + 16, 0);
    const int actualBytes =
        proc_listpids(PROC_ALL_PIDS, 0, pids.data(), static_cast<int>(pids.size() * sizeof(pid_t)));
    if (actualBytes <= 0) {
        return names;
    }
    const size_t count = static_cast<size_t>(actualBytes) / sizeof(pid_t);
    for (size_t i = 0; i < count; ++i) {
        if (pids[i] <= 0) {
            continue;
        }
        char nameBuf[PROC_PIDPATHINFO_MAXSIZE] = {};
        if (proc_name(pids[i], nameBuf, sizeof(nameBuf)) > 0) {
            names.append(QString::fromUtf8(nameBuf));
        }
    }

#elif defined(Q_OS_LINUX)
    QDir procDir(QStringLiteral("/proc"));
    const QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& pidStr : entries) {
        bool isPid = false;
        (void)pidStr.toInt(&isPid);
        if (!isPid) {
            continue;
        }
        QFile commFile(QStringLiteral("/proc/%1/comm").arg(pidStr));
        if (commFile.open(QIODevice::ReadOnly)) {
            const QString comm = QString::fromUtf8(commFile.readLine()).trimmed();
            if (!comm.isEmpty()) {
                names.append(comm);
            }
        }
    }
#endif

    return names;
}

} // namespace kmedia
