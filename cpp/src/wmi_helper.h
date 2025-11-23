#pragma once

#include "hdd_collector.h"
#include <vector>
#include <string>
#include <comdef.h>
#include <Wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")

class WMIHelper {
public:
    WMIHelper();
    ~WMIHelper();
    
    bool initialize();
    std::vector<DiskInfo> getDiskDrives();
    
    // Получить DeviceID для диска по индексу (для S.M.A.R.T. данных)
    std::string getDeviceId(int index);
    
private:
    IWbemLocator* pLoc_;
    IWbemServices* pSvc_;
    bool initialized_;
    std::vector<std::string> deviceIds_;  // Кэш DeviceID для дисков
    
    void cleanup();
    std::string getWmiProperty(IWbemClassObject* pclsObj, const wchar_t* propertyName);
    int64_t getWmiPropertyInt64(IWbemClassObject* pclsObj, const wchar_t* propertyName);
};

