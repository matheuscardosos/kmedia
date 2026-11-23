#include "models/PlaylistModel.h"

#include "core/DatabaseManager.h"
#include "core/MeetingSections.h"
#include "core/SystemUtils.h"
#include "media/PlaybackController.h"
#include "media/ThumbnailCache.h"
#include "parsers/JwlPlaylistParser.h"
#include "parsers/JwpubParser.h"

#include <QColor>
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QHash>
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
#include <functional>
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

PlaylistModel::PlaylistModel(QObject* parent) : QAbstractListModel(parent)
{
    connect(this, &PlaylistModel::activeIndexChanged, this, &PlaylistModel::activeThumbnailChanged);
}

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
            m_currentDateIso = existing.front().meetingDate;
            if (!existing.front().meetingType.isEmpty()) {
                m_currentMeetingKind = existing.front().meetingType;
            }
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
    if (m_thumbnailCache != nullptr) {
        disconnect(m_thumbnailCache, nullptr, this, nullptr);
    }
    m_thumbnailCache = thumbnailCache;
    if (m_thumbnailCache != nullptr) {
        connect(m_thumbnailCache, &ThumbnailCache::thumbnailReady, this,
                &PlaylistModel::onThumbnailReady);
    }
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
        return item.sectionId;
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
    case GroupKeyRole:
        return item.groupKey;
    case GroupExpandedRole:
        return item.groupKey.isEmpty() || !m_collapsedGroups.contains(item.groupKey);
    case IsGroupStartRole:
        return isGroupStart(index.row());
    case GroupItemCountRole:
        return groupItemCount(item.groupKey);
    case ItemKindRole:
        return item.itemKind;
    case HiddenRole:
        return item.hidden;
    case TagTypeRole:
        return item.tagType;
    case TagValueRole:
        return item.tagValue;
    case PubMediaIdRole:
        return item.pubMediaId;
    case StreamUrlRole:
        return item.streamUrl;
    case PlaybackRateRole:
        return item.playbackRate;
    case IsCbsRole:
        return item.isCbs;
    case DividerColorRole:
        return item.dividerColor;
    case SlideIntervalMsRole:
        return item.slideIntervalMs;
    case PairedAudioPathRole:
        return item.pairedAudioPath;
    case RowVisibleRole:
        return isRowVisible(index.row());
    case SectionLabelRole:
        return sectionLabel(item.sectionId);
    case SourceRole:
        return item.source;
    case SelectedRole:
        return item.entryId > 0 && m_selectedIds.contains(item.entryId);
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
        {GroupKeyRole, "groupKey"},
        {GroupExpandedRole, "groupExpanded"},
        {IsGroupStartRole, "isGroupStart"},
        {GroupItemCountRole, "groupItemCount"},
        {ItemKindRole, "itemKind"},
        {HiddenRole, "hidden"},
        {TagTypeRole, "tagType"},
        {TagValueRole, "tagValue"},
        {PubMediaIdRole, "pubMediaId"},
        {StreamUrlRole, "streamUrl"},
        {PlaybackRateRole, "playbackRate"},
        {IsCbsRole, "isCbs"},
        {DividerColorRole, "dividerColor"},
        {SlideIntervalMsRole, "slideIntervalMs"},
        {PairedAudioPathRole, "pairedAudioPath"},
        {RowVisibleRole, "rowVisible"},
        {SectionLabelRole, "sectionLabel"},
        {SourceRole, "source"},
        {SelectedRole, "selected"},
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

bool PlaylistModel::activeIsSynced() const
{
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_items.size())) {
        return false;
    }
    // Media from the sync or the shared folder would come back if it were deleted.
    const QString& source = m_items.at(static_cast<size_t>(m_activeIndex)).source;
    return source == QStringLiteral("dynamic") || source == QStringLiteral("watched");
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

int PlaylistModel::importUrls(const QList<QUrl>& urls, const QString& sectionId)
{
    int added = 0;
    for (const QUrl& url : urls) {
        const QString localPath = url.isLocalFile() ? url.toLocalFile() : url.toString();
        if (importLocalFile(localPath, sectionId)) {
            ++added;
        }
    }
    return added;
}

