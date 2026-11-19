#pragma once

#include "core/MeetingTimer.h"
#include "integrations/DisplayManager.h"
#include "integrations/ObsWebSocketClient.h"
#include "integrations/ProcessMonitor.h"
#include "integrations/ZoomBridge.h"
#include "media/BackgroundMusicEngine.h"
#include "media/MpvRendererItem.h"
#include "media/PdfViewerController.h"
#include "media/PlaybackController.h"
#include "models/PlaylistModel.h"
#include "network/DownloadService.h"
#include "network/MeetingCatalogService.h"
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
