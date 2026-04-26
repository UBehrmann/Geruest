#include "DbExecutor.hpp"

#include <algorithm>

namespace geruest::db {

DbExecutor::DbExecutor(std::size_t threadCount) : _pool(std::max<std::size_t>(threadCount, 1)) {}

DbExecutor::~DbExecutor() { _pool.join(); }

}  // namespace geruest::db
