#include "RuntimeInfo.h"

#include <QCoreApplication>
#include <QtGlobal>

#include <mpv/client.h>

namespace kmedia {

QString RuntimeInfo::appVersion()
{
    const QString version = QCoreApplication::applicationVersion();
    return version.isEmpty() ? QStringLiteral(KMEDIA_APP_VERSION) : version;
}

QString RuntimeInfo::qtVersion()
{
    return QString::fromLatin1(qVersion());
}

QString RuntimeInfo::mpvVersion()
{
    const unsigned long version = mpv_client_api_version();
    const unsigned long major = version >> 16;
    const unsigned long minor = version & 0xFFFFUL;
    return QStringLiteral("%1.%2").arg(major).arg(minor);
}

unsigned long RuntimeInfo::mpvApiVersionRaw()
{
    return mpv_client_api_version();
}

} // namespace kmedia
