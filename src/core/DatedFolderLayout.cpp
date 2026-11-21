#include "core/DatedFolderLayout.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace kmedia {

namespace {

constexpr int kMaxFileNameBytes = 255;

QString sectionOrderPath(const QString& dayFolder)
{
    return QDir(dayFolder).filePath(DatedFolderLayout::sectionOrderFileName());
}

// The placement file is hidden on Windows so it stays out of the way in shared folders.
void setHiddenOnWindows(const QString& path, bool hidden)
{
#ifdef Q_OS_WIN
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    const DWORD attributes = GetFileAttributesW(nativePath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return;
    }
    const DWORD wanted = hidden ? (attributes | FILE_ATTRIBUTE_HIDDEN)
                                : (attributes & ~static_cast<DWORD>(FILE_ATTRIBUTE_HIDDEN));
    if (wanted != attributes) {
        SetFileAttributesW(nativePath.c_str(), wanted);
    }
#else
    Q_UNUSED(path)
    Q_UNUSED(hidden)
#endif
}

QJsonObject readSectionOrderObject(const QString& dayFolder)
{
    QFile file(sectionOrderPath(dayFolder));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    // A cloud client can leave the file empty or half written while it syncs; treat that as
    // "no placements yet" and let the next refresh read the completed file.
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    return doc.object();
}

bool writeSectionOrderObject(const QString& dayFolder, const QJsonObject& root)
{
    if (!QDir().mkpath(dayFolder)) {
        return false;
    }
    const QString path = sectionOrderPath(dayFolder);
    setHiddenOnWindows(path, false);
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!out.commit()) {
        return false;
    }
    setHiddenOnWindows(path, true);
    return true;
}

QString truncateUtf8(const QString& text, int maxBytes)
{
    QString out = text;
    while (out.toUtf8().size() > maxBytes && !out.isEmpty()) {
        out.chop(1);
        if (!out.isEmpty() && out.back().isHighSurrogate()) {
            out.chop(1);
        }
    }
    return out;
}

} // namespace

double ExportedFileNameParts::order() const
{
    return (static_cast<double>(sectionNumber) * 1000.0) + static_cast<double>(itemNumber);
}

QString DatedFolderLayout::sectionOrderFileName()
{
    return QStringLiteral(".section-order.json");
}

bool DatedFolderLayout::isDayFolderName(const QString& name)
{
    return dateFromDayFolderName(name).isValid();
}

QDate DatedFolderLayout::dateFromDayFolderName(const QString& name)
{
    static const QRegularExpression kDayFolderPattern(QStringLiteral(R"(^\d{4}-\d{2}-\d{2}$)"));
    if (!kDayFolderPattern.match(name).hasMatch()) {
        return {};
    }
    return QDate::fromString(name, Qt::ISODate);
}

QString DatedFolderLayout::dayFolderName(const QDate& date)
{
    return date.isValid() ? date.toString(Qt::ISODate) : QString();
}

QDate DatedFolderLayout::dateForFile(const QString& filePath)
{
    if (filePath.trimmed().isEmpty()) {
        return {};
    }
    return dateFromDayFolderName(QFileInfo(filePath).absoluteDir().dirName());
}

bool DatedFolderLayout::isLongPastDayFolder(const QString& name, const QDate& today)
{
    const QDate folderDate = dateFromDayFolderName(name);
    return folderDate.isValid() && today.isValid() &&
           folderDate < today.addDays(-kPastDayFolderMaxAgeDays);
}

int DatedFolderLayout::pruneDayFolders(const QString& rootDir, const QDate& today)
{
    const QDir root(rootDir);
    if (rootDir.trimmed().isEmpty() || !root.exists() || !today.isValid()) {
        return 0;
    }

    static const QRegularExpression kPrunablePattern(
        QStringLiteral(R"(^(?:\d{8}|\d{4}-\d{2}-\d{2})$)"));
    int removed = 0;
    const QFileInfoList entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        const QString name = entry.fileName();
        if (!kPrunablePattern.match(name).hasMatch()) {
            continue;
        }
        const QDate folderDate = name.size() == 8
                                     ? QDate::fromString(name, QStringLiteral("yyyyMMdd"))
                                     : QDate::fromString(name, Qt::ISODate);
        if (!folderDate.isValid()) {
            continue;
        }
        // A day folder stays until the week it belongs to is over.
        const QDate weekSunday = folderDate.addDays(7 - folderDate.dayOfWeek());
        if (weekSunday < today && QDir(entry.absoluteFilePath()).removeRecursively()) {
            ++removed;
        }
    }
    return removed;
}

QHash<QString, DatedFilePlacement> DatedFolderLayout::readSectionOrder(const QString& dayFolder)
{
    QHash<QString, DatedFilePlacement> placements;
    const QJsonObject root = readSectionOrderObject(dayFolder);
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const QJsonObject value = it.value().toObject();
        DatedFilePlacement placement;
        placement.sectionId = value.value(QStringLiteral("section")).toString().trimmed();
        const QJsonValue order = value.value(QStringLiteral("order"));
        placement.hasOrder = order.isDouble();
        placement.order = order.toDouble();
        if (placement.sectionId.isEmpty() && !placement.hasOrder) {
            continue;
        }
        placements.insert(it.key(), placement);
    }
    return placements;
}

