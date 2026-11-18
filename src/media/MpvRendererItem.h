#pragma once

#include <QQuickFramebufferObject>
#include <QString>
#include <QUrl>
#include <QtQmlIntegration/qqmlintegration.h>

#include <memory>

struct mpv_handle;

class MpvRendererItem : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged FINAL)
    Q_PROPERTY(bool paused READ isPaused WRITE setPaused NOTIFY pausedChanged FINAL)
    Q_PROPERTY(bool mediaLoaded READ isMediaLoaded NOTIFY mediaLoadedChanged FINAL)
    Q_PROPERTY(QString hardwareDecoder READ hardwareDecoder NOTIFY hardwareDecoderChanged FINAL)

public:
    struct WakeupBridge;

    explicit MpvRendererItem(QQuickItem* parent = nullptr);
    ~MpvRendererItem() override;

    Q_DISABLE_COPY_MOVE(MpvRendererItem)

    [[nodiscard]] Renderer* createRenderer() const override;

    [[nodiscard]] QUrl source() const;
    void setSource(const QUrl& source);

    [[nodiscard]] bool isPaused() const;
    void setPaused(bool paused);

    [[nodiscard]] bool isMediaLoaded() const;
    [[nodiscard]] QString hardwareDecoder() const;

    [[nodiscard]] std::shared_ptr<mpv_handle> sharedHandle() const;

public slots:
    void loadFile(const QUrl& url);
    void stop();

signals:
    void sourceChanged();
    void pausedChanged();
    void mediaLoadedChanged();
    void hardwareDecoderChanged();
    void playbackError(const QString& message);

private slots:
    void processMpvEvents();

private:
    void initializeMpv();

    std::shared_ptr<mpv_handle> m_mpv;
    std::shared_ptr<WakeupBridge> m_wakeupBridge;
    QUrl m_source;
    bool m_paused{true};
    bool m_mediaLoaded{false};
    QString m_hardwareDecoder{QStringLiteral("auto-safe")};
};
