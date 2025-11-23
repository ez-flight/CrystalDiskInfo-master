#pragma once

#include <string>

class HttpClient {
public:
    HttpClient();
    ~HttpClient();
    
    bool post(const std::string& url, const std::string& data, 
              std::string& response, int& statusCode);
    
private:
    bool parseUrl(const std::string& url, std::string& host, 
                  std::string& path, int& port, bool& https);
};