bool PlaylistModel::importLocalFile(const QString& filePath, const QString& sectionId)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        return false;
    }
    const QString targetSection = sectionId.trimmed().isEmpty()
                                      ? QString(MeetingSections::kImportedMedia)
                                      : sectionId.trimmed();

    const QString ext = info.suffix().toLower();
    if (ext == QStringLiteral("jwpub")) {
        emit publicationFileRequested(QUrl::fromLocalFile(info.absoluteFilePath()), targetSection);
        return false;
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
            entry.sectionId = targetSection;
            entry.filePath = target;
            if (!je.thumbnailArchivePath.isEmpty()) {
                const QString thumbPath = QDir(outDir).filePath(je.thumbnailArchivePath);
                if (QFileInfo::exists(thumbPath)) {
                    entry.thumbnailPath = thumbPath;
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
    entry.sectionId = targetSection;
    entry.filePath = info.absoluteFilePath();
    entry.sourceKind = QStringLiteral("local");
    return appendEntry(entry);
}

bool PlaylistModel::appendEntry(const PlaylistEntryView& input)
{
    const bool isDivider = (input.itemKind == QStringLiteral("divider"));
    if (!isDivider && input.filePath.trimmed().isEmpty() && input.streamUrl.trimmed().isEmpty() &&
        input.title.trimmed().isEmpty()) {
        return false;
    }

    PlaylistEntryView entry = input;
    if (entry.sortOrderOriginal <= 0) {
        entry.sortOrderOriginal = static_cast<int>(m_items.size());
    }
    if (entry.sectionId.isEmpty()) {
        entry.sectionId = MeetingSections::kImportedMedia;
    }
    if (entry.originalSection.isEmpty()) {
        entry.originalSection = entry.sectionId;
    }
    enrichEntryMetadata(entry);
    applyThumbnail(entry);

    if (m_database != nullptr && m_database->isOpen() && m_playlistId > 0) {
        if (!entry.filePath.trimmed().isEmpty()) {
            MediaItemRecord mediaRec;
            mediaRec.filePath = entry.filePath;
            mediaRec.title = entry.title;
            mediaRec.mediaType = entry.kindLabel.toLower();
            mediaRec.durationMs = entry.durationMs;
            mediaRec.fileSize = entry.fileSizeBytes;
            mediaRec.width = entry.width;
            mediaRec.height = entry.height;
            mediaRec.sourceKind = entry.sourceKind;
            entry.mediaItemId = m_database->upsertMediaItem(mediaRec);
        }

        PlaylistEntryRecord rowRec;
        rowRec.playlistId = m_playlistId;
        rowRec.mediaItemId = entry.mediaItemId;
        rowRec.sectionId = entry.sectionId;
        rowRec.sectionTitle = sectionLabel(entry.sectionId);
        rowRec.customTitle = entry.title;
        rowRec.sortOrder = static_cast<int>(m_items.size());
        rowRec.trimStartMs = entry.trimStartMs;
        rowRec.trimEndMs = entry.trimEndMs;
        rowRec.zoomScale = entry.zoomScale;
        rowRec.panX = entry.panX;
        rowRec.panY = entry.panY;
        rowRec.pdfPage = entry.pdfPage;
        rowRec.isRepeatable = entry.repeatable;
        rowRec.groupKey = entry.groupKey;
        rowRec.itemKind = entry.itemKind;
        rowRec.hidden = entry.hidden;
        rowRec.tagType = entry.tagType;
        rowRec.tagValue = entry.tagValue;
        rowRec.pubMediaId = entry.pubMediaId;
        rowRec.streamUrl = entry.streamUrl;
        rowRec.playbackRate = entry.playbackRate;
        rowRec.isCbs = entry.isCbs;
        rowRec.originalSection = entry.originalSection;
        rowRec.sortOrderOriginal = entry.sortOrderOriginal;
        rowRec.dividerColor = entry.dividerColor;
        rowRec.slideIntervalMs = entry.slideIntervalMs;
        rowRec.pairedAudioPath = entry.pairedAudioPath;
        rowRec.source = entry.source;
        rowRec.uniqueKey = entry.uniqueKey;
        rowRec.thumbnailPath = entry.thumbnailPath;
        entry.entryId = m_database->addPlaylistEntry(rowRec);
    }

    const int insertRow = static_cast<int>(m_items.size());
    beginInsertRows(QModelIndex(), insertRow, insertRow);
    m_items.push_back(std::move(entry));
    endInsertRows();
    emit countChanged();
    syncSectionsFromItems();
    return true;
}

bool PlaylistModel::moveItem(int fromIndex, int toIndex)
{
    const int size = static_cast<int>(m_items.size());
    if (fromIndex < 0 || fromIndex >= size || toIndex < 0 || toIndex >= size ||
        fromIndex == toIndex) {
        return false;
    }

    const QString targetSection = m_items.at(static_cast<size_t>(toIndex)).sectionId;
    const int destRow = (toIndex > fromIndex) ? (toIndex + 1) : toIndex;
    beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), destRow);

    PlaylistEntryView moved = m_items.at(static_cast<size_t>(fromIndex));
    const bool sectionChanged = !targetSection.isEmpty() && moved.sectionId != targetSection;
    if (sectionChanged) {
        moved.sectionId = targetSection;
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
                                                         moved.sectionId);
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

    const qint64 removedEntryId = m_items.at(static_cast<size_t>(index)).entryId;
    beginRemoveRows(QModelIndex(), index, index);
    m_items.erase(m_items.begin() + index);
    endRemoveRows();
    if (m_selectedIds.remove(removedEntryId)) {
        emit selectionChanged();
    }

    if (removedEntryId > 0 && m_database != nullptr && m_database->isOpen() && m_playlistId > 0) {
        (void)m_database->deletePlaylistEntry(removedEntryId, m_playlistId);
    }

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
    pruneSelection();
    emit countChanged();
    emit activeIndexChanged();
    emit sectionsChanged();
}

