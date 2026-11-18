#include "ApplicationController.h"

#include "core/RuntimeInfo.h"

ApplicationController::ApplicationController(QObject* parent) : QObject(parent) {}

QString ApplicationController::appVersion() const
{
    return kmedia::RuntimeInfo::appVersion();
}

QString ApplicationController::qtVersion() const
{
    return kmedia::RuntimeInfo::qtVersion();
}

QString ApplicationController::mpvVersion() const
{
    return kmedia::RuntimeInfo::mpvVersion();
}
