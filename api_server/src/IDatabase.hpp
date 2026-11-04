#pragma once
#include <pqxx/pqxx>
#include <string>
#include <memory>
#include <vector>
#include <utility>
#include <tuple>
#include <optional>

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
    
    // Access to underlying connection for direct operations (FlowsApi)
    virtual std::shared_ptr<pqxx::connection> connection() = 0;
    
    // User operations
    virtual std::string hash_password(const std::string& password) = 0;
    virtual bool verify_password(const std::string& password, const std::string& hash) = 0;
    virtual bool user_exists_by_email(const std::string& email) = 0;
    virtual bool user_exists_by_nickname(const std::string& nickname) = 0;
    virtual std::pair<bool, std::string> create_user(
        const std::string& first_name,
        const std::string& last_name,
        const std::optional<std::string>& nickname,
        const std::string& email,
        const std::string& password_hash
    ) = 0;
    virtual std::tuple<bool, std::string, std::string> get_user_by_email(const std::string& email) = 0;
    virtual std::tuple<bool, std::string, std::string> get_user_by_nickname(const std::string& nickname) = 0;
    
    // Session operations
    virtual std::pair<bool, std::string> create_session(const std::string& user_id, const std::string& token_hash, const std::string& expires_at) = 0;
    virtual std::tuple<bool, std::string, std::string> get_user_by_session_token(const std::string& token_hash) = 0;
    virtual bool delete_session(const std::string& token_hash) = 0;
    virtual bool delete_sessions_for_user(const std::string& user_id) = 0;
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
    std::shared_ptr<pqxx::connection> connection() override;
    
    // User operations
    std::string hash_password(const std::string& password) override;
    bool verify_password(const std::string& password, const std::string& hash) override;
    bool user_exists_by_email(const std::string& email) override;
    bool user_exists_by_nickname(const std::string& nickname) override;
    std::pair<bool, std::string> create_user(
        const std::string& first_name,
        const std::string& last_name,
        const std::optional<std::string>& nickname,
        const std::string& email,
        const std::string& password_hash
    ) override;
    std::tuple<bool, std::string, std::string> get_user_by_email(const std::string& email) override;
    std::tuple<bool, std::string, std::string> get_user_by_nickname(const std::string& nickname) override;
    
    // Session operations
    std::pair<bool, std::string> create_session(const std::string& user_id, const std::string& token_hash, const std::string& expires_at) override;
    std::tuple<bool, std::string, std::string> get_user_by_session_token(const std::string& token_hash) override;
    bool delete_session(const std::string& token_hash) override;
    bool delete_sessions_for_user(const std::string& user_id) override;

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

inline std::shared_ptr<pqxx::connection> Database::connection() {
    return conn_;
}

// User operations
inline std::string Database::hash_password(const std::string& password) {
    return password;
}

inline bool Database::verify_password(const std::string& password, const std::string& hash) {
    return password == hash;
}

inline bool Database::user_exists_by_email(const std::string& email) {
    pqxx::nontransaction txn(*conn_);
    return !txn.exec_params("SELECT 1 FROM users WHERE email = $1", email.c_str()).empty();
}

inline bool Database::user_exists_by_nickname(const std::string& nickname) {
    pqxx::nontransaction txn(*conn_);
    return !txn.exec_params("SELECT 1 FROM users WHERE nickname = $1", nickname.c_str()).empty();
}

inline std::pair<bool, std::string> Database::create_user(
    const std::string& first_name,
    const std::string& last_name,
    const std::optional<std::string>& nickname,
    const std::string& email,
    const std::string& password_hash
) {
    try {
        pqxx::work txn(*conn_);
        const char* nickname_cstr = nickname.has_value() ? nickname->c_str() : nullptr;
        auto r = txn.exec_params(
            "INSERT INTO users (first_name, last_name, nickname, email, password_hash) VALUES ($1, $2, $3, $4, $5) RETURNING id, email, nickname",
            first_name.c_str(), last_name.c_str(), nickname_cstr, email.c_str(), password_hash.c_str()
        );
        txn.commit();
        
        if (r.empty()) {
            return {false, "Failed to create user"};
        }
        
        std::string user_id = r[0]["id"].c_str();
        return {true, user_id};
    } catch (const std::exception& e) {
        return {false, e.what()};
    }
}

inline std::tuple<bool, std::string, std::string> Database::get_user_by_email(const std::string& email) {
    pqxx::nontransaction txn(*conn_);
    auto r = txn.exec_params("SELECT id, password_hash FROM users WHERE email = $1", email.c_str());
    
    if (r.empty()) {
        return {false, "", ""};
    }
    
    std::string user_id = r[0]["id"].c_str();
    std::string password_hash = r[0]["password_hash"].c_str();
    return {true, user_id, password_hash};
}

inline std::tuple<bool, std::string, std::string> Database::get_user_by_nickname(const std::string& nickname) {
    pqxx::nontransaction txn(*conn_);
    auto r = txn.exec_params("SELECT id, password_hash FROM users WHERE nickname = $1", nickname.c_str());
    
    if (r.empty()) {
        return {false, "", ""};
    }
    
    std::string user_id = r[0]["id"].c_str();
    std::string password_hash = r[0]["password_hash"].c_str();
    return {true, user_id, password_hash};
}

// Session operations
inline std::pair<bool, std::string> Database::create_session(
    const std::string& user_id,
    const std::string& token_hash,
    const std::string& expires_at
) {
    try {
        pqxx::work txn(*conn_);
        auto r = txn.exec_params(
            "INSERT INTO sessions (user_id, token_hash, expires_at) VALUES ($1, $2, $3) RETURNING id",
            user_id.c_str(), token_hash.c_str(), expires_at.c_str()
        );
        txn.commit();
        
        if (r.empty()) {
            return {false, "Failed to create session"};
        }
        
        return {true, "Session created"};
    } catch (const std::exception& e) {
        return {false, e.what()};
    }
}

inline std::tuple<bool, std::string, std::string> Database::get_user_by_session_token(const std::string& token_hash) {
    pqxx::nontransaction txn(*conn_);
    auto r = txn.exec_params(
        "SELECT u.id, u.email FROM users u JOIN sessions s ON u.id = s.user_id WHERE s.token_hash = $1 AND s.expires_at > NOW()",
        token_hash.c_str()
    );
    
    if (r.empty()) {
        return {false, "", ""};
    }
    
    std::string user_id = r[0]["id"].c_str();
    std::string email = r[0]["email"].c_str();
    return {true, user_id, email};
}

inline bool Database::delete_session(const std::string& token_hash) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params("DELETE FROM sessions WHERE token_hash = $1", token_hash.c_str());
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

inline bool Database::delete_sessions_for_user(const std::string& user_id) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params("DELETE FROM sessions WHERE user_id = $1", user_id.c_str());
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}
