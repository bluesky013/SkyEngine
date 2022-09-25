cmake_minimum_required(VERSION 3.10.0)

project(SkyeEngine)

find_package(game-activity REQUIRED CONFIG)

add_library(AndroidLauncher SHARED
        Main.cpp
        )

target_link_libraries(AndroidLauncher
        android
        log
        game-activity::game-activity
        )
