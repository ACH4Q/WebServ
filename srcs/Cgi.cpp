#include "../includes/Cgi.hpp"
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <ctime>
#include <signal.h>

CgiHandler::CgiHandler(const std::string& exec, const std::string& path)
    : envp(NULL), cgiExecutable(exec), scriptPath(path) {}

CgiHandler::~CgiHandler() 
{
    freeEnvp();
}

void CgiHandler::initEnv(const HttpRequest& req, const RouteResult& route) 
{
    (void)route;
    env["GATEWAY_INTERFACE"] = "CGI/1.1";
    env["SERVER_PROTOCOL"]   = req.getVersion();
    env["SERVER_SOFTWARE"]   = "webserv/1.0";
    env["REDIRECT_STATUS"]   = "200"; 
    env["REQUEST_METHOD"]    = req.getMethod();
    env["SCRIPT_FILENAME"]   = scriptPath;
    
    std::string fullPath = req.getPath();
    size_t questionMark = fullPath.find('?');
    
    if (questionMark != std::string::npos) 
    {
        env["QUERY_STRING"] = fullPath.substr(questionMark + 1);
        env["PATH_INFO"]    = fullPath.substr(0, questionMark);
    } 
    else 
    {
        env["QUERY_STRING"] = "";
        env["PATH_INFO"]    = fullPath;
    }
    
    if (req.getMethod() == "POST") 
    {
        std::map<std::string, std::string>::const_iterator it;
        it = req.getHeaders().find("Content-Type");
        if (it != req.getHeaders().end())
            env["CONTENT_TYPE"] = it->second;
            
        it = req.getHeaders().find("Content-Length");
        if (it != req.getHeaders().end())
            env["CONTENT_LENGTH"] = it->second;
    }
    
    for (std::map<std::string, std::string>::const_iterator it = req.getHeaders().begin(); it != req.getHeaders().end(); ++it) 
    {
        std::string headerName = "HTTP_" + it->first;
        for (size_t i = 0; i < headerName.length(); ++i) 
        {
            if (headerName[i] == '-') headerName[i] = '_';
            headerName[i] = std::toupper(headerName[i]);
        }
        env[headerName] = it->second;
    }
}

char** CgiHandler::mapToEnvp() 
{
    envp = new char*[env.size() + 1];
    int i = 0;
    for (std::map<std::string, std::string>::const_iterator it = env.begin(); it != env.end(); ++it) 
    {
        std::string envString = it->first + "=" + it->second;
        envp[i] = new char[envString.length() + 1];
        std::strcpy(envp[i], envString.c_str());
        i++;
    }
    envp[i] = NULL;
    return envp;
}

void CgiHandler::freeEnvp() 
{
    if (envp != NULL) 
    {
        for (int i = 0; envp[i] != NULL; i++) 
        {
            delete[] envp[i];
        }
        delete[] envp;
        envp = NULL;
    }
}

std::string CgiHandler::executeCgi(const HttpRequest& req, const RouteResult& route) 
{
    initEnv(req, route);
    char** envArray = mapToEnvp();
    
    int fdIn = -1;
    if (req.getMethod() == "POST" && !req.getBodyFilename().empty()) 
    {
        fdIn = open(req.getBodyFilename().c_str(), O_RDONLY);
    }
    
    std::ostringstream outNameStream;
    outNameStream << "./www/html/www/files/uploads/cgi_out_" << time(NULL) << "_" << getpid() << ".tmp";
    std::string outFilename = outNameStream.str();
    
    int fdOut = open(outFilename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fdOut == -1) 
    {
        freeEnvp();
        if (fdIn != -1)
            close(fdIn);
        return "HTTP/1.1 500 Internal Server Error\r\n\r\nFailed to create CGI output file.";
    }
    
    pid_t pid = fork();
    if (pid == -1) 
    {
        freeEnvp();
        if (fdIn != -1)
            close(fdIn);
        close(fdOut);
        return "HTTP/1.1 500 Internal Server Error\r\n\r\nFork failed.";
    }
    
    if (pid == 0) 
    {
        if (fdIn != -1) 
        {
            dup2(fdIn, STDIN_FILENO);
            close(fdIn);
        }
        dup2(fdOut, STDOUT_FILENO);
        close(fdOut);
        
        char* argv[3];
        argv[0] = const_cast<char*>(cgiExecutable.c_str());
        argv[1] = const_cast<char*>(scriptPath.c_str());
        argv[2] = NULL;
        
        execve(argv[0], argv, envArray);
        exit(1); 
    }
    if (fdIn != -1)
        close(fdIn);
    close(fdOut);
    time_t startTime = time(NULL);
    int status;
    bool timedOut = false;

    while (true) 
    {
        pid_t res = waitpid(pid, &status, WNOHANG);
        
        if (res == pid)
        {
            break; 
        } 
        else if (res == -1)
        {
            break;
        }
        if (time(NULL) - startTime >= 10) 
        {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            timedOut = true;
            break;
        }
        
        usleep(10000);
    }
    freeEnvp();
    if (timedOut) 
    {
        std::remove(outFilename.c_str());
        return "HTTP/1.1 504 Gateway Timeout\r\n\r\nCGI Script timed out after 10 seconds.";
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) 
    {
        std::remove(outFilename.c_str());
        return "HTTP/1.1 502 Bad Gateway\r\n\r\nCGI Script Crashed.";
    }
    std::ifstream outFile(outFilename.c_str());
    std::stringstream buffer;
    buffer << outFile.rdbuf();
    outFile.close();
    std::remove(outFilename.c_str());
    
    return buffer.str();
}