bool PlaylistModel::activateIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
        return false;
    }

    const PlaylistEntryView& targetItem = m_items.at(static_cast<size_t>(index));
    if (targetItem.itemKind == QStringLiteral("divider")) {
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
        const QUrl mediaUrl =
            (!item.filePath.isEmpty()) ? QUrl::fromLocalFile(item.filePath) : QUrl(item.streamUrl);
        if (mediaUrl.isValid()) {
            m_playback->loadMediaWithKind(
                mediaUrl, static_cast<PlaybackController::MediaKind>(item.mediaKind), item.title,
                item.trimStartMs, item.trimEndMs);
            m_playback->setFallbackDurationMs(item.durationMs);
            m_playback->setViewport(item.zoomScale, item.panX, item.panY);
        }
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
    persistEntryAt(m_activeIndex);
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
    persistEntryAt(m_activeIndex);
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
        {"demo_01_opening.png", "tgw", "Opening Song", "Visual (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 12000, 0, 12000, 0},
        {"demo_02_song_126.png", "tgw", "Song 126", "Image (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 64000, 0, 0, 1},
        {"demo_03_bible_reading.png", "tgw", "Bible Reading: Proverbs 15:1-4", "Image (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 43000, 0, 0, 2},
        {"demo_04_talk_peaceable.png", "ayfm", "Talk: Being Peaceable", "Visual (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 755000, 5000, 755000, 3},
        {"demo_05_song_129.png", "lac", "Song 129", "Image (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 62000, 0, 0, 4},
        {"demo_06_study.png", "lac", "Congregation Bible Study", "Visual (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 1100000, 0, 0, 5},
        {"demo_07_song_130.png", "lac", "Song 130", "Image (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 63000, 0, 0, 6},
        {"demo_08_closing.png", "lac", "Closing Song", "Visual (16:9)",
         static_cast<int>(PlaybackController::MediaKind::Image), 28000, 0, 0, 7},
    };

    for (const DemoSeed& seed : midweekSeeds) {
        const QString imgPath = QDir(demoAssetDir).filePath(QString::fromUtf8(seed.fileName));
        (void)createScenicDemoSlide(imgPath, seed.styleIndex);

        PlaylistEntryView entry;
        entry.title = QString::fromUtf8(seed.title);
        entry.subtitle = QString::fromUtf8(seed.subtitle);
        entry.sectionId = QString::fromUtf8(seed.section);
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
                row.sectionId = MeetingSections::kWatchtowerStudy;
                row.sectionTitle = MeetingSections::translatedLabel(row.sectionId);
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
        if (rec->meetingDate != m_currentDateIso) {
            // Another day was picked from the playlist list; its stored flag is the best guess.
            m_currentIsCoWeek =
                rec->isSpecialEvent && rec->meetingType != QStringLiteral("memorial");
        }
        m_currentDateIso = rec->meetingDate;
        if (!rec->meetingType.isEmpty()) {
            m_currentMeetingKind = rec->meetingType;
        }
    }

    // One joined query per day instead of one media lookup per row.
    const std::vector<PlaylistItemRow> rows = m_database->listPlaylistItemRows(m_playlistId);
    beginResetModel();
    m_items.clear();
    m_activeIndex = -1;

    for (const PlaylistItemRow& itemRow : rows) {
        const PlaylistEntryRecord& row = itemRow.entry;
        const std::optional<MediaItemRecord>& media = itemRow.media;
        const bool isStandalone = (row.itemKind == QStringLiteral("divider")) ||
                                  !row.streamUrl.trimmed().isEmpty() ||
                                  !row.customTitle.trimmed().isEmpty();
        if (!media.has_value() && !isStandalone) {
            continue;
        }
        PlaylistEntryView view;
        view.entryId = row.id;
        view.mediaItemId = row.mediaItemId;
        view.title = !row.customTitle.isEmpty()
                         ? row.customTitle
                         : (media.has_value() ? media->title : QStringLiteral("Media"));
        view.sectionId = row.sectionId;
        if (view.sectionId.isEmpty()) {
            view.sectionId = MeetingSections::idForLegacyTitle(row.sectionTitle);
        }
        if (view.sectionId.isEmpty()) {
            view.sectionId = MeetingSections::kImportedMedia;
        }
        view.filePath = media.has_value() ? media->filePath : QString();
        view.durationMs = media.has_value() ? media->durationMs : 0;
        view.width = media.has_value() ? media->width : 0;
        view.height = media.has_value() ? media->height : 0;
        view.trimStartMs = row.trimStartMs;
        view.trimEndMs = row.trimEndMs;
        view.zoomScale = row.zoomScale;
        view.panX = row.panX;
        view.panY = row.panY;
        view.pdfPage = row.pdfPage;
        view.repeatable = row.isRepeatable;
        view.sourceKind = media.has_value() ? media->sourceKind
                                            : (row.itemKind == QStringLiteral("divider")
                                                   ? QStringLiteral("divider")
                                                   : QStringLiteral("stream"));
        view.groupKey = row.groupKey;
        view.itemKind = row.itemKind.isEmpty() ? QStringLiteral("media") : row.itemKind;
        view.hidden = row.hidden;
        view.tagType = row.tagType;
        view.tagValue = row.tagValue;
        view.pubMediaId = row.pubMediaId;
        view.streamUrl = row.streamUrl;
        view.playbackRate = row.playbackRate > 0.0 ? row.playbackRate : 1.0;
        view.isCbs = row.isCbs;
        view.originalSection = row.originalSection.isEmpty() ? view.sectionId : row.originalSection;
        view.sortOrderOriginal = row.sortOrderOriginal;
        view.dividerColor = row.dividerColor;
        view.slideIntervalMs = row.slideIntervalMs;
        view.pairedAudioPath = row.pairedAudioPath;
        view.source = row.source;
        view.uniqueKey = row.uniqueKey;
        view.thumbnailPath = row.thumbnailPath;
        enrichEntryMetadata(view);
        applyThumbnail(view);
        m_items.push_back(std::move(view));
    }

    endResetModel();
    loadSectionsForPlaylist();
    syncSectionsFromItems();
    emit workspaceDateChanged();
    emit countChanged();
    emit activeIndexChanged();
}

QVariantList PlaylistModel::sections() const
{
    struct SectionStats
    {
        int itemCount{0};
        int firstIndex{-1};
    };
    QHash<QString, SectionStats> stats;
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        const PlaylistEntryView& item = m_items.at(static_cast<size_t>(i));
        SectionStats& entry = stats[item.sectionId];
        if (entry.firstIndex < 0) {
            entry.firstIndex = i;
        }
        if (item.itemKind != QStringLiteral("divider") && (!item.hidden || m_showHidden)) {
            ++entry.itemCount;
        }
    }

    QVariantList list;
    list.reserve(static_cast<qsizetype>(m_sections.size()));
    for (const PlaylistSectionEntry& sec : m_sections) {
        const SectionStats sectionStats = stats.value(sec.id);
        QVariantMap map;
        map.insert(QStringLiteral("id"), sec.id);
        map.insert(QStringLiteral("name"), sectionLabel(sec.id));
        map.insert(QStringLiteral("color"), sec.colorHex);
        map.insert(QStringLiteral("repeatable"), sec.repeatable);
        map.insert(QStringLiteral("collapsed"), sec.collapsed);
        map.insert(QStringLiteral("documentTitle"), sec.documentTitle);
        map.insert(QStringLiteral("isMeetingSection"), MeetingSections::isMeetingSection(sec.id));
        map.insert(QStringLiteral("itemCount"), sectionStats.itemCount);
        map.insert(QStringLiteral("firstItemIndex"), sectionStats.firstIndex);
        list.append(map);
    }
    return list;
}

QString PlaylistModel::defaultColorForSection(const QString& sectionId)
{
    return MeetingSections::defaultColor(sectionId);
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

QString PlaylistModel::colorForSection(const QString& sectionId) const
{
    const int idx = findSectionIndex(sectionId);
    if (idx >= 0) {
        return m_sections.at(static_cast<size_t>(idx)).colorHex;
    }
    return defaultColorForSection(sectionId.trimmed());
}

QString PlaylistModel::sectionLabel(const QString& sectionId) const
{
    const int idx = findSectionIndex(sectionId);
    if (idx >= 0 && !m_sections.at(static_cast<size_t>(idx)).label.isEmpty()) {
        return m_sections.at(static_cast<size_t>(idx)).label;
    }
    const QString translated = MeetingSections::translatedLabel(sectionId.trimmed());
    return translated.isEmpty() ? sectionId.trimmed() : translated;
}

bool PlaylistModel::ensureSection(const QString& sectionId, const QString& colorHex)
{
    const QString cleaned = sectionId.trimmed();
    if (cleaned.isEmpty()) {
        return false;
    }
    if (findSectionIndex(cleaned) >= 0) {
        if (!colorHex.trimmed().isEmpty()) {
            return setSectionColor(cleaned, colorHex);
        }
        return true;
    }
    PlaylistSectionEntry section;
    section.id = cleaned;
    section.colorHex = normalizeHexColor(colorHex, defaultColorForSection(cleaned));
    m_sections.push_back(section);
    saveSectionsForPlaylist();
    emit sectionsChanged();
    return true;
}

QString PlaylistModel::createSection(const QString& label, const QString& colorHex)
{
    const QString cleanedLabel = label.trimmed();
    for (const PlaylistSectionEntry& existing : m_sections) {
        if (!cleanedLabel.isEmpty() &&
            sectionLabel(existing.id).compare(cleanedLabel, Qt::CaseInsensitive) == 0) {
            return {};
        }
    }
    PlaylistSectionEntry section;
    section.id = MeetingSections::createCustomId();
    section.label = cleanedLabel;
    section.colorHex = normalizeHexColor(colorHex, MeetingSections::randomCustomColor());
    m_sections.push_back(section);
    saveSectionsForPlaylist();
    emit sectionsChanged();
    return section.id;
}

bool PlaylistModel::renameSection(const QString& sectionId, const QString& newLabel)
{
    const QString cleanedLabel = newLabel.trimmed();
    const int idx = findSectionIndex(sectionId);
    if (idx < 0 || cleanedLabel.isEmpty() ||
        !MeetingSections::isCustomSection(sectionId.trimmed())) {
        return false;
    }
    for (int i = 0; i < static_cast<int>(m_sections.size()); ++i) {
        if (i != idx && sectionLabel(m_sections.at(static_cast<size_t>(i)).id)
                                .compare(cleanedLabel, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    m_sections[static_cast<size_t>(idx)].label = cleanedLabel;
    saveSectionsForPlaylist();
    emit sectionsChanged();
    if (!m_items.empty()) {
        emit dataChanged(createIndex(0, 0), createIndex(static_cast<int>(m_items.size()) - 1, 0),
                         {SectionLabelRole});
    }
    return true;
}

bool PlaylistModel::setSectionSubtitle(const QString& sectionId, const QString& subtitle)
{
    const int idx = findSectionIndex(sectionId);
    if (idx < 0 && !ensureSection(sectionId)) {
        return false;
    }
    const size_t target = static_cast<size_t>(idx >= 0 ? idx : findSectionIndex(sectionId));
    if (m_sections[target].documentTitle == subtitle.trimmed()) {
        return true;
    }
    m_sections[target].documentTitle = subtitle.trimmed();
    saveSectionsForPlaylist();
    emit sectionsChanged();
    return true;
}

bool PlaylistModel::setSectionColor(const QString& sectionId, const QString& colorHex)
{
    const QString cleaned = sectionId.trimmed();
    if (cleaned.isEmpty()) {
        return false;
    }
    const QString normalized = normalizeHexColor(colorHex, QString{});
    if (normalized.isEmpty()) {
        return false;
    }

    const int idx = findSectionIndex(cleaned);
    if (idx < 0) {
        PlaylistSectionEntry section;
        section.id = cleaned;
        section.colorHex = normalized;
        m_sections.push_back(section);
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

bool PlaylistModel::removeSection(const QString& sectionId)
{
    const QString cleaned = sectionId.trimmed();
    const int idx = findSectionIndex(cleaned);
    if (idx < 0 || MeetingSections::isMeetingSection(cleaned) ||
        cleaned == MeetingSections::kImportedMedia) {
        return false;
    }
    // Media of a deleted section is kept under the added media section.
    bool movedAny = false;
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        PlaylistEntryView& item = m_items[static_cast<size_t>(i)];
        if (item.sectionId != cleaned) {
            continue;
        }
        item.sectionId = MeetingSections::kImportedMedia;
        movedAny = true;
        if (m_database != nullptr && m_database->isOpen() && m_playlistId > 0 && item.entryId > 0) {
            (void)m_database->updatePlaylistEntrySection(item.entryId, m_playlistId,
                                                         item.sectionId);
        }
        emit dataChanged(createIndex(i, 0), createIndex(i, 0),
                         {SectionTitleRole, SectionLabelRole});
    }
    m_sections.erase(m_sections.begin() + idx);
    if (movedAny) {
        (void)ensureSection(MeetingSections::kImportedMedia);
    }
    saveSectionsForPlaylist();
    emit sectionsChanged();
    return true;
}

bool PlaylistModel::moveSection(const QString& sectionId, int delta)
{
    if (delta == 0 || m_sections.size() < 2) {
        return false;
    }
    const int idx = findSectionIndex(sectionId);
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
                if (!taken[i] && m_items[i].sectionId == sec.id) {
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

bool PlaylistModel::assignItemToSection(int index, const QString& sectionId)
{
    const int size = static_cast<int>(m_items.size());
    const QString cleanedSection = sectionId.trimmed();
    if (index < 0 || index >= size || cleanedSection.isEmpty()) {
        return false;
    }

    (void)ensureSection(cleanedSection);

    int destIndex = -1;
    for (int i = 0; i < size; ++i) {
        if (i == index) {
            continue;
        }
        if (m_items.at(static_cast<size_t>(i)).sectionId == cleanedSection) {
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
            const int rowSecOrder = findSectionIndex(m_items.at(static_cast<size_t>(i)).sectionId);
            if (rowSecOrder >= 0 && rowSecOrder > targetSecOrder) {
                destIndex = (i > index) ? (i - 1) : i;
                break;
            }
        }
    } else if (destIndex < index) {
        ++destIndex;
    }

    m_items[static_cast<size_t>(index)].sectionId = cleanedSection;
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
        emit dataChanged(changedIdx, changedIdx, {SectionTitleRole, SectionLabelRole});
        emit activeIndexChanged();
        persistReorder();
    } else {
        const QModelIndex changedIdx = createIndex(index, 0);
        emit dataChanged(changedIdx, changedIdx, {SectionTitleRole, SectionLabelRole});
    }

    syncSectionsFromItems();
    return true;
}

int PlaylistModel::findSectionIndex(const QString& sectionId) const
{
    const QString cleaned = sectionId.trimmed();
    for (int i = 0; i < static_cast<int>(m_sections.size()); ++i) {
        if (m_sections.at(static_cast<size_t>(i)).id == cleaned) {
            return i;
        }
    }
    return -1;
}

void PlaylistModel::syncSectionsFromItems()
{
    bool changed = false;
    for (const PlaylistEntryView& item : m_items) {
        const QString sec = item.sectionId.trimmed();
        if (sec.isEmpty() || findSectionIndex(sec) >= 0) {
            continue;
        }
        PlaylistSectionEntry section;
        section.id = sec;
        section.colorHex = defaultColorForSection(sec);
        m_sections.push_back(section);
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
    const auto rec = m_database->findPlaylistById(m_playlistId);
    if (!rec.has_value() || rec->sectionsJson.isEmpty()) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(rec->sectionsJson.toUtf8());
    for (const QJsonValue& val : doc.array()) {
        const QJsonObject obj = val.toObject();
        PlaylistSectionEntry section;
        section.id = obj.value(QStringLiteral("id")).toString().trimmed();
        section.label = obj.value(QStringLiteral("label")).toString().trimmed();
        if (section.id.isEmpty()) {
            const QString legacyName = obj.value(QStringLiteral("name")).toString().trimmed();
            section.id = MeetingSections::idForLegacyTitle(legacyName);
            if (section.id.isEmpty()) {
                section.id = MeetingSections::createCustomId();
                section.label = legacyName;
            }
        }
        if (findSectionIndex(section.id) >= 0) {
            continue;
        }
        section.colorHex = normalizeHexColor(obj.value(QStringLiteral("color")).toString(),
                                             defaultColorForSection(section.id));
        section.repeatable = obj.value(QStringLiteral("repeatable")).toBool(false);
        section.collapsed = obj.value(QStringLiteral("collapsed")).toBool(false);
        section.documentTitle = obj.value(QStringLiteral("documentTitle")).toString();
        m_sections.push_back(section);
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
        obj.insert(QStringLiteral("id"), sec.id);
        if (!sec.label.isEmpty()) {
            obj.insert(QStringLiteral("label"), sec.label);
        }
        obj.insert(QStringLiteral("color"), sec.colorHex);
        if (sec.repeatable) {
            obj.insert(QStringLiteral("repeatable"), true);
        }
        if (sec.collapsed) {
            obj.insert(QStringLiteral("collapsed"), true);
        }
        if (!sec.documentTitle.isEmpty()) {
            obj.insert(QStringLiteral("documentTitle"), sec.documentTitle);
        }
        arr.append(obj);
    }
    if (auto rec = m_database->findPlaylistById(m_playlistId); rec.has_value()) {
        rec->sectionsJson = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        (void)m_database->updatePlaylistMetadata(*rec);
    }
}

void PlaylistModel::applyThumbnail(PlaylistEntryView& entry)
{
    const bool picture = entry.mediaKind == static_cast<int>(PlaybackController::MediaKind::Image);
    const bool pdf = entry.mediaKind == static_cast<int>(PlaybackController::MediaKind::Pdf);
    // Videos and audio covers need a frame extractor; without one they keep their icon.
    const bool playable =
        (entry.mediaKind == static_cast<int>(PlaybackController::MediaKind::Video) ||
         entry.mediaKind == static_cast<int>(PlaybackController::MediaKind::Audio)) &&
        m_thumbnailCache != nullptr && m_thumbnailCache->hasCustomExtractor();
    QString source;
    if (!entry.thumbnailPath.isEmpty() && QFileInfo::exists(entry.thumbnailPath)) {
        source = entry.thumbnailPath;
    } else if ((picture || pdf || playable) && entry.fileExists && !entry.filePath.isEmpty()) {
        source = entry.filePath;
    }
    if (source.isEmpty()) {
        if (picture && !entry.fileExists) {
            entry.thumbnailUrl.clear();
        }
        return;
    }
    if (m_thumbnailCache == nullptr || m_thumbnailCache->cacheDir().isEmpty()) {
        if (source != entry.filePath || picture) {
            entry.thumbnailUrl = QUrl::fromLocalFile(source).toString();
        }
        return;
    }

    // Rows show a small picture decoded once on a worker thread, never the full-size file.
    entry.thumbnailKey = ThumbnailCache::makeKeyForFile(source);
    const QString cached = m_thumbnailCache->cachedPathIfPresent(entry.thumbnailKey);
    if (!cached.isEmpty()) {
        entry.thumbnailUrl = QUrl::fromLocalFile(cached).toString();
        return;
    }
    entry.thumbnailUrl.clear();
    m_thumbnailCache->requestThumbnail(entry.thumbnailKey, source);
}

void PlaylistModel::onThumbnailReady(const QString& key, const QString& cachedPath)
{
    const QString url = QUrl::fromLocalFile(cachedPath).toString();
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        PlaylistEntryView& item = m_items[static_cast<size_t>(i)];
        if (item.thumbnailKey != key || item.thumbnailUrl == url) {
            continue;
        }
        item.thumbnailUrl = url;
        emit dataChanged(createIndex(i, 0), createIndex(i, 0), {ThumbnailUrlRole});
        if (i == m_activeIndex) {
            emit activeThumbnailChanged();
        }
    }
}

void PlaylistModel::enrichEntryMetadata(PlaylistEntryView& entry)
{
    if (entry.itemKind == QStringLiteral("divider")) {
        entry.kindLabel = QStringLiteral("DIV");
        entry.formatLabel = QStringLiteral("Divider");
        entry.resolutionLabel = QStringLiteral("Separator");
        entry.fileExists = true;
        if (entry.sourceKind.isEmpty()) {
            entry.sourceKind = QStringLiteral("divider");
        }
        return;
    }

    if (entry.filePath.trimmed().isEmpty() && !entry.streamUrl.trimmed().isEmpty()) {
        entry.kindLabel = QStringLiteral("STRM");
        entry.formatLabel = QStringLiteral("Stream (>35m)");
        entry.resolutionLabel = QStringLiteral("Cloud Stream");
        entry.fileExists = true;
        if (entry.mediaKind == 0) {
            entry.mediaKind = static_cast<int>(PlaybackController::MediaKind::Video);
        }
        return;
    }

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
        entry.fileExists && (entry.width <= 0 || entry.height <= 0)) {
        // Read once at import; reloads reuse the size stored with the media item.
        const QImageReader reader(entry.filePath);
        const QSize sz = reader.size();
        if (sz.isValid()) {
            entry.width = sz.width();
            entry.height = sz.height();
        }
    }
    if (entry.width > 0 && entry.height > 0) {
        entry.resolutionLabel = aspectRatioLabel(entry.width, entry.height);
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

void PlaylistModel::persistEntryAt(int index) const
{
    if (m_database == nullptr || !m_database->isOpen() || m_playlistId <= 0 || index < 0 ||
        index >= static_cast<int>(m_items.size())) {
        return;
    }
    const PlaylistEntryView& view = m_items.at(static_cast<size_t>(index));
    if (view.entryId <= 0) {
        return;
    }
    PlaylistEntryRecord rec;
    rec.id = view.entryId;
    rec.playlistId = m_playlistId;
    rec.mediaItemId = view.mediaItemId;
    rec.sectionId = view.sectionId;
    rec.sectionTitle = sectionLabel(view.sectionId);
    rec.customTitle = view.title;
    rec.sortOrder = index;
    rec.trimStartMs = view.trimStartMs;
    rec.trimEndMs = view.trimEndMs;
    rec.zoomScale = view.zoomScale;
    rec.panX = view.panX;
    rec.panY = view.panY;
    rec.pdfPage = view.pdfPage;
    rec.isRepeatable = view.repeatable;
    rec.groupKey = view.groupKey;
    rec.itemKind = view.itemKind;
    rec.hidden = view.hidden;
    rec.tagType = view.tagType;
    rec.tagValue = view.tagValue;
    rec.pubMediaId = view.pubMediaId;
    rec.streamUrl = view.streamUrl;
    rec.playbackRate = view.playbackRate;
    rec.isCbs = view.isCbs;
    rec.originalSection = view.originalSection;
    rec.sortOrderOriginal = view.sortOrderOriginal;
    rec.dividerColor = view.dividerColor;
    rec.slideIntervalMs = view.slideIntervalMs;
    rec.pairedAudioPath = view.pairedAudioPath;
    rec.source = view.source;
    rec.uniqueKey = view.uniqueKey;
    rec.thumbnailPath = view.thumbnailPath;
    (void)m_database->updatePlaylistEntryFull(rec);
}

QString PlaylistModel::currentDateIso() const
{
    return m_currentDateIso;
}

QString PlaylistModel::currentMeetingKind() const
{
    return m_currentMeetingKind;
}

bool PlaylistModel::currentIsCoWeek() const
{
    return m_currentIsCoWeek;
}

bool PlaylistModel::showHidden() const
{
    return m_showHidden;
}

void PlaylistModel::setShowHidden(bool showHidden)
{
    if (m_showHidden == showHidden) {
        return;
    }
    m_showHidden = showHidden;
    emit showHiddenChanged();
    emit sectionsChanged();
    if (!m_items.empty()) {
        emit dataChanged(createIndex(0, 0), createIndex(static_cast<int>(m_items.size()) - 1, 0),
                         {RowVisibleRole, HiddenRole});
    }
}

QString PlaylistModel::searchQuery() const
{
    return m_searchQuery;
}

void PlaylistModel::setSearchQuery(const QString& query)
{
    if (m_searchQuery == query) {
        return;
    }
    m_searchQuery = query;
    emit searchQueryChanged();
    if (!m_items.empty()) {
        emit dataChanged(createIndex(0, 0), createIndex(static_cast<int>(m_items.size()) - 1, 0),
                         {RowVisibleRole});
    }
}

int PlaylistModel::hiddenCount() const
{
    int total = 0;
    for (const PlaylistEntryView& item : m_items) {
        if (item.hidden) {
            ++total;
        }
    }
    return total;
}

void PlaylistModel::ensureFixedMeetingSections(const QString& meetingKind, bool isCoVisit)
{
    for (const QString& sectionId : MeetingSections::meetingSectionIds(meetingKind, isCoVisit)) {
        (void)ensureSection(sectionId);
    }
}

void PlaylistModel::reloadFromDatabase()
{
    const qint64 activeEntryId =
        (m_activeIndex >= 0 && m_activeIndex < static_cast<int>(m_items.size()))
            ? m_items.at(static_cast<size_t>(m_activeIndex)).entryId
            : 0;
    loadFromDatabase();
    ensureFixedMeetingSections(m_currentMeetingKind, m_currentIsCoWeek);
    pruneSelection();
    if (activeEntryId <= 0) {
        return;
    }
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        if (m_items.at(static_cast<size_t>(i)).entryId == activeEntryId) {
            // Only the highlight moves back; whatever is playing keeps playing.
            m_activeIndex = i;
            emit activeIndexChanged();
            break;
        }
    }
}

bool PlaylistModel::openWorkspaceForDate(const QString& dateIso, const QString& meetingKind,
                                         const QString& displayTitle, bool isCoVisit)
{
    const QString cleanDate = dateIso.trimmed();
    if (cleanDate.isEmpty()) {
        return false;
    }
    const QString cleanKind = meetingKind.trimmed().isEmpty() ? QStringLiteral("custom")
                                                              : meetingKind.trimmed().toLower();

    if (cleanDate != m_currentDateIso) {
        clearSelection();
    }
    m_currentDateIso = cleanDate;
    m_currentMeetingKind = cleanKind;
    m_currentIsCoWeek = isCoVisit;
    m_collapsedGroups.clear();

    if (m_database != nullptr && m_database->isOpen()) {
        const QString title = displayTitle.trimmed().isEmpty()
                                  ? QStringLiteral("%1 | %2").arg(cleanDate, cleanKind.toUpper())
                                  : displayTitle.trimmed();
        const PlaylistRecord rec = m_database->findOrCreatePlaylistForDate(
            cleanDate, cleanKind, title, isCoVisit || cleanKind == QStringLiteral("memorial"),
            isCoVisit ? MeetingSections::defaultColor(MeetingSections::kCircuitOverseer)
                      : QString());
        if (rec.id <= 0) {
            return false;
        }
        m_playlistId = rec.id;
        setPlaylistName(rec.name);
        refreshAvailablePlaylists();
        loadFromDatabase();
    } else {
        clear();
        setPlaylistName(displayTitle.trimmed().isEmpty() ? cleanDate : displayTitle.trimmed());
    }

    ensureFixedMeetingSections(cleanKind, isCoVisit);
    if (!m_items.empty() && m_activeIndex < 0) {
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
            if (isRowVisible(i) &&
                m_items.at(static_cast<size_t>(i)).itemKind != QStringLiteral("divider")) {
                (void)activateIndex(i);
                break;
            }
        }
    }
    emit workspaceDateChanged();
    emit availablePlaylistsChanged();
    return true;
}

bool PlaylistModel::addDivider(const QString& title, const QString& sectionId,
                               const QString& colorHex, int afterIndex)
{
    const QString cleanTitle =
        title.trimmed().isEmpty() ? QStringLiteral("Divider") : title.trimmed();
    const QString sec = sectionId.trimmed().isEmpty()
                            ? (m_sections.empty() ? QString(MeetingSections::kImportedMedia)
                                                  : m_sections.front().id)
                            : sectionId.trimmed();

    PlaylistEntryView entry;
    entry.title = cleanTitle;
    entry.subtitle = QStringLiteral("Separator");
    entry.sectionId = sec;
    entry.itemKind = QStringLiteral("divider");
    entry.dividerColor = normalizeHexColor(colorHex, QStringLiteral("#D47B2A"));
    entry.sourceKind = QStringLiteral("divider");
    if (!appendEntry(entry)) {
        return false;
    }

    const int lastIdx = static_cast<int>(m_items.size()) - 1;
    if (afterIndex >= 0 && afterIndex < lastIdx - 1) {
        (void)moveItem(lastIdx, afterIndex + 1);
    }
    return true;
}

bool PlaylistModel::setItemHidden(int index, bool hidden)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
        return false;
    }
    PlaylistEntryView& entry = m_items[static_cast<size_t>(index)];
    if (entry.hidden == hidden) {
        return true;
    }
    entry.hidden = hidden;
    persistEntryAt(index);
    emit dataChanged(createIndex(index, 0), createIndex(index, 0), {HiddenRole, RowVisibleRole});
    emit countChanged();
    emit sectionsChanged();
    return true;
}

bool PlaylistModel::toggleItemHidden(int index)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
        return false;
    }
    return setItemHidden(index, !m_items.at(static_cast<size_t>(index)).hidden);
}

bool PlaylistModel::updateItemTag(int index, const QString& tagType, const QString& tagValue)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
        return false;
    }
    PlaylistEntryView& entry = m_items[static_cast<size_t>(index)];
    entry.tagType = tagType.trimmed().toLower();
    entry.tagValue = tagValue.trimmed();
    persistEntryAt(index);
    emit dataChanged(createIndex(index, 0), createIndex(index, 0), {TagTypeRole, TagValueRole});
    return true;
}

bool PlaylistModel::setItemTitle(int index, const QString& title)
{
    const QString cleaned = title.trimmed();
    if (cleaned.isEmpty() || index < 0 || index >= static_cast<int>(m_items.size())) {
        return false;
    }
    PlaylistEntryView& entry = m_items[static_cast<size_t>(index)];
    if (entry.title == cleaned) {
        return true;
    }
    entry.title = cleaned;
    persistEntryAt(index);
    emit dataChanged(createIndex(index, 0), createIndex(index, 0), {TitleRole});
    if (index == m_activeIndex) {
        emit activeIndexChanged();
        emit activeItemEdited();
    }
    return true;
}

bool PlaylistModel::setItemPlaybackRate(int index, double rate)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
        return false;
    }
    const double clamped = qBound(0.5, rate, 15.0);
    PlaylistEntryView& entry = m_items[static_cast<size_t>(index)];
    entry.playbackRate = clamped;
    persistEntryAt(index);
    emit dataChanged(createIndex(index, 0), createIndex(index, 0), {PlaybackRateRole});
    return true;
}

bool PlaylistModel::setItemSlideInterval(int index, int intervalMs)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
        return false;
    }
    const int clamped = qBound(0, intervalMs, 600000);
    PlaylistEntryView& entry = m_items[static_cast<size_t>(index)];
    entry.slideIntervalMs = clamped;
    persistEntryAt(index);
    emit dataChanged(createIndex(index, 0), createIndex(index, 0), {SlideIntervalMsRole});
    return true;
}

