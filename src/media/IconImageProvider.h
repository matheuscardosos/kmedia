#pragma once

#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QQuickImageProvider>

namespace kmedia {

/**
 * Draws the bundled white SVG icons in any color as plain images (`image://icon/<category>/
 * <name>/<rrggbb>`), so every icon is one cached texture instead of an extra shader layer.
 */
class IconImageProvider : public QQuickImageProvider
{
public:
    explicit IconImageProvider(QString iconRoot = QStringLiteral(":/qt/qml/KMedia/UI/icons"));

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    [[nodiscard]] QByteArray svgBytes(const QString& category, const QString& name);

    QString m_iconRoot;
    QMutex m_mutex;
    QHash<QString, QByteArray> m_svgByIcon;
};

} // namespace kmedia
