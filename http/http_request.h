//
// Created by Yuqi on 2023/2/22.
//

#ifndef MYWEBSERVER_HTTP_REQUEST_H
#define MYWEBSERVER_HTTP_REQUEST_H

#include <cstdio>
#include <errno.h>

class HttpRequest {

public:

    enum HTTP_CODE
    {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    HttpRequest() {}
    ~HttpRequest() = default;

};

#endif //MYWEBSERVER_HTTP_REQUEST_H
