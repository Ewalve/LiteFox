cmake_minimum_required(VERSION 3.10.0)
project(LiteFox VERSION 0.1.0 LANGUAGES C CXX DESCRIPTION "The Foxiest Analysis System")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(LiteFox src/main.cpp)

if(MSVC)
    add_compile_definitions(_WIN32_WINNT=0x0601)
    add_compile_options(/utf-8)
    target_compile_options(LiteFox PRIVATE /Zc:preprocessor)
endif()
target_include_directories(LiteFox PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/headers"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/libraries/boost_1_91_0"
)
target_link_libraries(LiteFox PRIVATE 
    OpenSSL::Crypto 
    OpenSSL::SSL
    Threads::Threads
)
target_compile_definitions(LiteFox PRIVATE 
    CROW_USE_BOOST
    CROW_ENABLE_SSL
)

if(WIN32)
    # Get the root directory of your OpenSSL installation
    get_filename_component(OPENSSL_ROOT_DIR "${OPENSSL_INCLUDE_DIR}" DIRECTORY)
    
    # Locate the actual DLL files inside the bin/ directory
    set(SSL_DLL_PATH "${OPENSSL_ROOT_DIR}/bin/libssl-4-x64.dll")
    set(CRYPTO_DLL_PATH "${OPENSSL_ROOT_DIR}/bin/libcrypto-4-x64.dll")

    # Force a direct copy of the exact files into your build folder
    add_custom_command(
        TARGET LiteFox 
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                "${SSL_DLL_PATH}" 
                "${CRYPTO_DLL_PATH}" 
                "$<TARGET_FILE_DIR:LiteFox>"
        COMMENT "Force-copying OpenSSL x64 DLLs directly from bin folder..."
    )
endif()
