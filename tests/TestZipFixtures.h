#pragma once

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QUuid>

#include <cstdint>
#include <ctime>
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>
#include <utility>
#include <vector>

namespace kmedia::test_helpers {

inline bool writeZipFile(const QString& zipPath,
                         const std::vector<std::pair<QString, QByteArray>>& entries)
{
    void* writer = mz_zip_writer_create();
    if (writer == nullptr) {
        return false;
    }

    const QByteArray pathUtf8 = zipPath.toUtf8();
    if (mz_zip_writer_open_file(writer, pathUtf8.constData(), 0, 0) != MZ_OK) {
        mz_zip_writer_delete(&writer);
        return false;
    }

    bool ok = true;
    for (const auto& [entryName, payload] : entries) {
        mz_zip_file fileInfo{};
        const QByteArray nameUtf8 = entryName.toUtf8();
        fileInfo.filename = nameUtf8.constData();
        fileInfo.modified_date = std::time(nullptr);
        fileInfo.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
        fileInfo.flag = MZ_ZIP_FLAG_UTF8;

        if (mz_zip_writer_add_buffer(writer, payload.constData(),
                                     static_cast<int32_t>(payload.size()), &fileInfo) != MZ_OK) {
            ok = false;
            break;
        }
    }

    if (mz_zip_writer_close(writer) != MZ_OK) {
        ok = false;
    }
    mz_zip_writer_delete(&writer);
    return ok;
}

inline QByteArray buildSqliteBytes(const QString& tempDir, const QStringList& statements)
{
    const QString dbPath = QDir(tempDir).filePath(
        QStringLiteral("%1.db").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    const QString connName =
        QStringLiteral("test_fixture_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            return {};
        }

        QSqlQuery query(db);
        for (const QString& stmt : statements) {
            if (!query.exec(stmt)) {
                db.close();
                QSqlDatabase::removeDatabase(connName);
                return {};
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connName);

    QFile file(dbPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    file.close();
    QFile::remove(dbPath);
    return bytes;
}

inline bool writeJwpubFixture(const QString& outputPath, const QString& workTempDir,
                              const QByteArray& manifestJson, const QByteArray& innerDbBytes,
                              const QString& dbEntryName,
                              const std::vector<std::pair<QString, QByteArray>>& innerAssets)
{
    std::vector<std::pair<QString, QByteArray>> contentsEntries;
    contentsEntries.emplace_back(dbEntryName, innerDbBytes);
    for (const auto& asset : innerAssets) {
        contentsEntries.push_back(asset);
    }

    const QString contentsPath = QDir(workTempDir).filePath(QStringLiteral("contents.tmp.zip"));
    if (!writeZipFile(contentsPath, contentsEntries)) {
        return false;
    }

    QFile contentsFile(contentsPath);
    if (!contentsFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray contentsBytes = contentsFile.readAll();
    contentsFile.close();
    QFile::remove(contentsPath);

    std::vector<std::pair<QString, QByteArray>> outerEntries;
    outerEntries.emplace_back(QStringLiteral("manifest.json"), manifestJson);
    outerEntries.emplace_back(QStringLiteral("contents"), contentsBytes);
    return writeZipFile(outputPath, outerEntries);
}

} // namespace kmedia::test_helpers