bool DatedFolderLayout::mergeSectionOrder(const QString& dayFolder,
                                          const QHash<QString, DatedFilePlacement>& placements)
{
    if (dayFolder.trimmed().isEmpty()) {
        return false;
    }
    QJsonObject root = readSectionOrderObject(dayFolder);
    for (auto it = placements.constBegin(); it != placements.constEnd(); ++it) {
        QJsonObject value;
        if (it->hasOrder) {
            value.insert(QStringLiteral("order"), it->order);
        }
        value.insert(QStringLiteral("section"), it->sectionId);
        root.insert(it.key(), value);
    }
    return writeSectionOrderObject(dayFolder, root);
}

bool DatedFolderLayout::removeFromSectionOrder(const QString& dayFolder, const QString& fileName)
{
    QJsonObject root = readSectionOrderObject(dayFolder);
    if (!root.contains(fileName)) {
        return true;
    }
    root.remove(fileName);
    return writeSectionOrderObject(dayFolder, root);
}

std::optional<ExportedFileNameParts>
DatedFolderLayout::parseExportedFileName(const QString& fileName)
{
    static const QRegularExpression kExportedPattern(
        QStringLiteral(R"(^(\d{1,3})\s+(\S.*?)\s-\s(\d{1,3})\s)"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpressionMatch match = kExportedPattern.match(fileName);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    ExportedFileNameParts parts;
    parts.sectionNumber = match.captured(1).toInt();
    parts.sectionLabel = match.captured(2);
    parts.itemNumber = match.captured(3).toInt();
    return parts;
}

QString DatedFolderLayout::legacySectionId(const QString& fileName)
{
    static const QRegularExpression kLegacyPattern(QStringLiteral(R"(^Section-([^-]+) - )"));
    const QRegularExpressionMatch match = kLegacyPattern.match(fileName);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString DatedFolderLayout::exportedFileName(int sectionNumber, const QString& sectionLabel,
                                            int itemNumber, int itemCount,
                                            const QString& songPrefix, const QString& title,
                                            const QString& extension)
{
    const int itemDigits = itemCount > 99 ? 3 : 2;
    const QString sectionText = QStringLiteral("%1").arg(sectionNumber, 2, 10, QLatin1Char('0'));
    const QString itemText = QStringLiteral("%1").arg(itemNumber, itemDigits, 10, QLatin1Char('0'));
    return sectionText + QLatin1Char(' ') + sanitizeExportName(sectionLabel) +
           QStringLiteral(" - ") + itemText + QLatin1Char(' ') + songPrefix +
           sanitizeExportName(title) + extension;
}

QString DatedFolderLayout::fitPathToByteLimit(const QString& filePath, int maxBytes)
{
    if (filePath.toUtf8().size() <= maxBytes) {
        return filePath;
    }
    const QFileInfo info(filePath);
    const QString dir = info.path();
    const QString extension =
        info.suffix().isEmpty() ? QString() : QLatin1Char('.') + info.suffix();
    const QString baseName = info.completeBaseName();
    // A stable tag keeps a shortened name identical between export runs.
    const QString tag =
        QLatin1Char('_') +
        QString::fromLatin1(
            QCryptographicHash::hash(baseName.toUtf8(), QCryptographicHash::Sha1).toHex().left(4));
    const auto pathFor = [&](const QString& base) {
        return QDir(dir).filePath(base + tag + extension);
    };

    QString trimmed = baseName;
    while (!trimmed.isEmpty() && pathFor(trimmed).toUtf8().size() > maxBytes) {
        trimmed.chop(1);
        if (!trimmed.isEmpty() && trimmed.back().isHighSurrogate()) {
            trimmed.chop(1);
        }
    }
    return pathFor(trimmed);
}

QString DatedFolderLayout::sanitizeExportName(const QString& raw)
{
    static const QRegularExpression kIllegalChars(QStringLiteral(R"([/?<>\\:*|"])"));
    static const QRegularExpression kReservedDots(QStringLiteral(R"(^\.+$)"));
    static const QRegularExpression kWindowsReserved(
        QStringLiteral(R"(^(con|prn|aux|nul|com\d|lpt\d)(\..*)?$)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kTrailingDotsAndSpaces(QStringLiteral(R"([. ]+$)"));

    QString cleaned;
    cleaned.reserve(raw.size());
    for (const QChar ch : raw) {
        const char16_t code = ch.unicode();
        if (code <= u'\x1F' || (code >= u'\x80' && code <= u'\x9F')) {
            continue;
        }
        cleaned.append(ch);
    }
    cleaned.remove(kIllegalChars);
    cleaned.remove(kReservedDots);
    cleaned.remove(kWindowsReserved);
    cleaned.remove(kTrailingDotsAndSpaces);
    return truncateUtf8(cleaned, kMaxFileNameBytes);
}

} // namespace kmedia
