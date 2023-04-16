#include <iostream>
#include "webserver.h"

int main() {
    std::cout << " ========= Start Webserver ========= " << std::endl;

    // DataBase
    string user = "www";
    string password = "980124";
    string databaseName = "my_test";
    int sql_num = 8;

    int port = 9999;
    int thread_num = 8;
    int trigmode = 0;       // 0LT+LT
    int actor_model = 0;   // 模式  0是Proactor    1是Reactor

    int close_log = 1;  // 0开启日志 1关闭日志
    int log_write = 1;  // 同步日志

    int opt_linger = 0;

    WebServer server;

    server.init(user, password, databaseName, sql_num, close_log, log_write, port, thread_num,  opt_linger, trigmode, actor_model);

    server.log_write();

    server.sql_pool();

    server.thread_pool();

    server.eventListen();

    server.eventLoop();



    return 0;
}
