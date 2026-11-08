#include "crow.h"
#include "DbPool.hpp"
#include "IDatabase.hpp"
#include "FlowsApi.hpp"
#include "AuthApi.hpp"
#include <pqxx/pqxx>
#include <iostream>

// Create a separate function for running the app so tests can use the API without main()
void run_app() {
    // 1. Initialize DB Pool
    std::string conn_str = "host=postgres port=5432 dbname=flows_db user=flows_db_admin password=an_admin_test_password";
    DbPool::instance().init(conn_str, 16); // 16 connections for 16 threads

    // Create database interface
    auto db = std::make_shared<Database>(std::make_shared<pqxx::connection>(conn_str));
    
    FlowsApi api(db);
    AuthApi auth_api(db);

    crow::SimpleApp app;

    // ==========================================
    // AUTHENTICATION ROUTES
    // ==========================================
    
    // Register a new user (POST /api/v1/auth/register)
    CROW_ROUTE(app, "/api/v1/auth/register").methods(crow::HTTPMethod::POST)([&auth_api](const crow::request& req) {
        return auth_api.register_user(req);
    });
    
    // Login user (POST /api/v1/auth/login)
    CROW_ROUTE(app, "/api/v1/auth/login").methods(crow::HTTPMethod::POST)([&auth_api](const crow::request& req) {
        return auth_api.login(req);
    });
    
    // Get current user (GET /api/v1/auth/me) - requires auth
    CROW_ROUTE(app, "/api/v1/auth/me").methods(crow::HTTPMethod::GET)([&auth_api](const crow::request& req) {
        return auth_api.get_current_user(req);
    });
    
    // Logout (DELETE /api/v1/auth/logout) - requires auth
    CROW_ROUTE(app, "/api/v1/auth/logout").methods(crow::HTTPMethod::DELETE)([&auth_api](const crow::request& req) {
        return auth_api.logout(req);
    });
    
    // Get session info (GET /api/v1/auth/session) - requires auth
    CROW_ROUTE(app, "/api/v1/auth/session").methods(crow::HTTPMethod::GET)([&auth_api](const crow::request& req) {
        return auth_api.get_session_info(req);
    });

    // ==========================================
    // FLOWS API ROUTES
    // ==========================================
    // CREATE root flow (POST /api/v1/flows)
    // ==========================================
    CROW_ROUTE(app, "/api/v1/flows").methods(crow::HTTPMethod::POST)([&api](const crow::request& req) {
        return api.create_flow(req);
    });

    // CREATE nested flow within a parent (POST /api/v1/flows/<parent_flow_id>/child)
    CROW_ROUTE(app, "/api/v1/flows/<string>/child").methods(crow::HTTPMethod::POST)([&api](const crow::request& req, const std::string& parent_flow_id) {
        return api.create_flow(req, parent_flow_id);
    });

    // ==========================================
    // READ (GET /api/v1/flows/<id>)
    // ==========================================
    CROW_ROUTE(app, "/api/v1/flows/<string>").methods(crow::HTTPMethod::GET)([&api](const std::string& id) {
        return api.get_flow(id);
    });

    // ==========================================
    // READ ALL (GET /api/v1/flows)
    // ==========================================
    CROW_ROUTE(app, "/api/v1/flows").methods(crow::HTTPMethod::GET)([&api]() {
        return api.get_all_flows();
    });

    // ==========================================
    // UPDATE (PUT /api/v1/flows/<id>)
    // ==========================================
    CROW_ROUTE(app, "/api/v1/flows/<string>").methods(crow::HTTPMethod::PUT)([&api](const crow::request& req, const std::string& id) {
        return api.update_flow(req, id);
    });

    // ==========================================
    // DELETE (DELETE /api/v1/flows/<id>)
    // ==========================================
    CROW_ROUTE(app, "/api/v1/flows/<string>").methods(crow::HTTPMethod::DELETE)([&api](const std::string& id) {
        return api.delete_flow(id);
    });

    // ==========================================
    // ITEMS API ROUTES
    // ==========================================
    // GET items in a flow (GET /api/v1/flows/<flow_id>/items)
    CROW_ROUTE(app, "/api/v1/flows/<string>/items").methods(crow::HTTPMethod::GET)([&api](const std::string& flow_id) {
        return api.get_flow_items(flow_id);
    });

    // UPDATE item order (PATCH /api/v1/flows/<flow_id>/items/<item_id>)
    CROW_ROUTE(app, "/api/v1/flows/<string>/items/<string>").methods(crow::HTTPMethod::PATCH)([&api](const crow::request& req, const std::string& flow_id, const std::string& item_id) {
        return api.update_item_order(req, flow_id, item_id);
    });

    // ==========================================
    // TASK API ROUTES
    // ==========================================
    // CREATE task within a flow (POST /api/v1/flows/<flow_id>/tasks)
    CROW_ROUTE(app, "/api/v1/flows/<string>/tasks").methods(crow::HTTPMethod::POST)([&api](const crow::request& req, const std::string& flow_id) {
        return api.create_task(req, flow_id);
    });

    // READ task (GET /api/v1/tasks/<id>)
    CROW_ROUTE(app, "/api/v1/tasks/<string>").methods(crow::HTTPMethod::GET)([&api](const std::string& id) {
        return api.get_task(id);
    });

    // UPDATE task (PUT /api/v1/tasks/<id>)
    CROW_ROUTE(app, "/api/v1/tasks/<string>").methods(crow::HTTPMethod::PUT)([&api](const crow::request& req, const std::string& id) {
        return api.update_task(req, id);
    });

    // DELETE task (DELETE /api/v1/tasks/<id>)
    CROW_ROUTE(app, "/api/v1/tasks/<string>").methods(crow::HTTPMethod::DELETE)([&api](const std::string& id) {
        return api.delete_task(id);
    });

    // 2. Start the Server
    std::cout << "Starting Crow server on port 8080...\n";
    app.port(8080).multithreaded().run();
}

int main() {
    run_app();
    return 0;
}
