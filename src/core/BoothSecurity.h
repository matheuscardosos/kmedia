#pragma once

#include <QString>

namespace kmedia {

/**
 * Cryptographic PIN hashing and startup tab resolution helpers for Booth settings protection.
 */
class BoothSecurity
{
public:
    [[nodiscard]] static bool isValidPinFormat(const QString& pin);
    [[nodiscard]] static QString generateSalt();
    [[nodiscard]] static QString hashPin(const QString& pin, const QString& salt);
    [[nodiscard]] static bool verifyPin(const QString& candidatePin, const QString& salt,
                                        const QString& expectedHash);
    [[nodiscard]] static int parseTabArgument(const QString& raw);
};

} // namespace kmedia
