
cmake_minimum_required(VERSION 3.15)

project(IoTMartBackend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)

add_executable(iotmart_server
    main.cpp
    routes.cpp
    database.cpp
)

target_include_directories(iotmart_server PRIVATE ${PQXX_INCLUDE_DIRS})

target_link_libraries(iotmart_server PRIVATE
    PkgConfig::PQXX
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

