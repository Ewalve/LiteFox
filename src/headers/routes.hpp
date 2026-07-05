#pragma once
#include "crow.h"
#include <filesystem>
#include <functional>
namespace fs = std::filesystem;
namespace routes_system{
    void init_routes(crow::App<crow::UTF8>& app){
        const char* workdir_env = std::getenv("APP_WORKDIR");
        if(workdir_env == nullptr){
            std::cerr << "[LiteFox] Ошибка: Переменная окружения APP_WORKDIR не установлена!" << std::endl;
            return;
        };

        CROW_ROUTE(app, "/litefox")
        .methods("GET"_method)
        ([]() {
            crow::response res;
            res.set_header("Content-Type", "text/html; charset=utf-8");
            res.body = std::string("Hello from LiteFox! LiteFox Engine is running successfully! 🚀🦊");
            return res;
        });
        CROW_ROUTE(app, "/assets/<path>")
        ([workdir_env](const crow::request& req, crow::response& res, std::string path){
            if (path.find("..") != std::string::npos) {
                res.code = 400;
                res.end();
                return;
            }
            fs::path filepath = fs::path(workdir_env) / "frontend" / "dist" / "assets" / path;
            if(!fs::exists(filepath) || fs::is_directory(filepath)){
                res.code = 404;
                res.end();
                return;
            }
            res.set_static_file_info_unsafe(filepath.string());
            res.add_header("Cache-Control", "public, max-age=31536000, immutable"); //кэширование

            //определение mime типа
            std::string ext = filepath.extension().string();
            if (ext == ".js" || ext == ".mjs") {
                res.set_header("Content-Type", "application/javascript; charset=utf-8");
            } else if (ext == ".css") {
                res.set_header("Content-Type", "text/css; charset=utf-8");
            } else if (ext == ".svg") {
                res.set_header("Content-Type", "image/svg+xml; charset=utf-8");
            } else if (ext == ".png") {
                res.set_header("Content-Type", "image/png");
            } else if (ext == ".jpg" || ext == ".jpeg") {
                res.set_header("Content-Type", "image/jpeg");
            } else if (ext == ".json") {
                res.set_header("Content-Type", "application/json; charset=utf-8");
            };

            res.end();
        });
        auto serve_html = [](const crow::request& req, crow::response& res, fs::path filepath){
            res.set_static_file_info_unsafe(filepath.string());

           //определение mime типа
            std::string ext = filepath.extension().string();
            if (ext == ".js" || ext == ".mjs") {
                res.set_header("Content-Type", "application/javascript; charset=utf-8");
            } else if (ext == ".css") {
                res.set_header("Content-Type", "text/css; charset=utf-8");
            } else if (ext == ".svg") {
                res.set_header("Content-Type", "image/svg+xml; charset=utf-8");
            } else if (ext == ".png") {
                res.set_header("Content-Type", "image/png");
            } else if (ext == ".jpg" || ext == ".jpeg") {
                res.set_header("Content-Type", "image/jpeg");
            } else if (ext == ".json") {
                res.set_header("Content-Type", "application/json; charset=utf-8");
            };
        };

        CROW_ROUTE(app, "/<path>")
        ([workdir_env, serve_html](const crow::request& req, crow::response& res, std::string path) {
            if (path.find("..") != std::string::npos) {
                res.code = 400;
                res.end();
                return;
            }
            fs::path filepath = fs::path(workdir_env) / "frontend" / "dist" / "index.html";
            if(!fs::exists(filepath) || fs::is_directory(filepath)){
                res.code = 404;
                res.end();
                return;
            }
            serve_html(req, res, filepath);
            res.end();
        });
        CROW_ROUTE(app, "/")
        ([workdir_env, serve_html](const crow::request& req, crow::response& res) {
            fs::path filepath = fs::path(workdir_env) / "frontend" / "dist" / "index.html";
            if(!fs::exists(filepath) || fs::is_directory(filepath)){
                res.code = 404;
                res.end();
                return;
            }
            serve_html(req, res, filepath);
            res.end();
        });
    };
}