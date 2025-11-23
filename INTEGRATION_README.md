# Интеграция отправки данных на сервер в CrystalDiskInfo

## Обзор

В проект CrystalDiskInfo добавлена функциональность отправки данных о дисках на сервер в формате JSON, совместимом с API проекта Windows HDD Collector.

## Добавленные файлы

1. **HttpClient.h / HttpClient.cpp** - HTTP клиент для отправки POST запросов на сервер
2. **DataCollector.h / DataCollector.cpp** - Модуль для сбора данных о дисках и преобразования в JSON

## Использование

### 1. Добавление в проект

Добавьте следующие файлы в проект Visual Studio:
- `HttpClient.h`
- `HttpClient.cpp`
- `DataCollector.h`
- `DataCollector.cpp`

### 2. Добавление библиотек

В настройках проекта добавьте ссылки на библиотеки:
- `winhttp.lib` (для WinHTTP API)
- `iphlpapi.lib` (для получения IP и MAC адресов)
- `ws2_32.lib` (для работы с сетью)

### 3. Добавление пункта меню

В файле `DiskInfo.rc` добавьте новый пункт меню (например, в меню "Файл" или "Инструменты"):

```rc
MENUITEM "Отправить данные на сервер...", ID_SEND_TO_SERVER
```

И добавьте ID в `Resource.h`:
```cpp
#define ID_SEND_TO_SERVER 0xE100
```

### 4. Добавление обработчика

В `DiskInfoDlg.h` добавьте:
```cpp
#include "DataCollector.h"
CDataCollector m_DataCollector;
afx_msg void OnSendToServer();
```

В `DiskInfoDlg.cpp` добавьте в карту сообщений:
```cpp
ON_COMMAND(ID_SEND_TO_SERVER, &CDiskInfoDlg::OnSendToServer)
```

И реализацию:
```cpp
void CDiskInfoDlg::OnSendToServer()
{
    // Получаем URL сервера из настроек или запрашиваем у пользователя
    CString serverUrl = _T("http://10.3.3.7:5000/api/hdd_collect");
    
    // Можно добавить диалог для ввода URL
    // CServerUrlDlg dlg;
    // if (dlg.DoModal() == IDOK) {
    //     serverUrl = dlg.m_ServerUrl;
    // }
    
    CString response;
    if (m_DataCollector.SendToServer(serverUrl, m_Ata, response)) {
        AfxMessageBox(_T("Данные успешно отправлены на сервер!"), MB_OK | MB_ICONINFORMATION);
    } else {
        CString errorMsg;
        errorMsg.Format(_T("Ошибка при отправке данных:\n%s"), response);
        AfxMessageBox(errorMsg, MB_OK | MB_ICONERROR);
    }
}
```

### 5. Настройки сервера

Можно добавить настройки сервера в `DiskInfo.ini`:

```cpp
// Чтение настроек
CString serverHost = GetPrivateProfileString(_T("Server"), _T("Host"), _T("10.3.3.7"), m_Ini);
int serverPort = GetPrivateProfileInt(_T("Server"), _T("Port"), 5000, m_Ini);
CString serverUrl;
serverUrl.Format(_T("http://%s:%d/api/hdd_collect"), serverHost, serverPort);
```

## Формат данных

Данные отправляются в формате JSON, совместимом с API проекта Windows HDD Collector:

```json
{
  "hostname": "ComputerName",
  "disks": [
    {
      "serial_number": "ABC123",
      "model": "Samsung SSD 970 EVO",
      "size_gb": 931,
      "media_type": "SSD",
      "interface": "NVMe",
      "power_on_hours": 1234,
      "power_on_count": 567,
      "health_status": "Здоров"
    }
  ]
}
```

## Особенности

- Автоматическое определение IP и MAC адреса машины (записываются в комментарии)
- Поддержка всех типов дисков (ATA, SATA, NVMe, SCSI, USB)
- Использование уже собранных S.M.A.R.T. данных из CrystalDiskInfo
- Поддержка HTTP и HTTPS
- Обработка ошибок и таймаутов

## Компиляция

1. Откройте `DiskInfo.sln` в Visual Studio
2. Добавьте новые файлы в проект
3. Добавьте необходимые библиотеки
4. Скомпилируйте проект

## Тестирование

После компиляции можно протестировать отправку данных:
1. Запустите CrystalDiskInfo
2. Выберите меню "Отправить данные на сервер"
3. Проверьте ответ сервера

