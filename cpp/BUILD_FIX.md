# Исправление ошибок компиляции

## Проблема: Конфликт winsock.h и winsock2.h

При компиляции возникают ошибки типа:
```
error C2011: sockaddr: переопределение типа "struct"
error C2375: accept: переопределение; другое связывание
```

### Причина

Windows SDK включает старый `winsock.h` автоматически при включении `windows.h`, а мы используем `winsock2.h`. Это вызывает конфликт определений.

### Решение

Нужно включить `winsock2.h` **ПЕРЕД** `windows.h`, или определить `WIN32_LEAN_AND_MEAN` перед включением `windows.h`.

Исправления уже внесены в код. Правильный порядок включения:

```cpp
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#endif
```

## Проверка исправлений

Все файлы были обновлены:
- ✅ `cpp/src/main.cpp` - правильный порядок включений
- ✅ `cpp/src/hdd_collector.cpp` - правильный порядок включений  
- ✅ `cpp/src/http_client.cpp` - WIN32_LEAN_AND_MEAN определен

## Повторная сборка

После исправлений попробуйте снова:

```bash
cd cpp/build
cmake --build . --config Release
```

Если ошибки остаются, попробуйте очистить build:

```bash
cd cpp/build
nmake clean
# или удалите папку build и создайте заново
cd ..
rmdir /s /q build
mkdir build
cd build
cmake .. -G "NMake Makefiles"
cmake --build . --config Release
```

