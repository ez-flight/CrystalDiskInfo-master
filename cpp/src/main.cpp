#ifdef _WIN32
// КРИТИЧЕСКИ ВАЖНО: winsock2.h должен быть включен ПЕРЕД windows.h
// чтобы избежать конфликта с winsock.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <sddl.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

#include "hdd_collector.h"
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

// Простой парсер аргументов командной строки
struct Arguments {
    std::string host = "10.3.3.7";  // Сервер по умолчанию
    int port = 5000;
    std::string saveFile;
    std::string configFile;
    bool help = false;
};

Arguments parseArguments(int argc, char* argv[]) {
    Arguments args;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            args.help = true;
        } else if (arg == "--host" && i + 1 < argc) {
            args.host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            args.port = std::stoi(argv[++i]);
        } else if (arg == "--save" && i + 1 < argc) {
            args.saveFile = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            args.configFile = argv[++i];
        }
    }
    
    return args;
}

bool loadConfig(const std::string& filename, Arguments& args) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл конфигурации: " << filename << std::endl;
        return false;
    }
    
    std::string line;
    std::string json;
    while (std::getline(file, line)) {
        json += line;
    }
    file.close();
    
    // Простой парсинг JSON (для упрощения, можно использовать библиотеку)
    size_t hostPos = json.find("\"host\"");
    if (hostPos != std::string::npos) {
        size_t start = json.find('"', hostPos + 7);
        size_t end = json.find('"', start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            args.host = json.substr(start + 1, end - start - 1);
        }
    }
    
    size_t portPos = json.find("\"port\"");
    if (portPos != std::string::npos) {
        size_t start = json.find_first_of("0123456789", portPos);
        size_t end = json.find_first_not_of("0123456789", start);
        if (start != std::string::npos) {
            args.port = std::stoi(json.substr(start, end - start));
        }
    }
    
    return true;
}

void printHelp() {
    std::cout << "Windows HDD Collector (C++)\n";
    std::cout << "Сбор информации о жестких дисках и отправка на сервер\n\n";
    std::cout << "Использование:\n";
    std::cout << "  hdd_collector.exe [опции]\n\n";
    std::cout << "Опции:\n";
    std::cout << "  --host HOST        Адрес сервера (по умолчанию: 10.3.3.7)\n";
    std::cout << "  --port PORT        Порт сервера (по умолчанию: 5000)\n";
    std::cout << "  --save FILE        Сохранить данные в JSON файл\n";
    std::cout << "  --config FILE      Путь к файлу конфигурации\n";
    std::cout << "  --help, -h         Показать эту справку\n\n";
    std::cout << "Примеры:\n";
    std::cout << "  hdd_collector.exe --host 192.168.1.100 --port 5000\n";
    std::cout << "  hdd_collector.exe --host server.example.com --save data.json\n";
    std::cout << "  hdd_collector.exe --config config.json\n";
}

bool isRunningAsAdmin() {
#ifdef _WIN32
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    
    return isAdmin == TRUE;
#else
    return false;
#endif
}

int main(int argc, char* argv[]) {
    // Настройка кодировки консоли для Windows
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    // Проверка прав администратора
    if (!isRunningAsAdmin()) {
        std::cerr << "ВНИМАНИЕ: Программа не запущена от имени администратора!" << std::endl;
        std::cerr << "S.M.A.R.T. данные могут быть недоступны." << std::endl;
        std::cerr << "Для сбора S.M.A.R.T. данных запустите программу от имени администратора." << std::endl;
    } else {
        std::cout << "Программа запущена от имени администратора." << std::endl;
    }
    
    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Ошибка инициализации Winsock" << std::endl;
        return 1;
    }
#endif
    
    Arguments args = parseArguments(argc, argv);
    
    if (args.help) {
        printHelp();
        return 0;
    }
    
    // Загрузка конфигурации из файла если указан
    if (!args.configFile.empty()) {
        loadConfig(args.configFile, args);
    }
    
    // Вывод заголовка
    std::cout << "============================================================" << std::endl;
    std::cout << "Сбор информации о жестких дисках" << std::endl;
    std::cout << "============================================================" << std::endl;
    
    // Создание коллектора
    HDDCollector collector(args.host, args.port);
    
    // Получаем сетевую информацию для комментариев
    // Комментарий: IP адрес машины, на которой находятся диски
    std::string localIP = collector.getLocalIPAddress();
    // Комментарий: MAC адрес сетевой карты
    std::string macAddress = collector.getMacAddress();
    
    // Выводим информацию в консоль
    std::cout << "// IP адрес машины: " << localIP << std::endl;
    std::cout << "// MAC адрес сетевой карты: " << macAddress << std::endl;
    
    // Сбор информации
    SystemInfo info = collector.collectAllInfo();
    
    // Вывод информации о найденных дисках
    std::cout << "\nНайдено дисков: " << info.disks.size() << std::endl;
    for (size_t i = 0; i < info.disks.size(); ++i) {
        const auto& disk = info.disks[i];
        std::cout << "\nДиск " << (i + 1) << ":" << std::endl;
        std::cout << "  Модель: " << (disk.model.empty() ? "N/A" : disk.model) << std::endl;
        std::cout << "  Серийный номер: " << (disk.serialNumber.empty() ? "N/A" : disk.serialNumber) << std::endl;
        std::cout << "  Объем: " << (disk.sizeGb > 0 ? std::to_string(disk.sizeGb) : "N/A") << " ГБ" << std::endl;
        if (disk.powerOnHours >= 0) {
            std::cout << "  Наработка: " << disk.powerOnHours << " часов" << std::endl;
        } else {
            std::cout << "  Наработка: N/A (S.M.A.R.T. данные не собраны)" << std::endl;
        }
        if (disk.powerOnCount >= 0) {
            std::cout << "  Включений: " << disk.powerOnCount << std::endl;
        } else {
            std::cout << "  Включений: N/A (S.M.A.R.T. данные не собраны)" << std::endl;
        }
        if (!disk.healthStatus.empty()) {
            std::cout << "  Здоровье: " << disk.healthStatus << std::endl;
        }
    }
    
    // Сохранение в файл если указано
    if (!args.saveFile.empty()) {
        collector.saveToFile(info, args.saveFile);
    }
    
    // Отправка на сервер (только если не указан --save)
    if (args.saveFile.empty()) {
        std::cout << "\nОтправка данных на сервер " << args.host << ":" << args.port << "..." << std::endl;
        if (collector.sendToServer(info)) {
            std::cout << "\n✓ Готово!" << std::endl;
#ifdef _WIN32
        WSACleanup();
#endif
            return 0;
        } else {
            std::cout << "\n✗ Ошибка при отправке данных" << std::endl;
#ifdef _WIN32
            WSACleanup();
#endif
            return 1;
        }
    } else {
        // Если указан --save, не отправляем на сервер
        std::cout << "\n✓ Данные собраны и сохранены в файл. Отправка на сервер пропущена." << std::endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }
}

