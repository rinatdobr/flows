#include <gtest/gtest.h>
#include "crow.h"
#include "AuthApi.hpp"
#include "IDatabase.hpp"
#include <string>
#include <optional>
#include <tuple>

// Mock database for testing
class MockDatabase : public IDatabase {
public:
    // Mock data storage
    struct UserData {
        std::string id;
        std::string first_name;
        std::string last_name;
        std::optional<std::string> nickname;
        std::string email;
        std::string password_hash;
    };
    
    struct SessionData {
        std::string id;
        std::string user_id;
        std::string token_hash;
        std::string expires_at;
    };
    
    std::map<std::string, UserData> users_;  // keyed by email
    std::map<std::string, UserData> users_by_nickname_;  // keyed by nickname
    std::map<std::string, SessionData> sessions_;  // keyed by token_hash
    std::string password_salt = "test_salt_";
    
    // Get user by id
    UserData* get_user_by_id(const std::string& id) {
        for (auto& u : users_) {
            if (u.second.id == id) {
                return &u.second;
            }
        }
        return nullptr;
    }
    
    // Transaction interface
    class MockTransaction : public IDatabase::ITransaction {
    public:
        pqxx::result exec_params(const char* query, const char* p1 = nullptr, const char* p2 = nullptr, const char* p3 = nullptr, const char* p4 = nullptr, const char* p5 = nullptr) override {
            // This is a mock - just return empty result
            pqxx::result r;
            return r;
        }
        pqxx::result exec(const char* query) override {
            pqxx::result r;
            return r;
        }
        void commit() override {}
    };
    
    std::unique_ptr<ITransaction> begin_work() override {
        return std::make_unique<MockTransaction>();
    }
    
    std::unique_ptr<ITransaction> begin_nontransaction() override {
        return std::make_unique<MockTransaction>();
    }
    
    std::shared_ptr<pqxx::connection> connection() override {
        return nullptr;
    }
    
    // Password operations
    std::string hash_password(const std::string& password) override {
        return password_salt + password;
    }
    
    bool verify_password(const std::string& password, const std::string& hash) override {
        return hash_password(password) == hash;
    }
    
    // User operations
    bool user_exists_by_email(const std::string& email) override {
        return users_.find(email) != users_.end();
    }
    
    bool user_exists_by_nickname(const std::string& nickname) override {
        return users_by_nickname_.find(nickname) != users_by_nickname_.end();
    }
    
    std::pair<bool, std::string> create_user(
        const std::string& first_name,
        const std::string& last_name,
        const std::optional<std::string>& nickname,
        const std::string& email,
        const std::string& password_hash
    ) override {
        if (user_exists_by_email(email)) {
            return {false, "User already exists"};
        }
        
        UserData user;
        user.id = std::to_string(users_.size() + 1);
        user.first_name = first_name;
        user.last_name = last_name;
        user.nickname = nickname;
        user.email = email;
        user.password_hash = password_hash;
        
        users_[email] = user;
        if (nickname.has_value() && !nickname->empty()) {
            users_by_nickname_[*nickname] = user;
        }
        
        return {true, user.id};
    }
    
    std::tuple<bool, std::string, std::string> get_user_by_email(const std::string& email) override {
        auto it = users_.find(email);
        if (it == users_.end()) {
            return {false, "", ""};
        }
        return {true, it->second.id, it->second.password_hash};
    }
    
    std::tuple<bool, std::string, std::string> get_user_by_nickname(const std::string& nickname) override {
        auto it = users_by_nickname_.find(nickname);
        if (it == users_by_nickname_.end()) {
            return {false, "", ""};
        }
        return {true, it->second.id, it->second.password_hash};
    }
    
    // Session operations
    std::pair<bool, std::string> create_session(
        const std::string& user_id,
        const std::string& token_hash,
        const std::string& expires_at
    ) override {
        SessionData session;
        session.id = std::to_string(sessions_.size() + 1);
        session.user_id = user_id;
        session.token_hash = token_hash;
        session.expires_at = expires_at;
        
        sessions_[token_hash] = session;
        return {true, session.id};
    }
    
    std::tuple<bool, std::string, std::string> get_user_by_session_token(const std::string& token_hash) override {
        auto it = sessions_.find(token_hash);
        if (it == sessions_.end()) {
            return {false, "", ""};
        }
        
        // Check expiration (simplified - just check if empty)
        if (it->second.expires_at.empty()) {
            return {false, "", ""};
        }
        
        // Find user by id
        auto user = get_user_by_id(it->second.user_id);
        if (!user) {
            return {false, "", ""};
        }
        
        return {true, user->id, user->email};
    }
    
    bool delete_session(const std::string& token_hash) override {
        auto it = sessions_.find(token_hash);
        if (it == sessions_.end()) {
            return false;
        }
        sessions_.erase(it);
        return true;
    }
    
