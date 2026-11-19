#include "models/PlaylistModel.h"

#include "core/DatabaseManager.h"
#include "core/SystemUtils.h"
#include "media/PlaybackController.h"
#include "media/ThumbnailCache.h"
#include "parsers/JwlPlaylistParser.h"
#include "parsers/JwpubParser.h"

#include <QColor>
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTemporaryDir>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace kmedia {

namespace {

bool createScenicDemoSlide(const QString& filePath, int styleIndex)
{
    QImage img(960, 540, QImage::Format_RGBA8888);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto drawMountainLayer = [&](const QColor& color, double baseY, double p1, double p2,
                                       double p3, double p4) {
        QPainterPath ridge;
        ridge.moveTo(0, 540);
        ridge.lineTo(0, baseY);
        ridge.lineTo(220, p1);
        ridge.lineTo(470, p2);
        ridge.lineTo(730, p3);
        ridge.lineTo(960, p4);
        ridge.lineTo(960, 540);
        ridge.closeSubpath();
        p.setPen(Qt::NoPen);
        p.fillPath(ridge, color);
    };

    switch (styleIndex % 8) {
    case 0: {
        // Golden mountain sunrise
        QLinearGradient sky(0, 0, 0, 540);
        sky.setColorAt(0.0, QColor(34, 52, 82));
        sky.setColorAt(0.45, QColor(148, 128, 124));
        sky.setColorAt(0.75, QColor(242, 169, 86));
        sky.setColorAt(1.0, QColor(36, 44, 62));
        p.fillRect(img.rect(), sky);

        QRadialGradient sun(485, 295, 290);
        sun.setColorAt(0.0, QColor(255, 245, 200, 235));
        sun.setColorAt(0.25, QColor(255, 194, 92, 175));
        sun.setColorAt(1.0, QColor(255, 150, 60, 0));
        p.fillRect(img.rect(), sun);

        drawMountainLayer(QColor(54, 68, 92, 220), 330, 275, 320, 260, 315);
        drawMountainLayer(QColor(32, 42, 58, 245), 395, 345, 392, 338, 385);
        drawMountainLayer(QColor(22, 28, 38), 455, 430, 448, 420, 460);
        break;
    }
    case 1: {
        // Calm forest lake & mountain reflection
        QLinearGradient sky(0, 0, 0, 320);
        sky.setColorAt(0.0, QColor(68, 118, 156));
        sky.setColorAt(1.0, QColor(186, 212, 222));
        p.fillRect(0, 0, 960, 320, sky);

        drawMountainLayer(QColor(52, 92, 96), 270, 205, 250, 195, 245);
        drawMountainLayer(QColor(30, 64, 58), 305, 265, 295, 268, 298);

        QLinearGradient water(0, 320, 0, 540);
        water.setColorAt(0.0, QColor(72, 116, 134));
        water.setColorAt(0.5, QColor(42, 78, 92));
        water.setColorAt(1.0, QColor(24, 48, 58));
        p.fillRect(0, 320, 960, 220, water);

        p.setPen(QPen(QColor(210, 234, 242, 45), 2));
        for (int y = 345; y < 525; y += 22) {
            p.drawLine(140, y, 820, y);
        }
        break;
    }
    case 2: {
        // Warm study desk with open scripture volume
        QLinearGradient wall(0, 0, 960, 540);
        wall.setColorAt(0.0, QColor(56, 38, 26));
        wall.setColorAt(0.6, QColor(102, 68, 42));
        wall.setColorAt(1.0, QColor(38, 24, 16));
        p.fillRect(img.rect(), wall);

        QRadialGradient lamp(480, 220, 360);
        lamp.setColorAt(0.0, QColor(255, 226, 178, 175));
        lamp.setColorAt(0.5, QColor(218, 148, 86, 95));
        lamp.setColorAt(1.0, QColor(60, 36, 22, 0));
        p.fillRect(img.rect(), lamp);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(68, 46, 30));
        p.drawRoundedRect(QRectF(230, 170, 500, 255), 14, 14);
        p.setBrush(QColor(244, 236, 220));
        p.drawRoundedRect(QRectF(248, 184, 228, 224), 8, 8);
        p.drawRoundedRect(QRectF(484, 184, 228, 224), 8, 8);
        p.setPen(QPen(QColor(186, 172, 148), 3));
        for (int line = 0; line < 7; ++line) {
            const int y = 216 + line * 24;
            p.drawLine(274, y, 450, y);
            p.drawLine(510, y, 686, y);
        }
        break;
    }
    case 3: {
        // Warm auditorium spotlight & stage backdrop
        QLinearGradient stage(0, 0, 960, 540);
        stage.setColorAt(0.0, QColor(26, 30, 40));
        stage.setColorAt(0.55, QColor(52, 44, 42));
        stage.setColorAt(1.0, QColor(22, 20, 22));
        p.fillRect(img.rect(), stage);

        QRadialGradient keyLight(380, 140, 380);
        keyLight.setColorAt(0.0, QColor(255, 198, 134, 145));
        keyLight.setColorAt(0.6, QColor(255, 140, 72, 55));
        keyLight.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(img.rect(), keyLight);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(74, 52, 38));
        p.drawRoundedRect(QRectF(660, 90, 220, 450), 10, 10);
        p.setBrush(QColor(98, 68, 48));
        p.drawRoundedRect(QRectF(285, 290, 150, 250), 8, 8);
        p.setBrush(QColor(220, 214, 204, 190));
        p.drawEllipse(QRectF(315, 155, 90, 96));
        break;
    }
    case 4: {
        // Morning misty valley
        QLinearGradient sky(0, 0, 0, 540);
        sky.setColorAt(0.0, QColor(124, 148, 162));
        sky.setColorAt(0.5, QColor(228, 208, 174));
        sky.setColorAt(1.0, QColor(88, 106, 86));
        p.fillRect(img.rect(), sky);

        QRadialGradient haze(320, 240, 320);
        haze.setColorAt(0.0, QColor(255, 244, 210, 200));
        haze.setColorAt(1.0, QColor(255, 224, 168, 0));
        p.fillRect(img.rect(), haze);

        drawMountainLayer(QColor(110, 132, 118, 210), 310, 260, 295, 250, 305);
        drawMountainLayer(QColor(72, 98, 80, 240), 390, 340, 385, 345, 392);
        break;
    }
    case 5: {
        // Warm studio conversational lighting
        QLinearGradient bg(0, 0, 960, 540);
        bg.setColorAt(0.0, QColor(58, 72, 68));
        bg.setColorAt(0.5, QColor(112, 120, 96));
        bg.setColorAt(1.0, QColor(42, 46, 44));
        p.fillRect(img.rect(), bg);

        QRadialGradient warmWindow(720, 180, 380);
        warmWindow.setColorAt(0.0, QColor(255, 232, 188, 165));
        warmWindow.setColorAt(1.0, QColor(255, 190, 110, 0));
        p.fillRect(img.rect(), warmWindow);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(44, 66, 88, 210));
        p.drawRoundedRect(QRectF(195, 275, 210, 265), 18, 18);
        p.setBrush(QColor(186, 134, 78, 210));
        p.drawRoundedRect(QRectF(555, 275, 210, 265), 18, 18);
        break;
    }
    case 6: {
        // Alpine twilight sky
        QLinearGradient sky(0, 0, 0, 540);
        sky.setColorAt(0.0, QColor(44, 92, 148));
        sky.setColorAt(0.55, QColor(112, 168, 214));
        sky.setColorAt(1.0, QColor(28, 56, 84));
        p.fillRect(img.rect(), sky);

        drawMountainLayer(QColor(58, 108, 148, 220), 300, 235, 290, 215, 295);
        drawMountainLayer(QColor(34, 72, 104, 245), 375, 320, 368, 315, 382);
        break;
    }
    default: {
        // Sunset ocean clouds horizon
        QLinearGradient dusk(0, 0, 0, 540);
        dusk.setColorAt(0.0, QColor(74, 110, 152));
        dusk.setColorAt(0.45, QColor(208, 154, 134));
        dusk.setColorAt(0.75, QColor(255, 194, 128));
        dusk.setColorAt(1.0, QColor(126, 142, 168));
        p.fillRect(img.rect(), dusk);

        QRadialGradient glow(480, 320, 280);
        glow.setColorAt(0.0, QColor(255, 248, 224, 220));
        glow.setColorAt(0.4, QColor(255, 186, 114, 130));
        glow.setColorAt(1.0, QColor(255, 140, 80, 0));
        p.fillRect(img.rect(), glow);
        break;
    }
    }

    p.end();
    return img.save(filePath, "PNG");
}

