#include "crow.h"
#include "DbPool.hpp"
#include "IDatabase.hpp"
#include "FlowsApi.hpp"
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

    crow::SimpleApp app;

    // ==========================================
    // CREATE (POST /api/v1/flows)
    // ==========================================
    CROW_ROUTE(app, "/api/v1/flows").methods(crow::HTTPMethod::POST)([&api](const crow::request& req) {
        return api.create_flow(req);
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

    // 2. Start the Server
    std::cout << "Starting Crow server on port 8080...\n";
    app.port(8080).multithreaded().run();
}

int main() {
    run_app();
    return 0;
}
