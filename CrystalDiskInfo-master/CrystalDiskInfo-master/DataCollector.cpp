/*---------------------------------------------------------------------------*/
//       Author : hiyohiyo (modified for server data transmission)
//         Mail : hiyohiyo@crystalmark.info
//          Web : https://crystalmark.info/
//      License : MIT License
/*---------------------------------------------------------------------------*/
#include "stdafx.h"
#include "DataCollector.h"
#include <comdef.h>
#include <Wbemidl.h>
#include <iphlpapi.h>
#include <dxgi.h>
#include <ws2tcpip.h>
#include <map>
#include <time.h>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "dxgi.lib")

// Конструктор и деструктор
CDataCollector::CDataCollector() {
}

CDataCollector::~CDataCollector() {
}

// Вспомогательные методы
CString CDataCollector::EscapeJsonString(const CString& str) {
    CString result;
    result.Preallocate(str.GetLength() * 2); // Резервируем память
    
    for (int i = 0; i < str.GetLength(); i++) {
        TCHAR c = str[i];
        switch (c) {
            case _T('\"'): result += _T("\\\""); break;
            case _T('\\'): result += _T("\\\\"); break;
            case _T('/'): result += _T("\\/"); break;
            case _T('\b'): result += _T("\\b"); break;
            case _T('\f'): result += _T("\\f"); break;
            case _T('\n'): result += _T("\\n"); break;
            case _T('\r'): result += _T("\\r"); break;
            case _T('\t'): result += _T("\\t"); break;
            default:
                if (c >= 0 && c < 32) {
                    CString hex;
                    hex.Format(_T("\\u%04X"), (unsigned int)c);
                    result += hex;
                } else {
                    result += c;
                }
                break;
        }
    }
    return result;
}

CString CDataCollector::GetHostname() {
    CString result;
    TCHAR hostname[256];
    DWORD size = sizeof(hostname) / sizeof(TCHAR);
    
    if (GetComputerName(hostname, &size)) {
        result = hostname;
    } else {
        // Fallback: используем gethostname через WMI Win32_ComputerSystem
        IWbemLocator* pIWbemLocator = NULL;
        IWbemServices* pIWbemServices = NULL;
        IEnumWbemClassObject* pEnumCOMDevs = NULL;
        IWbemClassObject* pCOMDev = NULL;
        ULONG uReturned = 0;
        
        try {
            if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                IID_IWbemLocator, (LPVOID*)&pIWbemLocator))) {
                if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                    NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServices))) {
                    if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                        NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                        if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(_T("WQL")),
                            _bstr_t(_T("Select Name from Win32_ComputerSystem")), 
                            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs))) {
                            if (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1) {
                                VARIANT pVal;
                                VariantInit(&pVal);
                                if (pCOMDev->Get(L"Name", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                    result = pVal.bstrVal;
                                    VariantClear(&pVal);
                                }
                            }
                        }
                    }
                }
            }
        } catch (...) {
        }
        
        if (pCOMDev) pCOMDev->Release();
        if (pEnumCOMDevs) pEnumCOMDevs->Release();
        if (pIWbemServices) pIWbemServices->Release();
        if (pIWbemLocator) pIWbemLocator->Release();
    }
    
    if (result.IsEmpty()) {
        result = _T("UNKNOWN");
    }
    
    return result;
}

CString CDataCollector::GetCurrentTimestamp() {
    CString result;
    time_t now = time(NULL);
    struct tm timeinfo;
    
    if (gmtime_s(&timeinfo, &now) == 0) {
        result.Format(_T("%04d-%02d-%02dT%02d:%02d:%02dZ"),
            timeinfo.tm_year + 1900,
            timeinfo.tm_mon + 1,
            timeinfo.tm_mday,
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec);
    }
    
    return result;
}

CString CDataCollector::GetLocalIPAddress() {
    CString result;
    ULONG bufferSize = 0;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    
    // Получаем размер буфера
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &bufferSize) == ERROR_BUFFER_OVERFLOW) {
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);
        if (pAddresses) {
            if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &bufferSize) == NO_ERROR) {
                PIP_ADAPTER_ADDRESSES pCurrentAddress = pAddresses;
                
                while (pCurrentAddress) {
                    // Ищем первый активный IPv4 или IPv6 адрес (не loopback)
                    if (pCurrentAddress->OperStatus == IfOperStatusUp) {
                        PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrentAddress->FirstUnicastAddress;
                        while (pUnicast) {
                            if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                                // IPv4 адрес
                                sockaddr_in* pSockAddr = (sockaddr_in*)pUnicast->Address.lpSockaddr;
                                unsigned char* bytes = (unsigned char*)&(pSockAddr->sin_addr.S_un.S_addr);
                                CString ipStr;
                                ipStr.Format(_T("%d.%d.%d.%d"), bytes[0], bytes[1], bytes[2], bytes[3]);
                                result = ipStr;
                                
                                free(pAddresses);
                                return result;
                            }
                            pUnicast = pUnicast->Next;
                        }
                    }
                    pCurrentAddress = pCurrentAddress->Next;
                }
            }
            free(pAddresses);
        }
    }
    
    return result;
}

CString CDataCollector::GetMacAddress() {
    CString result;
    ULONG bufferSize = 0;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    
    // Получаем размер буфера
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &bufferSize) == ERROR_BUFFER_OVERFLOW) {
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);
        if (pAddresses) {
            if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &bufferSize) == NO_ERROR) {
                PIP_ADAPTER_ADDRESSES pCurrentAddress = pAddresses;
                
                while (pCurrentAddress) {
                    // Ищем первый активный адаптер (не loopback, не туннельный)
                    if (pCurrentAddress->OperStatus == IfOperStatusUp &&
                        pCurrentAddress->IfType != IF_TYPE_SOFTWARE_LOOPBACK &&
                        pCurrentAddress->IfType != IF_TYPE_TUNNEL &&
                        pCurrentAddress->PhysicalAddressLength == 6) { // MAC адрес должен быть 6 байт
                        
                        // Форматируем MAC адрес
                        CString macStr;
                        for (ULONG i = 0; i < pCurrentAddress->PhysicalAddressLength; i++) {
                            if (i > 0) macStr += _T(":");
                            CString hex;
                            hex.Format(_T("%02X"), (unsigned char)pCurrentAddress->PhysicalAddress[i]);
                            macStr += hex;
                        }
                        
                        free(pAddresses);
                        return macStr;
                    }
                    pCurrentAddress = pCurrentAddress->Next;
                }
            }
            free(pAddresses);
        }
    }
    
    return result;
}

