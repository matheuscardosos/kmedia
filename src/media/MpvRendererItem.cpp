#include "MpvRendererItem.h"

#include <QByteArray>
#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QPointer>
#include <QQuickOpenGLUtils>
#include <QtGlobal>

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>

#include <cstring>
#include <memory>
#include <mutex>
#include <utility>

struct MpvRendererItem::WakeupBridge
{
    std::mutex mutex;
    QPointer<MpvRendererItem> item;
};

namespace {

void* resolveGlProcAddress(void* /*ctx*/, const char* name)
{
    auto* glContext = QOpenGLContext::currentContext();
    if (glContext == nullptr) {
        return nullptr;
    }

    const QFunctionPointer proc = glContext->getProcAddress(QByteArray(name));
    void* address = nullptr;
    static_assert(sizeof(address) == sizeof(proc), "Function pointer size mismatch");
    std::memcpy(&address, &proc, sizeof(address));
    return address;
}

void onMpvRenderUpdate(void* ctx)
{
    auto* bridge = static_cast<MpvRendererItem::WakeupBridge*>(ctx);
    if (bridge == nullptr) {
        return;
    }

    std::scoped_lock lock(bridge->mutex);
    const QPointer<MpvRendererItem> target = bridge->item;
    if (target.isNull()) {
        return;
    }

    QMetaObject::invokeMethod(
        target.data(),
        [target]() {
            if (!target.isNull()) {
                target->update();
            }
        },
        Qt::QueuedConnection);
}

void onMpvWakeup(void* ctx)
{
    auto* bridge = static_cast<MpvRendererItem::WakeupBridge*>(ctx);
    if (bridge == nullptr) {
        return;
    }

    std::scoped_lock lock(bridge->mutex);
    const QPointer<MpvRendererItem> target = bridge->item;
    if (target.isNull()) {
        return;
    }

    QMetaObject::invokeMethod(target.data(), "processMpvEvents", Qt::QueuedConnection);
}

class MpvFboRenderer final : public QQuickFramebufferObject::Renderer
{
public:
    MpvFboRenderer(std::shared_ptr<mpv_handle> mpv,
                   std::shared_ptr<MpvRendererItem::WakeupBridge> bridge)
        : m_mpv(std::move(mpv)), m_bridge(std::move(bridge))
    {
        if (m_mpv == nullptr || m_bridge == nullptr) {
            return;
        }

        mpv_opengl_init_params glInitParams{&resolveGlProcAddress, nullptr};
        int advancedControl = 1;

        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInitParams},
            {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advancedControl},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };

        if (mpv_render_context_create(&m_renderCtx, m_mpv.get(), params) < 0) {
            qWarning("KMedia: failed to initialize mpv OpenGL render context");
            m_renderCtx = nullptr;
            return;
        }

        mpv_render_context_set_update_callback(m_renderCtx, &onMpvRenderUpdate, m_bridge.get());
    }

    ~MpvFboRenderer() override
    {
        if (m_renderCtx != nullptr) {
            mpv_render_context_set_update_callback(m_renderCtx, nullptr, nullptr);
            mpv_render_context_free(m_renderCtx);
            m_renderCtx = nullptr;
        }
    }

    Q_DISABLE_COPY_MOVE(MpvFboRenderer)

    void synchronize(QQuickFramebufferObject* /*item*/) override {}

    [[nodiscard]] QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override
    {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        return new QOpenGLFramebufferObject(size, format);
    }

    void render() override
    {
        if (m_renderCtx == nullptr) {
            return;
        }

        QOpenGLFramebufferObject* fbo = framebufferObject();
        if (fbo == nullptr) {
            return;
        }

        mpv_opengl_fbo mpvFbo{
            static_cast<int>(fbo->handle()),
            fbo->width(),
            fbo->height(),
            0,
        };
        int flipY = 1;

        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mpvFbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flipY},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };

        mpv_render_context_render(m_renderCtx, params);
        QQuickOpenGLUtils::resetOpenGLState();
    }

private:
    std::shared_ptr<mpv_handle> m_mpv;
    std::shared_ptr<MpvRendererItem::WakeupBridge> m_bridge;
    mpv_render_context* m_renderCtx{nullptr};
};

} // namespace

MpvRendererItem::MpvRendererItem(QQuickItem* parent)
    : QQuickFramebufferObject(parent), m_wakeupBridge(std::make_shared<WakeupBridge>())
{
    {
        std::scoped_lock lock(m_wakeupBridge->mutex);
        m_wakeupBridge->item = this;
    }
    setTextureFollowsItemSize(true);
    initializeMpv();
}

MpvRendererItem::~MpvRendererItem()
{
    if (m_wakeupBridge != nullptr) {
        std::scoped_lock lock(m_wakeupBridge->mutex);
        m_wakeupBridge->item = nullptr;
    }
    if (m_mpv != nullptr) {
        mpv_set_wakeup_callback(m_mpv.get(), nullptr, nullptr);
    }
}

QQuickFramebufferObject::Renderer* MpvRendererItem::createRenderer() const
{
    return new MpvFboRenderer(m_mpv, m_wakeupBridge);
}

