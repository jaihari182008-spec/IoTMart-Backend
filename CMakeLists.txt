cmake_minimum_required(VERSION 3.15)

project(IoTMartBackend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libpqxx CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(iotmart_server
    main.cpp
    routes.cpp
    database.cpp
)

target_link_libraries(iotmart_server PRIVATE
    libpqxx::pqxx
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# Notes:
# - crow_all.h is a header-only single-file include, no linking needed.
# - Build:  mkdir build && cd build && cmake .. && make
# - Run:    ./iotmart_server   (reads DB_HOST, DB_PORT, DB_NAME, DB_USER,
#                                DB_PASSWORD, PORT from environment)
