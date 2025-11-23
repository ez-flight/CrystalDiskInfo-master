#include "json_helper.h"
#include <sstream>
#include <iomanip>

std::string JsonHelper::toJson(const SystemInfo& info) {
    std::ostringstream oss;
    oss << "{\n";
    
    // hostname - необязательное поле согласно API документации
    if (!info.hostname.empty()) {
        oss << "  \"hostname\": \"" << escapeJsonString(info.hostname) << "\",\n";
    }
    
    // disks - обязательное поле
    oss << "  \"disks\": [\n";
    
    for (size_t i = 0; i < info.disks.size(); ++i) {
        oss << "    " << diskToJson(info.disks[i]);
        if (i < info.disks.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    
    oss << "  ]\n";
    oss << "}";
    
    return oss.str();
}

std::string JsonHelper::diskToJson(const DiskInfo& disk) {
    std::ostringstream oss;
    oss << "{\n";
    
    // Обязательные поля для новых дисков (согласно API документации)
    // serial_number - обязательно всегда
    oss << "      \"serial_number\": \"" << escapeJsonString(disk.serialNumber) << "\"";
    
    // model - обязательно для новых дисков
    if (!disk.model.empty()) {
        oss << ",\n      \"model\": \"" << escapeJsonString(disk.model) << "\"";
    }
    
    // size_gb - обязательно для новых дисков
    if (disk.sizeGb > 0) {
        oss << ",\n      \"size_gb\": " << disk.sizeGb;
    }
    
    // Необязательные поля (согласно API документации)
    if (!disk.mediaType.empty()) {
        oss << ",\n      \"media_type\": \"" << escapeJsonString(disk.mediaType) << "\"";
    }
    
    if (!disk.manufacturer.empty()) {
        oss << ",\n      \"manufacturer\": \"" << escapeJsonString(disk.manufacturer) << "\"";
    }
    
    if (!disk.interfaceType.empty()) {
        oss << ",\n      \"interface\": \"" << escapeJsonString(disk.interfaceType) << "\"";
    }
    
    // S.M.A.R.T. данные (необязательные, отправляем если собраны)
    // Отправляем power_on_hours если он был собран (>= 0 означает что значение установлено)
    if (disk.powerOnHours >= 0) {
        oss << ",\n      \"power_on_hours\": " << disk.powerOnHours;
    }
    
    if (disk.powerOnCount >= 0) {
        oss << ",\n      \"power_on_count\": " << disk.powerOnCount;
    }
    
    if (!disk.healthStatus.empty()) {
        oss << ",\n      \"health_status\": \"" << escapeJsonString(disk.healthStatus) << "\"";
    }
    
    oss << "\n    }";
    
    return oss.str();
}

std::string JsonHelper::escapeJsonString(const std::string& str) {
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

