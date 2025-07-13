/**
 * @file initDB.hpp
 * @date 28.07.24.
 *
 * @author Urs Behrmann
 *
 * @brief This function is used to initialize the database.
 */

#ifndef INITDB_HPP
#define INITDB_HPP

// Logs db
#define LOGS "logs"
#define LOG_TYPES "logTypes"

#include "SQLiteInterfaceSingleton.hpp"

inline void initDB() {

    SQLiteInterfaceSingleton& db = SQLiteInterfaceSingleton::getInstance(
        "/data/server.db");

    // Init db Logs
    // Create LOG_TYPES table
    db.create(LOG_TYPES, {
                  {"id INTEGER PRIMARY KEY AUTOINCREMENT"},
                  {"logType TEXT UNIQUE"}
              });

    // Create LOGS table with foreign keys referencing the LOG_TYPES and DOMAINS tables
    db.create(LOGS, {
                  {"id INTEGER PRIMARY KEY AUTOINCREMENT"},
                  {"logTypeID INTEGER"},
                  {"timestamp DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL"},
                  {"ip TEXT"},
                  {"message TEXT"},
                  {"FOREIGN KEY (logTypeID) REFERENCES logTypes (id)"},
              });
}

// Order of statements in sqlite3
// SELECT
// FROM
// JOIN
// WHERE
// GROUP BY
// HAVING
// ORDER BY
// LIMIT


#endif //INITDB_HPP
