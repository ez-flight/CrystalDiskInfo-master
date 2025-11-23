#include "smart_helper.h"
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32

// S.M.A.R.T. атрибуты ID
#define SMART_ATTR_POWER_ON_HOURS        9
#define SMART_ATTR_POWER_CYCLE_COUNT     12

// Структура для S.M.A.R.T. чтения через ATA
// Формат: 512 байт данных
// Байты 0-1: Revision (WORD)
// Байты 2+: Атрибуты SMART (каждый атрибут 12 байт)
#pragma pack(push, 1)
struct SmartAttribute {
    BYTE id;
    WORD flags;
    BYTE currentValue;
    BYTE worstValue;
    BYTE rawValue[6];
    BYTE reserved;
};
#pragma pack(pop)

// Комментарий: IP адрес машины, на которой находятся диски
// Комментарий: MAC адрес сетевой карты
HANDLE SmartHelper::openDisk(const std::string& devicePath) {
    // Преобразуем путь в формат Windows
    std::string winPath = "\\\\.\\" + devicePath;
    
    HANDLE hDevice = CreateFileA(
        winPath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        std::cerr << "  Ошибка открытия диска " << winPath << ": код " << error << std::endl;
        if (error == 5) {
            std::cerr << "    ERROR_ACCESS_DENIED - нет прав доступа" << std::endl;
        } else if (error == 2) {
            std::cerr << "    ERROR_FILE_NOT_FOUND - диск не найден" << std::endl;
        } else if (error == 87) {
            std::cerr << "    ERROR_INVALID_PARAMETER - неверный параметр" << std::endl;
        }
    }
    
    return hDevice;
}

bool SmartHelper::readSmartAttributes(HANDLE hDevice, DiskInfo& diskInfo) {
    if (hDevice == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Формируем структуру с буфером для ATA команды
    struct {
        ATA_PASS_THROUGH_EX apt;
        BYTE buffer[512];
    } aptWithBuffer;
    
    ZeroMemory(&aptWithBuffer, sizeof(aptWithBuffer));
    
    // Настройка ATA_PASS_THROUGH_EX структуры
    aptWithBuffer.apt.Length = sizeof(ATA_PASS_THROUGH_EX);
    aptWithBuffer.apt.AtaFlags = ATA_FLAGS_DATA_IN | ATA_FLAGS_DRDY_REQUIRED;
    aptWithBuffer.apt.DataTransferLength = 512;
    aptWithBuffer.apt.TimeOutValue = 10;
    aptWithBuffer.apt.DataBufferOffset = offsetof(decltype(aptWithBuffer), buffer);
    aptWithBuffer.apt.CurrentTaskFile[0] = 0;  // Features register (low)
    aptWithBuffer.apt.CurrentTaskFile[3] = 0;  // Features register (high)
    
    // ATA команда: SMART READ DATA (0xD0)
    // Features: 0x01 (SMART enable), Command: 0xD0 (Read SMART attributes)
    aptWithBuffer.apt.CurrentTaskFile[6] = 0xD0;  // Command: SMART READ DATA
    aptWithBuffer.apt.CurrentTaskFile[4] = 0x01;  // Features: SMART enable
    aptWithBuffer.apt.CurrentTaskFile[1] = 0x01;  // Sector count (1 sector = 512 bytes)
    aptWithBuffer.apt.CurrentTaskFile[2] = 0x01;  // Sector number
    aptWithBuffer.apt.CurrentTaskFile[7] = 0xA0;  // Device/Head register
    
    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_ATA_PASS_THROUGH,
        &aptWithBuffer,
        sizeof(aptWithBuffer),
        &aptWithBuffer,
        sizeof(aptWithBuffer),
        &bytesReturned,
        NULL
    );
    
    if (!result) {
        DWORD error = GetLastError();
        std::cerr << "  Ошибка DeviceIoControl IOCTL_ATA_PASS_THROUGH: код " << error << std::endl;
        if (error == 1) {
            std::cerr << "    ERROR_INVALID_FUNCTION - функция не поддерживается (возможно, SCSI/NVMe диск)" << std::endl;
        } else if (error == 87) {
            std::cerr << "    ERROR_INVALID_PARAMETER - неверный параметр" << std::endl;
        }
    }
    
    if (result && bytesReturned > 0) {
        // Парсим полученные данные
        BYTE* data = aptWithBuffer.buffer;
        
        // Проверяем ревизию (первые 2 байта)
        WORD revision = *reinterpret_cast<WORD*>(data);
        if (revision != 0x0001 && revision != 0x0002) {
            std::cerr << "  Неверная ревизия S.M.A.R.T. данных: 0x" << std::hex << revision << std::dec << std::endl;
            return false;
        }
        
        // Парсим атрибуты (начинаются с байта 2, каждый атрибут - 12 байт)
        for (int i = 0; i < 30; ++i) {
            BYTE* attrPtr = data + 2 + (i * 12);
            BYTE attrId = attrPtr[0];
            
            if (attrId == 0x00 || attrId == 0xFF) {
                continue;  // Пустой слот или конец списка
            }
            
            // Извлекаем rawValue (байты 5-10, где 5-8 это 4 байта значения)
            if (attrId == SMART_ATTR_POWER_ON_HOURS) {
                // Часы наработки (little-endian, первые 4 байта rawValue)
                unsigned int hours = 0;
                hours = static_cast<unsigned int>(attrPtr[5]) |
                        (static_cast<unsigned int>(attrPtr[6]) << 8) |
                        (static_cast<unsigned int>(attrPtr[7]) << 16) |
                        (static_cast<unsigned int>(attrPtr[8]) << 24);
                
                // Устанавливаем значение даже если оно 0 (если оно в разумном диапазоне)
                if (hours < 1000000) {  // Разрешаем 0 и положительные значения
                    diskInfo.powerOnHours = static_cast<int>(hours);
                    std::cerr << "  Найдено Power On Hours: " << hours << std::endl;
                }
            } else if (attrId == SMART_ATTR_POWER_CYCLE_COUNT) {
                // Количество включений
                unsigned int count = 0;
                count = static_cast<unsigned int>(attrPtr[5]) |
                        (static_cast<unsigned int>(attrPtr[6]) << 8) |
                        (static_cast<unsigned int>(attrPtr[7]) << 16) |
                        (static_cast<unsigned int>(attrPtr[8]) << 24);
                
                // Устанавливаем значение даже если оно 0 (если оно в разумном диапазоне)
                if (count < 1000000) {  // Разрешаем 0 и положительные значения
                    diskInfo.powerOnCount = static_cast<int>(count);
                    std::cerr << "  Найдено Power Cycle Count: " << count << std::endl;
                }
            }
        }
        
        return true;
    }
    
    return false;
}