bool PlaylistModel::pairAudioWithItem(int index, const QString& audioFilePath)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
        return false;
    }
    PlaylistEntryView& entry = m_items[static_cast<size_t>(index)];
    entry.pairedAudioPath = audioFilePath.trimmed();
    persistEntryAt(index);
    emit dataChanged(createIndex(index, 0), createIndex(index, 0), {PairedAudioPathRole});
    return true;
}

bool PlaylistModel::toggleGroupExpanded(const QString& groupKey)
{
    const QString key = groupKey.trimmed();
    if (key.isEmpty()) {
        return false;
    }
    if (m_collapsedGroups.contains(key)) {
        m_collapsedGroups.remove(key);
    } else {
        m_collapsedGroups.insert(key);
    }
    emit groupStateChanged();
    if (!m_items.empty()) {
        emit dataChanged(createIndex(0, 0), createIndex(static_cast<int>(m_items.size()) - 1, 0),
                         {GroupExpandedRole, RowVisibleRole});
    }
    return true;
}

bool PlaylistModel::setSectionRepeatable(const QString& sectionId, bool repeatable)
{
    const int idx = findSectionIndex(sectionId);
    if (idx < 0) {
        return false;
    }
    m_sections[static_cast<size_t>(idx)].repeatable = repeatable;
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        PlaylistEntryView& item = m_items[static_cast<size_t>(i)];
        if (item.sectionId == m_sections[static_cast<size_t>(idx)].id &&
            item.itemKind != QStringLiteral("divider")) {
            item.repeatable = repeatable;
            persistEntryAt(i);
            emit dataChanged(createIndex(i, 0), createIndex(i, 0), {RepeatableRole});
        }
    }
    saveSectionsForPlaylist();
    emit sectionsChanged();
    return true;
}

