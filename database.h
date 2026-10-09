#pragma once
#include <pqxx/pqxx>
#include <string>
#include <memory>

// Builds a PostgreSQL connection using environment variables:
// DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD
// Falls back to sensible local defaults if a variable is not set.
class Database {
public:
    // Returns a fresh connection. libpqxx connections are cheap enough
    // to open per-request for a simple app like this (no manual pooling
    // needed to keep the code beginner-friendly).
    static std::unique_ptr<pqxx::connection> connect();

    // Builds the libpqxx connection string from environment variables.
    static std::string buildConnString();

private:
    static std::string getEnvOrDefault(const char* name, const std::string& def);
};
