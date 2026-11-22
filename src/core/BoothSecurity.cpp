#include "core/BoothSecurity.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QRandomGenerator>

namespace kmedia {

bool BoothSecurity::isValidPinFormat(const QString& pin)
{
    const QString cleaned = pin.trimmed();
    if (cleaned.size() < 4 || cleaned.size() > 12) {
        return false;
    }
    for (const QChar ch : cleaned) {
        if (!ch.isLetterOrNumber()) {
            return false;
        }
    }
    return true;
}

QString BoothSecurity::generateSalt()
{
    auto* rng = QRandomGenerator::system();
    const quint64 high = rng->generate64();
    const quint64 low = rng->generate64();
    QByteArray raw(16, Qt::Uninitialized);
    for (int i = 0; i < 8; ++i) {
        raw[i] = static_cast<char>((high >> (i * 8)) & 0xFF);
        raw[i + 8] = static_cast<char>((low >> (i * 8)) & 0xFF);
    }
    return QString::fromLatin1(raw.toHex());
}

QString BoothSecurity::hashPin(const QString& pin, const QString& salt)
{
    const QString cleanedPin = pin.trimmed();
    const QString cleanedSalt = salt.trimmed();
    if (cleanedPin.isEmpty() || cleanedSalt.isEmpty()) {
        return {};
    }
    const QByteArray payload = (cleanedSalt + QLatin1Char(':') + cleanedPin).toUtf8();
    return QString::fromLatin1(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

bool BoothSecurity::verifyPin(const QString& candidatePin, const QString& salt,
                              const QString& expectedHash)
{
    if (expectedHash.trimmed().isEmpty() || candidatePin.trimmed().isEmpty()) {
        return false;
    }
    const QByteArray actual = hashPin(candidatePin, salt).toLatin1();
    const QByteArray expected = expectedHash.trimmed().toLower().toLatin1();
    if (actual.size() != expected.size() || actual.isEmpty()) {
        return false;
    }
    unsigned char diff = 0;
    for (int i = 0; i < actual.size(); ++i) {
        diff |= static_cast<unsigned char>(actual.at(i) ^ expected.at(i));
    }
    return diff == 0;
}

int BoothSecurity::parseTabArgument(const QString& raw)
{
    // The calendar, library, playlists, and downloads moved into the meeting page and settings,
    // so their old names open the meeting page.
    const QString key = raw.trimmed().toLower();
    if (key == QStringLiteral("settings") || key == QStringLiteral("1")) {
        return 1;
    }
    return 0;
}

} // namespace kmedia