QString aspectRatioLabel(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return {};
    }
    const double ratio = static_cast<double>(width) / static_cast<double>(height);
    if (qAbs(ratio - (16.0 / 9.0)) < 0.05) {
        return QStringLiteral("%1 x %2 (16:9)").arg(width).arg(height);
    }
    if (qAbs(ratio - (4.0 / 3.0)) < 0.05) {
        return QStringLiteral("%1 x %2 (4:3)").arg(width).arg(height);
    }
    return QStringLiteral("%1 x %2").arg(width).arg(height);
}

} // namespace

PlaylistModel::PlaylistModel(QObject* parent) : QAbstractListModel(parent) {}

void PlaylistModel::attachDatabase(DatabaseManager* database, qint64 playlistId)
{
    m_database = database;
    m_playlistId = playlistId;

    if (m_database == nullptr || !m_database->isOpen()) {
        return;
    }

    if (m_playlistId <= 0) {
        const std::vector<PlaylistRecord> existing = m_database->listPlaylists();
        if (!existing.empty()) {
            m_playlistId = existing.front().id;
            setPlaylistName(existing.front().name);
        } else {
            PlaylistRecord defaultPl;
            defaultPl.name = m_playlistName;
            defaultPl.meetingType = QStringLiteral("midweek");
            m_playlistId = m_database->createPlaylist(defaultPl);
        }
    }
    refreshAvailablePlaylists();
    loadFromDatabase();
}

void PlaylistModel::attachPlaybackController(PlaybackController* playback)
{
    m_playback = playback;
}

void PlaylistModel::attachThumbnailCache(ThumbnailCache* thumbnailCache)
{
    m_thumbnailCache = thumbnailCache;
}

void PlaylistModel::setPackageExtractDir(const QString& extractDir)
{
    m_extractDir = extractDir;
}

int PlaylistModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size())) {
        return {};
    }

    const PlaylistEntryView& item = m_items.at(static_cast<size_t>(index.row()));
    switch (role) {
    case EntryIdRole:
        return item.entryId;
    case TitleRole:
        return item.title;
    case SubtitleRole:
        return item.subtitle;
    case SectionTitleRole:
        return item.sectionTitle;
    case FilePathRole:
        return item.filePath;
    case ThumbnailUrlRole:
        return item.thumbnailUrl;
    case KindLabelRole:
        return item.kindLabel;
    case FormatLabelRole:
        return item.formatLabel;
    case ResolutionLabelRole:
        return item.resolutionLabel;
    case MediaKindRole:
        return item.mediaKind;
    case DurationMsRole:
        return item.durationMs;
    case FormattedDurationRole:
        return item.durationMs > 0 ? SystemUtils::formatDurationMs(item.durationMs)
                                   : QStringLiteral("--:--");
    case TrimStartMsRole:
        return item.trimStartMs;
    case TrimEndMsRole:
        return item.trimEndMs;
    case FileSizeBytesRole:
        return item.fileSizeBytes;
    case ZoomScaleRole:
        return item.zoomScale;
    case PanXRole:
        return item.panX;
    case PanYRole:
        return item.panY;
    case PdfPageRole:
        return item.pdfPage;
    case RepeatableRole:
        return item.repeatable;
    case FileExistsRole:
        return item.fileExists;
    case SourceKindRole:
        return item.sourceKind;
    case ActiveRole:
        return index.row() == m_activeIndex;
    default:
        return {};
    }
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    return {
        {EntryIdRole, "entryId"},
        {TitleRole, "title"},
        {SubtitleRole, "subtitle"},
        {SectionTitleRole, "section"},
        {FilePathRole, "filePath"},
        {ThumbnailUrlRole, "thumbUrl"},
        {KindLabelRole, "kindLabel"},
        {FormatLabelRole, "formatLabel"},
        {ResolutionLabelRole, "resolutionLabel"},
        {MediaKindRole, "mediaKind"},
        {DurationMsRole, "durationMs"},
        {FormattedDurationRole, "formattedDuration"},
        {TrimStartMsRole, "trimStart"},
        {TrimEndMsRole, "trimEnd"},
        {FileSizeBytesRole, "fileSizeBytes"},
        {ZoomScaleRole, "zoomScale"},
        {PanXRole, "panX"},
        {PanYRole, "panY"},
        {PdfPageRole, "pdfPage"},
        {RepeatableRole, "repeatable"},
        {FileExistsRole, "fileExists"},
        {SourceKindRole, "sourceKind"},
        {ActiveRole, "isActive"},
    };
}

int PlaylistModel::count() const
{
    return static_cast<int>(m_items.size());
}

int PlaylistModel::activeIndex() const
{
    return m_activeIndex;
}

QString PlaylistModel::playlistName() const
{
    return m_playlistName;
}

void PlaylistModel::setPlaylistName(const QString& name)
{
    const QString cleaned = name.trimmed();
    if (cleaned.isEmpty() || m_playlistName == cleaned) {
        return;
    }
    m_playlistName = cleaned;
    emit playlistNameChanged();
}

QString PlaylistModel::playlistSubtitle() const
{
    if (!m_playlistSubtitle.isEmpty()) {
        return m_playlistSubtitle;
    }
    return QDate::currentDate().toString(QStringLiteral("dddd, MMMM d, yyyy"));
}

void PlaylistModel::setPlaylistSubtitle(const QString& subtitle)
{
    const QString cleaned = subtitle.trimmed();
    if (m_playlistSubtitle == cleaned) {
        return;
    }
    m_playlistSubtitle = cleaned;
    emit playlistSubtitleChanged();
}

