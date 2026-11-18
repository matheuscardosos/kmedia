#include "media/PdfViewerController.h"

#include <QDir>
#include <QMetaObject>
#include <QThread>

namespace kmedia {

PdfViewerController::PdfViewerController(QObject* parent) : QObject(parent) {}

QUrl PdfViewerController::source() const
{
    return m_source;
}

bool PdfViewerController::isLoaded() const
{
    return m_document.status() == QPdfDocument::Status::Ready && m_document.pageCount() > 0;
}

int PdfViewerController::pageCount() const
{
    return isLoaded() ? m_document.pageCount() : 0;
}

int PdfViewerController::currentPage() const
{
    return m_currentPage;
}

int PdfViewerController::revision() const
{
    return m_revision;
}

QString PdfViewerController::currentPageImageUrl() const
{
    if (!isLoaded()) {
        return {};
    }
    return QStringLiteral("image://kmedia-pdf/%1/%2").arg(m_revision).arg(m_currentPage);
}

QSizeF PdfViewerController::currentPagePointSize() const
{
    if (!isLoaded()) {
        return {};
    }
    return m_document.pagePointSize(m_currentPage);
}

double PdfViewerController::zoomScale() const
{
    return m_zoomScale;
}

void PdfViewerController::setZoomScale(double zoomScale)
{
    const double clamped = qBound(1.0, zoomScale, 8.0);
    if (qFuzzyCompare(m_zoomScale, clamped)) {
        return;
    }
    m_zoomScale = clamped;
    emit viewportChanged();
}

double PdfViewerController::panX() const
{
    return m_panX;
}

void PdfViewerController::setPanX(double panX)
{
    const double clamped = qBound(-0.5, panX, 0.5);
    if (qFuzzyCompare(m_panX, clamped)) {
        return;
    }
    m_panX = clamped;
    emit viewportChanged();
}

double PdfViewerController::panY() const
{
    return m_panY;
}

void PdfViewerController::setPanY(double panY)
{
    const double clamped = qBound(-0.5, panY, 0.5);
    if (qFuzzyCompare(m_panY, clamped)) {
        return;
    }
    m_panY = clamped;
    emit viewportChanged();
}

QString PdfViewerController::errorMessage() const
{
    return m_errorMessage;
}

QImage PdfViewerController::renderPage(int pageIndex, const QSize& requestedSize)
{
    if (!isLoaded() || pageIndex < 0 || pageIndex >= m_document.pageCount()) {
        return {};
    }

    const QSizeF pts = m_document.pagePointSize(pageIndex);
    if (pts.width() <= 0.0 || pts.height() <= 0.0) {
        return {};
    }

    QSize renderSize;
    if (requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0) {
        renderSize = requestedSize;
    } else if (requestedSize.width() > 0) {
        const int h =
            qMax(1, static_cast<int>((pts.height() * requestedSize.width()) / pts.width()));
        renderSize = QSize(requestedSize.width(), h);
    } else {
        const int defaultWidth = 1920;
        const int h = qMax(1, static_cast<int>((pts.height() * defaultWidth) / pts.width()));
        renderSize = QSize(defaultWidth, h);
    }

    return m_document.render(pageIndex, renderSize);
}

bool PdfViewerController::load(const QUrl& url)
{
    const QString localPath =
        url.isLocalFile() ? QDir::cleanPath(url.toLocalFile()) : url.toString();
    if (localPath.trimmed().isEmpty()) {
        close();
        return false;
    }

    m_document.close();
    const QPdfDocument::Error err = m_document.load(localPath);
    if (err != QPdfDocument::Error::None || m_document.pageCount() <= 0) {
        m_source.clear();
        m_currentPage = 0;
        m_errorMessage = QStringLiteral("Failed to open PDF document");
        ++m_revision;
        emit documentChanged();
        emit currentPageChanged();
        return false;
    }

    m_source = url;
    m_currentPage = 0;
    m_errorMessage.clear();
    ++m_revision;
    resetViewport();
    emit documentChanged();
    emit currentPageChanged();
    return true;
}

void PdfViewerController::close()
{
    const bool hadDoc = isLoaded() || !m_source.isEmpty();
    m_document.close();
    m_source.clear();
    m_currentPage = 0;
    m_errorMessage.clear();
    ++m_revision;
    resetViewport();
    if (hadDoc) {
        emit documentChanged();
        emit currentPageChanged();
    }
}

void PdfViewerController::nextPage()
{
    if (isLoaded() && m_currentPage + 1 < m_document.pageCount()) {
        goToPage(m_currentPage + 1);
    }
}

void PdfViewerController::previousPage()
{
    if (isLoaded() && m_currentPage > 0) {
        goToPage(m_currentPage - 1);
    }
}

void PdfViewerController::goToPage(int pageIndex)
{
    if (!isLoaded()) {
        return;
    }
    const int clamped = qBound(0, pageIndex, m_document.pageCount() - 1);
    if (m_currentPage == clamped) {
        return;
    }
    m_currentPage = clamped;
    emit currentPageChanged();
}

void PdfViewerController::resetViewport()
{
    setZoomScale(1.0);
    setPanX(0.0);
    setPanY(0.0);
}

PdfPageImageProvider::PdfPageImageProvider(PdfViewerController* controller)
    : QQuickImageProvider(QQuickImageProvider::Image), m_controller(controller)
{
}

QImage PdfPageImageProvider::requestImage(const QString& id, QSize* size,
                                          const QSize& requestedSize)
{
    if (m_controller.isNull()) {
        return {};
    }

    const QStringList parts = id.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return {};
    }

    bool ok = false;
    const int pageIndex = parts.last().toInt(&ok);
    if (!ok) {
        return {};
    }

    QImage image;
    if (QThread::currentThread() == m_controller->thread()) {
        image = m_controller->renderPage(pageIndex, requestedSize);
    } else {
        QMetaObject::invokeMethod(
            m_controller.data(),
            [&]() { image = m_controller->renderPage(pageIndex, requestedSize); },
            Qt::BlockingQueuedConnection);
    }

    if (size != nullptr) {
        *size = image.size();
    }
    return image;
}

} // namespace kmedia
