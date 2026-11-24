#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>

#include <memory>

class QCamera;
class QMediaCaptureSession;
class QMediaDevices;

namespace kmedia {

/**
 * A camera shown as the background of the presentation screen, inspired by Meeting Media
 * Manager: offered to sign language congregations, it takes the place of the standby picture and
 * the yeartext while nothing is presented. Qt Multimedia is only touched once the camera list is
 * asked for or a camera is shown, so the app starts as fast as without it.
 */
class CameraController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList cameras READ cameras NOTIFY camerasChanged FINAL)
    Q_PROPERTY(QString cameraId READ cameraId WRITE setCameraId NOTIFY cameraIdChanged FINAL)
    Q_PROPERTY(bool offered READ isOffered NOTIFY offeredChanged FINAL)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged FINAL)
    Q_PROPERTY(Problem problem READ problem NOTIFY problemChanged FINAL)

public:
    // Why the chosen camera is not showing; the UI words it.
    enum class Problem { None, NotConnected, PermissionDenied, Failed };
    Q_ENUM(Problem)

    explicit CameraController(QObject* parent = nullptr);
    ~CameraController() override;

    Q_DISABLE_COPY_MOVE(CameraController)

    // Each camera as {id, name}.
    [[nodiscard]] QVariantList cameras() const;
    Q_INVOKABLE void refreshCameras();

    [[nodiscard]] QString cameraId() const;
    void setCameraId(const QString& cameraId);

    // The camera is offered to sign language congregations, and kept once one is chosen.
    void setSignLanguage(bool signLanguage);
    [[nodiscard]] bool isOffered() const;

    [[nodiscard]] bool isActive() const;
    [[nodiscard]] Problem problem() const;

    // The QML VideoOutput of the presentation screen; null stops the camera.
    Q_INVOKABLE void bindVideoOutput(QObject* videoOutput);

signals:
    void camerasChanged();
    void cameraIdChanged();
    void offeredChanged();
    void activeChanged();
    void problemChanged();

private:
    void updateCapture();
    void startCamera();
    void stopCamera();
    void setProblem(Problem problem);

    QMediaDevices* m_devices{nullptr};
    std::unique_ptr<QMediaCaptureSession> m_session;
    std::unique_ptr<QCamera> m_camera;
    QPointer<QObject> m_videoOutput;
    QVariantList m_cameras;
    QString m_cameraId;
    Problem m_problem{Problem::None};
    bool m_signLanguage{false};
};

} // namespace kmedia
