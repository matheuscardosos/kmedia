#pragma once

#include <QByteArray>
#include <QString>

#include <optional>
#include <vector>

namespace kmedia {

struct ZipEntryInfo
{
    QString name;
    qint64 uncompressedSize{0};
    qint64 compressedSize{0};
    bool isDirectory{false};
};

struct ZipLimits
{
    int maxEntries{20000};
    qint64 maxEntryBytes{256LL * 1024 * 1024};
    qint64 maxTotalBytes{2LL * 1024 * 1024 * 1024};
};

/**
 * RAII ZIP archive inspector and selective extractor backed by minizip-ng.
 * Every archive operation enforces Zip-Slip path validation and decompression caps.
 */
class ZipArchiveReader
{
public:
    ZipArchiveReader() = default;
    ~ZipArchiveReader();

    ZipArchiveReader(const ZipArchiveReader&) = delete;
    ZipArchiveReader& operator=(const ZipArchiveReader&) = delete;
    ZipArchiveReader(ZipArchiveReader&&) = delete;
    ZipArchiveReader& operator=(ZipArchiveReader&&) = delete;

    [[nodiscard]] bool openFile(const QString& archivePath, QString* error = nullptr);
    [[nodiscard]] bool openMemory(const QByteArray& zipBytes, QString* error = nullptr);
    void close();

    [[nodiscard]] bool isOpen() const;

    [[nodiscard]] std::vector<ZipEntryInfo> listEntries(const ZipLimits& limits = {},
                                                        QString* error = nullptr) const;

    [[nodiscard]] std::optional<QByteArray> readEntry(const QString& entryName,
                                                      qint64 maxBytes = 64LL * 1024 * 1024,
                                                      QString* error = nullptr) const;

    [[nodiscard]] bool extractEntries(const QString& outputDir,
                                      const std::vector<QString>& entryFilter = {},
                                      const ZipLimits& limits = {},
                                      std::vector<QString>* extractedPaths = nullptr,
                                      QString* error = nullptr) const;

    [[nodiscard]] static bool isSafeRelativeEntryName(const QString& entryName);
    [[nodiscard]] static std::optional<QString> resolveSafeOutputPath(const QString& rootDir,
                                                                      const QString& entryName);

private:
    bool validateAllEntries(const ZipLimits& limits, std::vector<ZipEntryInfo>* outEntries,
                            QString* error) const;

    void* m_reader{nullptr};
    QByteArray m_memoryBacking;
};

} // namespace kmedia
