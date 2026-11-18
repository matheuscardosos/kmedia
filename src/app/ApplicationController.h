#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

#include <memory>

namespace kmedia {
class ProfileManager;
class ThumbnailCache;
} // namespace kmedia

class ApplicationController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString appVersion READ appVersion CONSTANT FINAL)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT FINAL)
    Q_PROPERTY(QString mpvVersion READ mpvVersion CONSTANT FINAL)
    Q_PROPERTY(QString activeProfileName READ activeProfileName NOTIFY activeProfileChanged FINAL)
    Q_PROPERTY(QString memoryUsage READ memoryUsage CONSTANT FINAL)

public:
    explicit ApplicationController(QObject* parent = nullptr);
    ~ApplicationController() override;

    Q_DISABLE_COPY_MOVE(ApplicationController)

    [[nodiscard]] QString appVersion() const;
    [[nodiscard]] QString qtVersion() const;
    [[nodiscard]] QString mpvVersion() const;
    [[nodiscard]] QString activeProfileName() const;
    [[nodiscard]] QString memoryUsage() const;

signals:
    void activeProfileChanged();

private:
    std::unique_ptr<kmedia::ProfileManager> m_profileManager;
    std::unique_ptr<kmedia::ThumbnailCache> m_thumbnailCache;
};