bool PlaylistModel::toggleSectionCollapsed(const QString& sectionId)
{
    const int idx = findSectionIndex(sectionId);
    if (idx < 0) {
        return false;
    }
    m_sections[static_cast<size_t>(idx)].collapsed =
        !m_sections[static_cast<size_t>(idx)].collapsed;
    saveSectionsForPlaylist();
    emit sectionsChanged();
    if (!m_items.empty()) {
        emit dataChanged(createIndex(0, 0), createIndex(static_cast<int>(m_items.size()) - 1, 0),
                         {RowVisibleRole});
    }
    return true;
}

bool PlaylistModel::resetSortOrder()
{
    if (m_items.empty()) {
        return true;
    }
    beginResetModel();
    for (PlaylistEntryView& item : m_items) {
        if (!item.originalSection.isEmpty()) {
            item.sectionId = item.originalSection;
        }
    }
    std::stable_sort(m_items.begin(), m_items.end(),
                     [](const PlaylistEntryView& a, const PlaylistEntryView& b) {
                         return a.sortOrderOriginal < b.sortOrderOriginal;
                     });
    m_activeIndex = m_items.empty() ? -1 : 0;
    endResetModel();

    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        persistEntryAt(i);
    }
    persistReorder();
    syncSectionsFromItems();
    emit activeIndexChanged();
    return true;
}

