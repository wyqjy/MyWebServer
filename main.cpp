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
    int trigmode = 0;
    int actor_model = 0;

    int close_log = 0;

    WebServer server;

    server.init(user, password, databaseName, sql_num, close_log, port, thread_num, trigmode, actor_model);

    server.sql_pool();

    server.thread_pool();

    server.eventListen();

    server.eventLoop();



    return 0;
}
