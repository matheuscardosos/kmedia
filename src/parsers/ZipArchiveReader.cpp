#include "parsers/ZipArchiveReader.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <limits>
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>
#include <vector>

namespace kmedia {

namespace {

constexpr int kChunkBytes = 64 * 1024;

QString normalizeSeparators(const QString& path)
{
    QString out = path;
    out.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return out;
}

void assignError(QString* error, const QString& message)
{
    if (error != nullptr) {
        *error = message;
    }
}

bool containsEntry(const std::vector<QString>& filter, const QString& entryName)
{
    const QString normalizedTarget = normalizeSeparators(entryName);
    return std::any_of(filter.begin(), filter.end(), [&normalizedTarget](const QString& item) {
        return normalizeSeparators(item) == normalizedTarget;
    });
}

} // namespace

ZipArchiveReader::~ZipArchiveReader()
{
    close();
}

bool ZipArchiveReader::openFile(const QString& archivePath, QString* error)
{
    close();
    if (archivePath.trimmed().isEmpty() || !QFileInfo::exists(archivePath)) {
        assignError(error, QStringLiteral("Archive file does not exist: %1").arg(archivePath));
        return false;
    }

    void* reader = mz_zip_reader_create();
    if (reader == nullptr) {
        assignError(error, QStringLiteral("Failed to allocate minizip-ng reader"));
        return false;
    }

    const QByteArray utf8Path = archivePath.toUtf8();
    const int32_t rc = mz_zip_reader_open_file(reader, utf8Path.constData());
    if (rc != MZ_OK) {
        mz_zip_reader_delete(&reader);
        assignError(
            error,
            QStringLiteral("Failed to open archive '%1' (code %2)").arg(archivePath).arg(rc));
        return false;
    }

    m_reader = reader;
    return true;
}

bool ZipArchiveReader::openMemory(const QByteArray& zipBytes, QString* error)
{
    close();
    if (zipBytes.isEmpty()) {
        assignError(error, QStringLiteral("Cannot open empty memory archive"));
        return false;
    }
    if (zipBytes.size() > std::numeric_limits<int32_t>::max()) {
        assignError(error, QStringLiteral("In-memory archive exceeds 32-bit buffer limit"));
        return false;
    }

    m_memoryBacking = zipBytes;
    void* reader = mz_zip_reader_create();
    if (reader == nullptr) {
        m_memoryBacking.clear();
        assignError(error, QStringLiteral("Failed to allocate minizip-ng reader"));
        return false;
    }

    const int32_t rc = mz_zip_reader_open_buffer(
        reader, reinterpret_cast<const uint8_t*>(m_memoryBacking.constData()),
        static_cast<int32_t>(m_memoryBacking.size()), 0);
    if (rc != MZ_OK) {
        mz_zip_reader_delete(&reader);
        m_memoryBacking.clear();
        assignError(error, QStringLiteral("Failed to open in-memory zip (code %1)").arg(rc));
        return false;
    }

    m_reader = reader;
    return true;
}

void ZipArchiveReader::close()
{
    if (m_reader != nullptr) {
        mz_zip_reader_close(m_reader);
        mz_zip_reader_delete(&m_reader);
        m_reader = nullptr;
    }
    m_memoryBacking.clear();
}

bool ZipArchiveReader::isOpen() const
{
    return m_reader != nullptr;
}

bool ZipArchiveReader::isSafeRelativeEntryName(const QString& entryName)
{
    if (entryName.isEmpty() || entryName.contains(QChar(u'\0'))) {
        return false;
    }

    const QString normalized = normalizeSeparators(entryName);
    if (normalized.startsWith(QLatin1Char('/'))) {
        return false;
    }

    // Reject Windows drive-letter prefixes such as "C:/..." or "D:foo".
    if (normalized.size() >= 2 && normalized.at(0).isLetter() &&
        normalized.at(1) == QLatin1Char(':')) {
        return false;
    }

    const QStringList segments = normalized.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    for (const QString& segment : segments) {
        if (segment == QStringLiteral("..")) {
            return false;
        }
    }

    return true;
}

std::optional<QString> ZipArchiveReader::resolveSafeOutputPath(const QString& rootDir,
                                                               const QString& entryName)
{
    if (rootDir.trimmed().isEmpty() || !isSafeRelativeEntryName(entryName)) {
        return std::nullopt;
    }

    QString rel = normalizeSeparators(entryName);
    while (rel.endsWith(QLatin1Char('/'))) {
        rel.chop(1);
    }
    if (rel.isEmpty()) {
        return std::nullopt;
    }

    const QString cleanRoot = QDir::cleanPath(QFileInfo(rootDir).absoluteFilePath());
    const QString candidate = QDir::cleanPath(cleanRoot + QLatin1Char('/') + rel);
    if (candidate != cleanRoot && !candidate.startsWith(cleanRoot + QLatin1Char('/'))) {
        return std::nullopt;
    }

    return candidate;
}

bool ZipArchiveReader::validateAllEntries(const ZipLimits& limits,
                                          std::vector<ZipEntryInfo>* outEntries,
                                          QString* error) const
{
    if (m_reader == nullptr) {
        assignError(error, QStringLiteral("Archive is not open"));
        return false;
    }

    std::vector<ZipEntryInfo> collected;
    qint64 totalBytes = 0;

    int32_t rc = mz_zip_reader_goto_first_entry(m_reader);
    while (rc == MZ_OK) {
        mz_zip_file* fileInfo = nullptr;
        if (mz_zip_reader_entry_get_info(m_reader, &fileInfo) != MZ_OK || fileInfo == nullptr ||
            fileInfo->filename == nullptr) {
            assignError(error, QStringLiteral("Corrupt zip entry metadata"));
            return false;
        }

        const QString name = QString::fromUtf8(fileInfo->filename);
        if (!isSafeRelativeEntryName(name)) {
            assignError(error, QStringLiteral("Unsafe zip entry path rejected: %1").arg(name));
            return false;
        }

        if (static_cast<int>(collected.size()) >= limits.maxEntries) {
            assignError(error, QStringLiteral("Archive exceeds entry count failsafe"));
            return false;
        }

        const bool isDir =
            mz_zip_reader_entry_is_dir(m_reader) == MZ_OK || name.endsWith(QLatin1Char('/'));
        if (fileInfo->uncompressed_size < 0 || fileInfo->compressed_size < 0) {
            assignError(error, QStringLiteral("Negative entry size rejected: %1").arg(name));
            return false;
        }
        if (fileInfo->uncompressed_size > limits.maxEntryBytes) {
            assignError(error, QStringLiteral("Archive entry exceeds size failsafe: %1").arg(name));
            return false;
        }
        if (totalBytes > limits.maxTotalBytes - fileInfo->uncompressed_size) {
            assignError(error, QStringLiteral("Archive total uncompressed size exceeds failsafe"));
            return false;
        }
        totalBytes += fileInfo->uncompressed_size;

        ZipEntryInfo entry;
        entry.name = normalizeSeparators(name);
        entry.uncompressedSize = fileInfo->uncompressed_size;
        entry.compressedSize = fileInfo->compressed_size;
        entry.isDirectory = isDir;
        collected.push_back(std::move(entry));

        rc = mz_zip_reader_goto_next_entry(m_reader);
    }

    if (rc != MZ_END_OF_LIST) {
        assignError(error,
                    QStringLiteral("Archive central directory iteration failed (%1)").arg(rc));
        return false;
    }

    if (outEntries != nullptr) {
        *outEntries = std::move(collected);
    }
    return true;
}

std::vector<ZipEntryInfo> ZipArchiveReader::listEntries(const ZipLimits& limits,
                                                        QString* error) const
{
    std::vector<ZipEntryInfo> entries;
    if (!validateAllEntries(limits, &entries, error)) {
        return {};
    }
    return entries;
}

std::optional<QByteArray> ZipArchiveReader::readEntry(const QString& entryName, qint64 maxBytes,
                                                      QString* error) const
{
    if (m_reader == nullptr) {
        assignError(error, QStringLiteral("Archive is not open"));
        return std::nullopt;
    }
    if (!isSafeRelativeEntryName(entryName)) {
        assignError(error, QStringLiteral("Unsafe entry name rejected: %1").arg(entryName));
        return std::nullopt;
    }

    const QByteArray utf8Name = normalizeSeparators(entryName).toUtf8();
    if (mz_zip_reader_locate_entry(m_reader, utf8Name.constData(), 0) != MZ_OK) {
        assignError(error, QStringLiteral("Entry not found in archive: %1").arg(entryName));
        return std::nullopt;
    }

    mz_zip_file* fileInfo = nullptr;
    if (mz_zip_reader_entry_get_info(m_reader, &fileInfo) != MZ_OK || fileInfo == nullptr) {
        assignError(error, QStringLiteral("Failed to read entry info: %1").arg(entryName));
        return std::nullopt;
    }
    if (mz_zip_reader_entry_is_dir(m_reader) == MZ_OK) {
        assignError(error,
                    QStringLiteral("Cannot read directory entry into buffer: %1").arg(entryName));
        return std::nullopt;
    }
    if (fileInfo->uncompressed_size < 0 || fileInfo->uncompressed_size > maxBytes) {
        assignError(error, QStringLiteral("Entry exceeds memory read cap: %1").arg(entryName));
        return std::nullopt;
    }

    if (mz_zip_reader_entry_open(m_reader) != MZ_OK) {
        assignError(error, QStringLiteral("Failed to open archive entry: %1").arg(entryName));
        return std::nullopt;
    }

    QByteArray result;
    if (fileInfo->uncompressed_size > 0 &&
        fileInfo->uncompressed_size <= std::numeric_limits<int>::max()) {
        result.reserve(static_cast<qsizetype>(fileInfo->uncompressed_size));
    }

    std::vector<char> chunk(kChunkBytes);
    int32_t bytesRead = 0;
    bool overflowed = false;
    while ((bytesRead = mz_zip_reader_entry_read(m_reader, chunk.data(),
                                                 static_cast<int32_t>(chunk.size()))) > 0) {
        if (static_cast<qint64>(result.size()) + bytesRead > maxBytes) {
            overflowed = true;
            break;
        }
        result.append(chunk.data(), static_cast<qsizetype>(bytesRead));
    }
    const int32_t closeRc = mz_zip_reader_entry_close(m_reader);

    if (overflowed) {
        assignError(error,
                    QStringLiteral("Entry decompressed beyond maxBytes cap: %1").arg(entryName));
        return std::nullopt;
    }
    if (bytesRead < 0 || closeRc != MZ_OK) {
        assignError(error, QStringLiteral("Failed reading archive entry: %1").arg(entryName));
        return std::nullopt;
    }

    return result;
}

bool ZipArchiveReader::extractEntries(const QString& outputDir,
                                      const std::vector<QString>& entryFilter,
                                      const ZipLimits& limits, std::vector<QString>* extractedPaths,
                                      QString* error) const
{
    std::vector<ZipEntryInfo> entries;
    if (!validateAllEntries(limits, &entries, error)) {
        return false;
    }

    if (!QDir().mkpath(outputDir)) {
        assignError(error,
                    QStringLiteral("Failed to create extraction directory: %1").arg(outputDir));
        return false;
    }

    QSet<QString> createdDirs;
    std::vector<QString> writtenPaths;
    std::vector<char> chunk(kChunkBytes);

    for (const ZipEntryInfo& entry : entries) {
        if (!entryFilter.empty() && !containsEntry(entryFilter, entry.name)) {
            continue;
        }

        const std::optional<QString> targetPath = resolveSafeOutputPath(outputDir, entry.name);
        if (!targetPath.has_value()) {
            assignError(error,
                        QStringLiteral("Unsafe extraction target rejected: %1").arg(entry.name));
            return false;
        }

        if (entry.isDirectory) {
            if (!QDir().mkpath(*targetPath)) {
                assignError(error,
                            QStringLiteral("Failed to create subdirectory: %1").arg(*targetPath));
                return false;
            }
            continue;
        }

        const QString parentDir = QFileInfo(*targetPath).absolutePath();
        if (!createdDirs.contains(parentDir)) {
            if (!QDir().mkpath(parentDir)) {
                assignError(error,
                            QStringLiteral("Failed to create parent dir: %1").arg(parentDir));
                return false;
            }
            createdDirs.insert(parentDir);
        }

        const QByteArray utf8Name = entry.name.toUtf8();
        if (mz_zip_reader_locate_entry(m_reader, utf8Name.constData(), 0) != MZ_OK ||
            mz_zip_reader_entry_open(m_reader) != MZ_OK) {
            assignError(error,
                        QStringLiteral("Failed to open entry for extraction: %1").arg(entry.name));
            return false;
        }

        QSaveFile outFile(*targetPath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            mz_zip_reader_entry_close(m_reader);
            assignError(error, QStringLiteral("Failed to open output file: %1").arg(*targetPath));
            return false;
        }

        qint64 writtenForEntry = 0;
        int32_t bytesRead = 0;
        bool writeOk = true;
        while ((bytesRead = mz_zip_reader_entry_read(m_reader, chunk.data(),
                                                     static_cast<int32_t>(chunk.size()))) > 0) {
            writtenForEntry += bytesRead;
            if (writtenForEntry > limits.maxEntryBytes) {
                writeOk = false;
                assignError(
                    error, QStringLiteral("Decompressed entry exceeded limit: %1").arg(entry.name));
                break;
            }
            if (outFile.write(chunk.data(), bytesRead) != bytesRead) {
                writeOk = false;
                assignError(error, QStringLiteral("Disk write failed for: %1").arg(*targetPath));
                break;
            }
        }
        const int32_t closeRc = mz_zip_reader_entry_close(m_reader);

        if (!writeOk || bytesRead < 0 || closeRc != MZ_OK || !outFile.commit()) {
            outFile.cancelWriting();
            if (error != nullptr && error->isEmpty()) {
                assignError(error, QStringLiteral("Failed extracting entry: %1").arg(entry.name));
            }
            return false;
        }

        writtenPaths.push_back(*targetPath);
    }

    if (extractedPaths != nullptr) {
        *extractedPaths = std::move(writtenPaths);
    }
    return true;
}

} // namespace kmedia
