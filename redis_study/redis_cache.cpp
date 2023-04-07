//
// Created by Yuqi on 2023/4/6.
//

#include "redis_cache.h"

#include <typeinfo>

redis *redis::get_instance() {
    static redis *red = new redis();
    return red;
}

redis::~redis() {
    redisFree(connect);
    std::cout<<"redis connect free"<<std::endl;
}

redis::redis() {
    connect = redisConnect("43.143.195.140", 6379);
    if(connect->err){
        std::cout<<"redis connect failed"<<std::endl;
    }
    else{
        std::cout<<"redis connect success"<<std::endl;
    }

    redisReply *reply = (redisReply*) redisCommand(connect, "AUTH %s", "980124");
    if(reply->type == REDIS_REPLY_ERROR) {
        std::cout<<"Password failed: "<<reply->str<<std::endl;
    }
    else{
        std::cout<<"Password success  "<<reply->str<<std::endl;
    }
    freeReplyObject(reply);
}

bool redis::Get_command(char *request, char **result) {
    redisReply *reply;
//    std::cout<<"---"<<request<<std::endl;
    reply = (redisReply*)redisCommand(connect, "exists %s", request);
    if(reply->integer == 0){
//        std::cout<<"not found1"<<std::endl;
        freeReplyObject(reply);

        return false;
    }

    freeReplyObject(reply);
    redisReply *reply1 = (redisReply*) redisCommand(connect, "get %s", request);
//    std::cout<<"str类型：   "<<typeid(reply1->str).name()<<std::endl;

    char *tmp = NULL;
    tmp = (char *)malloc(strlen(reply1->str));
    strcpy(tmp, reply1->str);
    *result = tmp;
//    std::cout<<*result<<std::endl;
    freeReplyObject(reply1);      // 这个问题，释放后里面的内容就不存在了，所以返回值是乱码，如果不释放是不是会内存泄漏
    return true;

}

bool redis::Put_command(char *variable, char *val) {
    redisReply  *reply;
    reply = (redisReply*) redisCommand(connect,  "set %s %s", variable, val);
    std::string r = reply->str;
    freeReplyObject(reply);

//    redisCommand(connect, "expire %s %d", variable, 30);   // 设置生存时间

    if(r=="OK")
        return true;
    return false;
}