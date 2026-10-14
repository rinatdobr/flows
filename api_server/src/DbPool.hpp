#pragma once
#include <pqxx/pqxx>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>

class DbPool {
public:
    static DbPool& instance();
    void init(const std::string& conn_str, size_t pool_size);
    
    // RAII Connection wrapper
    class ConnectionGuard {
    public:
        ConnectionGuard(DbPool& pool, std::shared_ptr<pqxx::connection> conn);
        ~ConnectionGuard();
        pqxx::connection& get();

    private:
        DbPool& pool_;
        std::shared_ptr<pqxx::connection> conn_;
    };

    ConnectionGuard acquire();

private:
    DbPool() = default;
    void release(std::shared_ptr<pqxx::connection> conn);

    std::queue<std::shared_ptr<pqxx::connection>> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::string conn_str_;
};
