#include "media/CameraController.h"

#include <QCamera>
#include <QCameraDevice>
#include <QCoreApplication>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QVariantMap>

#if QT_CONFIG(permissions)
#include <QPermissions>
#endif

namespace kmedia {

namespace {

QCameraDevice findCamera(const QString& cameraId)
{
    const QByteArray id = cameraId.toUtf8();
    for (const QCameraDevice& device : QMediaDevices::videoInputs()) {
        if (device.id() == id) {
            return device;
        }
    }
    return {};
}

} // namespace

CameraController::CameraController(QObject* parent) : QObject(parent) {}

CameraController::~CameraController()
{
    stopCamera();
}

QVariantList CameraController::cameras() const
{
    return m_cameras;
}

void CameraController::refreshCameras()
{
    if (m_devices == nullptr) {
        m_devices = new QMediaDevices(this);
        connect(m_devices, &QMediaDevices::videoInputsChanged, this, [this]() {
            refreshCameras();
            updateCapture();
        });
    }
    QVariantList cameras;
    for (const QCameraDevice& device : QMediaDevices::videoInputs()) {
        cameras.append(QVariantMap{{QStringLiteral("id"), QString::fromUtf8(device.id())},
                                   {QStringLiteral("name"), device.description()}});
    }
    if (cameras != m_cameras) {
        m_cameras = cameras;
        emit camerasChanged();
    }
}

QString CameraController::cameraId() const
{
    return m_cameraId;
}

void CameraController::setCameraId(const QString& cameraId)
{
    if (m_cameraId == cameraId) {
        return;
    }
    const bool wasOffered = isOffered();
    m_cameraId = cameraId;
    emit cameraIdChanged();
    if (wasOffered != isOffered()) {
        emit offeredChanged();
    }
    setProblem(Problem::None);
    updateCapture();
}

void CameraController::setSignLanguage(bool signLanguage)
{
    if (m_signLanguage == signLanguage) {
        return;
    }
    const bool wasOffered = isOffered();
    m_signLanguage = signLanguage;
    if (wasOffered != isOffered()) {
        emit offeredChanged();
    }
}

bool CameraController::isOffered() const
{
    return m_signLanguage || !m_cameraId.isEmpty();
}

bool CameraController::isActive() const
{
    return m_camera != nullptr && m_camera->isActive();
}

CameraController::Problem CameraController::problem() const
{
    return m_problem;
}

void CameraController::setProblem(Problem problem)
{
    if (m_problem != problem) {
        m_problem = problem;
        emit problemChanged();
    }
}

void CameraController::bindVideoOutput(QObject* videoOutput)
{
    if (m_videoOutput == videoOutput) {
        return;
    }
    stopCamera();
    m_videoOutput = videoOutput;
    updateCapture();
}

void CameraController::updateCapture()
{
    if (m_cameraId.isEmpty() || m_videoOutput.isNull()) {
        stopCamera();
        return;
    }
    if (m_camera != nullptr && m_camera->isActive() &&
        m_camera->cameraDevice().id() == m_cameraId.toUtf8()) {
        return;
    }
#if QT_CONFIG(permissions)
    const QCameraPermission permission;
    switch (qApp->checkPermission(permission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(permission, this, [this]() { updateCapture(); });
        return;
    case Qt::PermissionStatus::Denied:
        stopCamera();
        setProblem(Problem::PermissionDenied);
        return;
    case Qt::PermissionStatus::Granted:
        break;
    }
#endif
    startCamera();
}

void CameraController::startCamera()
{
    stopCamera();
    const QCameraDevice device = findCamera(m_cameraId);
    if (device.isNull()) {
        // Watch for the camera to be plugged in.
        if (m_devices == nullptr) {
            refreshCameras();
        }
        setProblem(Problem::NotConnected);
        return;
    }
    if (m_session == nullptr) {
        m_session = std::make_unique<QMediaCaptureSession>();
    }
    m_camera = std::make_unique<QCamera>(device);
    connect(m_camera.get(), &QCamera::activeChanged, this, &CameraController::activeChanged);
    connect(m_camera.get(), &QCamera::errorOccurred, this,
            [this](QCamera::Error error, const QString& /*errorString*/) {
                if (error != QCamera::NoError) {
                    setProblem(Problem::Failed);
                }
            });
    m_session->setCamera(m_camera.get());
    m_session->setVideoOutput(m_videoOutput.data());
    setProblem(Problem::None);
    m_camera->start();
}

void CameraController::stopCamera()
{
    if (m_camera == nullptr) {
        return;
    }
    const bool wasActive = m_camera->isActive();
    m_camera->disconnect(this);
    m_camera->stop();
    if (m_session != nullptr) {
        m_session->setCamera(nullptr);
        m_session->setVideoOutput(nullptr);
    }
    m_camera.reset();
    if (wasActive) {
        emit activeChanged();
    }
}

} // namespace kmedia