// WMI методы для получения информации о системе
CString CDataCollector::GetOSName() {
    CString result;
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    IEnumWbemClassObject* pEnumCOMDevs = NULL;
    IWbemClassObject* pCOMDev = NULL;
    ULONG uReturned = 0;
    
    try {
        if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pIWbemLocator))) {
            if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServices))) {
                if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                    NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                    if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(_T("WQL")),
                        _bstr_t(_T("Select Caption from Win32_OperatingSystem")), 
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs))) {
                        if (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1) {
                            VARIANT pVal;
                            VariantInit(&pVal);
                            if (pCOMDev->Get(L"Caption", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                result = pVal.bstrVal;
                                result.Trim();
                                
                                // Нормализуем имя ОС
                                if (result.Find(_T("Microsoft")) != -1) {
                                    result.Replace(_T("Microsoft "), _T(""));
                                }
                                if (result.Find(_T("Windows")) != -1) {
                                    result = _T("Windows");
                                }
                                
                                VariantClear(&pVal);
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
    }
    
    if (pCOMDev) pCOMDev->Release();
    if (pEnumCOMDevs) pEnumCOMDevs->Release();
    if (pIWbemServices) pIWbemServices->Release();
    if (pIWbemLocator) pIWbemLocator->Release();
    
    return result;
}

CString CDataCollector::GetOSVersion() {
    CString result;
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    IEnumWbemClassObject* pEnumCOMDevs = NULL;
    IWbemClassObject* pCOMDev = NULL;
    ULONG uReturned = 0;
    
    try {
        if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pIWbemLocator))) {
            if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServices))) {
                if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                    NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                    if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(_T("WQL")),
                        _bstr_t(_T("Select Version from Win32_OperatingSystem")), 
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs))) {
                        if (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1) {
                            VARIANT pVal;
                            VariantInit(&pVal);
                            if (pCOMDev->Get(L"Version", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                result = pVal.bstrVal;
                                result.Trim();
                                
                                // Извлекаем основную версию (10 или 11)
                                int dotPos = result.Find(_T('.'));
                                if (dotPos > 0) {
                                    result = result.Left(dotPos);
                                }
                                
                                VariantClear(&pVal);
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
    }
    
    if (pCOMDev) pCOMDev->Release();
    if (pEnumCOMDevs) pEnumCOMDevs->Release();
    if (pIWbemServices) pIWbemServices->Release();
    if (pIWbemLocator) pIWbemLocator->Release();
    
    return result;
}

CString CDataCollector::GetOSBuild() {
    CString result;
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    IEnumWbemClassObject* pEnumCOMDevs = NULL;
    IWbemClassObject* pCOMDev = NULL;
    ULONG uReturned = 0;
    
    try {
        if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pIWbemLocator))) {
            if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServices))) {
                if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                    NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                    if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(_T("WQL")),
                        _bstr_t(_T("Select BuildNumber from Win32_OperatingSystem")), 
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs))) {
                        if (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1) {
                            VARIANT pVal;
                            VariantInit(&pVal);
                            if (pCOMDev->Get(L"BuildNumber", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                result = pVal.bstrVal;
                                result.Trim();
                                VariantClear(&pVal);
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
    }
    
    if (pCOMDev) pCOMDev->Release();
    if (pEnumCOMDevs) pEnumCOMDevs->Release();
    if (pIWbemServices) pIWbemServices->Release();
    if (pIWbemLocator) pIWbemLocator->Release();
    
    return result;
}

CString CDataCollector::GetOSEdition() {
    CString result;
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    IEnumWbemClassObject* pEnumCOMDevs = NULL;
    IWbemClassObject* pCOMDev = NULL;
    ULONG uReturned = 0;
    
    try {
        if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pIWbemLocator))) {
            if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServices))) {
                if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                    NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                    if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(_T("WQL")),
                        _bstr_t(_T("Select Caption from Win32_OperatingSystem")), 
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs))) {
                        if (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1) {
                            VARIANT pVal;
                            VariantInit(&pVal);
                            if (pCOMDev->Get(L"Caption", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                CString caption = pVal.bstrVal;
                                caption.Trim();
                                
                                // Извлекаем издание
                                if (caption.Find(_T("Pro")) != -1) {
                                    result = _T("Pro");
                                } else if (caption.Find(_T("Home")) != -1) {
                                    result = _T("Home");
                                } else if (caption.Find(_T("Enterprise")) != -1) {
                                    result = _T("Enterprise");
                                } else if (caption.Find(_T("Education")) != -1) {
                                    result = _T("Education");
                                } else if (caption.Find(_T("Server")) != -1) {
                                    result = _T("Server");
                                }
                                
                                VariantClear(&pVal);
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
    }
    
    if (pCOMDev) pCOMDev->Release();
    if (pEnumCOMDevs) pEnumCOMDevs->Release();
    if (pIWbemServices) pIWbemServices->Release();
    if (pIWbemLocator) pIWbemLocator->Release();
    
    return result;
}

CString CDataCollector::GetOSArchitecture() {
    CString result;
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    IEnumWbemClassObject* pEnumCOMDevs = NULL;
    IWbemClassObject* pCOMDev = NULL;
    ULONG uReturned = 0;
    
    try {
        if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pIWbemLocator))) {
            if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServices))) {
                if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                    NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                    if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(_T("WQL")),
                        _bstr_t(_T("Select OSArchitecture from Win32_OperatingSystem")), 
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs))) {
                        if (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1) {
                            VARIANT pVal;
                            VariantInit(&pVal);
                            if (pCOMDev->Get(L"OSArchitecture", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                result = pVal.bstrVal;
                                result.Trim();
                                
                                // Нормализуем архитектуру
                                if (result.Find(_T("64")) != -1) {
                                    result = _T("x64");
                                } else if (result.Find(_T("32")) != -1) {
                                    result = _T("x86");
                                } else if (result.Find(_T("ARM")) != -1) {
                                    result = _T("ARM64");
                                }
                                
                                VariantClear(&pVal);
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
    }
    
    if (pCOMDev) pCOMDev->Release();
    if (pEnumCOMDevs) pEnumCOMDevs->Release();
    if (pIWbemServices) pIWbemServices->Release();
    if (pIWbemLocator) pIWbemLocator->Release();
    
    return result;
}

CString CDataCollector::GetProcessor() {
    CString result;
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    IEnumWbemClassObject* pEnumCOMDevs = NULL;
    IWbemClassObject* pCOMDev = NULL;
    ULONG uReturned = 0;
    
    try {
        if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pIWbemLocator))) {
            if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServices))) {
                if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                    NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                    if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(_T("WQL")),
                        _bstr_t(_T("Select Name from Win32_Processor")), 
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs))) {
                        if (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1) {
                            VARIANT pVal;
                            VariantInit(&pVal);
                            if (pCOMDev->Get(L"Name", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                result = pVal.bstrVal;
                                result.Trim();
                                
                                // Нормализуем имя процессора: удаляем символы торговых марок
                                result.Replace(_T("(R)"), _T(""));
                                result.Replace(_T("(TM)"), _T(""));
                                result.Replace(_T("(tm)"), _T(""));
                                result.Replace(_T("(r)"), _T(""));
                                result.Replace(_T("®"), _T(""));
                                result.Replace(_T("™"), _T(""));
                                
                                // Удаляем двойные пробелы после удаления символов
                                result.Replace(_T("  "), _T(" "));
                                result.Trim();
                                
                                VariantClear(&pVal);
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
    }
    
    if (pCOMDev) pCOMDev->Release();
    if (pEnumCOMDevs) pEnumCOMDevs->Release();
    if (pIWbemServices) pIWbemServices->Release();
    if (pIWbemLocator) pIWbemLocator->Release();
    
    return result;
}

int CDataCollector::GetMemoryGB() {
    int result = 0;
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    IEnumWbemClassObject* pEnumCOMDevs = NULL;
    IWbemClassObject* pCOMDev = NULL;
    ULONG uReturned = 0;
    
    try {
        if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pIWbemLocator))) {
            if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServices))) {
                if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                    NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                    if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(_T("WQL")),
                        _bstr_t(_T("Select * from Win32_ComputerSystem")), 
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs))) {
                        if (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1) {
                            VARIANT pVal;
                            VariantInit(&pVal);
                            if (pCOMDev->Get(L"TotalPhysicalMemory", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                ULONGLONG totalMemory = 0;
                                // TotalPhysicalMemory обычно возвращается как VT_BSTR или VT_UI8
                                if (pVal.vt == VT_BSTR) {
                                    totalMemory = _wtoll(pVal.bstrVal);
                                } else if (pVal.vt == VT_UI8) {
                                    totalMemory = pVal.ullVal;
                                } else if (pVal.vt == VT_I8) {
                                    totalMemory = (ULONGLONG)pVal.llVal;
                                } else if (pVal.vt == VT_UI4) {
                                    totalMemory = (ULONGLONG)pVal.ulVal;
                                } else if (pVal.vt == VT_I4) {
                                    totalMemory = (ULONGLONG)pVal.lVal;
                                }
                                
                                // Конвертируем байты в ГБ с правильным округлением
                                // Используем округление для точности (не просто отбрасывание дробной части)
                                if (totalMemory > 0) {
                                    double memoryGB = (double)totalMemory / (1024.0 * 1024.0 * 1024.0);
                                    result = (int)(memoryGB + 0.5); // Округление до ближайшего целого
                                }
                                VariantClear(&pVal);
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
    }
    
    if (pCOMDev) pCOMDev->Release();
    if (pEnumCOMDevs) pEnumCOMDevs->Release();
    if (pIWbemServices) pIWbemServices->Release();
    if (pIWbemLocator) pIWbemLocator->Release();
    
    return result;
}

CString CDataCollector::GetMotherboard() {
    CString result;
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    IEnumWbemClassObject* pEnumCOMDevs = NULL;
    IWbemClassObject* pCOMDev = NULL;
    ULONG uReturned = 0;
    
    try {
        if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pIWbemLocator))) {
            if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServices))) {
                if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                    NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                    if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(_T("WQL")),
                        _bstr_t(_T("Select * from Win32_BaseBoard")), 
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs))) {
                        if (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1) {
                            CString manufacturer;
                            CString product;
                            VARIANT pVal;
                            
                            VariantInit(&pVal);
                            if (pCOMDev->Get(L"Manufacturer", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                manufacturer = pVal.bstrVal;
                                manufacturer.Trim();
                                VariantClear(&pVal);
                            }
                            
                            VariantInit(&pVal);
                            if (pCOMDev->Get(L"Product", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                product = pVal.bstrVal;
                                product.Trim();
                                VariantClear(&pVal);
                            }
                            
                            if (!manufacturer.IsEmpty() && !product.IsEmpty()) {
                                result = manufacturer + _T(" ") + product;
                            } else if (!manufacturer.IsEmpty()) {
                                result = manufacturer;
                            } else if (!product.IsEmpty()) {
                                result = product;
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
    }
    
    if (pCOMDev) pCOMDev->Release();
    if (pEnumCOMDevs) pEnumCOMDevs->Release();
    if (pIWbemServices) pIWbemServices->Release();
    if (pIWbemLocator) pIWbemLocator->Release();
    
    return result;
}

CString CDataCollector::GetBIOSVersion() {
    CString result;
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    IEnumWbemClassObject* pEnumCOMDevs = NULL;
    IWbemClassObject* pCOMDev = NULL;
    ULONG uReturned = 0;
    
    try {
        if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pIWbemLocator))) {
            if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServices))) {
                if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                    NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                    if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(_T("WQL")),
                        _bstr_t(_T("Select * from Win32_BIOS")), 
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs))) {
                        if (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1) {
                            VARIANT pVal;
                            VariantInit(&pVal);
                            
                            // Приоритет 1: Version (обычно содержит более полную информацию, например "ALASKA - 1072009")
                            if (pCOMDev->Get(L"Version", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                result = pVal.bstrVal;
                                result.Trim();
                                VariantClear(&pVal);
                            } else {
                                VariantClear(&pVal);
                                // Приоритет 2: SMBIOSBIOSVersion (например "4.6.5")
                                if (pCOMDev->Get(L"SMBIOSBIOSVersion", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                    result = pVal.bstrVal;
                                    result.Trim();
                                    VariantClear(&pVal);
                                } else {
                                    VariantClear(&pVal);
                                    // Приоритет 3: Name
                                    if (pCOMDev->Get(L"Name", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                        result = pVal.bstrVal;
                                        result.Trim();
                                        VariantClear(&pVal);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
    }
    
    if (pCOMDev) pCOMDev->Release();
    if (pEnumCOMDevs) pEnumCOMDevs->Release();
    if (pIWbemServices) pIWbemServices->Release();
    if (pIWbemLocator) pIWbemLocator->Release();
    
    return result;
}

CString CDataCollector::GetDomain() {
    CString result;
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    IEnumWbemClassObject* pEnumCOMDevs = NULL;
    IWbemClassObject* pCOMDev = NULL;
    ULONG uReturned = 0;
    
    try {
        if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pIWbemLocator))) {
            if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServices))) {
                if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                    NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                    if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(_T("WQL")),
                        _bstr_t(_T("Select Domain from Win32_ComputerSystem")), 
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs))) {
                        if (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1) {
                            VARIANT pVal;
                            VariantInit(&pVal);
                            if (pCOMDev->Get(L"Domain", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                result = pVal.bstrVal;
                                result.Trim();
                                VariantClear(&pVal);
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
    }
    
    if (pCOMDev) pCOMDev->Release();
    if (pEnumCOMDevs) pEnumCOMDevs->Release();
    if (pIWbemServices) pIWbemServices->Release();
    if (pIWbemLocator) pIWbemLocator->Release();
    
    return result;
}

CString CDataCollector::GetComputerRole() {
    CString result;
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    IEnumWbemClassObject* pEnumCOMDevs = NULL;
    IWbemClassObject* pCOMDev = NULL;
    ULONG uReturned = 0;
    
    try {
        if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pIWbemLocator))) {
            if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServices))) {
                if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                    NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                    if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(_T("WQL")),
                        _bstr_t(_T("Select DomainRole from Win32_ComputerSystem")), 
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs))) {
                        if (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1) {
                            VARIANT pVal;
                            VariantInit(&pVal);
                            if (pCOMDev->Get(L"DomainRole", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                int domainRole = 0;
                                if (pVal.vt == VT_I2 || pVal.vt == VT_UI2) {
                                    domainRole = pVal.iVal;
                                } else if (pVal.vt == VT_I4 || pVal.vt == VT_UI4) {
                                    domainRole = pVal.intVal;
                                } else if (pVal.vt == VT_BSTR) {
                                    domainRole = _ttoi(pVal.bstrVal);
                                }
                                
                                // Нормализуем роль
                                // 0 = Standalone Workstation, 1 = Member Workstation
                                // 2 = Standalone Server, 3 = Member Server
                                // 4 = Backup Domain Controller, 5 = Primary Domain Controller
                                if (domainRole == 4 || domainRole == 5) {
                                    result = _T("DOMAIN_CONTROLLER");
                                } else if (domainRole == 2 || domainRole == 3) {
                                    result = _T("SERVER");
                                } else {
                                    result = _T("WORKSTATION");
                                }
                                
                                VariantClear(&pVal);
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
    }
    
    if (pCOMDev) pCOMDev->Release();
    if (pEnumCOMDevs) pEnumCOMDevs->Release();
    if (pIWbemServices) pIWbemServices->Release();
    if (pIWbemLocator) pIWbemLocator->Release();
    
    return result;
}

// Преобразование данных диска в JSON
CString CDataCollector::DiskToJson(CAtaSmart::ATA_SMART_INFO& asi, int index) {
    CString json;
    json = _T("    {\n");
    
    bool firstField = true;
    
    // serial_number (обязательное поле)
    CString serialNumber = asi.SerialNumber;
    serialNumber.Trim();
    if (!serialNumber.IsEmpty()) {
        json += _T("      \"serial_number\": \"");
        json += EscapeJsonString(serialNumber);
        json += _T("\"");
        firstField = false;
    } else {
        // Если серийный номер пустой, пропускаем диск
        return _T("");
    }
    
    // model
    CString model = asi.Model;
    model.Trim();
    if (!model.IsEmpty()) {
        if (!firstField) json += _T(",");
        json += _T("\n      \"model\": \"");
        json += EscapeJsonString(model);
        json += _T("\"");
        firstField = false;
    }
    
    // size_gb
    int sizeGb = 0;
    if (asi.TotalDiskSize > 0) {
        // TotalDiskSize в МБ, конвертируем в ГБ
        sizeGb = (int)((asi.TotalDiskSize / 1024.0) + 0.5); // Округление
    } else if (asi.DiskSizeLba48 > 0) {
        // Используем LBA48
        ULONGLONG sectors = asi.DiskSizeLba48;
        DWORD sectorSize = asi.LogicalSectorSize > 0 ? asi.LogicalSectorSize : 512;
        ULONGLONG bytes = sectors * sectorSize;
        sizeGb = (int)((bytes / (1024.0 * 1024.0 * 1024.0)) + 0.5);
    } else if (asi.DiskSizeLba28 > 0) {
        // Используем LBA28
        ULONGLONG sectors = asi.DiskSizeLba28;
        DWORD sectorSize = asi.LogicalSectorSize > 0 ? asi.LogicalSectorSize : 512;
        ULONGLONG bytes = sectors * sectorSize;
        sizeGb = (int)((bytes / (1024.0 * 1024.0 * 1024.0)) + 0.5);
    }
    
    if (sizeGb > 0) {
        if (!firstField) json += _T(",");
        CString sizeStr;
        sizeStr.Format(_T("%d"), sizeGb);
        json += _T("\n      \"size_gb\": ");
        json += sizeStr;
        firstField = false;
    }
    
    // media_type
    CString mediaType;
    if (asi.IsSsd) {
        mediaType = _T("SSD");
    } else if (asi.InterfaceType == CAtaSmart::INTERFACE_TYPE_NVME) {
        mediaType = _T("NVMe");
    } else {
        mediaType = _T("HDD");
    }
    
    // Дополнительная проверка по модели
    CString modelUpper = model;
    modelUpper.MakeUpper();
    if (modelUpper.Find(_T("NVME")) != -1 || modelUpper.Find(_T("NVME")) != -1) {
        mediaType = _T("NVMe");
    } else if (modelUpper.Find(_T("SSD")) != -1 || modelUpper.Find(_T("SOLID")) != -1) {
        mediaType = _T("SSD");
    }
    
    if (!mediaType.IsEmpty()) {
        if (!firstField) json += _T(",");
        json += _T("\n      \"media_type\": \"");
        json += EscapeJsonString(mediaType);
        json += _T("\"");
        firstField = false;
    }
    
    // manufacturer (только если определен)
    CString manufacturer;
    CString modelUpper2 = model;
    modelUpper2.MakeUpper();
    if (modelUpper2.Find(_T("WESTERN DIGITAL")) != -1 || modelUpper2.Find(_T("WDC")) != -1 || modelUpper2.Find(_T("WD ")) != -1) {
        manufacturer = _T("Western Digital");
    } else if (modelUpper2.Find(_T("SEAGATE")) != -1 || modelUpper2.Find(_T("ST")) == 0) {
        manufacturer = _T("Seagate");
    } else if (modelUpper2.Find(_T("TOSHIBA")) != -1) {
        manufacturer = _T("Toshiba");
    } else if (modelUpper2.Find(_T("HP ")) != -1 || modelUpper2.Find(_T("HEWLETT")) != -1) {
        manufacturer = _T("HP");
    } else if (modelUpper2.Find(_T("SAMSUNG")) != -1) {
        manufacturer = _T("Samsung");
    } else if (modelUpper2.Find(_T("INTEL")) != -1) {
        manufacturer = _T("Intel");
    } else if (modelUpper2.Find(_T("KINGSTON")) != -1) {
        manufacturer = _T("Kingston");
    } else if (modelUpper2.Find(_T("SANDISK")) != -1) {
        manufacturer = _T("SanDisk");
    } else if (modelUpper2.Find(_T("ADATA")) != -1) {
        manufacturer = _T("ADATA");
    } else if (modelUpper2.Find(_T("CRUCIAL")) != -1) {
        manufacturer = _T("Crucial");
    } else if (modelUpper2.Find(_T("CORSAIR")) != -1) {
        manufacturer = _T("Corsair");
    }
    
    // manufacturer отправляется только если определен
    if (!manufacturer.IsEmpty()) {
        if (!firstField) json += _T(",");
        json += _T("\n      \"manufacturer\": \"");
        json += EscapeJsonString(manufacturer);
        json += _T("\"");
        firstField = false;
    }
    
    // interface
    CString interfaceType;
    if (asi.InterfaceType == CAtaSmart::INTERFACE_TYPE_NVME) {
        interfaceType = _T("NVMe");
    } else if (asi.InterfaceType == CAtaSmart::INTERFACE_TYPE_SATA) {
        interfaceType = _T("SATA");
    } else if (asi.InterfaceType == CAtaSmart::INTERFACE_TYPE_PATA) {
        interfaceType = _T("PATA");
    } else if (asi.InterfaceType == CAtaSmart::INTERFACE_TYPE_USB) {
        interfaceType = _T("USB");
    } else if (asi.InterfaceType == CAtaSmart::INTERFACE_TYPE_SCSI) {
        interfaceType = _T("SCSI");
    }
    
    if (!interfaceType.IsEmpty()) {
        if (!firstField) json += _T(",");
        json += _T("\n      \"interface\": \"");
        json += EscapeJsonString(interfaceType);
        json += _T("\"");
        firstField = false;
    }
    
    // power_on_hours
    int powerOnHours = -1;
    if (asi.MeasuredPowerOnHours > 0) {
        powerOnHours = asi.MeasuredPowerOnHours;
    } else if (asi.DetectedPowerOnHours > 0) {
        powerOnHours = asi.DetectedPowerOnHours;
    }
    
    if (powerOnHours >= 0) {
        if (!firstField) json += _T(",");
        CString hoursStr;
        hoursStr.Format(_T("%d"), powerOnHours);
        json += _T("\n      \"power_on_hours\": ");
        json += hoursStr;
        firstField = false;
    }
    
    // power_on_count
    if (asi.PowerOnCount >= 0) {
        if (!firstField) json += _T(",");
        CString countStr;
        countStr.Format(_T("%d"), asi.PowerOnCount);
        json += _T("\n      \"power_on_count\": ");
        json += countStr;
        firstField = false;
    }
    
    // health_status
    CString healthStatus;
    if (asi.DiskStatus == CAtaSmart::DISK_STATUS_GOOD) {
        healthStatus = _T("Good");
    } else if (asi.DiskStatus == CAtaSmart::DISK_STATUS_CAUTION) {
        healthStatus = _T("Caution");
    } else if (asi.DiskStatus == CAtaSmart::DISK_STATUS_BAD) {
        healthStatus = _T("Bad");
    } else {
        healthStatus = _T("Unknown");
    }
    
    if (!healthStatus.IsEmpty()) {
        if (!firstField) json += _T(",");
        json += _T("\n      \"health_status\": \"");
        json += EscapeJsonString(healthStatus);
        json += _T("\"");
        firstField = false;
    }
    
    json += _T("\n    }");
    
    return json;
}

// Основной метод сбора данных в JSON формат (API v2)
CString CDataCollector::CollectDiskData(CAtaSmart& ata, const CString& comment) {
    CString json;
    json = _T("{\n");
    
    // machine объект
    json += _T("  \"machine\": {\n");
    
    // hostname
    CString hostname = GetHostname();
    json += _T("    \"hostname\": \"");
    json += EscapeJsonString(hostname);
    json += _T("\"");
    
    // ip_address
    CString ipAddress = GetLocalIPAddress();
    if (!ipAddress.IsEmpty()) {
        json += _T(",\n    \"ip_address\": \"");
        json += EscapeJsonString(ipAddress);
        json += _T("\"");
    }
    
    // mac_address
    CString macAddress = GetMacAddress();
    if (!macAddress.IsEmpty()) {
        json += _T(",\n    \"mac_address\": \"");
        json += EscapeJsonString(macAddress);
        json += _T("\"");
    }
    
    // os объект
    CString osName = GetOSName();
    CString osVersion = GetOSVersion();
    CString osBuild = GetOSBuild();
    CString osEdition = GetOSEdition();
    CString osArchitecture = GetOSArchitecture();
    
    if (!osName.IsEmpty() || !osVersion.IsEmpty() || !osBuild.IsEmpty() || !osEdition.IsEmpty() || !osArchitecture.IsEmpty()) {
        json += _T(",\n    \"os\": {\n");
        bool firstOsField = true;
        
        if (!osName.IsEmpty()) {
            json += _T("      \"name\": \"");
            json += EscapeJsonString(osName);
            json += _T("\"");
            firstOsField = false;
        }
        
        if (!osVersion.IsEmpty()) {
            if (!firstOsField) json += _T(",");
            json += _T("\n      \"version\": \"");
            json += EscapeJsonString(osVersion);
            json += _T("\"");
            firstOsField = false;
        }
        
        if (!osBuild.IsEmpty()) {
            if (!firstOsField) json += _T(",");
            json += _T("\n      \"build\": \"");
            json += EscapeJsonString(osBuild);
            json += _T("\"");
            firstOsField = false;
        }
        
        if (!osEdition.IsEmpty()) {
            if (!firstOsField) json += _T(",");
            json += _T("\n      \"edition\": \"");
            json += EscapeJsonString(osEdition);
            json += _T("\"");
            firstOsField = false;
        }
        
        if (!osArchitecture.IsEmpty()) {
            if (!firstOsField) json += _T(",");
            json += _T("\n      \"architecture\": \"");
            json += EscapeJsonString(osArchitecture);
            json += _T("\"");
            firstOsField = false;
        }
        
        json += _T("\n    }");
    }
    
    // hardware объект
    CString processor = GetProcessor();
    int memoryGB = GetMemoryGB();
    CString motherboard = GetMotherboard();
    CString biosVersion = GetBIOSVersion();
    
    if (!processor.IsEmpty() || memoryGB > 0 || !motherboard.IsEmpty() || !biosVersion.IsEmpty()) {
        json += _T(",\n    \"hardware\": {\n");
        bool firstHwField = true;
        
        if (!processor.IsEmpty()) {
            json += _T("      \"processor\": \"");
            json += EscapeJsonString(processor);
            json += _T("\"");
            firstHwField = false;
        }
        
        if (memoryGB > 0) {
            if (!firstHwField) json += _T(",");
            CString memStr;
            memStr.Format(_T("%d"), memoryGB);
            json += _T("\n      \"memory_gb\": ");
            json += memStr;
            firstHwField = false;
        }
        
        if (!motherboard.IsEmpty()) {
            if (!firstHwField) json += _T(",");
            json += _T("\n      \"motherboard\": \"");
            json += EscapeJsonString(motherboard);
            json += _T("\"");
            firstHwField = false;
        }
        
        if (!biosVersion.IsEmpty()) {
            if (!firstHwField) json += _T(",");
            json += _T("\n      \"bios_version\": \"");
            json += EscapeJsonString(biosVersion);
            json += _T("\"");
            firstHwField = false;
        }
        
        json += _T("\n    }");
    }
    
    // network объект
    CString domain = GetDomain();
    CString computerRole = GetComputerRole();
    
    if (!domain.IsEmpty() || !computerRole.IsEmpty()) {
        json += _T(",\n    \"network\": {\n");
        bool firstNetField = true;
        
        if (!domain.IsEmpty()) {
            json += _T("      \"domain\": \"");
            json += EscapeJsonString(domain);
            json += _T("\"");
            firstNetField = false;
        }
        
        if (!computerRole.IsEmpty()) {
            if (!firstNetField) json += _T(",");
            json += _T("\n      \"computer_role\": \"");
            json += EscapeJsonString(computerRole);
            json += _T("\"");
            firstNetField = false;
        }
        
        json += _T("\n    }");
    }
    
    json += _T("\n  },\n");
    
    // collection_info объект
    json += _T("  \"collection_info\": {\n");
    json += _T("    \"timestamp\": \"");
    json += GetCurrentTimestamp();
    json += _T("\",\n");
    json += _T("    \"collector_version\": \"1.0.0\",\n");
    json += _T("    \"collector_type\": \"CrystalDiskInfo\"");
    
    if (!comment.IsEmpty()) {
        json += _T(",\n    \"comment\": \"");
        json += EscapeJsonString(comment);
        json += _T("\"");
    }
    
    json += _T("\n  },\n");
    
    // graphics_cards массив (на корневом уровне согласно API v2)
    CString graphicsInfo = GetGraphicsInfo();
    if (!graphicsInfo.IsEmpty()) {
        json += _T("  \"graphics_cards\": [\n");
        json += graphicsInfo;
        json += _T("\n  ],\n");
    }
    
    // memory_modules массив (на корневом уровне согласно API v2)
    CString memoryModulesInfo = GetMemoryModulesInfo();
    if (!memoryModulesInfo.IsEmpty()) {
        json += _T("  \"memory_modules\": [\n");
        json += memoryModulesInfo;
        json += _T("\n  ],\n");
    }
    
    // disks массив
    json += _T("  \"disks\": [\n");
    
    bool firstDisk = true;
    for (int i = 0; i < (int)ata.vars.GetCount(); i++) {
        CAtaSmart::ATA_SMART_INFO& asi = ata.vars[i];
        CString diskJson = DiskToJson(asi, i);
        
        if (!diskJson.IsEmpty()) {
            if (!firstDisk) {
                json += _T(",\n");
            }
            json += diskJson;
            firstDisk = false;
        }
    }
    
    json += _T("\n  ]\n");
    json += _T("}\n");
    
    return json;
}

// Получение информации о видеокартах
CString CDataCollector::GetGraphicsInfo() {
    CString result;
    std::vector<CString> graphicsList;
    
    // Сначала получаем данные через DXGI для более точной информации о памяти
    std::map<CString, ULONGLONG> dxgiMemoryMap;
    
    IDXGIFactory* pFactory = NULL;
    if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory))) {
        IDXGIAdapter* pAdapter = NULL;
        UINT adapterIndex = 0;
        
        while (pFactory->EnumAdapters(adapterIndex, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
            DXGI_ADAPTER_DESC adapterDesc;
            if (SUCCEEDED(pAdapter->GetDesc(&adapterDesc))) {
                CString gpuName = adapterDesc.Description;
                gpuName.Trim();
                
                // Используем DedicatedVideoMemory (выделенная видеопамять)
                ULONGLONG dedicatedMemory = adapterDesc.DedicatedVideoMemory;
                if (dedicatedMemory > 0 && !gpuName.IsEmpty()) {
                    dxgiMemoryMap[gpuName] = dedicatedMemory;
                }
            }
            pAdapter->Release();
            adapterIndex++;
        }
        pFactory->Release();
    }
    
    // Теперь получаем данные через WMI
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    IEnumWbemClassObject* pEnumCOMDevs = NULL;
    IWbemClassObject* pCOMDev = NULL;
    ULONG uReturned = 0;
    
    try {
        if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pIWbemLocator))) {
            if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServices))) {
                if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                    NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                    if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(_T("WQL")),
                        _bstr_t(_T("Select * from Win32_VideoController")), 
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs))) {
                        
                        while (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1) {
                            VARIANT pVal;
                            VariantInit(&pVal);
                            
                            CString name, manufacturer, memoryType, serialNumber;
                            ULONGLONG memoryBytes = 0;
                            
                            // Name (модель)
                            if (pCOMDev->Get(L"Name", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                name = pVal.bstrVal;
                                name.Trim();
                                VariantClear(&pVal);
                            }
                            
                            if (name.IsEmpty()) {
                                pCOMDev->Release();
                                pCOMDev = NULL;
                                continue;
                            }
                            
                            // Manufacturer
                            if (pCOMDev->Get(L"AdapterCompatibility", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                manufacturer = pVal.bstrVal;
                                manufacturer.Trim();
                                VariantClear(&pVal);
                            }
                            
                            // Memory (из WMI)
                            if (pCOMDev->Get(L"AdapterRAM", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                if (pVal.vt == VT_I4 || pVal.vt == VT_UI4) {
                                    memoryBytes = (ULONGLONG)pVal.ulVal;
                                } else if (pVal.vt == VT_I8 || pVal.vt == VT_UI8) {
                                    memoryBytes = pVal.ullVal;
                                } else if (pVal.vt == VT_BSTR) {
                                    memoryBytes = _wtoll(pVal.bstrVal);
                                }
                                VariantClear(&pVal);
                            }
                            
                            // Используем данные из DXGI если доступны (более точные)
                            if (dxgiMemoryMap.find(name) != dxgiMemoryMap.end()) {
                                memoryBytes = dxgiMemoryMap[name];
                            }
                            
                            // MemoryType - определяется эвристически
                            CString nameUpper = name;
                            nameUpper.MakeUpper();
                            if (nameUpper.Find(_T("RTX 30")) != -1 || nameUpper.Find(_T("RTX 40")) != -1) {
                                if (nameUpper.Find(_T("3090")) != -1 || nameUpper.Find(_T("4090")) != -1) {
                                    memoryType = _T("GDDR6X");
                                } else {
                                    memoryType = _T("GDDR6");
                                }
                            } else if (nameUpper.Find(_T("RTX 20")) != -1) {
                                memoryType = _T("GDDR6");
                            } else if (nameUpper.Find(_T("GTX 16")) != -1 || nameUpper.Find(_T("GTX 10")) != -1) {
                                if (nameUpper.Find(_T("1650")) != -1 || nameUpper.Find(_T("1660")) != -1) {
                                    memoryType = _T("GDDR6");
                                } else {
                                    memoryType = _T("GDDR5");
                                }
                            } else if (nameUpper.Find(_T("RX 6")) != -1 || nameUpper.Find(_T("RX 7")) != -1) {
                                memoryType = _T("GDDR6");
                            } else if (nameUpper.Find(_T("RADEON")) != -1 && nameUpper.Find(_T("RX")) != -1) {
                                memoryType = _T("GDDR5");
                            } else {
                                // По умолчанию пытаемся определить
                                if (nameUpper.Find(_T("GDDR6X")) != -1) {
                                    memoryType = _T("GDDR6X");
                                } else if (nameUpper.Find(_T("GDDR6")) != -1) {
                                    memoryType = _T("GDDR6");
                                } else if (nameUpper.Find(_T("GDDR5")) != -1) {
                                    memoryType = _T("GDDR5");
                                } else {
                                    memoryType = _T("Shared"); // Для интегрированных
                                }
                            }
                            
                            // SerialNumber - получаем из PnPDeviceID через Win32_PnPEntity
                            IWbemServices* pIWbemServicesPnp = NULL;
                            IEnumWbemClassObject* pEnumPnp = NULL;
                            IWbemClassObject* pPnpDev = NULL;
                            
                            if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                                NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServicesPnp))) {
                                if (SUCCEEDED(CoSetProxyBlanket(pIWbemServicesPnp, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                                    NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                                    CString query;
                                    query.Format(_T("Select PNPDeviceID from Win32_PnPEntity where Name = '%s'"), name);
                                    
                                    if (SUCCEEDED(pIWbemServicesPnp->ExecQuery(_bstr_t(_T("WQL")),
                                        _bstr_t(query), 
                                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumPnp))) {
                                        if (pEnumPnp && SUCCEEDED(pEnumPnp->Next(10000, 1, &pPnpDev, &uReturned)) && uReturned == 1) {
                                            VARIANT pValPnp;
                                            VariantInit(&pValPnp);
                                            if (pPnpDev->Get(L"PNPDeviceID", 0L, &pValPnp, NULL, NULL) == WBEM_S_NO_ERROR && pValPnp.vt > VT_NULL) {
                                                // PNPDeviceID может содержать серийный номер, но обычно это Hardware ID
                                                // Не используем его как серийный номер, только если это реальный серийный номер
                                                VariantClear(&pValPnp);
                                            }
                                            pPnpDev->Release();
                                        }
                                        pEnumPnp->Release();
                                    }
                                }
                                pIWbemServicesPnp->Release();
                            }
                            
                            // Формируем JSON для видеокарты
                            CString gpuJson;
                            gpuJson = _T("    {\n");
                            bool firstGpuField = true;
                            
                            // model (обязательное)
                            gpuJson += _T("      \"model\": \"");
                            gpuJson += EscapeJsonString(name);
                            gpuJson += _T("\"");
                            firstGpuField = false;
                            
                            // manufacturer
                            if (!manufacturer.IsEmpty()) {
                                if (!firstGpuField) gpuJson += _T(",");
                                gpuJson += _T("\n      \"manufacturer\": \"");
                                gpuJson += EscapeJsonString(manufacturer);
                                gpuJson += _T("\"");
                                firstGpuField = false;
                            }
                            
                            // memory_size в МБ
                            if (memoryBytes > 0) {
                                int ramMB = (int)(memoryBytes / (1024 * 1024));
                                if (ramMB > 0) {
                                    if (!firstGpuField) gpuJson += _T(",");
                                    CString memStr;
                                    memStr.Format(_T("%d"), ramMB);
                                    gpuJson += _T("\n      \"memory_size\": ");
                                    gpuJson += memStr;
                                    firstGpuField = false;
                                }
                            }
                            
                            // memory_type - определяется эвристически по модели видеокарты
                            if (!memoryType.IsEmpty()) {
                                gpuJson += _T(",\n      \"memory_type\": \"");
                                gpuJson += EscapeJsonString(memoryType);
                                gpuJson += _T("\"");
                                firstGpuField = false;
                            }
                            
                            // serial_number - пропускаем, так как Hardware ID не является уникальным серийным номером
                            
                            gpuJson += _T("\n    }");
                            graphicsList.push_back(gpuJson);
                            
                            pCOMDev->Release();
                            pCOMDev = NULL;
                        }
                    }
                }
            }
        }
    } catch (...) {
    }
    
    if (pCOMDev) pCOMDev->Release();
    if (pEnumCOMDevs) pEnumCOMDevs->Release();
    if (pIWbemServices) pIWbemServices->Release();
    if (pIWbemLocator) pIWbemLocator->Release();
    
    // Объединяем все видеокарты в массив JSON
    for (size_t i = 0; i < graphicsList.size(); i++) {
        if (i > 0) {
            result += _T(",\n");
        }
        result += graphicsList[i];
    }
    
    return result;
}