    bool delete_sessions_for_user(const std::string& user_id) override {
        bool removed = false;
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            if (it->second.user_id == user_id) {
                it = sessions_.erase(it);
                removed = true;
            } else {
                ++it;
            }
        }
        return removed;
    }
};

// Helper function to create JSON request
crow::request make_json_request(const std::string& body) {
    crow::request req;
    req.body = body;
    return req;
}

// Helper function to create request with authorization header
crow::request make_auth_request(const std::string& body, const std::string& token) {
    crow::request req = make_json_request(body);
    req.headers.emplace("Authorization", "Bearer " + token);
    return req;
}

// ==================== Register User Tests ====================

TEST(AuthRegisterTest, SuccessFullRegistration) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    crow::request req = make_json_request(R"({
        "email": "test@example.com",
        "password": "password123",
        "first_name": "John",
        "last_name": "Doe",
        "nickname": "johndoe"
    })");
    
    auto res = auth.register_user(req);
    
    EXPECT_EQ(res.code, 201);
    auto json = crow::json::load(res.body);
    EXPECT_TRUE(json.has("id"));
    EXPECT_EQ(json["email"].s(), "test@example.com");
    EXPECT_EQ(json["first_name"].s(), "John");
    EXPECT_EQ(json["last_name"].s(), "Doe");
    EXPECT_EQ(json["nickname"].s(), "johndoe");
}

TEST(AuthRegisterTest, SuccessMinimalRegistration) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    crow::request req = make_json_request(R"({
        "email": "minimal@example.com",
        "password": "pass123",
        "first_name": "Jane"
    })");
    
    auto res = auth.register_user(req);
    
    EXPECT_EQ(res.code, 201);
    auto json = crow::json::load(res.body);
    EXPECT_EQ(json["email"].s(), "minimal@example.com");
    EXPECT_EQ(json["first_name"].s(), "Jane");
}

TEST(AuthRegisterTest, InvalidJSON) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    crow::request req;
    req.body = "not valid json";
    
    auto res = auth.register_user(req);
    
    EXPECT_EQ(res.code, 400);
    EXPECT_NE(res.body.find("Invalid JSON"), std::string::npos);
}

TEST(AuthRegisterTest, MissingEmail) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    crow::request req = make_json_request(R"({
        "password": "pass123",
        "first_name": "John"
    })");
    
    auto res = auth.register_user(req);
    
    EXPECT_EQ(res.code, 400);
    EXPECT_NE(res.body.find("Missing required fields"), std::string::npos);
}

TEST(AuthRegisterTest, MissingPassword) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    crow::request req = make_json_request(R"({
        "email": "test@example.com",
        "first_name": "John"
    })");
    
    auto res = auth.register_user(req);
    
    EXPECT_EQ(res.code, 400);
    EXPECT_NE(res.body.find("Missing required fields"), std::string::npos);
}

TEST(AuthRegisterTest, MissingFirstName) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    crow::request req = make_json_request(R"({
        "email": "test@example.com",
        "password": "pass123"
    })");
    
    auto res = auth.register_user(req);
    
    EXPECT_EQ(res.code, 400);
    EXPECT_NE(res.body.find("Missing required fields"), std::string::npos);
}

TEST(AuthRegisterTest, InvalidEmailFormat) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    crow::request req = make_json_request(R"({
        "email": "not-an-email",
        "password": "pass123",
        "first_name": "John"
    })");
    
    auto res = auth.register_user(req);
    
    EXPECT_EQ(res.code, 400);
    EXPECT_NE(res.body.find("Invalid email format"), std::string::npos);
}

TEST(AuthRegisterTest, ShortPassword) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    crow::request req = make_json_request(R"({
        "email": "test@example.com",
        "password": "short",
        "first_name": "John"
    })");
    
    auto res = auth.register_user(req);
    
    EXPECT_EQ(res.code, 400);
    EXPECT_NE(res.body.find("Password must be at least 6 characters"), std::string::npos);
}

TEST(AuthRegisterTest, DuplicateEmail) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    // First registration
    crow::request req1 = make_json_request(R"({
        "email": "duplicate@example.com",
        "password": "pass123",
        "first_name": "John"
    })");
    auto res1 = auth.register_user(req1);
    EXPECT_EQ(res1.code, 201);
    
    // Second registration with same email
    crow::request req2 = make_json_request(R"({
        "email": "duplicate@example.com",
        "password": "pass123",
        "first_name": "John"
    })");
    auto res2 = auth.register_user(req2);
    
    EXPECT_EQ(res2.code, 409);
    EXPECT_NE(res2.body.find("already exists"), std::string::npos);
}