bool PlaylistModel::isRowVisible(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) {
        return false;
    }
    const PlaylistEntryView& item = m_items.at(static_cast<size_t>(row));
    if (item.hidden && !m_showHidden) {
        return false;
    }
    const int secIdx = findSectionIndex(item.sectionId);
    if (secIdx >= 0 && m_sections.at(static_cast<size_t>(secIdx)).collapsed) {
        return false;
    }
    if (!item.groupKey.isEmpty() && m_collapsedGroups.contains(item.groupKey) &&
        !isGroupStart(row)) {
        return false;
    }
    const QString q = m_searchQuery.trimmed();
    if (!q.isEmpty()) {
        const bool matched = item.title.contains(q, Qt::CaseInsensitive) ||
                             item.subtitle.contains(q, Qt::CaseInsensitive) ||
                             sectionLabel(item.sectionId).contains(q, Qt::CaseInsensitive) ||
                             item.tagValue.contains(q, Qt::CaseInsensitive) ||
                             item.groupKey.contains(q, Qt::CaseInsensitive);
        if (!matched) {
            return false;
        }
    }
    return true;
}

int PlaylistModel::groupItemCount(const QString& groupKey) const
{
    if (groupKey.trimmed().isEmpty()) {
        return 0;
    }
    int count = 0;
    for (const PlaylistEntryView& item : m_items) {
        if (item.groupKey == groupKey) {
            ++count;
        }
    }
    return count;
}