qint64 PlaylistModel::totalDurationMs() const
{
    qint64 sum = 0;
    for (const PlaylistEntryView& item : m_items) {
        if (item.durationMs > 0) {
            sum += item.durationMs;
        }
    }
    return sum;
}

QString PlaylistModel::formattedTotalDuration() const
{
    return SystemUtils::formatDurationMs(totalDurationMs());
}

QString PlaylistModel::activeTitle() const
{
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_items.size())) {
        return {};
    }
    return m_items.at(static_cast<size_t>(m_activeIndex)).title;
}

QString PlaylistModel::activeThumbnailUrl() const
{
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_items.size())) {
        return {};
    }
    return m_items.at(static_cast<size_t>(m_activeIndex)).thumbnailUrl;
}

QString PlaylistModel::activeFileName() const
{
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_items.size())) {
        return {};
    }
    return QFileInfo(m_items.at(static_cast<size_t>(m_activeIndex)).filePath).fileName();
}

QString PlaylistModel::activeFormatLabel() const
{
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_items.size())) {
        return {};
    }
    return m_items.at(static_cast<size_t>(m_activeIndex)).formatLabel;
}

QString PlaylistModel::activeResolutionLabel() const
{
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_items.size())) {
        return {};
    }
    return m_items.at(static_cast<size_t>(m_activeIndex)).resolutionLabel;
}

QString PlaylistModel::activeFileSizeFormatted() const
{
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_items.size())) {
        return {};
    }
    const qint64 bytes = m_items.at(static_cast<size_t>(m_activeIndex)).fileSizeBytes;
    return bytes > 0 ? SystemUtils::formatFileSize(bytes) : QStringLiteral("--");
}

QString PlaylistModel::activeSourceKind() const
{
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_items.size())) {
        return {};
    }
    return m_items.at(static_cast<size_t>(m_activeIndex)).sourceKind;
}

int PlaylistModel::activeMediaKind() const
{
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_items.size())) {
        return 0;
    }
    return m_items.at(static_cast<size_t>(m_activeIndex)).mediaKind;
}

qint64 PlaylistModel::activeDurationMs() const
{
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_items.size())) {
        return 0;
    }
    return m_items.at(static_cast<size_t>(m_activeIndex)).durationMs;
}

qint64 PlaylistModel::activeTrimStartMs() const
{
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_items.size())) {
        return 0;
    }
    return m_items.at(static_cast<size_t>(m_activeIndex)).trimStartMs;
}

qint64 PlaylistModel::activeTrimEndMs() const
{
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_items.size())) {
        return 0;
    }
    return m_items.at(static_cast<size_t>(m_activeIndex)).trimEndMs;
}

QStringList PlaylistModel::availablePlaylistNames() const
{
    return m_playlistNames;
}

int PlaylistModel::activePlaylistIndex() const
{
    const auto it = std::find(m_playlistIds.begin(), m_playlistIds.end(), m_playlistId);
    if (it == m_playlistIds.end()) {
        return m_playlistNames.isEmpty() ? -1 : 0;
    }
    return static_cast<int>(std::distance(m_playlistIds.begin(), it));
}

const std::vector<PlaylistEntryView>& PlaylistModel::items() const
{
    return m_items;
}

int PlaylistModel::importUrls(const QList<QUrl>& urls, const QString& defaultSection)
{
    int added = 0;
    for (const QUrl& url : urls) {
        const QString localPath = url.isLocalFile() ? url.toLocalFile() : url.toString();
        if (importLocalFile(localPath, defaultSection)) {
            ++added;
        }
    }
    return added;
}

bool PlaylistModel::importLocalFile(const QString& filePath, const QString& sectionTitle)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        return false;
    }

    const QString ext = info.suffix().toLower();
    if (ext == QStringLiteral("jwpub")) {
        const JwpubPackage pkg = JwpubParser::parse(filePath);
        if (!pkg.valid) {
            emit importNotice(pkg.errorMessage);
            return false;
        }
        std::vector<QString> assetPaths;
        for (const JwpubMediaItem& mm : pkg.mediaItems) {
            if (!mm.filePath.isEmpty()) {
                assetPaths.push_back(mm.filePath);
            }
        }
        const QString outDir = extractSubdirForPackage(filePath);
        std::vector<QString> extracted;
        QString err;
        if (!JwpubParser::extractMediaAssets(filePath, outDir, assetPaths, &extracted, &err)) {
            emit importNotice(err);
            return false;
        }
        int added = 0;
        for (const JwpubMediaItem& mm : pkg.mediaItems) {
            const QString target = QDir(outDir).filePath(mm.filePath);
            if (!QFileInfo::exists(target)) {
                continue;
            }
            PlaylistEntryView entry;
            entry.title = mm.label.isEmpty()
                              ? (mm.caption.isEmpty() ? mm.documentTitle : mm.caption)
                              : mm.label;
            if (entry.title.isEmpty()) {
                entry.title = QFileInfo(target).completeBaseName();
            }
            entry.sectionTitle = mm.documentTitle.isEmpty() ? sectionTitle : mm.documentTitle;
            entry.filePath = target;
            entry.sourceKind = QStringLiteral("jwpub");
            if (appendEntry(entry)) {
                ++added;
            }
        }
        return added > 0;
    }

    if (ext == QStringLiteral("jwlplaylist")) {
        const JwlPlaylistPackage pkg = JwlPlaylistParser::parse(filePath);
        if (!pkg.valid) {
            emit importNotice(pkg.errorMessage);
            return false;
        }
        const QString outDir = extractSubdirForPackage(filePath);
        std::vector<QString> extracted;
        QString err;
        if (!JwlPlaylistParser::extractAssets(filePath, outDir, {}, &extracted, &err)) {
            emit importNotice(err);
            return false;
        }
        int added = 0;
        for (const JwlPlaylistEntry& je : pkg.entries) {
            if (je.mediaArchivePath.isEmpty()) {
                continue;
            }
            const QString target = QDir(outDir).filePath(je.mediaArchivePath);
            if (!QFileInfo::exists(target)) {
                continue;
            }
            PlaylistEntryView entry;
            entry.title = je.label.isEmpty() ? je.originalFilename : je.label;
            if (entry.title.isEmpty()) {
                entry.title = QFileInfo(target).completeBaseName();
            }
            entry.sectionTitle = pkg.playlistName.isEmpty() ? sectionTitle : pkg.playlistName;
            entry.filePath = target;
            if (!je.thumbnailArchivePath.isEmpty()) {
                const QString thumbPath = QDir(outDir).filePath(je.thumbnailArchivePath);
                if (QFileInfo::exists(thumbPath)) {
                    entry.thumbnailUrl = QUrl::fromLocalFile(thumbPath).toString();
                }
            }
            entry.durationMs = je.durationMs;
            entry.trimStartMs = je.trimStartMs;
            entry.trimEndMs = je.trimEndMs;
            entry.repeatable = je.repeatLoop;
            entry.sourceKind = QStringLiteral("jwlplaylist");
            if (appendEntry(entry)) {
                ++added;
            }
        }
        return added > 0;
    }

    PlaylistEntryView entry;
    entry.title = info.completeBaseName();
    entry.sectionTitle = sectionTitle;
    entry.filePath = info.absoluteFilePath();
    entry.sourceKind = QStringLiteral("local");
    return appendEntry(entry);
}