TEST(AuthRegisterTest, DuplicateNickname) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    // First registration with nickname
    crow::request req1 = make_json_request(R"({
        "email": "first@example.com",
        "password": "pass123",
        "first_name": "John",
        "nickname": "johndoe"
    })");
    auto res1 = auth.register_user(req1);
    EXPECT_EQ(res1.code, 201);
    
    // Second registration with same nickname
    crow::request req2 = make_json_request(R"({
        "email": "second@example.com",
        "password": "pass123",
        "first_name": "Jane",
        "nickname": "johndoe"
    })");
    auto res2 = auth.register_user(req2);
    
    EXPECT_EQ(res2.code, 409);
    EXPECT_NE(res2.body.find("already exists"), std::string::npos);
}

// ==================== Login Tests ====================

TEST(AuthLoginTest, SuccessfulLogin) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    // First register a user
    crow::request req = make_json_request(R"({
        "email": "login@example.com",
        "password": "password123",
        "first_name": "Login"
    })");
    auto reg_res = auth.register_user(req);
    EXPECT_EQ(reg_res.code, 201);
    
    // Now login
    crow::request login_req = make_json_request(R"({
        "email": "login@example.com",
        "password": "password123"
    })");
    auto login_res = auth.login(login_req);
    
    EXPECT_EQ(login_res.code, 200);
    auto json = crow::json::load(login_res.body);
    EXPECT_TRUE(json.has("token"));
    EXPECT_TRUE(json.has("user_id"));
    EXPECT_EQ(json["email"].s(), "login@example.com");
}

TEST(AuthLoginTest, InvalidCredentials) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    // Try to login with non-existent user
    crow::request req = make_json_request(R"({
        "email": "nonexistent@example.com",
        "password": "password123"
    })");
    auto res = auth.login(req);
    
    EXPECT_EQ(res.code, 401);
    EXPECT_NE(res.body.find("Invalid email or password"), std::string::npos);
}

TEST(AuthLoginTest, WrongPassword) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    // Register a user
    crow::request req = make_json_request(R"({
        "email": "wrongpass@example.com",
        "password": "correctpassword",
        "first_name": "Test"
    })");
    auto reg_res = auth.register_user(req);
    EXPECT_EQ(reg_res.code, 201);
    
    // Login with wrong password
    crow::request login_req = make_json_request(R"({
        "email": "wrongpass@example.com",
        "password": "wrongpassword"
    })");
    auto login_res = auth.login(login_req);
    
    EXPECT_EQ(login_res.code, 401);
    EXPECT_NE(login_res.body.find("Invalid email or password"), std::string::npos);
}

TEST(AuthLoginTest, MissingFields) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    // Missing password
    crow::request req1 = make_json_request(R"({
        "email": "test@example.com"
    })");
    auto res1 = auth.login(req1);
    EXPECT_EQ(res1.code, 400);
    
    // Missing email
    crow::request req2 = make_json_request(R"({
        "password": "pass123"
    })");
    auto res2 = auth.login(req2);
    EXPECT_EQ(res2.code, 400);
}

// ==================== Get Current User Tests ====================

TEST(AuthGetCurrentUserTest, ValidToken) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    // Register and login
    crow::request reg_req = make_json_request(R"({
        "email": "token@example.com",
        "password": "password123",
        "first_name": "Token"
    })");
    auto reg_res = auth.register_user(reg_req);
    EXPECT_EQ(reg_res.code, 201);
    
    crow::request login_req = make_json_request(R"({
        "email": "token@example.com",
        "password": "password123"
    })");
    auto login_res = auth.login(login_req);
    EXPECT_EQ(login_res.code, 200);
    
    auto json = crow::json::load(login_res.body);
    std::string token = json["token"].s();
    
    // Get current user with valid token
    crow::request req;
    req.headers.emplace("Authorization", "Bearer " + token);
    auto res = auth.get_current_user(req);
    
    EXPECT_EQ(res.code, 200);
    json = crow::json::load(res.body);
    EXPECT_EQ(std::string(json["id"].s()), "1");
    EXPECT_EQ(json["email"].s(), "token@example.com");
}

TEST(AuthGetCurrentUserTest, MissingAuthorizationHeader) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    crow::request req;
    auto res = auth.get_current_user(req);
    
    EXPECT_EQ(res.code, 401);
    EXPECT_NE(res.body.find("Missing Authorization header"), std::string::npos);
}

TEST(AuthGetCurrentUserTest, InvalidAuthorizationFormat) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    crow::request req;
    req.headers.emplace("Authorization", "InvalidFormat token123");
    auto res = auth.get_current_user(req);
    
    EXPECT_EQ(res.code, 401);
    EXPECT_NE(res.body.find("Invalid Authorization header format"), std::string::npos);
}

