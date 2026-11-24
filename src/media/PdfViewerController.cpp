#include "media/PdfViewerController.h"

#include "media/PlaybackController.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QRegularExpression>
#include <QSet>
#include <QThread>

#include <algorithm>

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

std::vector<int> PdfViewerController::parsePageRange(const QString& rangeSpec, int pageCount)
{
    std::vector<int> pages;
    if (pageCount <= 0) {
        return pages;
    }

    const QString cleaned = rangeSpec.trimmed().toLower();
    if (cleaned.isEmpty() || cleaned == QStringLiteral("all") || cleaned == QStringLiteral("*")) {
        pages.reserve(static_cast<size_t>(pageCount));
        for (int p = 1; p <= pageCount; ++p) {
            pages.push_back(p);
        }
        return pages;
    }

    QSet<int> seen;
    const QStringList tokens = cleaned.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& rawToken : tokens) {
        const QString token = rawToken.trimmed();
        const qsizetype dashIdx = token.indexOf(QLatin1Char('-'));
        if (dashIdx > 0) {
            bool okA = false;
            bool okB = false;
            const int start = token.left(dashIdx).trimmed().toInt(&okA);
            const int end = token.mid(dashIdx + 1).trimmed().toInt(&okB);
            if (!okA || !okB) {
                continue;
            }
            const int lo = qBound(1, std::min(start, end), pageCount);
            const int hi = qBound(1, std::max(start, end), pageCount);
            for (int p = lo; p <= hi; ++p) {
                if (!seen.contains(p)) {
                    seen.insert(p);
                    pages.push_back(p);
                }
            }
        } else {
            bool ok = false;
            const int single = token.toInt(&ok);
            if (ok && single >= 1 && single <= pageCount && !seen.contains(single)) {
                seen.insert(single);
                pages.push_back(single);
            }
        }
    }
    return pages;
}

QStringList PdfViewerController::exportPagesToPng(const QString& pdfPath, const QString& destDir,
                                                  const QString& pageRangeSpec, int targetWidth,
                                                  QString* error)
{
    QStringList outPaths;
    const QString cleanPdf = QDir::cleanPath(pdfPath.trimmed());
    if (!QFileInfo::exists(cleanPdf)) {
        if (error != nullptr) {
            *error = QStringLiteral("PDF file does not exist");
        }
        return outPaths;
    }

    QPdfDocument doc;
    if (doc.load(cleanPdf) != QPdfDocument::Error::None || doc.pageCount() <= 0) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to open PDF document");
        }
        return outPaths;
    }

    const std::vector<int> pages = parsePageRange(pageRangeSpec, doc.pageCount());
    if (pages.empty()) {
        if (error != nullptr) {
            *error = QStringLiteral("No valid page numbers found in range");
        }
        return outPaths;
    }

    const QString cleanDest = QDir::cleanPath(destDir.trimmed());
    if (!QDir().mkpath(cleanDest)) {
        if (error != nullptr) {
            *error = QStringLiteral("Failed to create PDF output folder");
        }
        return outPaths;
    }

    const int widthPx = qBound(640, targetWidth, 3840);
    const QString baseName = QFileInfo(cleanPdf).completeBaseName();
    for (const int oneBasedPage : pages) {
        const int zeroIdx = oneBasedPage - 1;
        const QSizeF pts = doc.pagePointSize(zeroIdx);
        const int heightPx = (pts.width() > 0.0 && pts.height() > 0.0)
                                 ? qMax(1, static_cast<int>((pts.height() * widthPx) / pts.width()))
                                 : 1080;
        const QImage rendered = doc.render(zeroIdx, QSize(widthPx, heightPx));
        if (rendered.isNull()) {
            continue;
        }
        const QString imgPath = QDir(cleanDest).filePath(
            QStringLiteral("%1_p%2.png").arg(baseName).arg(oneBasedPage, 3, 10, QLatin1Char('0')));
        if (rendered.save(imgPath, "PNG")) {
            outPaths.append(imgPath);
        }
    }
    return outPaths;
}

PreviewFrameProvider::PreviewFrameProvider(PlaybackController* playback)
    : QQuickImageProvider(QQuickImageProvider::Image), m_playback(playback)
{
}

QImage PreviewFrameProvider::requestImage(const QString& /*id*/, QSize* size,
                                          const QSize& /*requestedSize*/)
{
    const QImage frame = m_playback.isNull() ? QImage() : m_playback->previewFrame();
    if (size != nullptr) {
        *size = frame.size();
    }
    return frame;
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
