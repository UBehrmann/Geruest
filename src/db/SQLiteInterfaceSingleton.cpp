/**
 * @file SQLiteInterfaceSingleton.cpp
 * @date 21.07.24
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to interact with an SQLite database.
 */

#include "SQLiteInterfaceSingleton.hpp"
#include <sstream>

SQLiteInterfaceSingleton::SQLiteInterfaceSingleton(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error("Failed to open database: " + std::string(sqlite3_errmsg(db)) + " - Path: " + dbPath);
    }

    // Set the busy timeout to 100ms
    if (sqlite3_busy_timeout(db, 100)) {
        throw std::runtime_error("Can't set busy timeout: " + std::string(sqlite3_errmsg(db)));
    }
}

SQLiteInterfaceSingleton::~SQLiteInterfaceSingleton() {
    sqlite3_close(db);
}

SQLiteInterfaceSingleton& SQLiteInterfaceSingleton::getInstance(const std::string& dbPath) {
    static SQLiteInterfaceSingleton instance(dbPath);
    return instance;
}

void SQLiteInterfaceSingleton::create(const std::string& tableName, const std::vector<std::string>& columns) {
    std::stringstream ss;
    ss << "CREATE TABLE IF NOT EXISTS " << tableName << " (";
    for (size_t i = 0; i < columns.size(); ++i) {
        ss << columns[i];
        if (i < columns.size() - 1) {
            ss << ", ";
        }
    }
    ss << ");";
    executeSQL(ss.str());
}

void SQLiteInterfaceSingleton::insert(const std::string& tableName, const std::vector<std::string>& values) {
    std::stringstream ss;
    ss << "INSERT INTO " << tableName << " VALUES (";
    for (size_t i = 0; i < values.size(); ++i) {
        ss << "'" << values[i] << "'";
        if (i < values.size() - 1) {
            ss << ", ";
        }
    }
    ss << ");";
    executeSQL(ss.str());
}

void SQLiteInterfaceSingleton::insert(const std::string& tableName, const SQLiteLine& columnValuePairs) {
    std::stringstream ss;
    ss << "INSERT INTO " << tableName;

    if(columnValuePairs.empty()) {
        ss << " DEFAULT VALUES;";
    }else {
        ss << " (";

        // Append column names
        for (size_t i = 0; i < columnValuePairs.size(); ++i) {
            ss << columnValuePairs[i].first;
            if (i < columnValuePairs.size() - 1) {
                ss << ", ";
            }
        }

        ss << ") VALUES (";

        // Append values
        for (size_t i = 0; i < columnValuePairs.size(); ++i) {
            ss << "'" << columnValuePairs[i].second << "'";
            if (i < columnValuePairs.size() - 1) {
                ss << ", ";
            }
        }

        ss << ");";
    }

    executeSQL(ss.str());
}

SQLiteTable SQLiteInterfaceSingleton::readAll(const std::string& tableName, const std::string& options) {

    return read("*", tableName, options);
}

SQLiteTable SQLiteInterfaceSingleton::read(const std::string& rowsToSelect, const std::string& tableName,
    const std::string& options) {
    std::stringstream ss;
    ss << "SELECT " << rowsToSelect << " FROM " << tableName << " ";
    if (!options.empty()) {
        ss << options;
    }
    ss << ";";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, ss.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)) + " - SQL: " + ss.str());
    }

    int columnCount = sqlite3_column_count(stmt);
    SQLiteTable rows;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::vector<std::pair<std::string, std::string>> row;
        for (int i = 0; i < columnCount; ++i) {
            std::string columnName = sqlite3_column_name(stmt, i);
            const char* columnValueCStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            std::string columnValue = columnValueCStr ? columnValueCStr : ""; // Handle NULL values
            row.emplace_back(columnName, columnValue);
        }
        rows.push_back(row);
    }

    sqlite3_finalize(stmt);
    return rows;
}

void SQLiteInterfaceSingleton::update(const std::string& tableName, const std::vector<std::string>& columnNames, const std::vector<std::string>& columnValues, const std::string& whereClause) {
    if (columnNames.size() != columnValues.size()) {
        throw std::invalid_argument("Column names size does not match column values size.");
    }

    std::stringstream ss;
    ss << "UPDATE " << tableName << " SET ";
    for (size_t i = 0; i < columnNames.size(); ++i) {
        ss << columnNames[i] << " = '" << columnValues[i] << "'";
        if (i < columnNames.size() - 1) {
            ss << ", ";
        }
    }
    if (!whereClause.empty()) {
        ss << " WHERE " << whereClause;
    }
    ss << ";";
    executeSQL(ss.str());
}

void SQLiteInterfaceSingleton::deleteFrom(const std::string& tableName, const std::string& whereClause) {
    std::stringstream ss;
    ss << "DELETE FROM " << tableName;
    if (!whereClause.empty()) {
        ss << " WHERE " << whereClause;
    }
    ss << ";";
    executeSQL(ss.str());
}

void SQLiteInterfaceSingleton::drop(const std::string& tableName) {
    std::stringstream ss;
    ss << "DROP TABLE IF EXISTS " << tableName << ";";
    executeSQL(ss.str());
}

void SQLiteInterfaceSingleton::emptyTable(const std::string& tableName, const std::string& options) {
    std::stringstream ss;
    ss << "DELETE FROM " << tableName;
    if(!options.empty()) {
        ss << " " << options;
    }
    ss << ";";
    executeSQL(ss.str());
}

void SQLiteInterfaceSingleton::executeSQL(const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string err = errMsg;
        sqlite3_free(errMsg);
        throw std::runtime_error("SQL error: " + err + " - SQL: " + sql);
    }
}
