#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QVariantList>

namespace kmedia {

class PlaylistModel;

/**
 * Lets the operator pick which pages of a PDF to add as pictures, inspired by Meeting Media
 * Manager: the pages are shown as thumbnails rendered on a worker thread, and the picked pages
 * are rendered to full width PNG pictures and added to the day.
 */
class PdfPageImporter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName NOTIFY documentChanged FINAL)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY documentChanged FINAL)
    // { page, thumbUrl } for every page, the thumbnail URL empty until it is rendered.
    Q_PROPERTY(QVariantList pages READ pages NOTIFY pagesChanged FINAL)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged FINAL)

public:
    static constexpr int kThumbnailWidth = 320;
    static constexpr int kPictureWidth = 1920;

    explicit PdfPageImporter(PlaylistModel* playlist, QObject* parent = nullptr);

    void setCacheRoot(const QString& cacheRoot);
    [[nodiscard]] QString fileName() const;
    [[nodiscard]] int pageCount() const;
    [[nodiscard]] QVariantList pages() const;
    [[nodiscard]] bool isBusy() const;

    Q_INVOKABLE void open(const QUrl& fileUrl);
    Q_INVOKABLE void addPages(const QList<int>& pages, const QString& sectionId);
    Q_INVOKABLE void addDocument(const QString& sectionId);
    Q_INVOKABLE void close();

    // How many pages a PDF has; 0 when it cannot be opened.
    [[nodiscard]] static int countPages(const QString& pdfPath);

signals:
    void documentChanged();
    void pagesChanged();
    void busyChanged();
    void pagesAdded(int count);

private:
    [[nodiscard]] QString workFolder() const;
    void setBusy(bool busy);

    QPointer<PlaylistModel> m_playlist;
    QString m_cacheRoot;
    QString m_pdfPath;
    int m_pageCount{0};
    QVariantList m_pages;
    quint64 m_generation{0};
    bool m_busy{false};
};

} // namespace kmedia