QUrl MpvRendererItem::source() const
{
    return m_source;
}

void MpvRendererItem::setSource(const QUrl& source)
{
    if (m_source == source) {
        return;
    }
    m_source = source;
    emit sourceChanged();

    if (source.isValid() && !source.isEmpty()) {
        loadFile(source);
    } else {
        stop();
    }
}

bool MpvRendererItem::isPaused() const
{
    return m_paused;
}

void MpvRendererItem::setPaused(bool paused)
{
    if (m_paused == paused) {
        return;
    }
    m_paused = paused;

    if (m_mpv != nullptr) {
        int flag = paused ? 1 : 0;
        mpv_set_property(m_mpv.get(), "pause", MPV_FORMAT_FLAG, &flag);
    }
    emit pausedChanged();
}

bool MpvRendererItem::isMediaLoaded() const
{
    return m_mediaLoaded;
}

QString MpvRendererItem::hardwareDecoder() const
{
    return m_hardwareDecoder;
}

std::shared_ptr<mpv_handle> MpvRendererItem::sharedHandle() const
{
    return m_mpv;
}

void MpvRendererItem::loadFile(const QUrl& url)
{
    if (m_mpv == nullptr || !url.isValid()) {
        return;
    }

    const QByteArray pathUtf8 =
        url.isLocalFile() ? url.toLocalFile().toUtf8() : url.toString().toUtf8();

    const char* cmd[] = {"loadfile", pathUtf8.constData(), nullptr};
    const int status = mpv_command_async(m_mpv.get(), 0, cmd);
    if (status < 0) {
        emit playbackError(QString::fromUtf8(mpv_error_string(status)));
    }
}

void MpvRendererItem::stop()
{
    if (m_mpv == nullptr) {
        return;
    }

    const char* cmd[] = {"stop", nullptr};
    mpv_command_async(m_mpv.get(), 0, cmd);
    if (m_mediaLoaded) {
        m_mediaLoaded = false;
        emit mediaLoadedChanged();
    }
}

void MpvRendererItem::initializeMpv()
{
    mpv_handle* rawHandle = mpv_create();
    if (rawHandle == nullptr) {
        qWarning("KMedia: failed to allocate mpv handle");
        return;
    }

    mpv_set_option_string(rawHandle, "vo", "libmpv");
    mpv_set_option_string(rawHandle, "hwdec", "auto-safe");
    mpv_set_option_string(rawHandle, "keep-open", "yes");
    mpv_set_option_string(rawHandle, "idle", "yes");
    mpv_set_option_string(rawHandle, "pause", "yes");
    mpv_set_option_string(rawHandle, "terminal", "no");

    if (mpv_initialize(rawHandle) < 0) {
        qWarning("KMedia: failed to initialize mpv instance");
        mpv_terminate_destroy(rawHandle);
        return;
    }

    m_mpv = std::shared_ptr<mpv_handle>(rawHandle, [bridge = m_wakeupBridge](mpv_handle* handle) {
        if (handle != nullptr) {
            mpv_set_wakeup_callback(handle, nullptr, nullptr);
            mpv_terminate_destroy(handle);
        }
    });

    mpv_observe_property(m_mpv.get(), 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv.get(), 0, "hwdec-current", MPV_FORMAT_STRING);
    mpv_set_wakeup_callback(m_mpv.get(), &onMpvWakeup, m_wakeupBridge.get());
}

void MpvRendererItem::processMpvEvents()
{
    if (m_mpv == nullptr) {
        return;
    }

    while (true) {
        const mpv_event* event = mpv_wait_event(m_mpv.get(), 0.0);
        if (event == nullptr || event->event_id == MPV_EVENT_NONE) {
            break;
        }

        switch (event->event_id) {
        case MPV_EVENT_FILE_LOADED:
            if (!m_mediaLoaded) {
                m_mediaLoaded = true;
                emit mediaLoadedChanged();
            }
            break;
        case MPV_EVENT_END_FILE:
            if (m_mediaLoaded) {
                m_mediaLoaded = false;
                emit mediaLoadedChanged();
            }
            break;
        case MPV_EVENT_PROPERTY_CHANGE: {
            const auto* prop = static_cast<const mpv_event_property*>(event->data);
            if (prop == nullptr || prop->name == nullptr || prop->data == nullptr) {
                break;
            }
            if (std::strcmp(prop->name, "pause") == 0 && prop->format == MPV_FORMAT_FLAG) {
                const bool paused = (*static_cast<const int*>(prop->data)) != 0;
                if (m_paused != paused) {
                    m_paused = paused;
                    emit pausedChanged();
                }
            } else if (std::strcmp(prop->name, "hwdec-current") == 0 &&
                       prop->format == MPV_FORMAT_STRING) {
                const char* value = *static_cast<char* const*>(prop->data);
                const QString decoder =
                    value != nullptr ? QString::fromUtf8(value) : QStringLiteral("none");
                if (m_hardwareDecoder != decoder) {
                    m_hardwareDecoder = decoder;
                    emit hardwareDecoderChanged();
                }
            }
            break;
        }
        default:
            break;
        }
    }
}
