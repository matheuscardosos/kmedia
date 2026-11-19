#include "core/BoothSecurity.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BoothSecurity validates PIN format, salts, hashes, and startup tabs cleanly",
          "[core][security]")
{
    REQUIRE_FALSE(kmedia::BoothSecurity::isValidPinFormat(QStringLiteral("123")));
    REQUIRE_FALSE(kmedia::BoothSecurity::isValidPinFormat(QStringLiteral("12 34")));
    REQUIRE(kmedia::BoothSecurity::isValidPinFormat(QStringLiteral("1914")));
    REQUIRE(kmedia::BoothSecurity::isValidPinFormat(QStringLiteral("Booth2026")));

    const QString salt = kmedia::BoothSecurity::generateSalt();
    REQUIRE(salt.size() == 32);
    const QString hash = kmedia::BoothSecurity::hashPin(QStringLiteral("1914"), salt);
    REQUIRE(hash.size() == 64);
    REQUIRE(kmedia::BoothSecurity::verifyPin(QStringLiteral("1914"), salt, hash));
    REQUIRE_FALSE(kmedia::BoothSecurity::verifyPin(QStringLiteral("1915"), salt, hash));
    REQUIRE_FALSE(kmedia::BoothSecurity::verifyPin(QStringLiteral(""), salt, hash));

    REQUIRE(kmedia::BoothSecurity::parseTabArgument(QStringLiteral("meeting")) == 0);
    REQUIRE(kmedia::BoothSecurity::parseTabArgument(QStringLiteral("calendar")) == 1);
    REQUIRE(kmedia::BoothSecurity::parseTabArgument(QStringLiteral("media")) == 2);
    REQUIRE(kmedia::BoothSecurity::parseTabArgument(QStringLiteral("playlists")) == 3);
    REQUIRE(kmedia::BoothSecurity::parseTabArgument(QStringLiteral("downloads")) == 4);
    REQUIRE(kmedia::BoothSecurity::parseTabArgument(QStringLiteral("settings")) == 5);
    REQUIRE(kmedia::BoothSecurity::parseTabArgument(QStringLiteral("2")) == 2);
    REQUIRE(kmedia::BoothSecurity::parseTabArgument(QStringLiteral("unknown")) == 0);
}
