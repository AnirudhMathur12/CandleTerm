cmake_minimum_required(VERSION 3.15)
project(StockTicker VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Generates compile_commands.json for editors (VSCode/NeoVim)

find_package(CURL REQUIRED)

find_package(nlohmann_json 3.2.0 QUIET)

add_executable(stock_ticker main.cpp)

if(CURL_FOUND)
    target_link_libraries(stock_ticker PRIVATE CURL::libcurl)
else()
    target_include_directories(stock_ticker PRIVATE ${CURL_INCLUDE_DIRS})
    target_link_libraries(stock_ticker PRIVATE ${CURL_LIBRARIES})
endif()

if(nlohmann_json_FOUND)
    target_link_libraries(stock_ticker PRIVATE nlohmann_json::nlohmann_json)
else()
    message(STATUS "nlohmann_json package not found via CMake. Checking standard paths...")
    
    if(APPLE AND EXISTS "/opt/homebrew/include")
        target_include_directories(stock_ticker PRIVATE "/opt/homebrew/include")
    endif()
    
    if(EXISTS "/usr/local/include")
        target_include_directories(stock_ticker PRIVATE "/usr/local/include")
    endif()
endif()
