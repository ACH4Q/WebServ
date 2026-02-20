#ifndef HTTPREQ_HPP
#define HTTPREQ_HPP

#include <vector>
#include <iostream>
#include <string>
#include <map>

class HttpReq
{
    private:
        std::string                         _method;
        std::string                         _path;
        std::string                         _version;
        std::map<std::string, std::string>  _headers;
        std::string                         _body;
    public:
};

#endif