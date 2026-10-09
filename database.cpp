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
    std::string host = "127.0.0.1";
    std::string port = "5432";
    std::string dbname = "iotmart";
    std::string user = "postgres";

    std::string password = "Hari@2008";

    std::ostringstream oss;

    oss << "host=" << host
        << " port=" << port
        << " dbname=" << dbname
        << " user=" << user
        << " password=" << password;

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
        std::cerr << "Database connection failed: "
                  << e.what() << std::endl;
        throw;
    }
}