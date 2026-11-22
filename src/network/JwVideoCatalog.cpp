#include "network/JwVideoCatalog.h"

#include "core/MeetingSections.h"
#include "core/SystemUtils.h"
#include "models/PlaylistModel.h"
#include "network/DocumentMediaImporter.h"
#include "network/PublicationStore.h"

#include <QVariantMap>

#include <algorithm>

namespace kmedia {

namespace {

const QString kLatestVideos = QStringLiteral("LatestVideos");
const QString kVideoOnDemand = QStringLiteral("VideoOnDemand");
// Audio described copies repeat the regular videos.
const QString kAudioDescriptions = QStringLiteral("VODAudioDescriptions");

} // namespace

JwVideoCatalog::JwVideoCatalog(CatalogClient* client, PublicationStore* store,
                               DocumentMediaImporter* importer, QObject* parent)
    : QObject(parent), m_client(client), m_store(store), m_importer(importer)
{
    // Videos arrive category by category; the list is refreshed a few times a second at most.
    m_publishTimer.setSingleShot(true);
    m_publishTimer.setInterval(250);
    connect(&m_publishTimer, &QTimer::timeout, this, &JwVideoCatalog::publish);
}

void JwVideoCatalog::setLanguage(const QString& languageCode)
{
    m_language = languageCode.trimmed();
}

bool JwVideoCatalog::isBusy() const
{
    return m_busy;
}

QVariantList JwVideoCatalog::videos() const
{
    return m_published;
}

void JwVideoCatalog::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

void JwVideoCatalog::load()
{
    if (m_client == nullptr || m_language.isEmpty() || m_busy ||
        (m_loadedLanguage == m_language && !m_videos.empty())) {
        return;
    }
    reload();
}

void JwVideoCatalog::reload()
{
    if (m_client == nullptr || m_language.isEmpty()) {
        return;
    }
    ++m_generation;
    m_queue.clear();
    m_inFlight = 0;
    m_videos.clear();
    m_videoIndex.clear();
    m_loadedLanguage = m_language;
    m_queue.push_back(Request{kLatestVideos, false, 0});
    m_queue.push_back(Request{kVideoOnDemand, true, 0});
    setBusy(true);
    publish();
    pump();
}

void JwVideoCatalog::pump()
{
    while (m_inFlight < kParallelRequests && !m_queue.empty()) {
        const Request request = m_queue.front();
        m_queue.pop_front();
        ++m_inFlight;
        const quint64 generation = m_generation;
        m_client->requestMediatorCategory(
            CatalogClient::mediatorCategoryUrl(m_loadedLanguage, request.category,
                                               request.subcategoriesOnly),
            this, [this, request, generation](bool ok, const JwVideoCategory& category) {
                if (generation != m_generation) {
                    return;
                }
                --m_inFlight;
                onCategory(request, ok, category);
                pump();
                if (m_inFlight == 0 && m_queue.empty()) {
                    publish();
                    setBusy(false);
                }
            });
    }
}

void JwVideoCatalog::onCategory(const Request& request, bool ok, const JwVideoCategory& category)
{
    if (!ok) {
        return;
    }
    if (request.subcategoriesOnly) {
        // Video On Demand holds groups of categories; their own categories hold the videos.
        for (const QString& sub : category.subcategories) {
            if (sub == kAudioDescriptions) {
                continue;
            }
            m_queue.push_back(request.depth == 0 ? Request{sub, true, 1} : Request{sub, false, 2});
        }
        return;
    }
    for (const JwVideo& video : category.videos) {
        if (m_videoIndex.contains(video.key)) {
            continue;
        }
        m_videoIndex.insert(video.key, m_videos.size());
        m_videos.push_back(video);
    }
    if (!m_publishTimer.isActive()) {
        m_publishTimer.start();
    }
}

void JwVideoCatalog::publish()
{
    std::vector<const JwVideo*> ordered;
    ordered.reserve(m_videos.size());
    for (const JwVideo& video : m_videos) {
        ordered.push_back(&video);
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const JwVideo* a, const JwVideo* b) {
        return a->firstPublished > b->firstPublished;
    });
    m_published.clear();
    m_published.reserve(static_cast<qsizetype>(ordered.size()));
    for (const JwVideo* video : ordered) {
        QVariantMap item;
        item.insert(QStringLiteral("key"), video->key);
        item.insert(QStringLiteral("title"), video->title);
        item.insert(QStringLiteral("imageUrl"), video->imageUrl);
        item.insert(QStringLiteral("duration"), SystemUtils::formatDurationMs(static_cast<qint64>(
                                                    video->durationSeconds * 1000.0)));
        m_published.append(item);
    }
    emit videosChanged();
}

void JwVideoCatalog::addVideo(const QString& key, const QString& sectionId)
{
    const auto found = m_videoIndex.constFind(key);
    if (found == m_videoIndex.constEnd() || m_store == nullptr || m_importer.isNull()) {
        emit videoAdded(false);
        return;
    }
    const JwVideo video = m_videos.at(found.value());
    const QString target =
        sectionId.isEmpty() ? QString(MeetingSections::kImportedMedia) : sectionId;
    m_store->resolveVideo(video, DownloadService::Priority::High, this,
                          [this, video, target](const ResolvedMediaFile& resolved) {
                              if (!resolved.ok || m_importer.isNull()) {
                                  emit videoAdded(false);
                                  return;
                              }
                              PlaylistEntryView entry;
                              entry.title = video.title;
                              entry.sectionId = target;
                              entry.filePath = resolved.filePath;
                              entry.streamUrl = resolved.streamUrl;
                              entry.thumbnailPath = resolved.thumbnailPath;
                              entry.durationMs =
                                  static_cast<qint64>(video.durationSeconds * 1000.0);
                              entry.pubMediaId = video.key;
                              entry.sourceKind = QStringLiteral("catalog");
                              entry.source = QStringLiteral("additional");
                              emit videoAdded(m_importer->addMedia(entry));
                          });
}

} // namespace kmedia
