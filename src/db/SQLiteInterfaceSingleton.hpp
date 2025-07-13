/**
 * @file SQLiteInterfaceSingleton.hpp
 * @date 21.07.24
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to interact with an SQLite database.
 */

#ifndef SQLITEINTERFACESINGLETON_HPP
#define SQLITEINTERFACESINGLETON_HPP

#include <sqlite3.h>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>

using SQLiteLine = std::vector<std::pair<std::string, std::string>>;
using SQLiteTable = std::vector<SQLiteLine>;

class SQLiteInterfaceSingleton {
public:
    static SQLiteInterfaceSingleton& getInstance(const std::string& dbPath);

    void create(const std::string& tableName, const std::vector<std::string>& columns);
    void insert(const std::string& tableName, const std::vector<std::string>& values);
    void insert(const std::string& tableName, const SQLiteLine& columnValuePairs);
    SQLiteTable readAll(const std::string& tableName, const std::string& options = "");
    SQLiteTable read(const std::string& rowsToSelect, const std::string& tableName, const std::string& options = "");
    void update(const std::string& tableName, const std::vector<std::string>& columnNames, const std::vector<std::string>& columnValues, const std::string& options);
    void deleteFrom(const std::string& tableName, const std::string& whereClause);
    void drop(const std::string& tableName);
    void emptyTable(const std::string& tableName, const std::string& options = "");


private:
    SQLiteInterfaceSingleton(const std::string& dbPath);
    ~SQLiteInterfaceSingleton();
    SQLiteInterfaceSingleton(const SQLiteInterfaceSingleton&) = delete;
    SQLiteInterfaceSingleton& operator=(const SQLiteInterfaceSingleton&) = delete;

    sqlite3* db{};
    void executeSQL(const std::string& sql);
};

#endif //SQLITEINTERFACESINGLETON_HPP