bool PlaylistModel::appendEntry(const PlaylistEntryView& input)
{
    if (input.filePath.trimmed().isEmpty()) {
        return false;
    }

    PlaylistEntryView entry = input;
    enrichEntryMetadata(entry);

    if (m_database != nullptr && m_database->isOpen() && m_playlistId > 0) {
        MediaItemRecord mediaRec;
        mediaRec.filePath = entry.filePath;
        mediaRec.title = entry.title;
        mediaRec.mediaType = entry.kindLabel.toLower();
        mediaRec.durationMs = entry.durationMs;
        mediaRec.fileSize = entry.fileSizeBytes;
        mediaRec.sourceKind = entry.sourceKind;
        entry.mediaItemId = m_database->upsertMediaItem(mediaRec);

        PlaylistEntryRecord rowRec;
        rowRec.playlistId = m_playlistId;
        rowRec.mediaItemId = entry.mediaItemId;
        rowRec.sectionTitle = entry.sectionTitle;
        rowRec.customTitle = entry.title;
        rowRec.sortOrder = static_cast<int>(m_items.size());
        rowRec.trimStartMs = entry.trimStartMs;
        rowRec.trimEndMs = entry.trimEndMs;
        rowRec.zoomScale = entry.zoomScale;
        rowRec.panX = entry.panX;
        rowRec.panY = entry.panY;
        rowRec.pdfPage = entry.pdfPage;
        rowRec.isRepeatable = entry.repeatable;
        entry.entryId = m_database->addPlaylistEntry(rowRec);
    }

    const QString thumbSource = entry.filePath;
    const int mediaKind = entry.mediaKind;
    const int insertRow = static_cast<int>(m_items.size());
    beginInsertRows(QModelIndex(), insertRow, insertRow);
    m_items.push_back(std::move(entry));
    endInsertRows();
    emit countChanged();
    syncSectionsFromItems();

    if (m_thumbnailCache != nullptr && !thumbSource.isEmpty() &&
        (mediaKind == static_cast<int>(PlaybackController::MediaKind::Image) ||
         mediaKind == static_cast<int>(PlaybackController::MediaKind::Pdf))) {
        m_thumbnailCache->requestThumbnail(ThumbnailCache::makeKeyForFile(thumbSource),
                                           thumbSource);
    }
    return true;
}

bool PlaylistModel::moveItem(int fromIndex, int toIndex)
{
    const int size = static_cast<int>(m_items.size());
    if (fromIndex < 0 || fromIndex >= size || toIndex < 0 || toIndex >= size ||
        fromIndex == toIndex) {
        return false;
    }

    const QString targetSection = m_items.at(static_cast<size_t>(toIndex)).sectionTitle;
    const int destRow = (toIndex > fromIndex) ? (toIndex + 1) : toIndex;
    beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), destRow);

    PlaylistEntryView moved = m_items.at(static_cast<size_t>(fromIndex));
    const bool sectionChanged = !targetSection.isEmpty() && moved.sectionTitle != targetSection;
    if (sectionChanged) {
        moved.sectionTitle = targetSection;
    }
    m_items.erase(m_items.begin() + fromIndex);
    m_items.insert(m_items.begin() + toIndex, moved);

    if (m_activeIndex == fromIndex) {
        m_activeIndex = toIndex;
    } else if (fromIndex < m_activeIndex && toIndex >= m_activeIndex) {
        --m_activeIndex;
    } else if (fromIndex > m_activeIndex && toIndex <= m_activeIndex) {
        ++m_activeIndex;
    }

    endMoveRows();
    if (sectionChanged) {
        const QModelIndex changedIdx = createIndex(toIndex, 0);
        emit dataChanged(changedIdx, changedIdx, {SectionTitleRole});
        if (m_database != nullptr && m_database->isOpen() && m_playlistId > 0 &&
            moved.entryId > 0) {
            (void)m_database->updatePlaylistEntrySection(moved.entryId, m_playlistId,
                                                         moved.sectionTitle);
        }
        syncSectionsFromItems();
    }
    emit activeIndexChanged();
    persistReorder();
    return true;
}

bool PlaylistModel::removeAt(int index)
{
    const int size = static_cast<int>(m_items.size());
    if (index < 0 || index >= size) {
        return false;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_items.erase(m_items.begin() + index);
    endRemoveRows();

    if (m_activeIndex == index) {
        m_activeIndex =
            m_items.empty() ? -1 : std::min(index, static_cast<int>(m_items.size()) - 1);
    } else if (index < m_activeIndex) {
        --m_activeIndex;
    }

    emit countChanged();
    emit activeIndexChanged();
    syncSectionsFromItems();
    persistReorder();
    return true;
}

void PlaylistModel::clear()
{
    if (m_items.empty() && m_sections.empty()) {
        return;
    }
    beginResetModel();
    m_items.clear();
    m_sections.clear();
    m_activeIndex = -1;
    endResetModel();
    emit countChanged();
    emit activeIndexChanged();
    emit sectionsChanged();
}

bool PlaylistModel::activateIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
        return false;
    }

    const int prevIndex = m_activeIndex;
    m_activeIndex = index;

    if (prevIndex >= 0 && prevIndex < static_cast<int>(m_items.size())) {
        emit dataChanged(createIndex(prevIndex, 0), createIndex(prevIndex, 0), {ActiveRole});
    }
    emit dataChanged(createIndex(m_activeIndex, 0), createIndex(m_activeIndex, 0), {ActiveRole});
    emit activeIndexChanged();
    emit activeItemEdited();

    const PlaylistEntryView& item = m_items.at(static_cast<size_t>(m_activeIndex));
    if (!m_playback.isNull()) {
        m_playback->loadMediaWithKind(QUrl::fromLocalFile(item.filePath),
                                      static_cast<PlaybackController::MediaKind>(item.mediaKind),
                                      item.title, item.trimStartMs, item.trimEndMs);
        m_playback->setFallbackDurationMs(item.durationMs);
        m_playback->setViewport(item.zoomScale, item.panX, item.panY);
    }
    return true;
}

bool PlaylistModel::activateNext()
{
    if (m_items.empty()) {
        return false;
    }
    const int nextIdx =
        (m_activeIndex + 1 < static_cast<int>(m_items.size())) ? (m_activeIndex + 1) : 0;
    return activateIndex(nextIdx);
}

bool PlaylistModel::activatePrevious()
{
    if (m_items.empty()) {
        return false;
    }
    const int prevIdx =
        (m_activeIndex > 0) ? (m_activeIndex - 1) : (static_cast<int>(m_items.size()) - 1);
    return activateIndex(prevIdx);
}

