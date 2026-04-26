#include "DbExecutor.hpp"

#include <algorithm>

namespace geruest::db {

DbExecutor::DbExecutor(std::size_t threadCount) : _pool(std::max<std::size_t>(threadCount, 1)) {}

DbExecutor::~DbExecutor() { join(); }

void DbExecutor::join() {
    if (_joined) {
        return;
    }
    _pool.join();
    _joined = true;
}

}  // namespace geruest::db