CString CDataCollector::GetMemoryModulesInfo() {
    CString result;
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    IEnumWbemClassObject* pEnumCOMDevs = NULL;
    IWbemClassObject* pCOMDev = NULL;
    ULONG uReturned = 0;
    
    std::vector<CString> memoryModulesList;
    
    try {
        if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pIWbemLocator))) {
            if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(_T("root\\cimv2")),
                NULL, NULL, 0L, 0, NULL, NULL, &pIWbemServices))) {
                if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                    NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE))) {
                    if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(_T("WQL")),
                        _bstr_t(_T("Select * from Win32_PhysicalMemory")), 
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs))) {
                        
                        while (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1) {
                            VARIANT pVal;
                            VariantInit(&pVal);
                            
                            CString capacityGB, memoryType, speed, manufacturer, partNumber, serialNumber, bankLabel, deviceLocator;
                            
                            // Capacity (объем в байтах) -> конвертируем в ГБ
                            if (pCOMDev->Get(L"Capacity", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                ULONGLONG capacityBytes = 0;
                                if (pVal.vt == VT_BSTR) {
                                    capacityBytes = _wtoll(pVal.bstrVal);
                                } else if (pVal.vt == VT_UI8) {
                                    capacityBytes = pVal.ullVal;
                                } else if (pVal.vt == VT_I8) {
                                    capacityBytes = (ULONGLONG)pVal.llVal;
                                } else if (pVal.vt == VT_UI4) {
                                    capacityBytes = (ULONGLONG)pVal.ulVal;
                                } else if (pVal.vt == VT_I4) {
                                    capacityBytes = (ULONGLONG)pVal.lVal;
                                }
                                
                                if (capacityBytes > 0) {
                                    double memGB = (double)capacityBytes / (1024.0 * 1024.0 * 1024.0);
                                    int memGBInt = (int)(memGB + 0.5); // Округление
                                    capacityGB.Format(_T("%d"), memGBInt);
                                }
                                VariantClear(&pVal);
                            }
                            
                            // MemoryType (тип памяти: 20=DDR, 21=DDR2, 24=DDR3, 26=DDR4, 34=DDR5)
                            if (pCOMDev->Get(L"MemoryType", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                int memTypeCode = 0;
                                if (pVal.vt == VT_I2 || pVal.vt == VT_UI2) {
                                    memTypeCode = pVal.iVal;
                                } else if (pVal.vt == VT_I4 || pVal.vt == VT_UI4) {
                                    memTypeCode = pVal.intVal;
                                } else if (pVal.vt == VT_BSTR) {
                                    memTypeCode = _ttoi(pVal.bstrVal);
                                }
                                
                                // Преобразуем код в строку
                                switch (memTypeCode) {
                                    case 20: memoryType = _T("DDR"); break;
                                    case 21: memoryType = _T("DDR2"); break;
                                    case 24: memoryType = _T("DDR3"); break;
                                    case 26: memoryType = _T("DDR4"); break;
                                    case 34: memoryType = _T("DDR5"); break;
                                    default:
                                        if (memTypeCode > 0) {
                                            memoryType.Format(_T("Unknown (%d)"), memTypeCode);
                                        }
                                        break;
                                }
                                VariantClear(&pVal);
                            }
                            
                            // Speed (частота в МГц) - пробуем несколько источников
                            int speedMHz = 0;
                            
                            // Приоритет 1: Speed (текущая частота работы)
                            if (pCOMDev->Get(L"Speed", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                if (pVal.vt == VT_I2 || pVal.vt == VT_UI2) {
                                    speedMHz = pVal.iVal;
                                } else if (pVal.vt == VT_I4 || pVal.vt == VT_UI4) {
                                    speedMHz = pVal.intVal;
                                } else if (pVal.vt == VT_BSTR) {
                                    speedMHz = _ttoi(pVal.bstrVal);
                                }
                                VariantClear(&pVal);
                            }
                            
                            // Приоритет 2: ConfiguredClockSpeed (сконфигурированная частота, если Speed не доступна)
                            if (speedMHz <= 0) {
                                if (pCOMDev->Get(L"ConfiguredClockSpeed", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                    if (pVal.vt == VT_I2 || pVal.vt == VT_UI2) {
                                        speedMHz = pVal.iVal;
                                    } else if (pVal.vt == VT_I4 || pVal.vt == VT_UI4) {
                                        speedMHz = pVal.intVal;
                                    } else if (pVal.vt == VT_BSTR) {
                                        speedMHz = _ttoi(pVal.bstrVal);
                                    }
                                    VariantClear(&pVal);
                                }
                            }
                            
                            // Приоритет 3: ConfiguredMemoryClockSpeed (для DDR5)
                            if (speedMHz <= 0) {
                                if (pCOMDev->Get(L"ConfiguredMemoryClockSpeed", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                    if (pVal.vt == VT_I2 || pVal.vt == VT_UI2) {
                                        speedMHz = pVal.iVal;
                                    } else if (pVal.vt == VT_I4 || pVal.vt == VT_UI4) {
                                        speedMHz = pVal.intVal;
                                    } else if (pVal.vt == VT_BSTR) {
                                        speedMHz = _ttoi(pVal.bstrVal);
                                    }
                                    VariantClear(&pVal);
                                }
                            }
                            
                            // Записываем частоту только если она больше 0
                            if (speedMHz > 0) {
                                speed.Format(_T("%d"), speedMHz);
                            }
                            
                            // Manufacturer (производитель)
                            if (pCOMDev->Get(L"Manufacturer", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                manufacturer = pVal.bstrVal;
                                manufacturer.Trim();
                                VariantClear(&pVal);
                            }
                            
                            // PartNumber (номер партии/модель)
                            if (pCOMDev->Get(L"PartNumber", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                partNumber = pVal.bstrVal;
                                partNumber.Trim();
                                VariantClear(&pVal);
                            }
                            
                            // SerialNumber (серийный номер)
                            if (pCOMDev->Get(L"SerialNumber", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                serialNumber = pVal.bstrVal;
                                serialNumber.Trim();
                                VariantClear(&pVal);
                            }
                            
                            // BankLabel (расположение слота)
                            if (pCOMDev->Get(L"BankLabel", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                bankLabel = pVal.bstrVal;
                                bankLabel.Trim();
                                VariantClear(&pVal);
                            }
                            
                            // DeviceLocator (расположение устройства)
                            if (pCOMDev->Get(L"DeviceLocator", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL) {
                                deviceLocator = pVal.bstrVal;
                                deviceLocator.Trim();
                                VariantClear(&pVal);
                            }
                            
                            // Формируем JSON объект для модуля памяти
                            CString moduleJson;
                            bool firstField = true;
                            
                            // capacity_gb (обязательное поле)
                            if (!capacityGB.IsEmpty()) {
                                moduleJson = _T("        {\n");
                                moduleJson += _T("          \"capacity_gb\": ");
                                moduleJson += capacityGB;
                                firstField = false;
                            } else {
                                pCOMDev->Release();
                                pCOMDev = NULL;
                                continue; // Пропускаем модули без объема
                            }
                            
                            // memory_type
                            if (!memoryType.IsEmpty()) {
                                if (!firstField) moduleJson += _T(",");
                                moduleJson += _T("\n          \"memory_type\": \"");
                                moduleJson += EscapeJsonString(memoryType);
                                moduleJson += _T("\"");
                                firstField = false;
                            }
                            
                            // speed_mhz
                            if (!speed.IsEmpty()) {
                                if (!firstField) moduleJson += _T(",");
                                moduleJson += _T("\n          \"speed_mhz\": ");
                                moduleJson += speed;
                                firstField = false;
                            }
                            
                            // manufacturer
                            if (!manufacturer.IsEmpty()) {
                                if (!firstField) moduleJson += _T(",");
                                moduleJson += _T("\n          \"manufacturer\": \"");
                                moduleJson += EscapeJsonString(manufacturer);
                                moduleJson += _T("\"");
                                firstField = false;
                            }
                            
                            // part_number
                            if (!partNumber.IsEmpty()) {
                                if (!firstField) moduleJson += _T(",");
                                moduleJson += _T("\n          \"part_number\": \"");
                                moduleJson += EscapeJsonString(partNumber);
                                moduleJson += _T("\"");
                                firstField = false;
                            }
                            
                            // serial_number
                            if (!serialNumber.IsEmpty()) {
                                if (!firstField) moduleJson += _T(",");
                                moduleJson += _T("\n          \"serial_number\": \"");
                                moduleJson += EscapeJsonString(serialNumber);
                                moduleJson += _T("\"");
                                firstField = false;
                            }
                            
                            // location (BankLabel или DeviceLocator)
                            CString location;
                            if (!bankLabel.IsEmpty()) {
                                location = bankLabel;
                            } else if (!deviceLocator.IsEmpty()) {
                                location = deviceLocator;
                            }
                            if (!location.IsEmpty()) {
                                if (!firstField) moduleJson += _T(",");
                                moduleJson += _T("\n          \"location\": \"");
                                moduleJson += EscapeJsonString(location);
                                moduleJson += _T("\"");
                                firstField = false;
                            }
                            
                            moduleJson += _T("\n        }");
                            
                            memoryModulesList.push_back(moduleJson);
                            
                            pCOMDev->Release();
                            pCOMDev = NULL;
                        }
                    }
                }
            }
        }
    } catch (...) {
    }
    
    if (pCOMDev) pCOMDev->Release();
    if (pEnumCOMDevs) pEnumCOMDevs->Release();
    if (pIWbemServices) pIWbemServices->Release();
    if (pIWbemLocator) pIWbemLocator->Release();
    
    // Объединяем все модули памяти в массив JSON
    for (size_t i = 0; i < memoryModulesList.size(); i++) {
        if (i > 0) {
            result += _T(",\n");
        }
        result += memoryModulesList[i];
    }
    
    return result;
}

// Парсинг JSON ответа от API
CString CDataCollector::ParseApiResponse(const CString& jsonResponse) {
    CString result;
    
    // Простой парсинг JSON ответа
    // Формат: {"processed": 3, "total": 3, "new": 1, "updated": 2, "error_count": 0, "errors": []}
    
    int processed = 0, total = 0, newCount = 0, updated = 0, errorCount = 0;
    
    // Извлекаем значения из JSON
    int pos = jsonResponse.Find(_T("\"processed\""));
    if (pos != -1) {
        pos = jsonResponse.Find(_T(":"), pos);
        if (pos != -1) {
            processed = _ttoi(jsonResponse.Mid(pos + 1).TrimLeft());
        }
    }
    
    pos = jsonResponse.Find(_T("\"total\""));
    if (pos != -1) {
        pos = jsonResponse.Find(_T(":"), pos);
        if (pos != -1) {
            total = _ttoi(jsonResponse.Mid(pos + 1).TrimLeft());
        }
    }
    
    pos = jsonResponse.Find(_T("\"new\""));
    if (pos != -1) {
        pos = jsonResponse.Find(_T(":"), pos);
        if (pos != -1) {
            newCount = _ttoi(jsonResponse.Mid(pos + 1).TrimLeft());
        }
    }
    
    pos = jsonResponse.Find(_T("\"updated\""));
    if (pos != -1) {
        pos = jsonResponse.Find(_T(":"), pos);
        if (pos != -1) {
            updated = _ttoi(jsonResponse.Mid(pos + 1).TrimLeft());
        }
    }
    
    pos = jsonResponse.Find(_T("\"error_count\""));
    if (pos != -1) {
        pos = jsonResponse.Find(_T(":"), pos);
        if (pos != -1) {
            errorCount = _ttoi(jsonResponse.Mid(pos + 1).TrimLeft());
        }
    }
    
    // Форматируем сообщение
    result.Format(_T("Processed: %d/%d, New: %d, Updated: %d"), processed, total, newCount, updated);
    
    if (errorCount > 0) {
        CString errorMsg;
        errorMsg.Format(_T(", Errors: %d"), errorCount);
        result += errorMsg;
        
        // Извлекаем ошибки
        pos = jsonResponse.Find(_T("\"errors\""));
        if (pos != -1) {
            int startBracket = jsonResponse.Find(_T("["), pos);
            int endBracket = jsonResponse.Find(_T("]"), startBracket);
            if (startBracket != -1 && endBracket != -1) {
                CString errors = jsonResponse.Mid(startBracket, endBracket - startBracket + 1);
                result += _T("\nErrors: ");
                result += errors;
            }
        }
    }
    
    return result;
}

// Отправка данных на сервер (с указанием сервера)
bool CDataCollector::SendToServer(const CString& serverHost, int serverPort, CAtaSmart& ata, CString& response, const CString& comment) {
    CString jsonData = CollectDiskData(ata, comment);
    
    // Сохраняем JSON в файл перед отправкой
    CString exePath;
    GetModuleFileName(NULL, exePath.GetBuffer(MAX_PATH), MAX_PATH);
    exePath.ReleaseBuffer();
    int lastSlash = exePath.ReverseFind(_T('\\'));
    if (lastSlash != -1) {
        exePath = exePath.Left(lastSlash + 1);
    }
    CString jsonFilePath = exePath + _T("hdd_collect_last.json");
    
    // Сохраняем JSON в файл (перезапись)
    FILE* pFile = NULL;
    _tfopen_s(&pFile, jsonFilePath, _T("wb"));
    if (pFile) {
        // Конвертируем в UTF-8 для сохранения
        int len = WideCharToMultiByte(CP_UTF8, 0, jsonData, -1, NULL, 0, NULL, NULL);
        if (len > 0) {
            char* utf8Data = new char[len];
            WideCharToMultiByte(CP_UTF8, 0, jsonData, -1, utf8Data, len, NULL, NULL);
            fwrite(utf8Data, 1, len - 1, pFile); // len - 1 чтобы не записывать null terminator
            delete[] utf8Data;
        }
        fclose(pFile);
    }
    
    // Формируем URL
    CString url;
    url.Format(_T("http://%s:%d/api/hdd_collect/v2"), serverHost, serverPort);
    
    // Отправляем данные
    int statusCode = 0;
    CString httpResponse;
    bool success = httpClient.Post(url, jsonData, httpResponse, statusCode);
    
    if (success && statusCode == 200) {
        response = ParseApiResponse(httpResponse);
        return true;
    } else {
        if (statusCode > 0) {
            response.Format(_T("HTTP Error %d: %s"), statusCode, (LPCTSTR)httpResponse);
        } else {
            response = _T("Failed to send data to server");
        }
        return false;
    }
}

// Отправка данных на сервер (читает настройки из ini файла)
bool CDataCollector::SendToServer(const CString& iniPath, CAtaSmart& ata, CString& response, const CString& comment) {
    ServerInfo server;
    if (GetDefaultServer(iniPath, server)) {
        return SendToServer(server.host, server.port, ata, response, comment);
    }
    
    // Fallback на старый формат
    TCHAR serverHost[256];
    GetPrivateProfileString(_T("Server"), _T("Host"), _T("10.3.3.7"), serverHost, 256, iniPath);
    int serverPort = GetPrivateProfileInt(_T("Server"), _T("Port"), 5000, iniPath);
    
    return SendToServer(CString(serverHost), serverPort, ata, response, comment);
}

// Сохранение данных в JSON файл
bool CDataCollector::SaveToFile(const CString& filename, CAtaSmart& ata) {
    CString jsonData = CollectDiskData(ata, _T(""));
    
    FILE* pFile = NULL;
    _tfopen_s(&pFile, filename, _T("wb"));
    if (pFile) {
        // Конвертируем в UTF-8 для сохранения
        int len = WideCharToMultiByte(CP_UTF8, 0, jsonData, -1, NULL, 0, NULL, NULL);
        if (len > 0) {
            char* utf8Data = new char[len];
            WideCharToMultiByte(CP_UTF8, 0, jsonData, -1, utf8Data, len, NULL, NULL);
            fwrite(utf8Data, 1, len - 1, pFile);
            delete[] utf8Data;
        }
        fclose(pFile);
        return true;
    }
    
    return false;
}

// Статические методы для работы с серверами
int CDataCollector::GetServers(const CString& iniPath, std::vector<ServerInfo>& servers) {
    servers.clear();
    
    // Читаем серверы из INI файла
    int index = 1;
    while (true) {
        CString sectionName;
        sectionName.Format(_T("Server_%d"), index);
        
        TCHAR name[256];
        TCHAR host[256];
        int port = 5000;
        int isDefault = 0;
        
        GetPrivateProfileString(sectionName, _T("Name"), _T(""), name, 256, iniPath);
        GetPrivateProfileString(sectionName, _T("Host"), _T(""), host, 256, iniPath);
        port = GetPrivateProfileInt(sectionName, _T("Port"), 5000, iniPath);
        isDefault = GetPrivateProfileInt(sectionName, _T("Default"), 0, iniPath);
        
        if (name[0] == 0 && host[0] == 0) {
            break; // Больше нет серверов
        }
        
        ServerInfo server;
        server.name = name;
        server.host = host;
        server.port = port;
        server.isDefault = (isDefault != 0);
        
        servers.push_back(server);
        index++;
    }
    
    return (int)servers.size();
}

bool CDataCollector::GetDefaultServer(const CString& iniPath, ServerInfo& server) {
    std::vector<ServerInfo> servers;
    GetServers(iniPath, servers);
    
    for (size_t i = 0; i < servers.size(); i++) {
        if (servers[i].isDefault) {
            server = servers[i];
            return true;
        }
    }
    
    // Если нет сервера по умолчанию, используем первый
    if (!servers.empty()) {
        server = servers[0];
        return true;
    }
    
    return false;
}

bool CDataCollector::GetServerByIndex(const CString& iniPath, int index, ServerInfo& server) {
    std::vector<ServerInfo> servers;
    GetServers(iniPath, servers);
    
    if (index >= 0 && index < (int)servers.size()) {
        server = servers[index];
        return true;
    }
    
    return false;
}

void CDataCollector::SetDefaultServer(const CString& iniPath, int index) {
    std::vector<ServerInfo> servers;
    GetServers(iniPath, servers);
    
    // Убираем флаг Default у всех серверов
    for (int i = 1; i <= (int)servers.size(); i++) {
        CString sectionName;
        sectionName.Format(_T("Server_%d"), i);
        WritePrivateProfileString(sectionName, _T("Default"), _T("0"), iniPath);
    }
    
    // Устанавливаем флаг Default для указанного сервера
    if (index >= 0 && index < (int)servers.size()) {
        CString sectionName;
        sectionName.Format(_T("Server_%d"), index + 1);
        WritePrivateProfileString(sectionName, _T("Default"), _T("1"), iniPath);
    }
}
