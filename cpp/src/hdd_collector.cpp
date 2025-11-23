#ifdef _WIN32
// КРИТИЧЕСКИ ВАЖНО: winsock2.h должен быть включен ПЕРЕД windows.h
// чтобы избежать конфликта с winsock.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#endif

#include "hdd_collector.h"
#include "wmi_helper.h"
#include "http_client.h"
#include "json_helper.h"
#include "smart_helper.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstring>

HDDCollector::HDDCollector(const std::string& serverHost, int serverPort)
    : serverHost_(serverHost), serverPort_(serverPort) {
}

HDDCollector::~HDDCollector() {
}

SystemInfo HDDCollector::collectAllInfo() {
    std::cout << "Сбор информации о жестких дисках..." << std::endl;
    
    SystemInfo info;
    info.hostname = getHostname();
    info.timestamp = getCurrentTimestamp();
    info.platform = getPlatform();
    info.disks = getDiskInfoWMI();
    
    return info;
}

std::vector<DiskInfo> HDDCollector::getDiskInfoWMI() {
    std::vector<DiskInfo> disks;
    
    try {
        WMIHelper wmi;
        disks = wmi.getDiskDrives();
        
        // Попытка получить S.M.A.R.T. данные для каждого диска через Windows API
        std::cout << "Сбор S.M.A.R.T. данных через Windows API..." << std::endl;
        for (size_t i = 0; i < disks.size(); ++i) {
            std::string deviceId = wmi.getDeviceId(static_cast<int>(i));
            if (!deviceId.empty()) {
                // Получаем S.M.A.R.T. данные напрямую через Windows API (без smartmontools)
                bool smartSuccess = getSmartData(deviceId, disks[i]);
                if (smartSuccess) {
                    std::cout << "  Диск " << (i + 1) << ": S.M.A.R.T. данные получены" << std::endl;
                } else {
                    std::cout << "  Диск " << (i + 1) << ": S.M.A.R.T. данные недоступны (возможно, нужны права администратора)" << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Ошибка при получении данных через WMI: " << e.what() << std::endl;
    }
    
    return disks;
}

bool HDDCollector::getSmartData(const std::string& deviceId, DiskInfo& diskInfo) {
    // Получение S.M.A.R.T. данных напрямую через Windows API
    // Не требует внешних утилит (smartmontools)
    
#ifdef _WIN32
    try {
        // Преобразуем DeviceID в путь к диску (например, PhysicalDrive0)
        std::string drivePath = "PhysicalDrive0";
        size_t pos = deviceId.find("PHYSICALDRIVE");
        if (pos != std::string::npos) {
            drivePath = deviceId.substr(pos);  // Получаем "PHYSICALDRIVE0"
        } else {
            // Пытаемся извлечь номер диска из DeviceID
            // Например, из "\\.\PHYSICALDRIVE0" получаем "PhysicalDrive0"
            size_t backslashPos = deviceId.find_last_of('\\');
            if (backslashPos != std::string::npos) {
                std::string part = deviceId.substr(backslashPos + 1);
                // Преобразуем в верхний регистр для поиска
                std::string upperPart = part;
                std::transform(upperPart.begin(), upperPart.end(), upperPart.begin(), ::toupper);
                if (upperPart.find("PHYSICALDRIVE") != std::string::npos) {
                    drivePath = part;
                }
            }
        }
        
        std::cerr << "  Попытка получить S.M.A.R.T. данные для: " << drivePath << " (из DeviceID: " << deviceId << ")" << std::endl;
        
        // Используем SmartHelper для чтения S.M.A.R.T. данных напрямую
        bool result = SmartHelper::getSmartData(drivePath, diskInfo);
        
        if (result) {
            std::cerr << "  S.M.A.R.T. данные успешно получены" << std::endl;
        } else {
            std::cerr << "  Не удалось получить S.M.A.R.T. данные" << std::endl;
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "  Исключение при получении S.M.A.R.T. данных: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "  Неизвестное исключение при получении S.M.A.R.T. данных" << std::endl;
        return false;
    }
#else
    (void)deviceId;     // Подавление предупреждений
    (void)diskInfo;
    return false;
#endif
}

bool HDDCollector::sendToServer(const SystemInfo& data) {
    try {
        std::string jsonData = JsonHelper::toJson(data);
        
        std::string url = "http://" + serverHost_ + ":" + std::to_string(serverPort_) + "/api/hdd_collect";
        
        HttpClient client;
        std::string response;
        int statusCode = 0;
        
        // Комментарий: IP адрес машины, на которой находятся диски
        std::string localIP = getLocalIPAddress();
        // Комментарий: MAC адрес сетевой карты
        std::string macAddress = getMacAddress();
        
        if (client.post(url, jsonData, response, statusCode)) {
            if (statusCode == 200) {
                std::cout << "✓ Данные успешно отправлены на сервер " << serverHost_ 
                          << ":" << serverPort_ << std::endl;
                std::cout << "  // IP адрес машины: " << localIP << std::endl;
                std::cout << "  // MAC адрес сетевой карты: " << macAddress << std::endl;
                
                // Попытка распарсить ответ сервера (если есть информация об обработанных дисках)
                // Ожидаемый формат: {"processed": 2, "total": 2, "new": 1, "updated": 1}
                size_t processedPos = response.find("\"processed\"");
                size_t totalPos = response.find("\"total\"");
                size_t newPos = response.find("\"new\"");
                size_t updatedPos = response.find("\"updated\"");
                
                if (processedPos != std::string::npos && totalPos != std::string::npos) {
                    // Извлекаем количество обработанных дисков
                    size_t procStart = response.find_first_of("0123456789", processedPos);
                    size_t procEnd = response.find_first_not_of("0123456789", procStart);
                    size_t totalStart = response.find_first_of("0123456789", totalPos);
                    size_t totalEnd = response.find_first_not_of("0123456789", totalStart);
                    
                    if (procStart != std::string::npos && totalStart != std::string::npos) {
                        int processed = std::stoi(response.substr(procStart, procEnd - procStart));
                        int total = std::stoi(response.substr(totalStart, totalEnd - totalStart));
                        std::cout << "  Обработано дисков: " << processed << "/" << total << std::endl;
                        
                        // Показываем новые и обновленные диски
                        if (newPos != std::string::npos) {
                            size_t newStart = response.find_first_of("0123456789", newPos);
                            size_t newEnd = response.find_first_not_of("0123456789", newStart);
                            if (newStart != std::string::npos) {
                                int newDisks = std::stoi(response.substr(newStart, newEnd - newStart));
                                if (newDisks > 0) {
                                    std::cout << "  ✓ Добавлено новых дисков в базу: " << newDisks << std::endl;
                                }
                            }
                        }
                        
                        if (updatedPos != std::string::npos) {
                            size_t updStart = response.find_first_of("0123456789", updatedPos);
                            size_t updEnd = response.find_first_not_of("0123456789", updStart);
                            if (updStart != std::string::npos) {
                                int updatedDisks = std::stoi(response.substr(updStart, updEnd - updStart));
                                if (updatedDisks > 0) {
                                    std::cout << "  ✓ Обновлено существующих дисков: " << updatedDisks << std::endl;
                                }
                            }
                        }
                    }
                }
                
                return true;
            } else {
                std::cerr << "✗ Ошибка отправки: HTTP " << statusCode << std::endl;
                std::cerr << "Ответ сервера: " << response << std::endl;
                return false;
            }
        } else {
            std::cerr << "✗ Ошибка: Не удалось подключиться к серверу " << serverHost_ 
                      << ":" << serverPort_ << std::endl;
            std::cerr << "Проверьте, что сервер запущен и доступен по сети" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "✗ Ошибка при отправке данных: " << e.what() << std::endl;
        return false;
    }
}

bool HDDCollector::saveToFile(const SystemInfo& data, const std::string& filename) {
    try {
        std::string jsonData = JsonHelper::toJson(data);
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Ошибка: Не удалось открыть файл " << filename << std::endl;
            return false;
        }
        
        // Добавляем комментарии с IP и MAC адресом в начало файла
        // Комментарий: IP адрес машины, на которой находятся диски
        std::string localIP = getLocalIPAddress();
        // Комментарий: MAC адрес сетевой карты
        std::string macAddress = getMacAddress();
        
        file << "// IP адрес машины: " << localIP << "\n";
        file << "// MAC адрес сетевой карты: " << macAddress << "\n";
        file << jsonData;
        file.close();
        
        std::cout << "Данные сохранены в " << filename << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Ошибка при сохранении файла: " << e.what() << std::endl;
        return false;
    }
}

std::string HDDCollector::getHostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
    return "unknown";
}

std::string HDDCollector::getPlatform() {
    std::string platform = "Windows";
    
#ifdef _WIN32
    // Используем упрощенный метод определения версии Windows
    // GetVersionEx устарел, но для простоты используем базовую информацию
    // Для более точного определения можно использовать RTL API
    
    // Простое определение версии через системные макросы
    #ifdef _WIN64
        platform = "Windows-64bit";
    #else
        platform = "Windows-32bit";
    #endif
    
    // Можно добавить более детальное определение через версию Windows API
    // Для упрощения используем базовую строку
    platform = "Windows-10-Build";
#endif
    
    return platform;
}

std::string HDDCollector::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

std::string HDDCollector::getLocalIPAddress() {
#ifdef _WIN32
    ULONG bufferSize = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
    DWORD dwRetVal = 0;
    
    // Получаем размер буфера
    pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);
    if (pAddresses == nullptr) {
        return "unknown";
    }
    
    dwRetVal = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &bufferSize);
    
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);
        if (pAddresses == nullptr) {
            return "unknown";
        }
        dwRetVal = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &bufferSize);
    }
    
    if (dwRetVal == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES pCurrentAddress = pAddresses;
        while (pCurrentAddress) {
            // Пропускаем loopback и неактивные адаптеры
            if (pCurrentAddress->IfType != IF_TYPE_SOFTWARE_LOOPBACK && 
                pCurrentAddress->OperStatus == IfOperStatusUp) {
                
                PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrentAddress->FirstUnicastAddress;
                while (pUnicast) {
                    if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                        sockaddr_in* sa_in = (sockaddr_in*)pUnicast->Address.lpSockaddr;
                        char ipStr[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(sa_in->sin_addr), ipStr, INET_ADDRSTRLEN);
                        free(pAddresses);
                        return std::string(ipStr);
                    }
                    pUnicast = pUnicast->Next;
                }
            }
            pCurrentAddress = pCurrentAddress->Next;
        }
    }
    
    if (pAddresses) {
        free(pAddresses);
    }
