#include <iostream>
#include "webserver.h"

int main() {
    std::cout << " ========= Start Webserver ========= " << std::endl;

    int port = 9999;
    int thread_num = 8;
    int trigmode = 0;
    int actor_model = 0;

    WebServer server;

    server.init(port, thread_num, trigmode, actor_model);

    server.thread_pool();

    server.eventListen();

    server.eventLoop();



    return 0;
}
