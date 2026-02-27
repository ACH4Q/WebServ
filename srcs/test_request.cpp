#include "../includes/HttpReq.hpp"
#include <iostream>
#include <string>

// Helper function to print the parsed request
void printRequest(const HttpRequest& req) {
    std::cout << "--- Parsed Request Result ---" << std::endl;
    std::cout << "Method: " << req.getMethod() << std::endl;
    std::cout << "Path:   " << req.getPath() << std::endl;
    std::cout << "State:  " << req.getState() << " (4 = Request_Finished)" << std::endl;
    
    std::cout << "Headers:" << std::endl;
    // Using C++98 iterator for the map
    std::map<std::string, std::string> headers = req.getHeaders();
    for (std::map<std::string, std::string>::iterator it = headers.begin(); it != headers.end(); ++it) {
        std::cout << "  [" << it->first << "] -> [" << it->second << "]" << std::endl;
    }
    
    std::cout << "Body:   [" << req.getBody() << "]" << std::endl;
    std::cout << "-----------------------------\n" << std::endl;
}

int main() {
    // ==========================================
    // TEST 1: A perfect, complete GET request
    // ==========================================
    std::cout << ">>> TEST 1: Complete GET Request <<<" << std::endl;
    HttpRequest req1;
    std::string data1 = "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\nUser-Agent: curl/7.68.0\r\n\r\n";
    
    req1.parse(data1);
    printRequest(req1);


    // ==========================================
    // TEST 2: A fragmented POST request
    // Simulates the non-blocking behavior of poll()
    // ==========================================
    std::cout << ">>> TEST 2: Fragmented POST Request <<<" << std::endl;
    HttpRequest req2;
    
    // Fragment 1: Just the request line and part of a header
    std::string frag1 = "POST /upload HTTP/1.1\r\nHost: loc";
    std::cout << "Receiving Fragment 1...\n";
    req2.parse(frag1);
    std::cout << "Current State: " << req2.getState() << " (Should not be Finished yet)\n\n";

    // Fragment 2: The rest of the headers and half the body
    std::string frag2 = "alhost\r\nContent-Length: 11\r\n\r\nhello";
    std::cout << "Receiving Fragment 2...\n";
    req2.parse(frag2);
    std::cout << "Current State: " << req2.getState() << " (Still waiting for body)\n\n";

    // Fragment 3: The rest of the body
    std::string frag3 = " world";
    std::cout << "Receiving Fragment 3...\n";
    req2.parse(frag3);
    
    printRequest(req2);

    return 0;
}