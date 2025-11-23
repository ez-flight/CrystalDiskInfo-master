#pragma once

#include "hdd_collector.h"
#include <string>

class JsonHelper {
public:
    static std::string toJson(const SystemInfo& info);
    
private:
    static std::string escapeJsonString(const std::string& str);
    static std::string diskToJson(const DiskInfo& disk);
};

