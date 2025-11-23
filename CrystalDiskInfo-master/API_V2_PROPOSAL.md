# Предложение: API v2 для сбора данных о компьютерах и жестких дисках

## Обзор

Предлагается расширенная версия API `/api/hdd_collect`, которая позволяет отправлять не только информацию о жестких дисках, но и полную информацию о компьютере/машине, на которой эти диски установлены.

## Цели

1. **Идентификация машины**: Уникальная идентификация компьютера в системе
2. **Инвентаризация**: Полная информация о конфигурации ПК
3. **Отслеживание**: История изменений конфигурации и расположения дисков
4. **Автоматизация**: Полностью автоматический сбор данных без ручного ввода

## Предлагаемая структура JSON

### Endpoint
```
POST /api/hdd_collect/v2
```

### Структура запроса

```json
{
  "machine": {
    "hostname": "PC-001",
    "ip_address": "192.168.1.100",
    "mac_address": "00:1B:44:11:3A:B7",
    "os": {
      "name": "Windows",
      "version": "10",
      "build": "19045",
      "edition": "Pro",
      "architecture": "x64"
    },
    "hardware": {
      "processor": "Intel Core i7-9700K",
      "memory_gb": 32,
      "motherboard": "ASUS PRIME B360M-A"
    },
    "network": {
      "domain": "WORKGROUP",
      "computer_role": "WORKSTATION"
    }
  },
  "collection_info": {
    "timestamp": "2024-01-15T10:30:00Z",
    "collector_version": "1.0.0",
    "collector_type": "CrystalDiskInfo",
    "comment": "Плановый сбор данных"
  },
  "disks": [
    {
      "serial_number": "SN123456789",
      "model": "Samsung 980 PRO 1TB",
      "size_gb": 1000,
      "media_type": "SSD",
      "manufacturer": "Samsung",
      "interface": "NVMe",
      "power_on_hours": 500,
      "power_on_count": 100,
      "health_status": "Good"
    }
  ]
}
```

## Детальное описание полей

### Корневой объект

| Параметр | Тип | Обязательный | Описание |
|----------|-----|--------------|----------|
| `machine` | object | Да | Информация о компьютере/машине |
| `collection_info` | object | Да | Метаданные о сборе данных |
| `disks` | array | Да | Массив объектов с данными о дисках |

### Объект `machine`

#### Базовые поля

| Параметр | Тип | Обязательный | Описание |
|----------|-----|--------------|----------|
| `hostname` | string | Да | Имя компьютера (уникальный идентификатор) |
| `ip_address` | string | Нет | IP-адрес машины в локальной сети |
| `mac_address` | string | Нет | MAC-адрес основного сетевого адаптера |

#### Объект `os` (операционная система)

| Параметр | Тип | Обязательный | Описание |
|----------|-----|--------------|----------|
| `name` | string | Нет | Название ОС (Windows, Linux, macOS) |
| `version` | string | Нет | Версия ОС (10, 11, etc.) |
| `build` | string | Нет | Номер сборки ОС |
| `edition` | string | Нет | Издание ОС (Pro, Home, Enterprise) |
| `architecture` | string | Нет | Архитектура (x64, x86, ARM64) |

#### Объект `hardware` (аппаратное обеспечение)

| Параметр | Тип | Обязательный | Описание |
|----------|-----|--------------|----------|
| `processor` | string | Нет | Модель процессора |
| `memory_gb` | integer | Нет | Объем оперативной памяти в ГБ |
| `motherboard` | string | Нет | Модель материнской платы |
| `bios_version` | string | Нет | Версия BIOS/UEFI |

#### Объект `network` (сетевая информация)

| Параметр | Тип | Обязательный | Описание |
|----------|-----|--------------|----------|
| `domain` | string | Нет | Домен или рабочая группа |
| `computer_role` | string | Нет | Роль компьютера (WORKSTATION, SERVER, DOMAIN_CONTROLLER) |
| `dns_suffix` | string | Нет | DNS суффикс |

### Объект `collection_info`

| Параметр | Тип | Обязательный | Описание |
|----------|-----|--------------|----------|
| `timestamp` | string (ISO 8601) | Нет | Время сбора данных в формате UTC |
| `collector_version` | string | Нет | Версия программы-сборщика |
| `collector_type` | string | Нет | Тип сборщика (CrystalDiskInfo, PowerShell, etc.) |
| `comment` | string | Нет | Комментарий пользователя |

### Объект `disks` (без изменений)

Структура массива дисков остается такой же, как в текущей версии API.

## Обратная совместимость

### Поддержка старой версии API

Старый endpoint `/api/hdd_collect` (v1) остается работать, но рекомендуется использовать новую версию.

### Миграция данных

При получении данных через v2 API:
- Если машина с таким `hostname` уже существует, обновляется информация
- Если машины нет, создается новая запись в таблице `machines`
- Диски связываются с машиной через внешний ключ

## Предлагаемая структура базы данных

### Новая таблица `machines`

