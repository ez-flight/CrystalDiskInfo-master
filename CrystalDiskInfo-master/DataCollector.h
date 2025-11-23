/*---------------------------------------------------------------------------*/
//       Author : hiyohiyo (modified for server data transmission)
//         Mail : hiyohiyo@crystalmark.info
//          Web : https://crystalmark.info/
//      License : MIT License
/*---------------------------------------------------------------------------*/
#pragma once

#include "AtaSmart.h"
#include "HttpClient.h"
#include <afx.h>
#include <vector>

// Структура для хранения информации о сервере
struct ServerInfo {
    CString name;        // Название сервера
    CString host;        // Адрес сервера
    int port;            // Порт
    bool isDefault;      // Флаг "по умолчанию"
    
    ServerInfo() : port(5000), isDefault(false) {}
};

class CDataCollector {
public:
    CDataCollector();
    ~CDataCollector();
    
    // Сбор данных о дисках из CAtaSmart в JSON формат (API v2)
    CString CollectDiskData(CAtaSmart& ata, const CString& comment = _T(""));
    
    // Отправка данных на сервер (читает настройки из ini файла)
    bool SendToServer(const CString& iniPath, CAtaSmart& ata, CString& response, const CString& comment = _T(""));
    
    // Отправка данных на сервер (с указанием сервера)
    bool SendToServer(const CString& serverHost, int serverPort, CAtaSmart& ata, CString& response, const CString& comment = _T(""));
    
    // Сохранение данных в JSON файл
    bool SaveToFile(const CString& filename, CAtaSmart& ata);
    
    // Получение IP адреса машины
    CString GetLocalIPAddress();
    
    // Получение MAC адреса сетевой карты
    CString GetMacAddress();
    
    // Получение информации о системе через WMI
    CString GetOSName();
    CString GetOSVersion();
    CString GetOSBuild();
    CString GetOSEdition();
    CString GetOSArchitecture();
    CString GetProcessor();
    int GetMemoryGB();
    CString GetMotherboard();
    CString GetBIOSVersion();
    CString GetDomain();
    CString GetComputerRole();
    CString GetCurrentTimestamp();
    
    // Получение информации о видеокартах
    CString GetGraphicsInfo(); // Возвращает JSON массив видеокарт
    
    // Получение информации о модулях памяти
    CString GetMemoryModulesInfo(); // Возвращает JSON массив модулей памяти
    
    // Работа с серверами
    static int GetServers(const CString& iniPath, std::vector<ServerInfo>& servers);
    static bool GetDefaultServer(const CString& iniPath, ServerInfo& server);
    static bool GetServerByIndex(const CString& iniPath, int index, ServerInfo& server);
    static void SetDefaultServer(const CString& iniPath, int index);
    
private:
    CHttpClient httpClient;
    
    // Преобразование данных диска в JSON
    CString DiskToJson(CAtaSmart::ATA_SMART_INFO& asi, int index);
    
    // Экранирование строки для JSON
    CString EscapeJsonString(const CString& str);
    
    // Получение hostname
    CString GetHostname();
    
    // Парсинг JSON ответа от API и форматирование сообщения
    CString ParseApiResponse(const CString& jsonResponse);
};

