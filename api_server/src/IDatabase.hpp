#pragma once
#include <pqxx/pqxx>
#include <string>
#include <memory>
#include <vector>

// Interface for database operations - allows mocking
class IDatabase {
public:
    virtual ~IDatabase() = default;
    
    // Transaction interface
    class ITransaction {
    public:
        virtual ~ITransaction() = default;
        virtual pqxx::result exec_params(const char* query, const char* p1 = nullptr, const char* p2 = nullptr, const char* p3 = nullptr, const char* p4 = nullptr, const char* p5 = nullptr) = 0;
        virtual pqxx::result exec(const char* query) = 0;
        virtual void commit() = 0;
    };
    
    virtual std::unique_ptr<ITransaction> begin_work() = 0;
    virtual std::unique_ptr<ITransaction> begin_nontransaction() = 0;
};

// Transaction wrapper
class DatabaseTransaction : public IDatabase::ITransaction {
public:
    explicit DatabaseTransaction(pqxx::work* work);
    explicit DatabaseTransaction(pqxx::nontransaction* txn);
    
    pqxx::result exec_params(const char* query, const char* p1 = nullptr, const char* p2 = nullptr, const char* p3 = nullptr, const char* p4 = nullptr, const char* p5 = nullptr) override;
    pqxx::result exec(const char* query) override;
    void commit() override;

private:
    std::unique_ptr<pqxx::work> work_txn_;
    std::unique_ptr<pqxx::nontransaction> read_txn_;
};

// Implementation that wraps actual database connection
class Database : public IDatabase {
public:
    explicit Database(std::shared_ptr<pqxx::connection> conn);
    
    std::unique_ptr<ITransaction> begin_work() override;
    std::unique_ptr<ITransaction> begin_nontransaction() override;

private:
    std::shared_ptr<pqxx::connection> conn_;
};

// Inline implementations
inline DatabaseTransaction::DatabaseTransaction(pqxx::work* work) : work_txn_(work) {}

inline DatabaseTransaction::DatabaseTransaction(pqxx::nontransaction* txn) : read_txn_(txn) {}

inline pqxx::result DatabaseTransaction::exec_params(const char* query, const char* p1, const char* p2, const char* p3, const char* p4, const char* p5) {
    if (work_txn_) {
        return work_txn_->exec_params(query, p1, p2, p3, p4, p5);
    } else {
        return read_txn_->exec_params(query, p1, p2, p3, p4, p5);
    }
}

inline pqxx::result DatabaseTransaction::exec(const char* query) {
    if (work_txn_) {
        return work_txn_->exec(query);
    } else {
        return read_txn_->exec(query);
    }
}

inline void DatabaseTransaction::commit() {
    if (work_txn_) {
        work_txn_->commit();
    }
}

inline Database::Database(std::shared_ptr<pqxx::connection> conn) : conn_(std::move(conn)) {}

inline std::unique_ptr<IDatabase::ITransaction> Database::begin_work() {
    return std::make_unique<DatabaseTransaction>(new pqxx::work(*conn_));
}

inline std::unique_ptr<IDatabase::ITransaction> Database::begin_nontransaction() {
    return std::make_unique<DatabaseTransaction>(new pqxx::nontransaction(*conn_));
}
