#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>

#include <utility>

namespace kmedia {

/**
 * Lightweight native OS process detector for booth companion applications (OBS Studio and Zoom).
 * Uses OS snapshot APIs directly without child shell processes.
 */
class ProcessMonitor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool obsRunning READ isObsRunning NOTIFY statusChanged FINAL)
    Q_PROPERTY(bool zoomRunning READ isZoomRunning NOTIFY statusChanged FINAL)
    Q_PROPERTY(bool monitoring READ isMonitoring NOTIFY monitoringChanged FINAL)
    Q_PROPERTY(int intervalMs READ intervalMs WRITE setIntervalMs NOTIFY intervalMsChanged FINAL)

public:
    explicit ProcessMonitor(QObject* parent = nullptr);
    ~ProcessMonitor() override = default;

    Q_DISABLE_COPY_MOVE(ProcessMonitor)

    [[nodiscard]] bool isObsRunning() const;
    [[nodiscard]] bool isZoomRunning() const;
    [[nodiscard]] bool isMonitoring() const;
    [[nodiscard]] int intervalMs() const;
    void setIntervalMs(int intervalMs);

    Q_INVOKABLE void startMonitoring();
    Q_INVOKABLE void stopMonitoring();
    Q_INVOKABLE void refreshNow();

    [[nodiscard]] static QStringList queryRunningProcessNames();
    [[nodiscard]] static std::pair<bool, bool>
    classifyProcessNames(const QStringList& processNames);

    void applySimulatedState(bool obsRunning, bool zoomRunning);

signals:
    void statusChanged();
    void monitoringChanged();
    void intervalMsChanged();

private:
    QTimer m_timer;
    int m_intervalMs{3000};
    bool m_obsRunning{false};
    bool m_zoomRunning{false};
};

} // namespace kmedia
