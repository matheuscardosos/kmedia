#include "parsers/SqlitePackageUtils.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace kmedia {

ScopedReadOnlySqliteDb::~ScopedReadOnlySqliteDb()
{
    close();
}

bool ScopedReadOnlySqliteDb::open(const QString& dbFilePath, QString* error)
{
    close();

    m_connectionName =
        QStringLiteral("kmedia_pkg_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(dbFilePath);
    db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));

    if (!db.open()) {
        if (error != nullptr) {
            *error = QStringLiteral("Failed to open SQLite package DB '%1': %2")
                         .arg(dbFilePath, db.lastError().text());
        }
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName.clear();
        return false;
    }

    m_isOpen = true;
    return true;
}

void ScopedReadOnlySqliteDb::close()
{
    if (m_connectionName.isEmpty()) {
        m_isOpen = false;
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isValid() && db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(m_connectionName);
    m_connectionName.clear();
    m_isOpen = false;
}

bool ScopedReadOnlySqliteDb::isOpen() const
{
    return m_isOpen;
}

QSqlDatabase ScopedReadOnlySqliteDb::database() const
{
    return m_isOpen ? QSqlDatabase::database(m_connectionName, false) : QSqlDatabase{};
}

bool ScopedReadOnlySqliteDb::tableExists(const QString& tableName) const
{
    if (!m_isOpen) {
        return false;
    }
    QSqlQuery query(database());
    query.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = :name LIMIT 1"));
    query.bindValue(QStringLiteral(":name"), tableName);
    return query.exec() && query.next();
}

QSet<QString> ScopedReadOnlySqliteDb::tableColumns(const QString& tableName) const
{
    QSet<QString> columns;
    if (!m_isOpen || tableName.isEmpty() || !tableExists(tableName)) {
        return columns;
    }

    QString sanitizedName = tableName;
    sanitizedName.replace(QLatin1Char('"'), QStringLiteral("\"\""));

    QSqlQuery query(database());
    if (!query.exec(QStringLiteral("PRAGMA table_info(\"%1\")").arg(sanitizedName))) {
        return columns;
    }
    while (query.next()) {
        columns.insert(query.value(1).toString());
    }
    return columns;
}

} // namespace kmedia
