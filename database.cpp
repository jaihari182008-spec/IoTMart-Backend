#include "database.h"
#include <cstdlib>
#include <sstream>
#include <iostream>

std::string Database::getEnvOrDefault(const char* name, const std::string& def)
{
    const char* val = std::getenv(name);

    if (val == nullptr || std::string(val).empty())
    {
        return def;
    }

    return std::string(val);
}

std::string Database::buildConnString()
{
    const char* dbUrl = std::getenv("DATABASE_URL");
    if (dbUrl && std::string(dbUrl).size() > 0)
    {
        return std::string(dbUrl);
    }

    std::string host = getEnvOrDefault("DB_HOST", "127.0.0.1");
    std::string port = getEnvOrDefault("DB_PORT", "5432");
    std::string dbname = getEnvOrDefault("DB_NAME", "iotmart");
    std::string user = getEnvOrDefault("DB_USER", "postgres");
    std::string password = getEnvOrDefault("DB_PASSWORD", "");
    std::string sslmode = getEnvOrDefault("DB_SSLMODE", "");

    std::ostringstream oss;

    oss << "host=" << host
        << " port=" << port
        << " dbname=" << dbname
        << " user=" << user;

    if (!password.empty())
    {
        oss << " password=" << password;
    }

    if (!sslmode.empty())
    {
        oss << " sslmode=" << sslmode;
    }

    return oss.str();
}

std::unique_ptr<pqxx::connection> Database::connect()
{
    try
    {
        auto conn = std::make_unique<pqxx::connection>(buildConnString());
        return conn;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Database connection failed. Please verify connection credentials and network availability." << std::endl;
        throw;
    }
}