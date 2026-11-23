#include "media/PdfPageImporter.h"

#include "media/PdfViewerController.h"
#include "models/PlaylistModel.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPdfDocument>
#include <QStandardPaths>
#include <QStringList>
#include <QThreadPool>
#include <QVariantMap>

namespace kmedia {

PdfPageImporter::PdfPageImporter(PlaylistModel* playlist, QObject* parent)
    : QObject(parent), m_playlist(playlist)
{
}

void PdfPageImporter::setCacheRoot(const QString& cacheRoot)
{
    m_cacheRoot = cacheRoot;
}

QString PdfPageImporter::fileName() const
{
    return QFileInfo(m_pdfPath).fileName();
}

int PdfPageImporter::pageCount() const
{
    return m_pageCount;
}

QVariantList PdfPageImporter::pages() const
{
    return m_pages;
}

bool PdfPageImporter::isBusy() const
{
    return m_busy;
}

void PdfPageImporter::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

int PdfPageImporter::countPages(const QString& pdfPath)
{
    QPdfDocument document;
    if (document.load(pdfPath) != QPdfDocument::Error::None) {
        return 0;
    }
    return document.pageCount();
}

QString PdfPageImporter::workFolder() const
{
    const QString root = m_cacheRoot.isEmpty()
                             ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                             : m_cacheRoot;
    const QByteArray key =
        QCryptographicHash::hash(m_pdfPath.toUtf8(), QCryptographicHash::Sha1).toHex().left(12);
    return QDir(root).filePath(QStringLiteral("PdfPages/") + QString::fromLatin1(key));
}

void PdfPageImporter::open(const QUrl& fileUrl)
{
    ++m_generation;
    m_pdfPath = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    m_pageCount = countPages(m_pdfPath);
    m_pages.clear();
    for (int page = 1; page <= m_pageCount; ++page) {
        m_pages.append(
            QVariantMap{{QStringLiteral("page"), page}, {QStringLiteral("thumbUrl"), QString()}});
    }
    emit documentChanged();
    emit pagesChanged();
    if (m_pageCount <= 0) {
        return;
    }

    // Thumbnails render one after the other on a worker and show up as they are ready.
    const quint64 generation = m_generation;
    const QString pdfPath = m_pdfPath;
    const QString folder = workFolder();
    const QPointer<PdfPageImporter> self(this);
    QThreadPool::globalInstance()->start([self, generation, pdfPath, folder]() {
        QPdfDocument document;
        if (document.load(pdfPath) != QPdfDocument::Error::None || !QDir().mkpath(folder)) {
            return;
        }
        for (int index = 0; index < document.pageCount(); ++index) {
            const QSizeF points = document.pagePointSize(index);
            const int height =
                points.width() > 0.0
                    ? qMax(1, static_cast<int>(points.height() * kThumbnailWidth / points.width()))
                    : kThumbnailWidth;
            const QImage image = document.render(index, QSize(kThumbnailWidth, height));
            const QString path =
                QDir(folder).filePath(QStringLiteral("thumb_%1.png").arg(index + 1));
            if (image.isNull() || !image.save(path, "PNG")) {
                continue;
            }
            if (self.isNull()) {
                return;
            }
            QMetaObject::invokeMethod(
                self.data(),
                [self, generation, index, path]() {
                    if (self.isNull() || generation != self->m_generation ||
                        index >= self->m_pages.size()) {
                        return;
                    }
                    QVariantMap entry = self->m_pages.at(index).toMap();
                    entry.insert(QStringLiteral("thumbUrl"), QUrl::fromLocalFile(path).toString());
                    self->m_pages[index] = entry;
                    emit self->pagesChanged();
                },
                Qt::QueuedConnection);
        }
    });
}

void PdfPageImporter::addPages(const QList<int>& pages, const QString& sectionId)
{
    if (m_pdfPath.isEmpty() || pages.isEmpty() || m_busy) {
        emit pagesAdded(0);
        return;
    }
    QStringList spec;
    for (const int page : pages) {
        spec.append(QString::number(page));
    }
    setBusy(true);
    const QString pdfPath = m_pdfPath;
    const QString folder = workFolder();
    const QString range = spec.join(QLatin1Char(','));
    const QPointer<PdfPageImporter> self(this);
    QThreadPool::globalInstance()->start([self, pdfPath, folder, range, sectionId]() {
        const QStringList pictures =
            PdfViewerController::exportPagesToPng(pdfPath, folder, range, kPictureWidth);
        if (self.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(
            self.data(),
            [self, pictures, sectionId]() {
                if (self.isNull()) {
                    return;
                }
                int added = 0;
                for (const QString& picture : pictures) {
                    if (!self->m_playlist.isNull() &&
                        self->m_playlist->importLocalFile(picture, sectionId)) {
                        ++added;
                    }
                }
                self->setBusy(false);
                emit self->pagesAdded(added);
            },
            Qt::QueuedConnection);
    });
}

void PdfPageImporter::addDocument(const QString& sectionId)
{
    const bool added = !m_playlist.isNull() && !m_pdfPath.isEmpty() &&
                       m_playlist->importPdfDocument(m_pdfPath, sectionId);
    emit pagesAdded(added ? 1 : 0);
}

void PdfPageImporter::close()
{
    ++m_generation;
    m_pdfPath.clear();
    m_pageCount = 0;
    m_pages.clear();
    emit documentChanged();
    emit pagesChanged();
}

} // namespace kmedia
