
#include "crow_all.h"
#include "routes.h"

#include <cstdlib>
#include <iostream>
#include <string>

static int getPortFromEnv()
{
    const char* portEnv = std::getenv("PORT");

    if (portEnv && std::string(portEnv).size() > 0)
    {
        try
        {
            return std::stoi(portEnv);
        }
        catch (...)
        {
            // Use default port
        }
    }

    return 18080;
}

int main()
{
    IoTMartApp app;

    // CORS
    auto& cors = app.get_middleware<crow::CORSHandler>();

    cors
        .global()
        .headers("Content-Type, Authorization")
        .methods(
            "GET"_method,
            "POST"_method,
            "PUT"_method,
            "DELETE"_method,
            "OPTIONS"_method
        )
        .origin("*");

    // Register API routes
    registerRoutes(app);

    int port = getPortFromEnv();

    std::cout
        << "IoTMart backend starting on http://0.0.0.0:"
        << port
        << std::endl;

    std::cout << "Starting server..." << std::endl;

    // Start server
    app.port(18080).multithreaded().run();

    std::cout << "Server run() returned." << std::endl;

    return 0;
}