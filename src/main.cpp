#include <iostream>
#include "headers/crow.h"
#include <windows.h>
int main(int, char**){
    crow::App<crow::UTF8> app;
    CROW_ROUTE(app, "/")
    ([]() {
        crow::response res;
        res.set_header("Content-Type", "text/html; charset=utf-8");
        
        // 2. Wrap the text in a native std::string container to protect the emoji bytes
        res.body = std::string("Hello from LiteFox! LiteFox Engine is running successfully! 🚀🦊");
        
        return res;
    });

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::cout << "Привет мир!" << std::endl;

    // Запуск сервера
    app.port(8080).multithreaded().run();
}