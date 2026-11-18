#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>

#include <clocale>

int main(int argc, char* argv[])
{
    // QQuickFramebufferObject with mpv_render_context requires an OpenGL RHI backend.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QSurfaceFormat surfaceFormat;
    surfaceFormat.setDepthBufferSize(24);
    surfaceFormat.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(surfaceFormat);

    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("KMedia"));
    app.setOrganizationDomain(QStringLiteral("kmedia.app"));
    app.setApplicationName(QStringLiteral("KMedia"));
    app.setApplicationVersion(QStringLiteral(KMEDIA_APP_VERSION));

    // libmpv requires C numeric locale to parse decimal numbers consistently.
    std::setlocale(LC_NUMERIC, "C");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);

    engine.loadFromModule("KMedia.UI", "Main");

    return app.exec();
}