```sql
CREATE TABLE machines (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    hostname VARCHAR(255) UNIQUE NOT NULL,
    ip_address VARCHAR(45),
    mac_address VARCHAR(17),
    os_name VARCHAR(50),
    os_version VARCHAR(50),
    os_build VARCHAR(50),
    os_edition VARCHAR(50),
    os_architecture VARCHAR(10),
    processor VARCHAR(255),
    memory_gb INTEGER,
    motherboard VARCHAR(255),
    bios_version VARCHAR(255),
    domain VARCHAR(255),
    computer_role VARCHAR(50),
    dns_suffix VARCHAR(255),
    first_seen DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_seen DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

### Обновление таблицы `pc_hard_drives`

Добавить поле `machine_id` для связи с таблицей `machines`:

```sql
ALTER TABLE pc_hard_drives 
ADD COLUMN machine_id INTEGER REFERENCES machines(id);
```

### Таблица истории машин `machine_history`

```sql
CREATE TABLE machine_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    machine_id INTEGER NOT NULL REFERENCES machines(id),
    changed_field VARCHAR(50),
    old_value TEXT,
    new_value TEXT,
    changed_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    comment TEXT
);
```

## Логика обработки на сервере

### Для машин

1. **Поиск машины**: По полю `hostname` (уникальный идентификатор)
2. **Если найдена**:
   - Обновить поля `last_seen = CURRENT_TIMESTAMP`
   - Обновить все поля из запроса, если они изменились
   - Записать изменения в `machine_history`
3. **Если не найдена**:
   - Создать новую запись в таблице `machines`
   - Записать в `machine_history` событие "Machine created"

### Для дисков

1. **Связь с машиной**: При создании/обновлении диска устанавливать `machine_id`
2. **Идентификация**: По `serial_number` (как в текущей версии)
3. **Обновление связей**: Если диск уже был привязан к другой машине, обновить связь

## Примеры запросов

### Минимальный запрос (только обязательные поля)

```json
{
  "machine": {
    "hostname": "PC-001"
  },
  "collection_info": {},
  "disks": [
    {
      "serial_number": "SN123456789",
      "model": "Samsung 980 PRO 1TB",
      "size_gb": 1000,
      "health_status": "Good"
    }
  ]
}
```

### Полный запрос (все доступные поля)

```json
{
  "machine": {
    "hostname": "WORKSTATION-05",
    "ip_address": "10.3.3.42",
    "mac_address": "00:1B:44:11:3A:B7",
    "os": {
      "name": "Windows",
      "version": "10",
      "build": "19045",
      "edition": "Pro",
      "architecture": "x64"
    },
    "hardware": {
      "processor": "Intel Core i7-9700K CPU @ 3.60GHz",
      "memory_gb": 32,
      "motherboard": "ASUS PRIME B360M-A",
      "bios_version": "BIOS Date: 03/15/19 10:15:45 Ver: 05.0000C"
    },
    "network": {
      "domain": "WORKGROUP",
      "computer_role": "WORKSTATION",
      "dns_suffix": "local"
    }
  },
  "collection_info": {
    "timestamp": "2024-01-15T10:30:00Z",
    "collector_version": "1.2.0",
    "collector_type": "CrystalDiskInfo",
    "comment": "Плановый сбор данных. Проверка после замены HDD на SSD."
  },
  "disks": [
    {
      "serial_number": "S6P7NS0X639479A",
      "model": "Samsung SSD 970 EVO Plus 1TB",
      "size_gb": 976,
      "media_type": "NVMe",
      "manufacturer": "Samsung",
      "interface": "NVMe",
      "power_on_hours": 843,
      "power_on_count": 291,
      "health_status": "Good"
    },
    {
      "serial_number": "AA000000000000000480",
      "model": "XrayDisk 512GB SSD",
      "size_gb": 500,
      "media_type": "SSD",
      "interface": "SATA",
      "power_on_hours": 4462,
      "power_on_count": 1455,
      "health_status": "Good"
    }
  ]
}
```

## Формат ответа

### Успешный ответ

```json
{
  "success": true,
  "machine": {
    "id": 42,
    "hostname": "PC-001",
    "status": "updated",
    "message": "Machine information updated"
  },
  "disks": {
    "processed": 3,
    "total": 3,
    "new": 1,
    "updated": 2
  },
  "timestamp": "2024-01-15T10:30:15Z"
}
```

### Ответ с ошибками

```json
{
  "success": false,
  "error": "Validation failed",
  "details": {
    "machine": {
      "hostname": "Field is required"
    },
    "disks": [
      {
        "index": 1,
        "serial_number": "Field is required"
      }
    ]
  },
  "timestamp": "2024-01-15T10:30:15Z"
}
```

## Преимущества новой версии

1. **Полная инвентаризация**: Вся информация о ПК в одном запросе
2. **Отслеживание**: История изменений конфигурации машин
3. **Идентификация**: Уникальная идентификация машин по hostname
4. **Связи**: Понятная связь между дисками и машинами
5. **Аналитика**: Возможность анализа по типам машин, ОС, конфигурациям
6. **Расширяемость**: Легко добавлять новые поля без изменения структуры

## План внедрения

### Этап 1: Подготовка
- [ ] Создать новую таблицу `machines`
- [ ] Добавить поле `machine_id` в `pc_hard_drives`
- [ ] Создать таблицу `machine_history`
- [ ] Разработать эндпоинт `/api/hdd_collect/v2`

### Этап 2: Реализация API
- [ ] Реализовать обработку объекта `machine`
- [ ] Реализовать обработку объекта `collection_info`
- [ ] Связать диски с машинами
- [ ] Реализовать историю изменений

### Этап 3: Клиентская часть
- [ ] Обновить CrystalDiskInfo для отправки расширенных данных
- [ ] Добавить сбор информации о системе (ОС, процессор, память)
- [ ] Добавить диалог ввода комментария
- [ ] Тестирование

### Этап 4: Миграция
- [ ] Миграция существующих данных (если возможно связать с машинами)
- [ ] Обновление документации
- [ ] Уведомление пользователей о новой версии API

## Альтернативный вариант (обратная совместимость)

Если требуется полная обратная совместимость, можно использовать один endpoint с определением версии через заголовок:

```
POST /api/hdd_collect
Headers:
  X-API-Version: 2.0
```

Если заголовок отсутствует или равен `1.0`, используется старая логика обработки.

## Заключение

Новая версия API позволит создать полноценную систему инвентаризации компьютеров и их компонентов, обеспечивая полную автоматизацию сбора данных и удобную аналитику.

