#include "core/MeetingSections.h"

#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QRandomGenerator>

#include <algorithm>
#include <atomic>

namespace kmedia {

namespace {

constexpr auto kTreasuresColor = "#3C7F8B";
constexpr auto kApplyColor = "#D68F00";
constexpr auto kLivingColor = "#BF2F13";
constexpr auto kAdditionalColor = "#945EB5";

} // namespace

bool MeetingSections::isMeetingSection(const QString& sectionId)
{
    return sectionId == kTreasures || sectionId == kApplyYourself ||
           sectionId == kLivingAsChristians || sectionId == kPublicTalk ||
           sectionId == kWatchtowerStudy || sectionId == kCircuitOverseer;
}

bool MeetingSections::isCustomSection(const QString& sectionId)
{
    return sectionId == kImportedMedia || sectionId.startsWith(kCustomPrefix);
}

QStringList MeetingSections::meetingSectionIds(const QString& meetingKind, bool isCoWeek)
{
    const QString kind = meetingKind.trimmed().toLower();
    QStringList ids;
    if (kind == QStringLiteral("midweek") || kind == QStringLiteral("mw")) {
        ids = {kTreasures, kApplyYourself, kLivingAsChristians};
    } else if (kind == QStringLiteral("weekend") || kind == QStringLiteral("we")) {
        ids = {kPublicTalk, kWatchtowerStudy};
    } else {
        return ids;
    }
    if (isCoWeek) {
        ids.append(kCircuitOverseer);
    }
    return ids;
}

QString MeetingSections::defaultColor(const QString& sectionId)
{
    if (sectionId == kTreasures) {
        return QString::fromLatin1(kTreasuresColor);
    }
    if (sectionId == kApplyYourself || sectionId == kWatchtowerStudy) {
        return QString::fromLatin1(kApplyColor);
    }
    if (sectionId == kLivingAsChristians) {
        return QString::fromLatin1(kLivingColor);
    }
    return QString::fromLatin1(kAdditionalColor);
}

QString MeetingSections::translatedLabel(const QString& sectionId)
{
    if (sectionId == kTreasures) {
        return QCoreApplication::translate("MeetingSections", "Treasures From God's Word");
    }
    if (sectionId == kApplyYourself) {
        return QCoreApplication::translate("MeetingSections",
                                           "Apply Yourself to the Field Ministry");
    }
    if (sectionId == kLivingAsChristians) {
        return QCoreApplication::translate("MeetingSections", "Living as Christians");
    }
    if (sectionId == kPublicTalk) {
        return QCoreApplication::translate("MeetingSections", "Public Talk");
    }
    if (sectionId == kWatchtowerStudy) {
        return QCoreApplication::translate("MeetingSections", "Watchtower Study");
    }
    if (sectionId == kCircuitOverseer) {
        return QCoreApplication::translate("MeetingSections", "Circuit Overseer");
    }
    if (isCustomSection(sectionId)) {
        return QCoreApplication::translate("MeetingSections", "Added Media");
    }
    return {};
}

QString MeetingSections::idForLegacyTitle(const QString& title)
{
    const QString key = title.trimmed().toLower();
    if (key.isEmpty()) {
        return {};
    }
    if (isMeetingSection(key) || key == kImportedMedia || key.startsWith(kCustomPrefix)) {
        return key;
    }

    struct LegacyTitle
    {
        const char* title;
        QLatin1StringView id;
    };
    static constexpr LegacyTitle kLegacyTitles[] = {
        {"treasures from god's word", kTreasures},
        {"treasures", kTreasures},
        {"tesouros da palavra de deus", kTreasures},
        {"apply yourself to the field ministry", kApplyYourself},
        {"apply yourself", kApplyYourself},
        {"faça seu melhor no ministério", kApplyYourself},
        {"living as christians", kLivingAsChristians},
        {"living", kLivingAsChristians},
        {"nossa vida cristã", kLivingAsChristians},
        {"public talk", kPublicTalk},
        {"discurso público", kPublicTalk},
        {"watchtower study", kWatchtowerStudy},
        {"estudo de a sentinela", kWatchtowerStudy},
        {"circuit overseer", kCircuitOverseer},
        {"superintendente de circuito", kCircuitOverseer},
        {"meeting", kImportedMedia},
        {"meeting media", kImportedMedia},
        {"imported media", kImportedMedia},
    };
    for (const LegacyTitle& legacy : kLegacyTitles) {
        if (key == QString::fromUtf8(legacy.title)) {
            return legacy.id;
        }
    }
    return {};
}

QString MeetingSections::createCustomId()
{
    // Strictly increasing so ids created within the same millisecond never collide.
    static std::atomic<qint64> lastId{0};
    qint64 previous = lastId.load();
    qint64 candidate = 0;
    do {
        candidate = std::max(QDateTime::currentMSecsSinceEpoch(), previous + 1);
    } while (!lastId.compare_exchange_weak(previous, candidate));
    return QString(kCustomPrefix) + QString::number(candidate);
}

QString MeetingSections::randomCustomColor()
{
    // Mid-range channels keep both white and dark text readable on the color.
    auto* random = QRandomGenerator::global();
    const QColor color(random->bounded(80, 231), random->bounded(80, 231),
                       random->bounded(80, 231));
    return color.name(QColor::HexRgb).toUpper();
}

} // namespace kmedia
