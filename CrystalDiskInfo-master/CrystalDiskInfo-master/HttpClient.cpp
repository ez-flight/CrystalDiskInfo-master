/*---------------------------------------------------------------------------*/
//       Author : hiyohiyo (modified for server data transmission)
//         Mail : hiyohiyo@crystalmark.info
//          Web : https://crystalmark.info/
//      License : MIT License
/*---------------------------------------------------------------------------*/
#include "stdafx.h"
#include "HttpClient.h"
#include <winhttp.h>
#include <afx.h>

#pragma comment(lib, "winhttp.lib")

CHttpClient::CHttpClient() : hSession_(NULL), hConnect_(NULL), hRequest_(NULL) {
}

CHttpClient::~CHttpClient() {
    Cleanup();
}

void CHttpClient::Cleanup() {
    if (hRequest_) {
        WinHttpCloseHandle(hRequest_);
        hRequest_ = NULL;
    }
    if (hConnect_) {
        WinHttpCloseHandle(hConnect_);
        hConnect_ = NULL;
    }
    if (hSession_) {
        WinHttpCloseHandle(hSession_);
        hSession_ = NULL;
    }
}

bool CHttpClient::ParseUrl(const CString& url, CString& host, CString& path, int& port, bool& https) {
    CString urlStr = url;
    urlStr.Trim();
    
    if (urlStr.IsEmpty()) {
        return false;
    }
    
    // Определяем протокол
    https = false;
    if (urlStr.Find(_T("https://")) == 0) {
        https = true;
        urlStr = urlStr.Mid(8);
    } else if (urlStr.Find(_T("http://")) == 0) {
        https = false;
        urlStr = urlStr.Mid(7);
    }
    
    // Парсим хост и порт
    int portPos = urlStr.Find(_T(':'));
    int pathPos = urlStr.Find(_T('/'));
    
    if (portPos != -1 && (pathPos == -1 || portPos < pathPos)) {
        // Есть порт
        host = urlStr.Left(portPos);
        CString portStr;
        if (pathPos != -1) {
            portStr = urlStr.Mid(portPos + 1, pathPos - portPos - 1);
            path = urlStr.Mid(pathPos);
        } else {
            portStr = urlStr.Mid(portPos + 1);
            path = _T("/");
        }
        port = _ttoi(portStr);
    } else {
        // Нет порта, используем стандартный
        if (pathPos != -1) {
            host = urlStr.Left(pathPos);
            path = urlStr.Mid(pathPos);
        } else {
            host = urlStr;
            path = _T("/");
        }
        port = https ? 443 : 80;
    }
    
    if (host.IsEmpty()) {
        return false;
    }
    
    return true;
}

