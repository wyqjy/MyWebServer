cmake_minimum_required(VERSION 3.16)
project(MyWebServer)

set(CMAKE_CXX_STANDARD 20)


set(INC_DIR /usr/include/mysql)
include_directories(${INC_DIR})
link_libraries(mysqlclient)

add_executable(server main.cpp  ./http/http_conn.cpp ./timer/lst_timer.cpp ./webserver.cpp ./CGImysql/sql_connection_pool.cpp ./log/log.cpp ./redis_study/redis_cache.cpp)

find_package(Threads REQUIRED)
target_link_libraries(server Threads::Threads)
target_link_libraries(server mysqlclient)
target_link_libraries(server -lhiredis)


# 对将数据写入文件，中断程序后，数据并没有写到文件的bug判断
add_executable(test_puts test_puts.cpp)
target_link_libraries(test_puts Threads::Threads)



add_executable(redis_test redis_study/redis_test.cpp redis_study/redis_cache.cpp)
target_link_libraries(redis_test -lhiredis)