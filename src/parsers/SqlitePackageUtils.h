#pragma once

#include <QSet>
#include <QSqlDatabase>
#include <QString>

namespace kmedia {

/**
 * RAII read-only handle over a SQLite package database file.
 * Ensures the SQLite connection is closed and removed before temporary sandboxes clean up.
 */
class ScopedReadOnlySqliteDb
{
public:
    ScopedReadOnlySqliteDb() = default;
    ~ScopedReadOnlySqliteDb();

    ScopedReadOnlySqliteDb(const ScopedReadOnlySqliteDb&) = delete;
    ScopedReadOnlySqliteDb& operator=(const ScopedReadOnlySqliteDb&) = delete;
    ScopedReadOnlySqliteDb(ScopedReadOnlySqliteDb&&) = delete;
    ScopedReadOnlySqliteDb& operator=(ScopedReadOnlySqliteDb&&) = delete;

    [[nodiscard]] bool open(const QString& dbFilePath, QString* error = nullptr);
    void close();

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] QSqlDatabase database() const;

    [[nodiscard]] bool tableExists(const QString& tableName) const;
    [[nodiscard]] QSet<QString> tableColumns(const QString& tableName) const;

private:
    QString m_connectionName;
    bool m_isOpen{false};
};

} // namespace kmedia
