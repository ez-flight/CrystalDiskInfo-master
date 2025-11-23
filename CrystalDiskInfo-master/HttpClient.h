/*---------------------------------------------------------------------------*/
//       Author : hiyohiyo (modified for server data transmission)
//         Mail : hiyohiyo@crystalmark.info
//          Web : https://crystalmark.info/
//      License : MIT License
/*---------------------------------------------------------------------------*/
#pragma once

#include "stdafx.h"
#include <afx.h>
#include <winhttp.h>

class CHttpClient {
public:
    CHttpClient();
    ~CHttpClient();
    
    // Отправка POST запроса на сервер
    bool Post(const CString& url, const CString& data, CString& response, int& statusCode);
    
    // Парсинг URL
    bool ParseUrl(const CString& url, CString& host, CString& path, int& port, bool& https);
    
private:
    // WinHTTP session handle
    HINTERNET hSession_;
    HINTERNET hConnect_;
    HINTERNET hRequest_;
    
    void Cleanup();
};

