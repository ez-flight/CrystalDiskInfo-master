#include "wmi_helper.h"
#include <iostream>
#include <sstream>

WMIHelper::WMIHelper() : pLoc_(nullptr), pSvc_(nullptr), initialized_(false) {
}

WMIHelper::~WMIHelper() {
    cleanup();
}

void WMIHelper::cleanup() {
    if (pSvc_) {
        pSvc_->Release();
        pSvc_ = nullptr;
    }
    if (pLoc_) {
        pLoc_->Release();
        pLoc_ = nullptr;
    }
    CoUninitialize();
    initialized_ = false;
}

bool WMIHelper::initialize() {
    if (initialized_) {
        return true;
    }
    
    HRESULT hres;
    
    // Инициализация COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        std::cerr << "Не удалось инициализировать COM. Ошибка: 0x" 
                  << std::hex << hres << std::endl;
        return false;
    }
    
    // Инициализация безопасности COM
    hres = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_NONE,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL
    );
    
    // Получение локальной WMI
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc_
    );
    
    if (FAILED(hres)) {
        std::cerr << "Не удалось создать IWbemLocator. Ошибка: 0x" 
                  << std::hex << hres << std::endl;
        CoUninitialize();
        return false;
    }
    
    // Подключение к WMI через локальную систему
    hres = pLoc_->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        &pSvc_
    );
    
    if (FAILED(hres)) {
        std::cerr << "Не удалось подключиться к WMI. Ошибка: 0x" 
                  << std::hex << hres << std::endl;
        pLoc_->Release();
        CoUninitialize();
        return false;
    }
    
    // Установка уровней безопасности
    hres = CoSetProxyBlanket(
        pSvc_,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );
    
    if (FAILED(hres)) {
        std::cerr << "Не удалось установить уровни безопасности. Ошибка: 0x" 
                  << std::hex << hres << std::endl;
        pSvc_->Release();
        pLoc_->Release();
        CoUninitialize();
        return false;
    }
    
    initialized_ = true;
    return true;
}

std::vector<DiskInfo> WMIHelper::getDiskDrives() {
    std::vector<DiskInfo> disks;
    deviceIds_.clear();  // Очищаем кэш DeviceID
    
    if (!initialized_ && !initialize()) {
        return disks;
    }
    
    // Запрос Win32_DiskDrive
    IEnumWbemClassObject* pEnumerator = nullptr;
    HRESULT hres = pSvc_->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_DiskDrive"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );
    
    if (FAILED(hres)) {
        std::cerr << "Не удалось выполнить запрос к WMI. Ошибка: 0x" 
                  << std::hex << hres << std::endl;
        return disks;
    }
    
    IWbemClassObject* pclsObj = nullptr;
    ULONG uReturn = 0;
    
    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        
        if (0 == uReturn) {
            break;
        }
        
        DiskInfo disk;
        
        // Получаем DeviceID (для S.M.A.R.T. данных)
        std::string deviceId = getWmiProperty(pclsObj, L"DeviceID");
        deviceIds_.push_back(deviceId);
        
        // Получаем свойства диска
        disk.model = getWmiProperty(pclsObj, L"Model");
        disk.serialNumber = getWmiProperty(pclsObj, L"SerialNumber");
        disk.sizeGb = getWmiPropertyInt64(pclsObj, L"Size") / (1024ULL * 1024ULL * 1024ULL);
        disk.interfaceType = getWmiProperty(pclsObj, L"InterfaceType");
        disk.mediaType = getWmiProperty(pclsObj, L"MediaType");
        disk.manufacturer = getWmiProperty(pclsObj, L"Manufacturer");
        
        // Удаляем пробелы
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\n\r"));
            s.erase(s.find_last_not_of(" \t\n\r") + 1);
        };
        trim(disk.model);
        trim(disk.serialNumber);
        trim(disk.manufacturer);
        
        disks.push_back(disk);
        
        pclsObj->Release();
    }
    
    pEnumerator->Release();
    
    return disks;
}

std::string WMIHelper::getDeviceId(int index) {
    if (index >= 0 && index < static_cast<int>(deviceIds_.size())) {
        return deviceIds_[index];
    }
    return "";
}

std::string WMIHelper::getWmiProperty(IWbemClassObject* pclsObj, const wchar_t* propertyName) {
    VARIANT vtProp;
    VariantInit(&vtProp);
    
    HRESULT hres = pclsObj->Get(propertyName, 0, &vtProp, 0, 0);
    
    if (SUCCEEDED(hres) && vtProp.vt != VT_NULL && vtProp.vt != VT_EMPTY) {
        std::string result;
        
        if (vtProp.vt == VT_BSTR) {
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, vtProp.bstrVal, -1, NULL, 0, NULL, NULL);
            if (size_needed > 0) {
                result.resize(size_needed - 1);
                WideCharToMultiByte(CP_UTF8, 0, vtProp.bstrVal, -1, &result[0], size_needed, NULL, NULL);
            }
        } else {
            // Преобразуем другие типы в строку
            VARIANT vtConverted;
            VariantInit(&vtConverted);
            VariantChangeType(&vtConverted, &vtProp, 0, VT_BSTR);
            if (vtConverted.vt == VT_BSTR) {
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, vtConverted.bstrVal, -1, NULL, 0, NULL, NULL);
                if (size_needed > 0) {
                    result.resize(size_needed - 1);
                    WideCharToMultiByte(CP_UTF8, 0, vtConverted.bstrVal, -1, &result[0], size_needed, NULL, NULL);
                }
            }
            VariantClear(&vtConverted);
        }
        
        VariantClear(&vtProp);
        return result;
    }
    
    VariantClear(&vtProp);
    return "";
}

int64_t WMIHelper::getWmiPropertyInt64(IWbemClassObject* pclsObj, const wchar_t* propertyName) {
    VARIANT vtProp;
    VariantInit(&vtProp);
    
    HRESULT hres = pclsObj->Get(propertyName, 0, &vtProp, 0, 0);
    
    if (SUCCEEDED(hres) && vtProp.vt != VT_NULL && vtProp.vt != VT_EMPTY) {
        int64_t result = 0;
        
        if (vtProp.vt == VT_I8 || vtProp.vt == VT_UI8) {
            result = vtProp.llVal;
        } else if (vtProp.vt == VT_I4 || vtProp.vt == VT_UI4) {
            result = static_cast<int64_t>(vtProp.lVal);
        } else if (vtProp.vt == VT_BSTR) {
            result = _wtoi64(vtProp.bstrVal);
        }
        
        VariantClear(&vtProp);
        return result;
    }
    
    VariantClear(&vtProp);
    return 0;
}

