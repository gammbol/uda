cmake_minimum_required(VERSION 3.30)
project(uda)

set(CROW_USE_BOOST ON)

set(CMAKE_CXX_STANDARD 20)

# Firebird
if (WIN32)
    set(FBCLIENT_INCLUDE_DIR "C:\\Program Files\\Firebird\\Firebird_5_0\\include")

    find_library(FBCLIENT_LIB
            NAMES fbclient_ms fbclient
            PATHS "C:/Program Files/Firebird/Firebird_5_0/lib"
            NO_DEFAULT_PATH
    )

    find_library(FBUTIL_LIB
            NAMES ib_util_ms
            PATHS "C:/Program Files/Firebird/Firebird_5_0/lib"
            NO_DEFAULT_PATH
    )
elseif (APPLE)
    find_path(FBCLIENT_INCLUDE_DIR ibase.h
            /opt/homebrew/include
            /usr/local/include
    )

    find_library(FBCLIENT_LIB fbclient
            /opt/homebrew/lib
            /usr/local/lib
    )
else ()
    find_path(FBCLIENT_INCLUDE_DIR ibase.h)
    find_library(FBCLIENT_LIB fbclient)
endif ()

if (NOT FBCLIENT_INCLUDE_DIR)
    message(FATAL_ERROR "Firebird include dir (ibase.h) not found")
endif ()

if (NOT FBCLIENT_LIB)
    message(FATAL_ERROR "fbclient library not found")
endif ()

add_subdirectory(deps/Crow)

add_executable(uda src/uda.cpp src/fb_connect.cpp)

target_include_directories(uda PRIVATE include ${FBCLIENT_INCLUDE_DIR})

if(WIN32)
    target_link_libraries(uda PRIVATE
            Crow::Crow ws2_32
            wsock32
            ${FBCLIENT_LIB}
            ${FBUTIL_LIB}
    )
else ()
    target_link_libraries(uda PRIVATE Crow::Crow ${FBCLIENT_LIB})
endif ()