cmake_minimum_required(VERSION 3.20)
project({{PROJECT_NAME}}Hap LANGUAGES C CXX)

# Hvigor discovers native modules through src/main/cpp. The game owns its sources in native/.
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../../../../../native" skyengine)