void PlaylistModel::updateActiveTrim(qint64 startMs, qint64 endMs)
{
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_items.size())) {
        return;
    }
    const auto [cleanStart, cleanEnd] = PlaybackController::normalizeTrimRange(startMs, endMs);
    PlaylistEntryView& item = m_items.at(static_cast<size_t>(m_activeIndex));
    item.trimStartMs = cleanStart;
    item.trimEndMs = cleanEnd;

    if (!m_playback.isNull()) {
        m_playback->setTrimRange(cleanStart, cleanEnd);
    }
    emit dataChanged(createIndex(m_activeIndex, 0), createIndex(m_activeIndex, 0),
                     {TrimStartMsRole, TrimEndMsRole});
    emit activeItemEdited();
}

void PlaylistModel::updateActiveViewport(double zoomScale, double panX, double panY)
{
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_items.size())) {
        return;
    }
    PlaylistEntryView& item = m_items.at(static_cast<size_t>(m_activeIndex));
    item.zoomScale = qBound(1.0, zoomScale, 8.0);
    item.panX = qBound(-0.5, panX, 0.5);
    item.panY = qBound(-0.5, panY, 0.5);

    if (!m_playback.isNull()) {
        m_playback->setViewport(item.zoomScale, item.panX, item.panY);
    }
    emit dataChanged(createIndex(m_activeIndex, 0), createIndex(m_activeIndex, 0),
                     {ZoomScaleRole, PanXRole, PanYRole});
}

void PlaylistModel::resetActiveTrim()
{
    updateActiveTrim(0, 0);
}

bool PlaylistModel::switchPlaylistByIndex(int index)
{
    if (m_database == nullptr || !m_database->isOpen()) {
        return false;
    }
    if (index < 0 || index >= static_cast<int>(m_playlistIds.size())) {
        return false;
    }
    const qint64 targetId = m_playlistIds.at(static_cast<size_t>(index));
    if (targetId == m_playlistId) {
        return true;
    }
    m_playlistId = targetId;
    if (index < m_playlistNames.size()) {
        setPlaylistName(m_playlistNames.at(index));
    }
    loadFromDatabase();
    if (!m_items.empty()) {
        (void)activateIndex(0);
    }
    emit availablePlaylistsChanged();
    return true;
}

bool PlaylistModel::switchAdjacentPlaylist(int delta)
{
    const int total = static_cast<int>(m_playlistIds.size());
    if (total <= 1) {
        return false;
    }
    const int cur = activePlaylistIndex();
    const int base = (cur >= 0) ? cur : 0;
    const int next = ((base + delta) % total + total) % total;
    return switchPlaylistByIndex(next);
}

bool PlaylistModel::createNewPlaylist(const QString& name)
{
    const QString cleaned = name.trimmed();
    if (cleaned.isEmpty()) {
        return false;
    }
    if (m_database != nullptr && m_database->isOpen()) {
        PlaylistRecord rec;
        rec.name = cleaned;
        rec.meetingType = QStringLiteral("custom");
        const qint64 createdId = m_database->createPlaylist(rec);
        if (createdId <= 0) {
            return false;
        }
        m_playlistId = createdId;
        setPlaylistName(cleaned);
        refreshAvailablePlaylists();
        loadFromDatabase();
        emit availablePlaylistsChanged();
        return true;
    }
    setPlaylistName(cleaned);
    return true;
}

