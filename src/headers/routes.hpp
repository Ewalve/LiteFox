#pragma once
#include "crow.h"

namespace routes_system{
    void init_routes(crow::App<crow::UTF8>& app){
        CROW_ROUTE(app, "/litefox")
        .methods("GET"_method)
        ([]() {
            crow::response res;
            res.set_header("Content-Type", "text/html; charset=utf-8");
            res.body = std::string("Hello from LiteFox! LiteFox Engine is running successfully! 🚀🦊");
            return res;
        });
    };
}