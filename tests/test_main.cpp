#include <QGuiApplication>

#include <catch2/catch_session.hpp>
#include <clocale>

int main(int argc, char* argv[])
{
#if defined(Q_OS_LINUX)
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && qEnvironmentVariableIsEmpty("DISPLAY") &&
        qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }
#endif

    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("KMedia"));
    app.setOrganizationDomain(QStringLiteral("kmedia.app"));
    app.setApplicationName(QStringLiteral("KMediaTests"));

    std::setlocale(LC_NUMERIC, "C");

    return Catch::Session().run(argc, argv);
}