bool PlaylistModel::isGroupStart(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) {
        return false;
    }
    const QString& key = m_items.at(static_cast<size_t>(row)).groupKey;
    if (key.isEmpty()) {
        return false;
    }
    return row == 0 || m_items.at(static_cast<size_t>(row - 1)).groupKey != key;
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

int PlaylistModel::selectionCount() const
{
    return static_cast<int>(m_selectedIds.size());
}

bool PlaylistModel::isRowSelected(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
        return false;
    }
    const qint64 entryId = m_items.at(static_cast<size_t>(index)).entryId;
    return entryId > 0 && m_selectedIds.contains(entryId);
}

QList<int> PlaylistModel::selectedRows() const
{
    QList<int> rows;
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        if (isRowSelected(i)) {
            rows.append(i);
        }
    }
    return rows;
}

int PlaylistModel::rowOfEntry(qint64 entryId) const
{
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        if (m_items.at(static_cast<size_t>(i)).entryId == entryId) {
            return i;
        }
    }
    return -1;
}

void PlaylistModel::setSelectedIds(const QSet<qint64>& ids)
{
    if (ids == m_selectedIds) {
        return;
    }
    QSet<qint64> changed = ids;
    changed.unite(m_selectedIds);
    changed.subtract(QSet<qint64>(ids).intersect(m_selectedIds));
    m_selectedIds = ids;
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        if (changed.contains(m_items.at(static_cast<size_t>(i)).entryId)) {
            const QModelIndex changedIndex = createIndex(i, 0);
            emit dataChanged(changedIndex, changedIndex, {SelectedRole});
        }
    }
    emit selectionChanged();
}

