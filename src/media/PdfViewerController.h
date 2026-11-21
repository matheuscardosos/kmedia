#pragma once

#include <QImage>
#include <QObject>
#include <QPdfDocument>
#include <QPointer>
#include <QQuickImageProvider>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <vector>

namespace kmedia {

class PdfViewerController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QUrl source READ source NOTIFY documentChanged FINAL)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY documentChanged FINAL)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY documentChanged FINAL)
    Q_PROPERTY(int currentPage READ currentPage WRITE goToPage NOTIFY currentPageChanged FINAL)
    Q_PROPERTY(int revision READ revision NOTIFY documentChanged FINAL)
    Q_PROPERTY(QString currentPageImageUrl READ currentPageImageUrl NOTIFY currentPageChanged FINAL)
    Q_PROPERTY(double zoomScale READ zoomScale WRITE setZoomScale NOTIFY viewportChanged FINAL)
    Q_PROPERTY(double panX READ panX WRITE setPanX NOTIFY viewportChanged FINAL)
    Q_PROPERTY(double panY READ panY WRITE setPanY NOTIFY viewportChanged FINAL)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY documentChanged FINAL)

public:
    explicit PdfViewerController(QObject* parent = nullptr);

    Q_DISABLE_COPY_MOVE(PdfViewerController)

    [[nodiscard]] QUrl source() const;
    [[nodiscard]] bool isLoaded() const;
    [[nodiscard]] int pageCount() const;
    [[nodiscard]] int currentPage() const;
    [[nodiscard]] int revision() const;
    [[nodiscard]] QString currentPageImageUrl() const;
    [[nodiscard]] QSizeF currentPagePointSize() const;
    [[nodiscard]] double zoomScale() const;
    void setZoomScale(double zoomScale);
    [[nodiscard]] double panX() const;
    void setPanX(double panX);
    [[nodiscard]] double panY() const;
    void setPanY(double panY);
    [[nodiscard]] QString errorMessage() const;

    [[nodiscard]] QImage renderPage(int pageIndex, const QSize& requestedSize);

    [[nodiscard]] static std::vector<int> parsePageRange(const QString& rangeSpec, int pageCount);
    [[nodiscard]] static QStringList
    exportPagesToPng(const QString& pdfPath, const QString& destDir, const QString& pageRangeSpec,
                     int targetWidth = 1920, QString* error = nullptr);

public slots:
    [[nodiscard]] bool load(const QUrl& url);
    void close();
    void nextPage();
    void previousPage();
    void goToPage(int pageIndex);
    void resetViewport();

signals:
    void documentChanged();
    void currentPageChanged();
    void viewportChanged();

private:
    QPdfDocument m_document;
    QUrl m_source;
    int m_currentPage{0};
    int m_revision{0};
    double m_zoomScale{1.0};
    double m_panX{0.0};
    double m_panY{0.0};
    QString m_errorMessage;
};

class PdfPageImageProvider final : public QQuickImageProvider
{
public:
    explicit PdfPageImageProvider(PdfViewerController* controller);

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    QPointer<PdfViewerController> m_controller;
};

} // namespace kmedia
