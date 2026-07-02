#include <iostream>
#include "headers/crow.h"
//#include <windows.h>
#include <string>
int main(int, char**){
    crow::App<crow::UTF8> app;
    
    CROW_ROUTE(app, "/gameId/<string>")
    .methods("POST"_method)
    ([](std::string gameId) {
        crow::response res;
        res.set_header("Content-Type", "text/html; charset=utf-8");
        res.body = std::string("Hello from LiteFox! LiteFox Engine is running successfully! 🚀🦊 GameId: ") + gameId;
        return res;
    });

    // SetConsoleOutputCP(CP_UTF8);
    // SetConsoleCP(CP_UTF8);
    // std::cout << "Привет мир!" << std::endl;

    // Запуск сервера
    app.port(8080).multithreaded().run();
}