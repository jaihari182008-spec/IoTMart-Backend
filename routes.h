#pragma once

#include "crow_all.h"

using IoTMartApp = crow::App<crow::CORSHandler>;

void registerRoutes(IoTMartApp& app);