#pragma once

#include "hdd_collector.h"

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>
#include <ntdddisk.h>
#endif

class SmartHelper {
public:
    // Получение S.M.A.R.T. данных напрямую через Windows API
    // Не требует внешних утилит (smartmontools)
    static bool getSmartData(const std::string& devicePath, DiskInfo& diskInfo);
    
private:
    // Открытие диска
    static HANDLE openDisk(const std::string& devicePath);
    
    // Чтение S.M.A.R.T. атрибутов (Power On Hours, Power Cycle Count)
    static bool readSmartAttributes(HANDLE hDevice, DiskInfo& diskInfo);
    
    // Чтение здоровья диска (SMART health status)
    static bool readSmartHealthStatus(HANDLE hDevice, DiskInfo& diskInfo);
};
