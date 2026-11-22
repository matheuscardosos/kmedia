#include "media/IconImageProvider.h"

#include <QFile>
#include <QMutexLocker>
#include <QPainter>
#include <QRegularExpression>
#include <QSvgRenderer>

#include <utility>

namespace kmedia {

namespace {

constexpr int kDefaultIconPixels = 48;

} // namespace

IconImageProvider::IconImageProvider(QString iconRoot)
    : QQuickImageProvider(QQuickImageProvider::Image), m_iconRoot(std::move(iconRoot))
{
}

QByteArray IconImageProvider::svgBytes(const QString& category, const QString& name)
{
    const QString key = category + QLatin1Char('/') + name;
    const QMutexLocker lock(&m_mutex);
    if (const auto found = m_svgByIcon.constFind(key); found != m_svgByIcon.cend()) {
        return found.value();
    }
    QFile file(QStringLiteral("%1/%2/%3.svg").arg(m_iconRoot, category, name));
    const QByteArray bytes = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    m_svgByIcon.insert(key, bytes);
    return bytes;
}

QImage IconImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    // "<category>/<name>/<rrggbb>": only bundled icons, never a path of the caller's choosing.
    static const QRegularExpression kIconId(
        QStringLiteral("^(lucide|custom)/([a-z0-9-]+)/([0-9a-fA-F]{6})$"));
    const QRegularExpressionMatch match = kIconId.match(id);
    const QSize target = requestedSize.isValid() && !requestedSize.isEmpty()
                             ? requestedSize
                             : QSize(kDefaultIconPixels, kDefaultIconPixels);
    QImage image(target, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (match.hasMatch()) {
        QByteArray svg = svgBytes(match.captured(1), match.captured(2));
        // The icons are drawn in white; the requested color takes its place.
        svg.replace("#FFFFFF", "#" + match.captured(3).toLatin1());
        svg.replace("#ffffff", "#" + match.captured(3).toLatin1());
        QSvgRenderer renderer(svg);
        if (renderer.isValid()) {
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            renderer.render(&painter);
        }
    }
    if (size != nullptr) {
        *size = image.size();
    }
    return image;
}

} // namespace kmedia
