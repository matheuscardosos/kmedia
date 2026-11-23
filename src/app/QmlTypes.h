#pragma once

#include "core/MeetingCalendarModel.h"
#include "core/MeetingTimer.h"
#include "integrations/DisplayManager.h"
#include "integrations/ObsWebSocketClient.h"
#include "integrations/ProcessMonitor.h"
#include "integrations/ZoomBridge.h"
#include "media/BackgroundMusicEngine.h"
#include "media/BackgroundMusicService.h"
#include "media/MediaAutoExport.h"
#include "media/MpvRendererItem.h"
#include "media/PdfPageImporter.h"
#include "media/PdfViewerController.h"
#include "media/PlaybackController.h"
#include "models/MeetingDayModel.h"
#include "models/PlaylistModel.h"
#include "network/BibleMediaCatalog.h"
#include "network/DownloadService.h"
#include "network/JwVideoCatalog.h"
#include "network/MeetingCatalogService.h"
#include "network/MeetingSyncEngine.h"
#include "network/PublicTalkMedia.h"
#include "network/PublicationMediaPicker.h"
#include "network/StudyBibleMedia.h"
#include "network/WatchedFolderMedia.h"
#include "network/YeartextProvider.h"
#include "parsers/CloudSyncWatcher.h"

#include <QtQmlIntegration/qqmlintegration.h>

struct MpvRendererItemForeign
{
    Q_GADGET
    QML_FOREIGN(MpvRendererItem)
    QML_NAMED_ELEMENT(MpvRendererItem)
};

struct PlaybackControllerForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::PlaybackController)
    QML_NAMED_ELEMENT(PlaybackController)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct BackgroundMusicServiceForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::BackgroundMusicService)
    QML_NAMED_ELEMENT(BackgroundMusicService)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct BackgroundMusicEngineForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::BackgroundMusicEngine)
    QML_NAMED_ELEMENT(BackgroundMusicEngine)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct DisplayManagerForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::DisplayManager)
    QML_NAMED_ELEMENT(DisplayManager)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct PdfViewerControllerForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::PdfViewerController)
    QML_NAMED_ELEMENT(PdfViewerController)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct DownloadServiceForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::DownloadService)
    QML_NAMED_ELEMENT(DownloadService)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct CloudSyncWatcherForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::CloudSyncWatcher)
    QML_NAMED_ELEMENT(CloudSyncWatcher)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct ProcessMonitorForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::ProcessMonitor)
    QML_NAMED_ELEMENT(ProcessMonitor)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct ObsWebSocketClientForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::ObsWebSocketClient)
    QML_NAMED_ELEMENT(ObsWebSocketClient)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct ZoomBridgeForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::ZoomBridge)
    QML_NAMED_ELEMENT(ZoomBridge)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct PlaylistModelForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::PlaylistModel)
    QML_NAMED_ELEMENT(PlaylistModel)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct MeetingDayModelForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::MeetingDayModel)
    QML_NAMED_ELEMENT(MeetingDayModel)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct WatchedFolderMediaForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::WatchedFolderMedia)
    QML_NAMED_ELEMENT(WatchedFolderMedia)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct MediaAutoExportForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::MediaAutoExport)
    QML_NAMED_ELEMENT(MediaAutoExport)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct PdfPageImporterForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::PdfPageImporter)
    QML_NAMED_ELEMENT(PdfPageImporter)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct YeartextProviderForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::YeartextProvider)
    QML_NAMED_ELEMENT(YeartextProvider)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct StudyBibleMediaForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::StudyBibleMedia)
    QML_NAMED_ELEMENT(StudyBibleMedia)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct BibleMediaCatalogForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::BibleMediaCatalog)
    QML_NAMED_ELEMENT(BibleMediaCatalog)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct JwVideoCatalogForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::JwVideoCatalog)
    QML_NAMED_ELEMENT(JwVideoCatalog)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct PublicationMediaPickerForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::PublicationMediaPicker)
    QML_NAMED_ELEMENT(PublicationMediaPicker)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct PublicTalkMediaForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::PublicTalkMedia)
    QML_NAMED_ELEMENT(PublicTalkMedia)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct MeetingSyncEngineForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::MeetingSyncEngine)
    QML_NAMED_ELEMENT(MeetingSyncEngine)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct MeetingTimerForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::MeetingTimer)
    QML_NAMED_ELEMENT(MeetingTimer)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct MeetingCatalogServiceForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::MeetingCatalogService)
    QML_NAMED_ELEMENT(MeetingCatalogService)
    QML_UNCREATABLE("Provided by ApplicationController")
};

struct MeetingCalendarModelForeign
{
    Q_GADGET
    QML_FOREIGN(kmedia::MeetingCalendarModel)
    QML_NAMED_ELEMENT(MeetingCalendarModel)
    QML_UNCREATABLE("Provided by ApplicationController")
};
