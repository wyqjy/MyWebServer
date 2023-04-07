//
// Created by Yuqi on 2023/4/6.
//

#ifndef MYWEBSERVER_REDIS_CACHE_H
#define MYWEBSERVER_REDIS_CACHE_H

#include <iostream>
#include <cstring>
#include <string>
#include <hiredis/hiredis.h>

class redis{
public:
    static redis* get_instance();

    bool Get_command(char *request, char **result);   // 执行一条获取命令，如果redis中存在这个结果，返回true,参数中result就是结果，不存在返回false

    bool Put_command(char *variable, char *val);                 // 执行保存命令， 将一条信息保存进redis数据库

private:
    redis();
    ~redis();

    redisContext *connect;
};


// 暂时还没有连接池，单条连接，超出作用域自己就调用析构函数了
//class redis_RAII{
//public:
//    redis_RAII();
//    ~redis_RAII();
//
//private:
//
//};




#endif //MYWEBSERVER_REDIS_CACHE_H
