# Windows HDD Collector

Проект для сбора информации о жестких дисках и отправки данных на сервер через API v2.

## Описание

Проект состоит из двух компонентов:

1. **C++ консольная утилита** (`cpp/`) - простая консольная программа для сбора данных о дисках
2. **CrystalDiskInfo с интеграцией** (`CrystalDiskInfo-master/`) - модифицированная версия CrystalDiskInfo с функцией отправки данных на сервер

## Возможности

- ✅ Сбор информации о жестких дисках (модель, серийный номер, размер, S.M.A.R.T. данные)
- ✅ Сбор информации о системе (ОС, процессор, память, материнская плата, BIOS)
- ✅ Сбор информации о видеокартах
- ✅ Сбор информации о модулях ОЗУ (по платам)
- ✅ Отправка данных на сервер через API v2
- ✅ Поддержка множественных серверов с автоматическим failover
- ✅ Автоматическая отправка данных при запуске программы

## API v2

Проект поддерживает API v2 для отправки комплексной информации о машинах и дисках. Подробнее см. [API_HDD_ENDPOINTS_V2.md](CrystalDiskInfo-master/CrystalDiskInfo-master/API_HDD_ENDPOINTS_V2.md).

## Структура проекта

```
windows_hdd_collector/
├── cpp/                    # C++ консольная утилита
│   ├── src/               # Исходный код
│   ├── build/             # Собранные файлы
│   └── CMakeLists.txt     # CMake конфигурация
└── CrystalDiskInfo-master/  # Модифицированный CrystalDiskInfo
    └── CrystalDiskInfo-master/
        ├── DataCollector.* # Модуль сбора и отправки данных
        ├── HttpClient.*    # HTTP клиент
        └── ...
```

## Сборка

### CrystalDiskInfo

1. Откройте `DiskInfo.sln` в Visual Studio
2. Выберите конфигурацию Release x64
3. Соберите проект

Исполняемый файл будет создан в `CrystalDiskInfo-master/Rugenia/DiskInfo64.exe`

### C++ консольная утилита

```bash
cd cpp
mkdir build
cd build
cmake .. -G "NMake Makefiles"
cmake --build . --config Release
```

## Использование

### CrystalDiskInfo

1. Запустите `DiskInfo64.exe`
2. Используйте меню "Function" -> "Отправить данные на сервер..." для отправки данных
3. Данные будут отправлены на сервер, настроенный в `DiskInfo.ini`

### Консольная утилита

```bash
hdd_collector.exe --host 10.3.3.7 --port 5000
```

Опции:
- `--host` - адрес сервера (по умолчанию: 10.3.3.7)
- `--port` - порт сервера (по умолчанию: 5000)
- `--save` - сохранить данные в JSON файл вместо отправки
- `--config` - путь к конфигурационному файлу

## Конфигурация

### DiskInfo.ini

Настройка серверов для CrystalDiskInfo:

```ini
[Server]
AutoSendOnStartup=1

[Server_1]
Name=Main Server
Host=10.3.3.7
Port=5000
Default=1

[Server_2]
Name=Backup Server
Host=192.168.100.106
Port=5000
Default=0
```

## API Endpoint

```
POST /api/hdd_collect/v2
```

## Лицензия

Проект использует код CrystalDiskInfo (MIT License) и дополнительные модули.