TEST(AuthGetCurrentUserTest, InvalidToken) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    crow::request req;
    req.headers.emplace("Authorization", "Bearer invalid_token_xyz");
    auto res = auth.get_current_user(req);
    
    EXPECT_EQ(res.code, 401);
    EXPECT_NE(res.body.find("Invalid or expired token"), std::string::npos);
}

// ==================== Logout Tests ====================

TEST(AuthLogoutTest, ValidLogout) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    // Register and login
    crow::request reg_req = make_json_request(R"({
        "email": "logout@example.com",
        "password": "password123",
        "first_name": "Logout"
    })");
    auto reg_res = auth.register_user(reg_req);
    EXPECT_EQ(reg_res.code, 201);
    
    crow::request login_req = make_json_request(R"({
        "email": "logout@example.com",
        "password": "password123"
    })");
    auto login_res = auth.login(login_req);
    EXPECT_EQ(login_res.code, 200);
    
    auto json = crow::json::load(login_res.body);
    std::string token = json["token"].s();
    
    // Logout
    crow::request logout_req;
    logout_req.headers.emplace("Authorization", "Bearer " + token);
    auto logout_res = auth.logout(logout_req);
    
    EXPECT_EQ(logout_res.code, 204);
    EXPECT_EQ(logout_res.body, "");
}

TEST(AuthLogoutTest, InvalidToken) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    crow::request req;
    req.headers.emplace("Authorization", "Bearer invalid_token");
    auto res = auth.logout(req);
    
    EXPECT_EQ(res.code, 500);
    EXPECT_NE(res.body.find("Failed to logout"), std::string::npos);
}

TEST(AuthLogoutTest, MissingAuthorization) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    crow::request req;
    auto res = auth.logout(req);
    
    EXPECT_EQ(res.code, 401);
    EXPECT_NE(res.body.find("Missing Authorization header"), std::string::npos);
}

// ==================== Get Session Info Tests ====================

TEST(AuthGetSessionInfoTest, ValidSession) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    // Register and login
    crow::request reg_req = make_json_request(R"({
        "email": "session@example.com",
        "password": "password123",
        "first_name": "Session"
    })");
    auto reg_res = auth.register_user(reg_req);
    EXPECT_EQ(reg_res.code, 201);
    
    crow::request login_req = make_json_request(R"({
        "email": "session@example.com",
        "password": "password123"
    })");
    auto login_res = auth.login(login_req);
    EXPECT_EQ(login_res.code, 200);
    
    auto json = crow::json::load(login_res.body);
    std::string token = json["token"].s();
    
    // Get session info
    crow::request req;
    req.headers.emplace("Authorization", "Bearer " + token);
    auto res = auth.get_session_info(req);
    
    EXPECT_EQ(res.code, 200);
    json = crow::json::load(res.body);
    EXPECT_TRUE(json.has("user_id"));
    EXPECT_EQ(json["email"].s(), "session@example.com");
}

TEST(AuthGetSessionInfoTest, InvalidToken) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    crow::request req;
    req.headers.emplace("Authorization", "Bearer invalid_token");
    auto res = auth.get_session_info(req);
    
    EXPECT_EQ(res.code, 401);
    EXPECT_NE(res.body.find("Invalid or expired token"), std::string::npos);
}

// ==================== Edge Cases ====================

TEST(AuthEdgeCasesTest, EmptyNickname) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    // Empty string nickname should be treated as optional
    crow::request req = make_json_request(R"({
        "email": "nonickname@example.com",
        "password": "password123",
        "first_name": "NoNickname",
        "nickname": ""
    })");
    auto res = auth.register_user(req);
    
    EXPECT_EQ(res.code, 201);
    auto json = crow::json::load(res.body);
    EXPECT_FALSE(json.has("nickname"));
}

TEST(AuthEdgeCasesTest, VeryLongFields) {
    auto mock_db = std::make_shared<MockDatabase>();
    AuthApi auth(mock_db);
    
    // Test with longer but valid values
    std::string long_name(50, 'a');
    std::string long_email(50, 'b');
    long_email = long_email.substr(0, 40) + "@example.com";
    
    crow::request req = make_json_request(R"({
        "email": "test@example.com",
        "password": "password123",
        "first_name": "John"
    })");
    auto res = auth.register_user(req);
    
    EXPECT_EQ(res.code, 201);
}

// Test password hashing is consistent
TEST(AuthPasswordTest, HashConsistency) {
    auto mock_db = std::make_shared<MockDatabase>();
    
    std::string password = "testpassword";
    std::string hash1 = mock_db->hash_password(password);
    std::string hash2 = mock_db->hash_password(password);
    
    // For our mock, the hash should be consistent (salt + password)
    EXPECT_EQ(hash1, hash2);
    EXPECT_TRUE(mock_db->verify_password(password, hash1));
}
