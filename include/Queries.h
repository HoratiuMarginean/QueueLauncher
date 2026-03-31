#pragma once

#include <qstring.h>

namespace queries
{
#pragma region Create Queries

  const QString createTableApp =
    "CREATE TABLE IF NOT EXISTS app ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " name TEXT NOT NULL,"
    " queue_index INTEGER"
    ")";

  const QString createTableLocalApp =
    "CREATE TABLE IF NOT EXISTS local_app ("
    " app_id INTEGER PRIMARY KEY,"
    " path TEXT NOT NULL,"
    " FOREIGN KEY(app_id) REFERENCES app(id)"
    ")";

  const QString createTableStore =
    "CREATE TABLE IF NOT EXISTS store ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " name TEXT NOT NULL,"
    " path TEXT NOT NULL"
    ")";

  const QString createTableStoreApp =
    "CREATE TABLE IF NOT EXISTS store_app ("
    " app_id INTEGER,"
    " store_id INTEGER,"
    " external_id INTEGER NOT NULL,"
    " PRIMARY KEY(app_id, store_id),"
    " FOREIGN KEY(app_id) REFERENCES app(id),"
    " FOREIGN KEY(store_id) REFERENCES store(id)"
    ")";

  const QString createTableOption =
    "CREATE TABLE IF NOT EXISTS option ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " name TEXT NOT NULL,"
    " value TEXT NOT NULL"
    ")";

#pragma endregion

#pragma region Select Queries

  const QString selectStorePathSteam =
    "SELECT path "
    "FROM store "
    "WHERE name = 'Steam'";

  const QString selectStoreIdSteam =
    "SELECT id "
    "FROM store "
    "WHERE name = 'Steam'";

#pragma endregion
}