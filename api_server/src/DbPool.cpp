#include "DbPool.hpp"

DbPool& DbPool::instance() {
    static DbPool pool;
    return pool;
}

void DbPool::init(const std::string& conn_str, size_t pool_size) {
    conn_str_ = conn_str;
    for (size_t i = 0; i < pool_size; ++i) {
        pool_.push(std::make_shared<pqxx::connection>(conn_str_));
    }
}

DbPool::ConnectionGuard DbPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]() { return !pool_.empty(); });
    auto conn = pool_.front();
    pool_.pop();
    return ConnectionGuard(*this, conn);
}

void DbPool::release(std::shared_ptr<pqxx::connection> conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    pool_.push(conn);
    cv_.notify_one();
}

DbPool::ConnectionGuard::ConnectionGuard(DbPool& pool, std::shared_ptr<pqxx::connection> conn)
    : pool_(pool), conn_(std::move(conn)) {}

DbPool::ConnectionGuard::~ConnectionGuard() {
    pool_.release(conn_);
}

pqxx::connection& DbPool::ConnectionGuard::get() {
    return *conn_;
}
