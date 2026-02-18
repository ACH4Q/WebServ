#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "Config.hpp"
#include <vector>
#include <string>

class ConfigParser
{
    private:
        std::vector<ServerConfig>   servers;
        std::vector<std::string>    tokens;
        size_t                      idx;

        void    checkFile(const std::string &filename);
        void    tokenize(const std::string &filename);
        void    parseServerBlock();
        void    parseLocationBlock(ServerConfig &server);
        void    expect(const std::string &token);
        int     toInt(const std::string &str);

    public:
        ConfigParser();
        ~ConfigParser();
        void                        parse(const std::string &filename);
        std::vector<ServerConfig>   getServers() const;
};

#endif