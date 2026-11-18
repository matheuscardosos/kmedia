#pragma once

#include <QString>

namespace kmedia {

struct RuntimeInfo
{
    [[nodiscard]] static QString appVersion();
    [[nodiscard]] static QString qtVersion();
    [[nodiscard]] static QString mpvVersion();
    [[nodiscard]] static unsigned long mpvApiVersionRaw();
};

} // namespace kmedia
