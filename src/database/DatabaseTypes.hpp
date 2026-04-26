#ifndef GERUEST_DATABASE_TYPES_HPP
#define GERUEST_DATABASE_TYPES_HPP

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace geruest::db {

using BindValue = std::variant<std::nullptr_t, std::int64_t, double, std::string>;

struct QueryRow {
    std::vector<std::string> columns;
};

struct QueryResult {
    std::vector<std::string> columnNames;
    std::vector<QueryRow> rows;
    std::uint64_t affectedRows = 0;
};

enum class Backend {
    None,
    Postgres,
    Sqlite,
};

}  // namespace geruest::db

#endif  // GERUEST_DATABASE_TYPES_HPP