bool CHttpClient::Post(const CString& url, const CString& data, CString& response, int& statusCode) {
    Cleanup();
    statusCode = 0;
    response.Empty();
    
    CString host, path;
    int port;
    bool https;
    
    if (!ParseUrl(url, host, path, port, https)) {
        response = _T("Failed to parse URL");
        return false;
    }
    
    // Инициализация WinHTTP
    hSession_ = WinHttpOpen(
        _T("CrystalDiskInfo/HDDCollector"),
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    
    if (!hSession_) {
        DWORD error = GetLastError();
        response.Format(_T("WinHttpOpen failed. Error code: %d (0x%X)"), error, error);
        Cleanup();
        return false;
    }
    
    // Установка таймаутов
    DWORD timeout = 30000; // 30 секунд
    WinHttpSetTimeouts(hSession_, timeout, timeout, timeout, timeout);
    
    // Подключение к серверу
    hConnect_ = WinHttpConnect(hSession_, host, port, 0);
    if (!hConnect_) {
        DWORD error = GetLastError();
        CString errorDesc;
        switch (error) {
            case ERROR_WINHTTP_CANNOT_CONNECT:
                errorDesc = _T("Cannot connect to server (ERROR_WINHTTP_CANNOT_CONNECT)");
                break;
            case ERROR_WINHTTP_CONNECTION_ERROR:
                errorDesc = _T("Connection error (ERROR_WINHTTP_CONNECTION_ERROR)");
                break;
            case ERROR_WINHTTP_TIMEOUT:
                errorDesc = _T("Connection timeout (ERROR_WINHTTP_TIMEOUT)");
                break;
            case ERROR_WINHTTP_NAME_NOT_RESOLVED:
                errorDesc = _T("Cannot resolve server name (ERROR_WINHTTP_NAME_NOT_RESOLVED)");
                break;
            default:
                errorDesc.Format(_T("WinHttpConnect failed. Error code: %d"), error);
                break;
        }
        response.Format(_T("Failed to connect:\n%s\n\nServer: %s:%d\nPath: %s"), 
                       errorDesc, host, port, path);
        Cleanup();
        return false;
    }
    
    // Создание запроса
    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    hRequest_ = WinHttpOpenRequest(
        hConnect_,
        _T("POST"),
        path,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );
    
    if (!hRequest_) {
        DWORD error = GetLastError();
        response.Format(_T("WinHttpOpenRequest failed. Error code: %d (0x%X)\nServer: %s:%d\nPath: %s"), error, error, host, port, path);
        Cleanup();
        return false;
    }
    
    // Установка заголовков
    CString headers = _T("Content-Type: application/json\r\n");
    
    // Конвертация данных в UTF-8
    int dataLen = data.GetLength();
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, data, dataLen, NULL, 0, NULL, NULL);
    char* utf8Data = new char[utf8Len + 1];
    WideCharToMultiByte(CP_UTF8, 0, data, dataLen, utf8Data, utf8Len, NULL, NULL);
    utf8Data[utf8Len] = '\0';
    
    // Отправка запроса
    bool result = false;
    statusCode = 0;
    
    if (!WinHttpSendRequest(
        hRequest_,
        headers,
        headers.GetLength(),
        utf8Data,
        utf8Len,
        utf8Len,
        0
    )) {
        // Ошибка отправки запроса
        DWORD error = GetLastError();
        CString errorDesc;
        switch (error) {
            case ERROR_WINHTTP_CANNOT_CONNECT:
                errorDesc = _T("Cannot connect to server (ERROR_WINHTTP_CANNOT_CONNECT)");
                break;
            case ERROR_WINHTTP_CONNECTION_ERROR:
                errorDesc = _T("Connection error (ERROR_WINHTTP_CONNECTION_ERROR)");
                break;
            case ERROR_WINHTTP_TIMEOUT:
                errorDesc = _T("Connection timeout (ERROR_WINHTTP_TIMEOUT)");
                break;
            case ERROR_WINHTTP_NAME_NOT_RESOLVED:
                errorDesc = _T("Cannot resolve server name (ERROR_WINHTTP_NAME_NOT_RESOLVED)");
                break;
            default:
                errorDesc.Format(_T("WinHttpSendRequest failed. Error code: %d"), error);
                break;
        }
        response.Format(_T("Failed to send request:\n%s\n\nServer: %s:%d\nPath: %s"), 
                       errorDesc, host, port, path);
        delete[] utf8Data;
        Cleanup();
        return false;
    }
    
    // Получение ответа с проверкой таймаута
    DWORD receiveResult = WinHttpReceiveResponse(hRequest_, NULL);
    if (!receiveResult) {
        // Ошибка получения ответа
        DWORD error = GetLastError();
        CString errorDesc;
        switch (error) {
            case ERROR_WINHTTP_TIMEOUT:
                errorDesc = _T("Receive timeout (ERROR_WINHTTP_TIMEOUT)");
                break;
            case ERROR_WINHTTP_CONNECTION_ERROR:
                errorDesc = _T("Connection error during receive (ERROR_WINHTTP_CONNECTION_ERROR)");
                break;
            case ERROR_WINHTTP_CANNOT_CONNECT:
                errorDesc = _T("Cannot connect (ERROR_WINHTTP_CANNOT_CONNECT)");
                break;
            default:
                errorDesc.Format(_T("WinHttpReceiveResponse failed. Error code: %d"), error);
                break;
        }
        response.Format(_T("Failed to receive response:\n%s\n\nServer: %s:%d"), errorDesc, host, port);
        delete[] utf8Data;
        Cleanup();
        return false;
    }
    
    // Получение статус-кода
    DWORD statusCodeSize = sizeof(DWORD);
    if (!WinHttpQueryHeaders(
        hRequest_,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        NULL,
        &statusCode,
        &statusCodeSize,
        NULL
    )) {
        // Не удалось получить статус-код
        statusCode = 0;
        response = _T("Failed to get HTTP status code");
        delete[] utf8Data;
        Cleanup();
        return false;
    }
    
    // Чтение ответа с проверкой ошибок
    DWORD bytesAvailable = 0;
    response.Empty();
    int readAttempts = 0;
    const int maxReadAttempts = 10000; // Защита от бесконечного цикла
    
    do {
        readAttempts++;
        if (readAttempts > maxReadAttempts) {
            response = _T("Too many read attempts, possible infinite loop");
            delete[] utf8Data;
            Cleanup();
            return false;
        }
        
        if (!WinHttpQueryDataAvailable(hRequest_, &bytesAvailable)) {
            // Ошибка при проверке доступных данных
            DWORD error = GetLastError();
            if (error != ERROR_WINHTTP_OPERATION_CANCELLED) {
                // Не критическая ошибка, просто закончим чтение
                break;
            }
        }
        
        if (bytesAvailable > 0) {
            char* buffer = new char[bytesAvailable + 1];
            DWORD bytesRead = 0;
            
            if (WinHttpReadData(hRequest_, buffer, bytesAvailable, &bytesRead)) {
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    CString chunk;
                    int requiredLen = MultiByteToWideChar(CP_UTF8, 0, buffer, bytesRead, NULL, 0);
                    if (requiredLen > 0) {
                        MultiByteToWideChar(CP_UTF8, 0, buffer, bytesRead, chunk.GetBuffer(requiredLen), requiredLen);
                        chunk.ReleaseBuffer(requiredLen);
                        response += chunk;
                    }
                }
            } else {
                // Ошибка чтения данных
                DWORD error = GetLastError();
                if (error != ERROR_WINHTTP_OPERATION_CANCELLED && error != ERROR_WINHTTP_CONNECTION_ERROR) {
                    // Не критическая ошибка
                }
                delete[] buffer;
                break;
            }
            
            delete[] buffer;
        } else {
            break; // Нет больше данных
        }
    } while (bytesAvailable > 0);
    
    // Успешный ответ - статус 200-299
    result = (statusCode >= 200 && statusCode < 300);
    
    delete[] utf8Data;
    Cleanup();
    
    return result;
}

