#pragma once

#include <string>
#include <vector>
#include <memory>

// Структура для информации о диске
struct DiskInfo {
    std::string model;
    std::string serialNumber;
    int64_t sizeGb = 0;
    std::string interfaceType;  // Переименовано из interface (зарезервированное слово в COM)
    std::string mediaType;
    std::string manufacturer;
    
    // S.M.A.R.T. данные (опционально)
    // Используем -1 как индикатор "не собрано", 0 и выше - собранные значения
    int powerOnHours = -1;  // -1 означает не собрано, >= 0 - собранное значение
    int powerOnCount = -1;  // -1 означает не собрано, >= 0 - собранное значение
    std::string healthStatus;
};

// Структура для итоговых данных
struct SystemInfo {
    std::string hostname;
    std::string timestamp;
    std::string platform;
    std::vector<DiskInfo> disks;
};

class HDDCollector {
public:
    HDDCollector(const std::string& serverHost, int serverPort = 5000);
    ~HDDCollector();

    // Сбор информации о дисках
    SystemInfo collectAllInfo();
    
    // Отправка данных на сервер
    bool sendToServer(const SystemInfo& data);
    
    // Сохранение данных в JSON файл
    bool saveToFile(const SystemInfo& data, const std::string& filename);
    
    // Получение сетевой информации
    // Комментарий: IP адрес машины, на которой находятся диски
    std::string getLocalIPAddress();
    // Комментарий: MAC адрес сетевой карты
    std::string getMacAddress();

private:
    std::string serverHost_;
    int serverPort_;
    
    // Получение информации о дисках через WMI
    std::vector<DiskInfo> getDiskInfoWMI();
    
    // Получение S.M.A.R.T. данных (опционально)
    bool getSmartData(const std::string& deviceId, DiskInfo& diskInfo);
    
    // Вспомогательные методы
    std::string getHostname();
    std::string getPlatform();
    std::string getCurrentTimestamp();
    std::string escapeJsonString(const std::string& str);
};

