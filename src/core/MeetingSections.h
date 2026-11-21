#pragma once

#include <QString>
#include <QStringList>

namespace kmedia {

/**
 * Stable meeting section ids, their default colors, and translated labels. Items and section
 * configs store the id, so section names follow the interface language instead of the language
 * of the publication the media came from.
 */
class MeetingSections
{
public:
    MeetingSections() = delete;

    static constexpr QLatin1StringView kTreasures{"tgw"};
    static constexpr QLatin1StringView kApplyYourself{"ayfm"};
    static constexpr QLatin1StringView kLivingAsChristians{"lac"};
    static constexpr QLatin1StringView kPublicTalk{"pt"};
    static constexpr QLatin1StringView kWatchtowerStudy{"wt"};
    static constexpr QLatin1StringView kCircuitOverseer{"circuit-overseer"};
    static constexpr QLatin1StringView kImportedMedia{"imported-media"};
    static constexpr QLatin1StringView kCustomPrefix{"custom-"};

    [[nodiscard]] static bool isMeetingSection(const QString& sectionId);
    [[nodiscard]] static bool isCustomSection(const QString& sectionId);
    [[nodiscard]] static QStringList meetingSectionIds(const QString& meetingKind, bool isCoWeek);
    [[nodiscard]] static QString defaultColor(const QString& sectionId);
    [[nodiscard]] static QString translatedLabel(const QString& sectionId);
    [[nodiscard]] static QString idForLegacyTitle(const QString& title);
    [[nodiscard]] static QString createCustomId();
    [[nodiscard]] static QString randomCustomColor();
};

} // namespace kmedia
