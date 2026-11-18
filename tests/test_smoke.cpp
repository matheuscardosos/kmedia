#include "core/RuntimeInfo.h"

#include <QCoreApplication>

#include <mpv/client.h>

#include <catch2/catch_test_macros.hpp>
#include <clocale>

TEST_CASE("RuntimeInfo exposes valid Qt and libmpv versions", "[smoke]")
{
    REQUIRE(!kmedia::RuntimeInfo::appVersion().isEmpty());
    REQUIRE(!kmedia::RuntimeInfo::qtVersion().isEmpty());
    REQUIRE(!kmedia::RuntimeInfo::mpvVersion().isEmpty());
    REQUIRE(kmedia::RuntimeInfo::mpvApiVersionRaw() > 0UL);
}

TEST_CASE("libmpv handle initializes and terminates cleanly", "[smoke][mpv]")
{
    std::setlocale(LC_NUMERIC, "C");

    mpv_handle* handle = mpv_create();
    REQUIRE(handle != nullptr);

    REQUIRE(mpv_set_option_string(handle, "vo", "null") >= 0);
    REQUIRE(mpv_set_option_string(handle, "ao", "null") >= 0);
    REQUIRE(mpv_set_option_string(handle, "hwdec", "auto-safe") >= 0);
    REQUIRE(mpv_initialize(handle) >= 0);

    mpv_terminate_destroy(handle);
}
