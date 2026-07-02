#include <iostream>
#include "headers/crow.h"
#include <string>
#include <cstdlib>
#include <thread>
#include <memory>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/asn1.h>
#include <chrono>

#include <boost/asio.hpp>

void setupAnalyticsRoutes(crow::App<crow::UTF8>& app) {
    CROW_ROUTE(app, "/litefox")
    .methods("GET"_method)
    ([]() {
        crow::response res;
        res.set_header("Content-Type", "text/html; charset=utf-8");
        res.body = std::string("Hello from LiteFox! LiteFox Engine is running successfully! 🚀🦊");
        return res;
    });
}
bool isCertificateExpiring(const std::string& cert_path, int days_threshold = 30) {
    BIO* bio = BIO_new_file(cert_path.c_str(), "r");
    if (!bio) {
        std::cerr << "[LiteFox OpenSSL] Ошибка: Не удалось открыть файл " << cert_path << std::endl;
        return true;
    }

    X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (!cert) {
        std::cerr << "[LiteFox OpenSSL] Ошибка: Не удалось распарсить X509 структуру" << std::endl;
        return true;
    }

    ASN1_TIME* not_after = X509_getm_notAfter(cert);
    
    int days = 0;
    int seconds = 0;

    ASN1_TIME_diff(&days, &seconds, nullptr, not_after);
    X509_free(cert);
    std::cout << "[LiteFox OpenSSL] Сертификат " << cert_path << " будет валиден еще " << days << " дней." << std::endl;
    return (days < days_threshold);
}
void startCertUpdateTimer(crow::App<crow::UTF8>& https_app, std::shared_ptr<boost::asio::steady_timer> timer) {
    timer->expires_after(std::chrono::hours(24));
    timer->async_wait([&https_app, timer](const crow::error_code& error) {
        if (!error) {
            std::string cert_path = "./certs/fullchain.pem";
            std::string key_path = "./certs/privkey.pem";

            std::cout << "[LiteFox Timer] Плановая проверка валидности сертификатов..." << std::endl;

            if (isCertificateExpiring(cert_path, 30)) {
                std::cout << "[LiteFox Timer] Внимание: Сертификат истекает или обновился! Перезагружаем в память..." << std::endl;
                try {
                    https_app.ssl_file(cert_path, key_path);
                    std::cout << "[LiteFox Timer] SSL-сертификаты успешно применены!" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "[LiteFox Timer ERROR] Ошибка применения SSL: " << e.what() << std::endl;
                }
            } else {
                std::cout << "[LiteFox Timer] Сертификат в порядке. Обновление памяти не требуется." << std::endl;
            }
            startCertUpdateTimer(https_app, timer);
        }
    });
}
int main(int, char**){
    crow::App<crow::UTF8> http_app;

    const char* prod_env = std::getenv("IS_PRODUCTION");
    std::string is_prod_str = (prod_env != nullptr) ? prod_env : "false";
    bool is_production = (is_prod_str == "true" || is_prod_str == "1");


    // HTTP маршруты
    CROW_ROUTE(http_app, "/.well-known/acme-challenge/<string>")
    ([](const crow::request& req, crow::response& res, std::string token){
        std::string file_path = "./www/certbot/.well-known/acme-challenge/" + token;
        res.set_static_file_info(file_path);
        res.end();
    });

    if(is_production){
        // Роут для обычного редиректа всех остальных страниц
        CROW_ROUTE(http_app, "/<path>")
        ([](const crow::request& req, crow::response& res, std::string path){
            std::string host = req.get_header_value("Host");

            const char* env_domain = std::getenv("APP_DOMAIN");
            std::string target_domain = (env_domain != nullptr) ? env_domain : "localhost";


            if (host.rfind("www.", 0) == 0) {
                host = host.substr(4); 
            }
            if (host.empty()) host = target_domain;


            std::string https_url = "https://" + host + req.raw_url;
            res.code = 301;
            res.add_header("Location", https_url);
            res.end();
        });
    }
    


    boost::asio::io_context timer_io_context;
    std::unique_ptr<std::thread> timer_thread;

    std::cout << "[LiteFox] Starting server..." << std::endl;
    if(is_production)
    {
        auto app_future = http_app.port(80).multithreaded().run_async();
        std::cout << "[LiteFox] Http server running on port 80..." << std::endl;

        crow::App<crow::UTF8> https_app;
        setupAnalyticsRoutes(https_app);
        std::cout << "[LiteFox] routes are setup!" << std::endl;

        //загружаем SSL сертификаты
        const char* domain_env = std::getenv("LITEFOX_DOMAIN");
        if(domain_env == nullptr){
            std::cerr << "[LiteFox] Ошибка: Переменная окружения LITEFOX_DOMAIN не установлена!" << std::endl;
            return 1;
        };

        const char* workdir_env = std::getenv("APP_WORKDIR");
        if(workdir_env == nullptr){
            std::cerr << "[LiteFox] Ошибка: Переменная окружения APP_WORKDIR не установлена!" << std::endl;
            return 1;
        };

        std::string cert_path = std::string(workdir_env) + "/certbot/conf/live/" + std::string(domain_env) + "/fullchain.pem";
        std::string key_path = std::string(workdir_env) + "/certbot/conf/live/" + std::string(domain_env) + "/privkey.pem";
        https_app.ssl_file(cert_path, key_path);
    
        std::cout << "[LiteFox] certs are setup!" << std::endl;

        auto timer = std::make_shared<boost::asio::steady_timer>(timer_io_context);
        startCertUpdateTimer(https_app, timer);

        std::cout << "[LiteFox] Production HTTPS Server running on port 443..." << std::endl;
        https_app.port(443).multithreaded().run();
    }
    else{
        std::cout << "[LiteFox] Development HTTP Server running on port 8080..." << std::endl;
        setupAnalyticsRoutes(http_app);
        http_app.port(8080).multithreaded().run();
    }
}