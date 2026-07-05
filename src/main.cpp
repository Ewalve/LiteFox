#include <iostream>
#include "headers/crow.h"
#include <string>
#include <cstdlib>
#include <thread>
#include <memory>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <functional>
#include <chrono>

#include <boost/asio.hpp>
#include "headers/routes.hpp"

std::string formatAsn1Time(ASN1_TIME* time) {
    if (!time) return "Unknown";
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) return "Error allocating BIO memory";

    ASN1_TIME_print(bio, time);
    
    char buffer[256];
    int bytes_read = BIO_read(bio, buffer, sizeof(buffer) - 1);
    BIO_free(bio);
    
    if (bytes_read <= 0) return "Error reading timestamp bytes";
    buffer[bytes_read] = '\0';
    
    return std::string(buffer);
}
ASN1_TIME* getDiskCertificateExpiry(const std::string& cert_path) {
    BIO* bio = BIO_new_file(cert_path.c_str(), "r");
    if (!bio) return nullptr;

    X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (!cert) return nullptr;

    // ВАЖНО: делаем дубликат времени, так как сам cert мы сейчас удалим
    ASN1_TIME* not_after = ASN1_STRING_dup(X509_getm_notAfter(cert));
    X509_free(cert);
    
    return not_after;
}
bool checkAndReloadCertificate(SSL_CTX* ssl_ctx, const std::string& cert_path) {
    if (!ssl_ctx) {
        std::cerr << "[LiteFox OpenSSL ERROR] Нативный контекст равен nullptr!" << std::endl;
        return false;
    }

    X509* mem_cert = SSL_CTX_get0_certificate(ssl_ctx);
    if (!mem_cert) {
        std::cerr << "[LiteFox OpenSSL ERROR] Не удалось получить сертификат из кучи!" << std::endl;
        return false;
    }

    ASN1_TIME* mem_not_after = X509_getm_notAfter(mem_cert);
    int days_left = 0;
    int seconds_left = 0;
    ASN1_TIME_diff(&days_left, &seconds_left, nullptr, mem_not_after);

    std::cout << "[LiteFox OpenSSL] Активный сертификат в куче действует до: " 
              << formatAsn1Time(mem_not_after) << " (осталось " << days_left << " дней)." << std::endl;

    if (days_left >= 30) {
        return false;
    }

    std::cout << "[LiteFox Timer] Сертификат в куче в зоне риска (< 30 дней). Проверяем диск..." << std::endl;

    ASN1_TIME* disk_not_after = getDiskCertificateExpiry(cert_path);
    if (!disk_not_after) {
        std::cerr << "[LiteFox Timer ERROR] Не удалось прочитать сертификат с диска: " << cert_path << std::endl;
        return false;
    }

    bool need_update = false;
    if (ASN1_TIME_compare(disk_not_after, mem_not_after) > 0) {
        std::cout << "[LiteFox Timer] На диске обнаружен БОЛЕЕ НОВЫЙ сертификат!" << std::endl;
        need_update = true;
    } else {
        std::cout << "[LiteFox Timer] На диске тот же старый сертификат. Обновление не требуется." << std::endl;
    }

    ASN1_STRING_free(disk_not_after);
    return need_update;
}
void checkAndReloadTick(crow::App<crow::UTF8>* https_app, 
                         SSL_CTX* native_ctx, 
                         std::string cert_path, 
                         std::string key_path) {
    std::cout << "[LiteFox Tick] Плановая проверка валидности сертификатов..." << std::endl;

    if (!https_app || !native_ctx) {
        std::cerr << "[LiteFox Tick ERROR] Критические объекты равны nullptr!" << std::endl;
        return;
    }

    // Передаем напрямую сохраненный рабочий указатель из кучи
    if (checkAndReloadCertificate(native_ctx, cert_path)) {
        std::cout << "[LiteFox Tick] Инициируем обновление кучи..." << std::endl;
        try {
            https_app->ssl_file(cert_path, key_path);
            std::cout << "[LiteFox Tick] SSL-сертификаты успешно обновлены в куче!" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[LiteFox Tick ERROR] Ошибка применения SSL: " << e.what() << std::endl;
        }
    } else {
        std::cout << "[LiteFox Tick] Проверка завершена. Обновление памяти не требуется." << std::endl;
    }
}
int main(int, char**){
    crow::App<crow::UTF8> http_app;
    http_app.server_name("LiteFox");

    const char* prod_env = std::getenv("IS_PRODUCTION");
    std::string is_prod_str = (prod_env != nullptr) ? prod_env : "false";
    bool is_production = (is_prod_str == "true" || is_prod_str == "1");

    const char* workdir_env = std::getenv("APP_WORKDIR");
    if(workdir_env == nullptr){
        std::cerr << "[LiteFox] Ошибка: Переменная окружения APP_WORKDIR не установлена!" << std::endl;
        return 1;
    };

    // HTTP маршруты
    CROW_ROUTE(http_app, "/.well-known/acme-challenge/<string>")
    ([&workdir_env](const crow::request& req, crow::response& res, std::string token){
        std::string file_path =  std::string(workdir_env) + "/www/certbot/.well-known/acme-challenge/" + token;
        res.set_static_file_info(file_path);
        res.end();
    });

    if(is_production){
        CROW_CATCHALL_ROUTE(http_app)([](const crow::request& req, crow::response& res){
            std::string host = req.get_header_value("Host");
            const char* env_domain = std::getenv("APP_DOMAIN");
            std::string target_domain = (env_domain != nullptr) ? env_domain : "localhost";

            if (host.rfind("www.", 0) == 0) host = host.substr(4); 
            if (host.empty()) host = target_domain;

            std::string https_url = "https://" + host + req.url;
            
            std::cout << "[LiteFox HTTP Redirect] Редирект на: " << https_url << std::endl;
            res.code = 301;
            res.set_header("Location", https_url);
            res.end();
        });
    }

    std::cout << "[LiteFox] Starting server..." << std::endl;
    if(is_production)
    {
        auto app_future = http_app.port(80).multithreaded().run_async();
        std::cout << "[LiteFox] Http server running on port 80..." << std::endl;

        crow::App<crow::UTF8> https_app;
        https_app.server_name("LiteFox");
        
        routes_system::init_routes(https_app);
        std::cout << "[LiteFox] routes are setup!" << std::endl;

        //загружаем SSL сертификаты
        const char* domain_env = std::getenv("LITEFOX_DOMAIN");
        if(domain_env == nullptr){
            std::cerr << "[LiteFox] Ошибка: Переменная окружения LITEFOX_DOMAIN не установлена!" << std::endl;
            return 1;
        };

        std::string cert_path = std::string(workdir_env) + "/certbot/conf/live/" + std::string(domain_env) + "/fullchain.pem";
        std::string key_path = std::string(workdir_env) + "/certbot/conf/live/" + std::string(domain_env) + "/privkey.pem";

        boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tlsv12_server);
        ssl_ctx.use_certificate_chain_file(cert_path);
        ssl_ctx.use_private_key_file(key_path, boost::asio::ssl::context::pem);

        SSL_CTX* stable_native_ctx = ssl_ctx.native_handle();
        https_app.ssl(std::move(ssl_ctx));

        https_app.tick(std::chrono::seconds(10), [&https_app, stable_native_ctx, cert_path, key_path]() {
            checkAndReloadTick(&https_app, stable_native_ctx, cert_path, key_path);
        });
    
        std::cout << "[LiteFox] certs are setup!" << std::endl;

        // auto timer = std::make_shared<boost::asio::steady_timer>(timer_io_context);
        // startCertUpdateTimer(https_app, timer);

        std::cout << "[LiteFox] Production HTTPS Server running on port 443..." << std::endl;
        https_app.port(443).multithreaded().run();
    }
    else{
        std::cout << "[LiteFox] Development HTTP Server running on port 8080..." << std::endl;
        routes_system::init_routes(http_app);
        http_app.port(8080).multithreaded().run();
    }
}