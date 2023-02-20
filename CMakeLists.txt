cmake_minimum_required(VERSION 3.16)
project(MyWebServer)

set(CMAKE_CXX_STANDARD 17)

add_executable(server main.cpp  ./http/http_conn.cpp ./timer/lst_timer.cpp ./webserver.cpp)
find_package(Threads REQUIRED)
target_link_libraries(server Threads::Threads)