void PlaylistModel::pruneSelection()
{
    if (m_selectedIds.isEmpty()) {
        return;
    }
    QSet<qint64> present;
    for (const PlaylistEntryView& item : m_items) {
        if (m_selectedIds.contains(item.entryId)) {
            present.insert(item.entryId);
        }
    }
    if (present.size() != m_selectedIds.size()) {
        m_selectedIds = present;
        emit selectionChanged();
    }
}

void PlaylistModel::toggleRowSelected(int index)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
        return;
    }
    const PlaylistEntryView& item = m_items.at(static_cast<size_t>(index));
    if (item.entryId <= 0 || item.itemKind == QStringLiteral("divider")) {
        return;
    }
    QSet<qint64> next = m_selectedIds;
    if (!next.remove(item.entryId)) {
        next.insert(item.entryId);
    }
    setSelectedIds(next);
}

void PlaylistModel::selectRows(const QList<int>& indexes)
{
    QSet<qint64> next;
    for (const int index : indexes) {
        if (index < 0 || index >= static_cast<int>(m_items.size())) {
            continue;
        }
        const PlaylistEntryView& item = m_items.at(static_cast<size_t>(index));
        if (item.entryId > 0 && item.itemKind != QStringLiteral("divider")) {
            next.insert(item.entryId);
        }
    }
    setSelectedIds(next);
}

void PlaylistModel::clearSelection()
{
    setSelectedIds({});
}

int PlaylistModel::setSelectedHidden(bool hidden)
{
    int changed = 0;
    for (const int row : selectedRows()) {
        if (m_items.at(static_cast<size_t>(row)).hidden != hidden && setItemHidden(row, hidden)) {
            ++changed;
        }
    }
    // Hidden items leave the list, so they leave the selection too.
    if (hidden) {
        clearSelection();
    }
    return changed;
}

int PlaylistModel::assignSelectedToSection(const QString& sectionId)
{
    QList<qint64> ids;
    for (const int row : selectedRows()) {
        ids.append(m_items.at(static_cast<size_t>(row)).entryId);
    }
    // One at a time in list order: each lands after the previous one in the section.
    int moved = 0;
    for (const qint64 entryId : ids) {
        const int row = rowOfEntry(entryId);
        if (row >= 0 && assignItemToSection(row, sectionId)) {
            ++moved;
        }
    }
    return moved;
}

int PlaylistModel::removeSelected()
{
    QList<int> rows = selectedRows();
    std::sort(rows.begin(), rows.end(), std::greater<>());
    int done = 0;
    for (const int row : rows) {
        const bool added =
            m_items.at(static_cast<size_t>(row)).source == QStringLiteral("additional");
        if (added ? removeAt(row) : setItemHidden(row, true)) {
            ++done;
        }
    }
    clearSelection();
    return done;
}

bool PlaylistModel::moveSelectedTo(int targetIndex)
{
    const int size = static_cast<int>(m_items.size());
    const QList<int> rows = selectedRows();
    if (rows.isEmpty() || targetIndex < 0 || targetIndex >= size || rows.contains(targetIndex)) {
        return false;
    }
    if (rows.size() == 1) {
        return moveItem(rows.first(), targetIndex);
    }

    // The final order: everything else in place, the block after the target when dragged
    // down and before it when dragged up.
    const bool downward = targetIndex > rows.first();
    std::vector<int> order;
    order.reserve(static_cast<size_t>(size));
    for (int i = 0; i < size; ++i) {
        if (rows.contains(i)) {
            continue;
        }
        if (i == targetIndex && !downward) {
            order.insert(order.end(), rows.begin(), rows.end());
        }
        order.push_back(i);
        if (i == targetIndex && downward) {
            order.insert(order.end(), rows.begin(), rows.end());
        }
    }

    // Single-row moves, so views keep their delegates (and a drag in progress keeps going).
    std::vector<int> current(static_cast<size_t>(size));
    for (int i = 0; i < size; ++i) {
        current[static_cast<size_t>(i)] = i;
    }
    for (int pos = 0; pos < size; ++pos) {
        const int wanted = order[static_cast<size_t>(pos)];
        if (current[static_cast<size_t>(pos)] == wanted) {
            continue;
        }
        const auto found = std::find(current.begin() + pos, current.end(), wanted);
        const int from = static_cast<int>(found - current.begin());
        beginMoveRows(QModelIndex(), from, from, QModelIndex(), pos);
        PlaylistEntryView moved = m_items.at(static_cast<size_t>(from));
        m_items.erase(m_items.begin() + from);
        m_items.insert(m_items.begin() + pos, moved);
        endMoveRows();
        current.erase(current.begin() + from);
        current.insert(current.begin() + pos, wanted);
    }

    // The block joins the target's section.
    const QString targetSection =
        m_items
            .at(static_cast<size_t>(std::find(current.begin(), current.end(), targetIndex) -
                                    current.begin()))
            .sectionId;
    bool sectionChanged = false;
    for (int i = 0; i < size; ++i) {
        PlaylistEntryView& item = m_items[static_cast<size_t>(i)];
        if (!isRowSelected(i) || targetSection.isEmpty() || item.sectionId == targetSection) {
            continue;
        }
        item.sectionId = targetSection;
        sectionChanged = true;
        const QModelIndex changedIndex = createIndex(i, 0);
        emit dataChanged(changedIndex, changedIndex, {SectionTitleRole});
        if (m_database != nullptr && m_database->isOpen() && m_playlistId > 0 && item.entryId > 0) {
            (void)m_database->updatePlaylistEntrySection(item.entryId, m_playlistId, targetSection);
        }
    }
    if (sectionChanged) {
        syncSectionsFromItems();
    }

    if (m_activeIndex >= 0) {
        m_activeIndex = static_cast<int>(std::find(current.begin(), current.end(), m_activeIndex) -
                                         current.begin());
        emit activeIndexChanged();
    }
    persistReorder();
    return true;
}

} // namespace kmedia
