#include "../includes/ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include <cctype>

ConfigParser::ConfigParser() : idx(0) {}

ConfigParser::~ConfigParser() {}

std::vector<ServerConfig> ConfigParser::getServers() const
{
    return servers;
}

void ConfigParser::checkFile(const std::string &filename)
{
    std::ifstream file(filename.c_str());
    if (!file.is_open())
        throw std::runtime_error("Error: Could not open config file.");
    file.close();
}

void ConfigParser::tokenize(const std::string &filename)
{
    std::ifstream file(filename.c_str());
    std::string line;
    while (std::getline(file, line))
    {
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos)
            line = line.substr(0, commentPos);
        std::string finalLine;
        for (size_t i = 0; i < line.size(); i++)
        {
            if (line[i] == '{' || line[i] == '}' || line[i] == ';')
            {
                finalLine += " ";
                finalLine += line[i];
                finalLine += " ";
            } else
            {
                finalLine += line[i];
            }
        }
        std::stringstream ss(finalLine);
        std::string word;
        while (ss >> word)
        {
            tokens.push_back(word);
        }
    }
}

void ConfigParser::parse(const std::string &filename)
{
    checkFile(filename);
    tokenize(filename);
    idx = 0;
    while (idx < tokens.size())
    {
        if (tokens[idx] == "server")
        {
            idx++;
            parseServerBlock();
        } else
        {
            throw std::runtime_error("Error: Unexpected token '" + tokens[idx] + "' outside server block");
        }
    }
}

void ConfigParser::parseServerBlock()
{
    expect("{");
    ServerConfig server;
    while (idx < tokens.size() && tokens[idx] != "}")
    {
        if (tokens[idx] == "listen")
        {
            idx++;
            if (!isValidPort(tokens[idx]))
                throw std::runtime_error("Error: Invalid port number: " + tokens[idx]);
            server.port = toInt(tokens[idx++]);
            expect(";");
        } 
        else if (tokens[idx] == "host")
        {
            idx++;
            if (!isValidIP(tokens[idx]))
                throw std::runtime_error("Error: Invalid IP address: " + tokens[idx]);
            server.host = tokens[idx++];
            expect(";");
        }
        else if (tokens[idx] == "server_name")
        {
            idx++;
            while (tokens[idx] != ";")
            {
                server.serverNames.push_back(tokens[idx++]);
            }
            expect(";");
        }
        else if (tokens[idx] == "root")
        {
            idx++;
            server.root = tokens[idx++];
            expect(";");
        }
        else if (tokens[idx] == "error_page")
        {
            idx++;
            if (!isNumeric(tokens[idx]))
                throw std::runtime_error("Error: Invalid error code: " + tokens[idx]);
            int code = toInt(tokens[idx++]);
            std::string page = tokens[idx++];
            server.errorPages[code] = page;
            expect(";");
        }
        else if (tokens[idx] == "client_max_body_size")
        {
             idx++;
             if (!isNumeric(tokens[idx]))
                 throw std::runtime_error("Error: Invalid client_max_body_size: " + tokens[idx]);
             server.maxBodySize = static_cast<unsigned long>(toInt(tokens[idx++]));
             expect(";");
        }
        else if (tokens[idx] == "location")
        {
            idx++;
            parseLocationBlock(server);
        }
        else
        {
            throw std::runtime_error("Error: Unknown directive in server block: " + tokens[idx]);
        }
    }
    expect("}");
    servers.push_back(server);
}

void ConfigParser::parseLocationBlock(ServerConfig &server)
{
    Location loc;
    loc.path = tokens[idx++];
    expect("{");
    while (idx < tokens.size() && tokens[idx] != "}")
    {
        if (tokens[idx] == "root")
        {
            idx++;
            loc.root = tokens[idx++];
            expect(";");
        }
        else if (tokens[idx] == "alias")
        {
            idx++;
            loc.alias = tokens[idx++];
            expect(";");
        }
        else if (tokens[idx] == "client_max_body_size")
        {
            idx++;
            if (!isNumeric(tokens[idx]))
                 throw std::runtime_error("Error: Invalid client_max_body_size in location: " + tokens[idx]);
            loc.maxBodySize = static_cast<unsigned long>(toInt(tokens[idx++]));
            expect(";");
        }
        else if (tokens[idx] == "index")
        {
            idx++;
            if (tokens[idx] != ";")
            {
                loc.index = tokens[idx++];
                while (tokens[idx] != ";")
                {
                    idx++; 
                }
            }
            expect(";");
        }
        else if (tokens[idx] == "autoindex")
        {
            idx++;
            if (tokens[idx] != "on" && tokens[idx] != "off")
                 throw std::runtime_error("Error: autoindex must be 'on' or 'off'");
            loc.autoindex = (tokens[idx++] == "on");
            expect(";");
        }
        else if (tokens[idx] == "allow_methods")
        {
            idx++;
            while (tokens[idx] != ";")
            {
                if (tokens[idx] != "GET" && tokens[idx] != "POST" && tokens[idx] != "DELETE")
                     throw std::runtime_error("Error: Invalid method '" + tokens[idx] + "'. Only GET, POST, DELETE allowed.");
                loc.methods.push_back(tokens[idx++]);
            }
            expect(";");
        }
        else if (tokens[idx] == "return")
        {
            idx++;
            if (tokens[idx + 1] != ";")
            {
                 if (!isNumeric(tokens[idx]))
                     throw std::runtime_error("Error: Invalid return code: " + tokens[idx]);
                 idx++;
            }
            loc.returnPath = tokens[idx++];
            expect(";");
        }
        else {
             throw std::runtime_error("Error: Unknown directive in location: " + tokens[idx]);
        }
    }
    expect("}");
    server.locations.push_back(loc);
}

void ConfigParser::expect(const std::string &token)
{
    if (idx >= tokens.size() || tokens[idx] != token)
    {
        throw std::runtime_error("Error: Expected '" + token + "' but found '" + tokens[idx] + "'");
    }
    idx++;
}

int ConfigParser::toInt(const std::string &str)
{
    return std::atoi(str.c_str());
}

bool ConfigParser::isNumeric(const std::string &str) const
{
    if (str.empty())
        return false;
    for (size_t i = 0; i < str.length(); ++i)
    {
        if (!isdigit(str[i])) return false;
    }
    return true;
}

bool ConfigParser::isValidPort(const std::string &portStr) const
{
    if (!isNumeric(portStr))
        return false;
    int port = std::atoi(portStr.c_str());
    return (port >= 1 && port <= 65535);
}

bool ConfigParser::isValidIP(const std::string &ip) const
{
    if (ip == "localhost")
        return true; 
    std::stringstream ss(ip);
    std::string token;
    int parts = 0;
    while (std::getline(ss, token, '.'))
    {
        if (token.empty() || token.length() > 3 || !isNumeric(token)) 
            return false;
        int num = std::atoi(token.c_str());
        if (num < 0 || num > 255) 
            return false;
        parts++;
    }
    return (parts == 4 && ss.eof());
}