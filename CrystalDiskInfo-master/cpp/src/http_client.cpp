#ifdef _WIN32
// КРИТИЧЕСКИ ВАЖНО: WIN32_LEAN_AND_MEAN исключает winsock.h из windows.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

#include "http_client.h"
#include <iostream>
#include <sstream>
#include <regex>

#pragma comment(lib, "winhttp.lib")

HttpClient::HttpClient() {
}

HttpClient::~HttpClient() {
}

bool HttpClient::parseUrl(const std::string& url, std::string& host, 
                          std::string& path, int& port, bool& https) {
    std::regex urlRegex(R"(^(https?)://([^:/]+)(?::(\d+))?(/.*)?$)");
    std::smatch match;
    
    if (std::regex_match(url, match, urlRegex)) {
        https = (match[1].str() == "https");
        host = match[2].str();
        path = match[4].matched ? match[4].str() : "/";
        port = match[3].matched ? std::stoi(match[3].str()) : (https ? 443 : 80);
        return true;
    }
    
    return false;
}

bool HttpClient::post(const std::string& url, const std::string& data, 
                      std::string& response, int& statusCode) {
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    bool success = false;
    
    try {
        std::string host, path;
        int port;
        bool https;
        
        if (!parseUrl(url, host, path, port, https)) {
            std::cerr << "Неверный формат URL: " << url << std::endl;
            return false;
        }
        
        // Преобразуем host в wide string
        std::wstring whost(host.begin(), host.end());
        std::wstring wpath(path.begin(), path.end());
        
        // Инициализация WinHTTP
        hSession = WinHttpOpen(
            L"HDD Collector/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
        
        if (!hSession) {
            std::cerr << "Не удалось открыть WinHTTP сессию. Ошибка: " 
                      << GetLastError() << std::endl;
            return false;
        }
        
        // Установка таймаута
        DWORD timeout = 30000; // 30 секунд
        WinHttpSetTimeouts(hSession, timeout, timeout, timeout, timeout);
        
        // Подключение к серверу
        hConnect = WinHttpConnect(
            hSession,
            whost.c_str(),
            static_cast<INTERNET_PORT>(port),
            0
        );
        
        if (!hConnect) {
            std::cerr << "Не удалось подключиться к серверу. Ошибка: " 
                      << GetLastError() << std::endl;
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        // Создание запроса
        hRequest = WinHttpOpenRequest(
            hConnect,
            L"POST",
            wpath.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            https ? WINHTTP_FLAG_SECURE : 0
        );
        
        if (!hRequest) {
            std::cerr << "Не удалось создать HTTP запрос. Ошибка: " 
                      << GetLastError() << std::endl;
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        // Установка заголовков
        std::wstring headers = L"Content-Type: application/json; charset=utf-8\r\n";
        if (!WinHttpAddRequestHeaders(
                hRequest,
                headers.c_str(),
                static_cast<DWORD>(-1),
                WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
            std::cerr << "Не удалось добавить заголовки. Ошибка: " 
                      << GetLastError() << std::endl;
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        // Отправка запроса (данные в UTF-8)
        // WinHttpSendRequest принимает данные как байты
        if (!WinHttpSendRequest(
                hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS,
                0,
                const_cast<LPVOID>(static_cast<const void*>(data.c_str())),
                static_cast<DWORD>(data.length()),
                static_cast<DWORD>(data.length()),
                0)) {
            std::cerr << "Не удалось отправить запрос. Ошибка: " 
                      << GetLastError() << std::endl;
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        // Получение ответа
        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            std::cerr << "Не удалось получить ответ. Ошибка: " 
                      << GetLastError() << std::endl;
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        // Получение кода статуса
        DWORD statusCodeSize = sizeof(statusCode);
        if (!WinHttpQueryHeaders(
                hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode,
                &statusCodeSize,
                WINHTTP_NO_HEADER_INDEX)) {
            std::cerr << "Не удалось получить код статуса. Ошибка: " 
                      << GetLastError() << std::endl;
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        // Чтение ответа
        response.clear();
        DWORD bytesAvailable = 0;
        do {
            if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
                break;
            }
            
            if (bytesAvailable == 0) {
                break;
            }
            
            std::vector<char> buffer(bytesAvailable + 1);
            DWORD bytesRead = 0;
            
            if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    response.append(buffer.data(), bytesRead);
                }
            } else {
                break;
            }
        } while (bytesAvailable > 0);
        
        success = true;
        
    } catch (const std::exception& e) {
        std::cerr << "Исключение при отправке HTTP запроса: " << e.what() << std::endl;
    }
    
    // Очистка
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    
    return success;
}

#include <vector>