#endif
    return "unknown";
}

// Комментарий: MAC адрес сетевой карты
std::string HDDCollector::getMacAddress() {
#ifdef _WIN32
    ULONG bufferSize = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
    DWORD dwRetVal = 0;
    
    // Получаем размер буфера
    pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);
    if (pAddresses == nullptr) {
        return "unknown";
    }
    
    dwRetVal = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &bufferSize);
    
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);
        if (pAddresses == nullptr) {
            return "unknown";
        }
        dwRetVal = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &bufferSize);
    }
    
    if (dwRetVal == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES pCurrentAddress = pAddresses;
        while (pCurrentAddress) {
            // Пропускаем loopback и неактивные адаптеры
            if (pCurrentAddress->IfType != IF_TYPE_SOFTWARE_LOOPBACK && 
                pCurrentAddress->OperStatus == IfOperStatusUp) {
                
                // Получаем MAC адрес
                if (pCurrentAddress->PhysicalAddressLength > 0) {
                    std::ostringstream oss;
                    for (DWORD i = 0; i < pCurrentAddress->PhysicalAddressLength; i++) {
                        if (i > 0) oss << ":";
                        oss << std::hex << std::setw(2) << std::setfill('0') 
                            << static_cast<int>(pCurrentAddress->PhysicalAddress[i]);
                    }
                    free(pAddresses);
                    return oss.str();
                }
            }
            pCurrentAddress = pCurrentAddress->Next;
        }
    }
    
    if (pAddresses) {
        free(pAddresses);
    }
#endif
    return "unknown";
}

std::string HDDCollector::escapeJsonString(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                        << static_cast<int>(c);
                } else {
                    oss << c;
                }
                break;
        }
    }
    return oss.str();
}