bool SmartHelper::readSmartHealthStatus(HANDLE hDevice, DiskInfo& diskInfo) {
    if (hDevice == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Используем SMART RETURN STATUS команду (0xDA)
    ATA_PASS_THROUGH_EX apt = { 0 };
    apt.Length = sizeof(ATA_PASS_THROUGH_EX);
    apt.AtaFlags = ATA_FLAGS_DRDY_REQUIRED;
    apt.TimeOutValue = 10;
    
    // ATA команда: SMART RETURN STATUS (0xDA)
    apt.CurrentTaskFile[6] = 0xDA;  // Command: SMART RETURN STATUS
    apt.CurrentTaskFile[1] = 0x4F;  // Features register
    apt.CurrentTaskFile[2] = 0xC2;  // Sector number
    apt.CurrentTaskFile[7] = 0xA0;  // Device/Head register
    
    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_ATA_PASS_THROUGH,
        &apt,
        sizeof(apt),
        &apt,
        sizeof(apt),
        &bytesReturned,
        NULL
    );
    
    if (result) {
        // Проверяем Error register (CurrentTaskFile[0])
        // Если бит 0 = 0, то диск здоров
        // Если бит 0 = 1, то есть предупреждение
        BYTE errorReg = apt.CurrentTaskFile[0];
        BYTE statusReg = apt.CurrentTaskFile[7];
        
        if ((errorReg & 0x01) == 0 && (statusReg & 0x02) != 0) {
            // Нет ошибок, диск готов - здоров
            diskInfo.healthStatus = "Здоров";
        } else {
            // Есть предупреждение или ошибка
            diskInfo.healthStatus = "Тревога";
        }
        return true;
    }
    
    return false;
}

bool SmartHelper::getSmartData(const std::string& devicePath, DiskInfo& diskInfo) {
    HANDLE hDevice = openDisk(devicePath);
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        // Ошибка открытия диска (возможно, нет прав или диск не поддерживается)
        // ERROR_ACCESS_DENIED (5) - нет прав администратора
        // ERROR_INVALID_PARAMETER (87) - неправильный параметр
        // ERROR_FILE_NOT_FOUND (2) - диск не найден
        return false;
    }
    
    bool success = false;
    
    // Пытаемся читать статус здоровья
    readSmartHealthStatus(hDevice, diskInfo);
    
    // Пытаемся читать атрибуты SMART
    if (readSmartAttributes(hDevice, diskInfo)) {
        success = true;
    }
    
    CloseHandle(hDevice);
    
    return success;
}

#else
// Не Windows платформы
bool SmartHelper::getSmartData(const std::string& devicePath, DiskInfo& diskInfo) {
    (void)devicePath;
    (void)diskInfo;
    return false;
}

HANDLE SmartHelper::openDisk(const std::string& devicePath) {
    (void)devicePath;
    return INVALID_HANDLE_VALUE;
}

bool SmartHelper::readSmartAttributes(HANDLE hDevice, DiskInfo& diskInfo) {
    (void)hDevice;
    (void)diskInfo;
    return false;
}

bool SmartHelper::readSmartHealthStatus(HANDLE hDevice, DiskInfo& diskInfo) {
    (void)hDevice;
    (void)diskInfo;
    return false;
}
#endif

