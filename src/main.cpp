#include "app/ApplicationController.h"
#include "core/BoothSecurity.h"
#include "media/PdfViewerController.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>
#include <QTimer>

#include <clocale>

namespace {

bool parseWindowSize(const QString& raw, int& outWidth, int& outHeight)
{
    const QStringList parts = raw.trimmed().toLower().split(QLatin1Char('x'));
    if (parts.size() != 2) {
        return false;
    }
    bool okW = false;
    bool okH = false;
    const int w = parts.at(0).toInt(&okW);
    const int h = parts.at(1).toInt(&okH);
    if (!okW || !okH || w < 360 || h < 520) {
        return false;
    }
    outWidth = w;
    outHeight = h;
    return true;
}

} // namespace

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
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/KMedia/UI/icons/icon.png")));

    const QStringList interFontPaths = {
        QStringLiteral(":/qt/qml/KMedia/UI/fonts/Inter-Regular.ttf"),
        QStringLiteral(":/qt/qml/KMedia/UI/fonts/Inter-Medium.ttf"),
        QStringLiteral(":/qt/qml/KMedia/UI/fonts/Inter-SemiBold.ttf"),
        QStringLiteral(":/qt/qml/KMedia/UI/fonts/Inter-Bold.ttf"),
    };
    int loadedFontId = -1;
    for (const QString& fontPath : interFontPaths) {
        const int id = QFontDatabase::addApplicationFont(fontPath);
        if (loadedFontId < 0 && id >= 0) {
            loadedFontId = id;
        }
    }
    if (loadedFontId >= 0) {
        const QStringList families = QFontDatabase::applicationFontFamilies(loadedFontId);
        if (!families.isEmpty()) {
            app.setFont(QFont(families.constFirst()));
        }
    }

    // libmpv requires C numeric locale to parse decimal numbers consistently.
    std::setlocale(LC_NUMERIC, "C");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Native congregation media manager and presentation workstation"));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption demoOption(
        QStringList{QStringLiteral("demo")},
        QStringLiteral("Launch KMedia with isolated sample data for testing and previews."));
    const QCommandLineOption screenshotOption(
        QStringList{QStringLiteral("screenshot")},
        QStringLiteral("Render the main window to a PNG file and exit."), QStringLiteral("path"));
    const QCommandLineOption windowSizeOption(
        QStringList{QStringLiteral("window-size")},
        QStringLiteral("Override initial main window size (e.g. 1280x800 or 420x740)."),
        QStringLiteral("WxH"));
    const QCommandLineOption delayOption(
        QStringList{QStringLiteral("screenshot-delay")},
        QStringLiteral("Delay in milliseconds before capturing the screenshot (default 350)."),
        QStringLiteral("ms"), QStringLiteral("350"));
    const QCommandLineOption themeOption(
        QStringList{QStringLiteral("theme")},
        QStringLiteral("Force UI color theme at startup (dark or light)."), QStringLiteral("mode"));
    const QCommandLineOption tabOption(
        QStringList{QStringLiteral("tab")},
        QStringLiteral("Select initial workspace tab (meeting, calendar, media, playlists, "
                       "downloads, settings)."),
        QStringLiteral("name"));

    parser.addOption(demoOption);
    parser.addOption(screenshotOption);
    parser.addOption(windowSizeOption);
    parser.addOption(delayOption);
    parser.addOption(themeOption);
    parser.addOption(tabOption);
    parser.process(app);

    if (parser.isSet(demoOption)) {
        ApplicationController::setStartupDemoMode(true);
    }
    if (parser.isSet(themeOption)) {
        ApplicationController::setStartupThemeOverride(parser.value(themeOption));
    }
    if (parser.isSet(tabOption)) {
        ApplicationController::setStartupTabIndex(
            kmedia::BoothSecurity::parseTabArgument(parser.value(tabOption)));
    }

    QQmlApplicationEngine engine;
    if (auto* controller = engine.singletonInstance<ApplicationController*>(
            "KMedia.UI", "ApplicationController")) {
        controller->attachQmlEngine(&engine);
        engine.addImageProvider(QStringLiteral("kmedia-pdf"),
                                new kmedia::PdfPageImageProvider(controller->pdfController()));
    }

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);

    engine.loadFromModule("KMedia.UI", "Main");

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    if (auto* mainWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst())) {
        int customWidth = 0;
        int customHeight = 0;
        if (parser.isSet(windowSizeOption) &&
            parseWindowSize(parser.value(windowSizeOption), customWidth, customHeight)) {
            mainWindow->resize(customWidth, customHeight);
        }

        if (parser.isSet(screenshotOption)) {
            const QString outPath = parser.value(screenshotOption);
            bool okDelay = false;
            const int delayMs = qBound(100, parser.value(delayOption).toInt(&okDelay), 5000);
            const int effectiveDelay = okDelay ? delayMs : 350;

            QTimer::singleShot(effectiveDelay, mainWindow, [mainWindow, outPath]() {
                const QFileInfo outInfo(outPath);
                QDir().mkpath(outInfo.absolutePath());
                const QImage captured = mainWindow->grabWindow();
                const bool saved = !captured.isNull() && captured.save(outPath, "PNG");
                QCoreApplication::exit(saved ? 0 : 2);
            });
        }
    }

    return app.exec();
}