void PlaylistModel::populateDemoWorkspace(const QString& demoAssetDir)
{
    QDir().mkpath(demoAssetDir);
    clear();
    setPlaylistName(QStringLiteral("Midweek Meeting"));
    setPlaylistSubtitle(QStringLiteral("Week of November 16, 2026"));

    struct DemoSeed
    {
        const char* fileName;
        const char* section;
        const char* title;
        const char* subtitle;
        int mediaKind;
        qint64 durationMs;
        qint64 trimStartMs;
        qint64 trimEndMs;
        int styleIndex;
    };

    // Midweek sections mirror the meeting workbook order: Treasures From God's Word,
    // Apply Yourself to the Field Ministry, and Living as Christians.
    const std::vector<DemoSeed> midweekSeeds = {
        {"demo_01_opening.png", "Treasures From God's Word", "Opening Song", "Visual (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 12000, 0, 12000, 0},
        {"demo_02_song_126.png", "Treasures From God's Word", "Song 126", "Image (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 64000, 0, 0, 1},
        {"demo_03_bible_reading.png", "Treasures From God's Word", "Bible Reading: Proverbs 15:1-4",
         "Image (16:9)", static_cast<int>(PlaybackController::MediaKind::Image), 43000, 0, 0, 2},
        {"demo_04_talk_peaceable.png", "Apply Yourself to the Field Ministry",
         "Talk: Being Peaceable", "Visual (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 755000, 5000, 755000, 3},
        {"demo_05_song_129.png", "Living as Christians", "Song 129", "Image (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 62000, 0, 0, 4},
        {"demo_06_study.png", "Living as Christians", "Congregation Bible Study", "Visual (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 1100000, 0, 0, 5},
        {"demo_07_song_130.png", "Living as Christians", "Song 130", "Image (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 63000, 0, 0, 6},
        {"demo_08_closing.png", "Living as Christians", "Closing Song", "Visual (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 28000, 0, 0, 7},
    };

    for (const DemoSeed& seed : midweekSeeds) {
        const QString imgPath = QDir(demoAssetDir).filePath(QString::fromUtf8(seed.fileName));
        (void)createScenicDemoSlide(imgPath, seed.styleIndex);

        PlaylistEntryView entry;
        entry.title = QString::fromUtf8(seed.title);
        entry.subtitle = QString::fromUtf8(seed.subtitle);
        entry.sectionTitle = QString::fromUtf8(seed.section);
        entry.filePath = imgPath;
        entry.thumbnailUrl = QUrl::fromLocalFile(imgPath).toString();
        entry.mediaKind = seed.mediaKind;
        entry.durationMs = seed.durationMs;
        entry.trimStartMs = seed.trimStartMs;
        entry.trimEndMs = seed.trimEndMs;
        entry.sourceKind = QStringLiteral("demo");
        (void)appendEntry(entry);
    }

    if (m_database != nullptr && m_database->isOpen()) {
        const std::vector<PlaylistRecord> existing = m_database->listPlaylists();
        if (existing.size() < 2) {
            PlaylistRecord weekend;
            weekend.name = QStringLiteral("Weekend Meeting");
            weekend.meetingType = QStringLiteral("weekend");
            const qint64 weekendId = m_database->createPlaylist(weekend);
            if (weekendId > 0) {
                const qint64 savedId = m_playlistId;
                const QString imgPath =
                    QDir(demoAssetDir).filePath(QStringLiteral("demo_06_study.png"));
                MediaItemRecord rec;
                rec.filePath = imgPath;
                rec.title = QStringLiteral("Watchtower Study Theme Illustration");
                rec.mediaType = QStringLiteral("img");
                rec.durationMs = 95000;
                rec.fileSize = QFileInfo(imgPath).size();
                rec.sourceKind = QStringLiteral("demo");
                const qint64 mediaId = m_database->upsertMediaItem(rec);

                PlaylistEntryRecord row;
                row.playlistId = weekendId;
                row.mediaItemId = mediaId;
                row.sectionTitle = QStringLiteral("Watchtower Study");
                row.customTitle = rec.title;
                row.sortOrder = 0;
                (void)m_database->addPlaylistEntry(row);
                m_playlistId = savedId;
            }
        }
        refreshAvailablePlaylists();
    }

    if (!m_items.empty()) {
        (void)activateIndex(0);
    }
}

void PlaylistModel::refreshAvailablePlaylists()
{
    m_playlistIds.clear();
    m_playlistNames.clear();
    if (m_database != nullptr && m_database->isOpen()) {
        for (const PlaylistRecord& rec : m_database->listPlaylists()) {
            m_playlistIds.push_back(rec.id);
            m_playlistNames.append(rec.name);
        }
    }
    emit availablePlaylistsChanged();
}

void PlaylistModel::loadFromDatabase()
{
    if (m_database == nullptr || !m_database->isOpen() || m_playlistId <= 0) {
        return;
    }

    if (const auto rec = m_database->findPlaylistById(m_playlistId)) {
        setPlaylistName(rec->name);
    }

    const std::vector<PlaylistEntryRecord> rows = m_database->listPlaylistEntries(m_playlistId);
    beginResetModel();
    m_items.clear();
    m_activeIndex = -1;

    for (const PlaylistEntryRecord& row : rows) {
        const std::optional<MediaItemRecord> media = m_database->findMediaItemById(row.mediaItemId);
        if (!media.has_value()) {
            continue;
        }
        PlaylistEntryView view;
        view.entryId = row.id;
        view.mediaItemId = row.mediaItemId;
        view.title = row.customTitle.isEmpty() ? media->title : row.customTitle;
        view.sectionTitle = row.sectionTitle;
        view.filePath = media->filePath;
        view.durationMs = media->durationMs;
        view.trimStartMs = row.trimStartMs;
        view.trimEndMs = row.trimEndMs;
        view.zoomScale = row.zoomScale;
        view.panX = row.panX;
        view.panY = row.panY;
        view.pdfPage = row.pdfPage;
        view.repeatable = row.isRepeatable;
        view.sourceKind = media->sourceKind;
        enrichEntryMetadata(view);
        m_items.push_back(std::move(view));
    }

    endResetModel();
    loadSectionsForPlaylist();
    syncSectionsFromItems();
    emit countChanged();
    emit activeIndexChanged();
}

QVariantList PlaylistModel::sections() const
{
    QVariantList list;
    list.reserve(static_cast<qsizetype>(m_sections.size()));
    for (const PlaylistSectionEntry& sec : m_sections) {
        int itemCount = 0;
        int firstIdx = -1;
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
            if (m_items.at(static_cast<size_t>(i))
                    .sectionTitle.compare(sec.name, Qt::CaseInsensitive) == 0) {
                ++itemCount;
                if (firstIdx < 0) {
                    firstIdx = i;
                }
            }
        }
        QVariantMap map;
        map.insert(QStringLiteral("name"), sec.name);
        map.insert(QStringLiteral("color"), sec.colorHex);
        map.insert(QStringLiteral("itemCount"), itemCount);
        map.insert(QStringLiteral("firstItemIndex"), firstIdx);
        list.append(map);
    }
    return list;
}

QString PlaylistModel::defaultColorForSection(const QString& sectionTitle)
{
    const QString t = sectionTitle.trimmed().toLower();
    if (t.contains(QStringLiteral("treasure")) || t.contains(QStringLiteral("tesouro"))) {
        return QStringLiteral("#5F6B73");
    }
    if (t.contains(QStringLiteral("apply yourself")) || t.contains(QStringLiteral("ministério"))) {
        return QStringLiteral("#B7801F");
    }
    if (t.contains(QStringLiteral("living")) || t.contains(QStringLiteral("vida cristã"))) {
        return QStringLiteral("#9A1F33");
    }
    if (t.contains(QStringLiteral("public talk")) || t.contains(QStringLiteral("discurso"))) {
        return QStringLiteral("#3E6EA8");
    }
    if (t.contains(QStringLiteral("watchtower")) || t.contains(QStringLiteral("sentinela"))) {
        return QStringLiteral("#2F7D8C");
    }
    if (t.contains(QStringLiteral("overseer")) || t.contains(QStringLiteral("superintendente"))) {
        return QStringLiteral("#3F7F5F");
    }
    if (t.contains(QStringLiteral("memorial")) || t.contains(QStringLiteral("celebração"))) {
        return QStringLiteral("#8C3B66");
    }
    if (t.contains(QStringLiteral("assembly")) || t.contains(QStringLiteral("convention")) ||
        t.contains(QStringLiteral("assembleia")) || t.contains(QStringLiteral("congresso"))) {
        return QStringLiteral("#6B4E9B");
    }
    return QStringLiteral("#945EB5");
}

QString PlaylistModel::normalizeHexColor(const QString& raw, const QString& fallback)
{
    const QString cleaned = raw.trimmed();
    const QColor parsed(cleaned);
    if (parsed.isValid()) {
        return parsed.name(QColor::HexRgb).toUpper();
    }
    return fallback;
}

QString PlaylistModel::colorForSection(const QString& sectionName) const
{
    const QString key = sectionName.trimmed();
    if (key.isEmpty()) {
        return QStringLiteral("#945EB5");
    }
    const int idx = findSectionIndex(key);
    if (idx >= 0) {
        return m_sections.at(static_cast<size_t>(idx)).colorHex;
    }
    return defaultColorForSection(key);
}

bool PlaylistModel::ensureSection(const QString& name, const QString& colorHex)
{
    const QString cleaned = name.trimmed();
    if (cleaned.isEmpty()) {
        return false;
    }
    if (findSectionIndex(cleaned) >= 0) {
        if (!colorHex.trimmed().isEmpty()) {
            return setSectionColor(cleaned, colorHex);
        }
        return true;
    }
    const QString resolvedColor = normalizeHexColor(colorHex, defaultColorForSection(cleaned));
    m_sections.push_back(PlaylistSectionEntry{cleaned, resolvedColor});
    saveSectionsForPlaylist();
    emit sectionsChanged();
    return true;
}

bool PlaylistModel::createSection(const QString& name, const QString& colorHex)
{
    const QString cleaned = name.trimmed();
    if (cleaned.isEmpty() || findSectionIndex(cleaned) >= 0) {
        return false;
    }
    m_sections.push_back(PlaylistSectionEntry{
        cleaned, normalizeHexColor(colorHex, defaultColorForSection(cleaned))});
    saveSectionsForPlaylist();
    emit sectionsChanged();
    return true;
}

bool PlaylistModel::renameSection(const QString& oldName, const QString& newName)
{
    const QString oldClean = oldName.trimmed();
    const QString newClean = newName.trimmed();
    if (oldClean.isEmpty() || newClean.isEmpty()) {
        return false;
    }
    const int idx = findSectionIndex(oldClean);
    if (idx < 0) {
        return false;
    }
    const int clashIdx = findSectionIndex(newClean);
    if (clashIdx >= 0 && clashIdx != idx) {
        return false;
    }

    const QString previousExact = m_sections.at(static_cast<size_t>(idx)).name;
    m_sections[static_cast<size_t>(idx)].name = newClean;

    bool updatedAnyRow = false;
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        PlaylistEntryView& entry = m_items[static_cast<size_t>(i)];
        if (entry.sectionTitle.compare(previousExact, Qt::CaseInsensitive) == 0 ||
            entry.sectionTitle.compare(oldClean, Qt::CaseInsensitive) == 0) {
            entry.sectionTitle = newClean;
            updatedAnyRow = true;
            const QModelIndex rowIdx = createIndex(i, 0);
            emit dataChanged(rowIdx, rowIdx, {SectionTitleRole});
        }
    }

    if (updatedAnyRow && m_database != nullptr && m_database->isOpen() && m_playlistId > 0) {
        (void)m_database->renamePlaylistEntrySections(m_playlistId, previousExact, newClean);
    }

    saveSectionsForPlaylist();
    emit sectionsChanged();
    return true;
}

bool PlaylistModel::setSectionColor(const QString& sectionName, const QString& colorHex)
{
    const QString cleaned = sectionName.trimmed();
    if (cleaned.isEmpty()) {
        return false;
    }
    const QString normalized = normalizeHexColor(colorHex, QString{});
    if (normalized.isEmpty()) {
        return false;
    }

    int idx = findSectionIndex(cleaned);
    if (idx < 0) {
        m_sections.push_back(PlaylistSectionEntry{cleaned, normalized});
    } else {
        if (m_sections.at(static_cast<size_t>(idx)).colorHex == normalized) {
            return true;
        }
        m_sections[static_cast<size_t>(idx)].colorHex = normalized;
    }

    saveSectionsForPlaylist();
    emit sectionsChanged();
    return true;
}

bool PlaylistModel::removeSection(const QString& sectionName)
{
    const QString cleaned = sectionName.trimmed();
    const int idx = findSectionIndex(cleaned);
    if (idx < 0) {
        return false;
    }
    for (const PlaylistEntryView& item : m_items) {
        if (item.sectionTitle.compare(cleaned, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    m_sections.erase(m_sections.begin() + idx);
    saveSectionsForPlaylist();
    emit sectionsChanged();
    return true;
}

bool PlaylistModel::moveSection(const QString& sectionName, int delta)
{
    if (delta == 0 || m_sections.size() < 2) {
        return false;
    }
    const int idx = findSectionIndex(sectionName);
    if (idx < 0) {
        return false;
    }
    const int targetIdx = qBound(0, idx + delta, static_cast<int>(m_sections.size()) - 1);
    if (targetIdx == idx) {
        return false;
    }

    const PlaylistSectionEntry movedSec = m_sections.at(static_cast<size_t>(idx));
    m_sections.erase(m_sections.begin() + idx);
    m_sections.insert(m_sections.begin() + targetIdx, movedSec);

    if (!m_items.empty()) {
        const qint64 activeEntryId =
            (m_activeIndex >= 0 && m_activeIndex < static_cast<int>(m_items.size()))
                ? m_items.at(static_cast<size_t>(m_activeIndex)).entryId
                : -1;
        const QString activeFile =
            (m_activeIndex >= 0 && m_activeIndex < static_cast<int>(m_items.size()))
                ? m_items.at(static_cast<size_t>(m_activeIndex)).filePath
                : QString{};

        std::vector<PlaylistEntryView> regrouped;
        regrouped.reserve(m_items.size());
        std::vector<bool> taken(m_items.size(), false);

        for (const PlaylistSectionEntry& sec : m_sections) {
            for (size_t i = 0; i < m_items.size(); ++i) {
                if (!taken[i] &&
                    m_items[i].sectionTitle.compare(sec.name, Qt::CaseInsensitive) == 0) {
                    regrouped.push_back(m_items[i]);
                    taken[i] = true;
                }
            }
        }
        for (size_t i = 0; i < m_items.size(); ++i) {
            if (!taken[i]) {
                regrouped.push_back(m_items[i]);
            }
        }

        beginResetModel();
        m_items = std::move(regrouped);
        if (m_activeIndex >= 0) {
            m_activeIndex = -1;
            for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
                const PlaylistEntryView& cur = m_items.at(static_cast<size_t>(i));
                if ((activeEntryId > 0 && cur.entryId == activeEntryId) ||
                    (!activeFile.isEmpty() && cur.filePath == activeFile)) {
                    m_activeIndex = i;
                    break;
                }
            }
        }
        endResetModel();
        emit activeIndexChanged();
        persistReorder();
    }

    saveSectionsForPlaylist();
    emit sectionsChanged();
    return true;
}

bool PlaylistModel::assignItemToSection(int index, const QString& targetSection)
{
    const int size = static_cast<int>(m_items.size());
    const QString cleanedSection = targetSection.trimmed();
    if (index < 0 || index >= size || cleanedSection.isEmpty()) {
        return false;
    }

    (void)ensureSection(cleanedSection);

    int destIndex = -1;
    for (int i = 0; i < size; ++i) {
        if (i == index) {
            continue;
        }
        if (m_items.at(static_cast<size_t>(i))
                .sectionTitle.compare(cleanedSection, Qt::CaseInsensitive) == 0) {
            destIndex = i;
        }
    }

    if (destIndex < 0) {
        const int targetSecOrder = findSectionIndex(cleanedSection);
        destIndex = size - 1;
        for (int i = 0; i < size; ++i) {
            if (i == index) {
                continue;
            }
            const int rowSecOrder =
                findSectionIndex(m_items.at(static_cast<size_t>(i)).sectionTitle);
            if (rowSecOrder >= 0 && rowSecOrder > targetSecOrder) {
                destIndex = (i > index) ? (i - 1) : i;
                break;
            }
        }
    } else if (destIndex < index) {
        ++destIndex;
    }

    m_items[static_cast<size_t>(index)].sectionTitle = cleanedSection;
    if (m_database != nullptr && m_database->isOpen() && m_playlistId > 0 &&
        m_items[static_cast<size_t>(index)].entryId > 0) {
        (void)m_database->updatePlaylistEntrySection(m_items[static_cast<size_t>(index)].entryId,
                                                     m_playlistId, cleanedSection);
    }

    if (destIndex != index && destIndex >= 0 && destIndex < size) {
        const int rowMoveDest = (destIndex > index) ? (destIndex + 1) : destIndex;
        beginMoveRows(QModelIndex(), index, index, QModelIndex(), rowMoveDest);
        const PlaylistEntryView moved = m_items.at(static_cast<size_t>(index));
        m_items.erase(m_items.begin() + index);
        m_items.insert(m_items.begin() + destIndex, moved);

        if (m_activeIndex == index) {
            m_activeIndex = destIndex;
        } else if (index < m_activeIndex && destIndex >= m_activeIndex) {
            --m_activeIndex;
        } else if (index > m_activeIndex && destIndex <= m_activeIndex) {
            ++m_activeIndex;
        }
        endMoveRows();
        const QModelIndex changedIdx = createIndex(destIndex, 0);
        emit dataChanged(changedIdx, changedIdx, {SectionTitleRole});
        emit activeIndexChanged();
        persistReorder();
    } else {
        const QModelIndex changedIdx = createIndex(index, 0);
        emit dataChanged(changedIdx, changedIdx, {SectionTitleRole});
    }

    syncSectionsFromItems();
    return true;
}

int PlaylistModel::findSectionIndex(const QString& name) const
{
    const QString cleaned = name.trimmed();
    for (int i = 0; i < static_cast<int>(m_sections.size()); ++i) {
        if (m_sections.at(static_cast<size_t>(i)).name.compare(cleaned, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

void PlaylistModel::syncSectionsFromItems()
{
    bool changed = false;
    for (const PlaylistEntryView& item : m_items) {
        const QString sec = item.sectionTitle.trimmed();
        if (sec.isEmpty() || findSectionIndex(sec) >= 0) {
            continue;
        }
        m_sections.push_back(PlaylistSectionEntry{sec, defaultColorForSection(sec)});
        changed = true;
    }
    if (changed) {
        saveSectionsForPlaylist();
    }
    emit sectionsChanged();
}

void PlaylistModel::loadSectionsForPlaylist()
{
    m_sections.clear();
    if (m_database == nullptr || !m_database->isOpen() || m_playlistId <= 0) {
        return;
    }
    const QString key = QStringLiteral("playlist/sections_%1").arg(m_playlistId);
    const std::optional<QString> raw = m_database->getSetting(key);
    if (!raw.has_value() || raw->isEmpty()) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(raw->toUtf8());
    if (!doc.isArray()) {
        return;
    }
    for (const QJsonValue& val : doc.array()) {
        const QJsonObject obj = val.toObject();
        const QString name = obj.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty() || findSectionIndex(name) >= 0) {
            continue;
        }
        const QString color = normalizeHexColor(obj.value(QStringLiteral("color")).toString(),
                                                defaultColorForSection(name));
        m_sections.push_back(PlaylistSectionEntry{name, color});
    }
}

void PlaylistModel::saveSectionsForPlaylist() const
{
    if (m_database == nullptr || !m_database->isOpen() || m_playlistId <= 0) {
        return;
    }
    QJsonArray arr;
    for (const PlaylistSectionEntry& sec : m_sections) {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), sec.name);
        obj.insert(QStringLiteral("color"), sec.colorHex);
        arr.append(obj);
    }
    const QString key = QStringLiteral("playlist/sections_%1").arg(m_playlistId);
    (void)m_database->setSetting(
        key, QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void PlaylistModel::enrichEntryMetadata(PlaylistEntryView& entry)
{
    const QUrl fileUrl = QUrl::fromLocalFile(entry.filePath);
    if (entry.mediaKind == 0) {
        entry.mediaKind = static_cast<int>(PlaybackController::detectMediaKind(fileUrl, true));
    }
    entry.kindLabel = kindLabelForMediaKind(entry.mediaKind);
    if (entry.thumbnailUrl.isEmpty() &&
        entry.mediaKind == static_cast<int>(PlaybackController::MediaKind::Image)) {
        entry.thumbnailUrl = fileUrl.toString();
    }

    const QFileInfo info(entry.filePath);
    entry.fileExists = info.exists();
    entry.fileSizeBytes = entry.fileExists ? info.size() : 0;

    const QString suffix = info.suffix().toUpper();
    if (entry.formatLabel.isEmpty()) {
        if (suffix == QStringLiteral("MP4")) {
            entry.formatLabel = QStringLiteral("MP4 (H.264)");
        } else if (!suffix.isEmpty()) {
            entry.formatLabel = suffix;
        } else {
            entry.formatLabel = entry.kindLabel;
        }
    }

    if (entry.mediaKind == static_cast<int>(PlaybackController::MediaKind::Image) &&
        entry.fileExists) {
        const QImageReader reader(entry.filePath);
        const QSize sz = reader.size();
        if (sz.isValid()) {
            entry.resolutionLabel = aspectRatioLabel(sz.width(), sz.height());
        }
    }
    if (entry.resolutionLabel.isEmpty()) {
        entry.resolutionLabel =
            entry.mediaKind == static_cast<int>(PlaybackController::MediaKind::Audio)
                ? QStringLiteral("Stereo PCM")
                : QStringLiteral("Auto");
    }

    if (entry.subtitle.isEmpty()) {
        switch (static_cast<PlaybackController::MediaKind>(entry.mediaKind)) {
        case PlaybackController::MediaKind::Video:
            entry.subtitle = QStringLiteral("Video (16:9)");
            break;
        case PlaybackController::MediaKind::Audio:
            entry.subtitle = QStringLiteral("Audio Track");
            break;
        case PlaybackController::MediaKind::Image:
            entry.subtitle = entry.resolutionLabel.contains(QStringLiteral("16:9"))
                                 ? QStringLiteral("Image (16:9)")
                                 : QStringLiteral("Image");
            break;
        case PlaybackController::MediaKind::Pdf:
            entry.subtitle = QStringLiteral("PDF Document");
            break;
        case PlaybackController::MediaKind::None:
            entry.subtitle = entry.kindLabel;
            break;
        }
    }
}

void PlaylistModel::persistReorder()
{
    if (m_database == nullptr || !m_database->isOpen() || m_playlistId <= 0) {
        return;
    }
    std::vector<qint64> orderedIds;
    orderedIds.reserve(m_items.size());
    for (const PlaylistEntryView& item : m_items) {
        if (item.entryId > 0) {
            orderedIds.push_back(item.entryId);
        }
    }
    if (!orderedIds.empty()) {
        (void)m_database->reorderPlaylistEntries(m_playlistId, orderedIds);
    }
}

QString PlaylistModel::extractSubdirForPackage(const QString& archivePath) const
{
    const QString baseRoot = m_extractDir.isEmpty()
                                 ? SystemUtils::standardCacheDir() + QStringLiteral("/packages")
                                 : m_extractDir;
    const QString safeFolder =
        SystemUtils::sanitizeFileName(QFileInfo(archivePath).completeBaseName());
    return QDir(baseRoot).filePath(safeFolder);
}

QString PlaylistModel::kindLabelForMediaKind(int kind)
{
    switch (static_cast<PlaybackController::MediaKind>(kind)) {
    case PlaybackController::MediaKind::Video:
        return QStringLiteral("VID");
    case PlaybackController::MediaKind::Audio:
        return QStringLiteral("AUD");
    case PlaybackController::MediaKind::Image:
        return QStringLiteral("IMG");
    case PlaybackController::MediaKind::Pdf:
        return QStringLiteral("PDF");
    case PlaybackController::MediaKind::None:
        return QStringLiteral("FILE");
    }
    return QStringLiteral("FILE");
}

} // namespace kmedia
