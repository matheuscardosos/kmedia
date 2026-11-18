#pragma once

#include "integrations/DisplayManager.h"
#include "media/BackgroundMusicEngine.h"
#include "media/MpvRendererItem.h"
#include "media/PdfViewerController.h"
#include "media/PlaybackController.h"
#include "network/DownloadService.h"
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
