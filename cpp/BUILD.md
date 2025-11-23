# Инструкции по сборке проекта

## Предварительные требования

1. **Visual Studio 2019 или выше** (рекомендуется) или другой компилятор с поддержкой C++17
2. **CMake 3.10 или выше** - https://cmake.org/download/
3. **Windows SDK** (обычно устанавливается вместе с Visual Studio)

## Сборка в Visual Studio 2019/2022

### Вариант 1: Открыть папку

1. Откройте Visual Studio
2. Выберите `File` → `Open` → `Folder...`
3. Выберите папку `cpp` проекта
4. Visual Studio автоматически обнаружит CMakeLists.txt
5. Дождитесь завершения конфигурации CMake
6. Выберите конфигурацию (Debug/Release) в панели инструментов
7. Нажмите `Build` → `Build All` или `Ctrl+Shift+B`
8. Исполняемый файл будет в `cpp\out\build\<config>\bin\HDDCollector.exe`

### Вариант 2: CMake проект

1. Откройте Visual Studio
2. Выберите `File` → `Open` → `CMake...`
3. Выберите файл `cpp\CMakeLists.txt`
4. Дождитесь завершения конфигурации
5. Выберите конфигурацию (Debug/Release)
6. Соберите проект

## Сборка из командной строки

### Visual Studio Developer Command Prompt

```cmd
cd cpp
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Исполняемый файл будет в `cpp\build\bin\Release\HDDCollector.exe`

### Для других генераторов

```cmd
# MinGW
cmake .. -G "MinGW Makefiles"
cmake --build .

# Ninja (требует установки Ninja)
cmake .. -G "Ninja"
cmake --build .
```

## Пошаговая инструкция

### Шаг 1: Установка CMake

1. Скачайте CMake с https://cmake.org/download/
2. Установите, выбрав опцию "Add CMake to system PATH"

### Шаг 2: Проверка установки

Откройте командную строку и проверьте:

```cmd
cmake --version
```

Должна вывестись версия CMake 3.10 или выше.

### Шаг 3: Создание директории сборки

```cmd
cd путь_к_проекту\cpp
mkdir build
cd build
```

### Шаг 4: Генерация файлов проекта

```cmd
cmake ..
```

Это создаст файлы проекта для вашего компилятора.

### Шаг 5: Сборка проекта

```cmd
# Для Visual Studio
cmake --build . --config Release

# Для MinGW или других
cmake --build .
```

### Шаг 6: Проверка результата

После успешной сборки проверьте наличие файла:

```
cpp\build\bin\Release\HDDCollector.exe
```

или

```
cpp\build\bin\HDDCollector.exe
```

## Настройка CMake

### Изменение компилятора

```cmd
cmake .. -DCMAKE_CXX_COMPILER=clang++
```

### Изменение стандарта C++

Отредактируйте CMakeLists.txt:

```cmake
set(CMAKE_CXX_STANDARD 20)  # вместо 17
```

### Сборка для разных архитектур

```cmd
# x64
cmake .. -A x64

# x86
cmake .. -A Win32

# ARM64
cmake .. -A ARM64
```

## Отладка проблем сборки

### Ошибка: "CMake could not find any build tool"

**Решение:** Установите Visual Studio Build Tools или другой компилятор.

### Ошибка: "Could not find Windows SDK"

**Решение:** 
1. Откройте Visual Studio Installer
2. Нажмите "Modify" для вашей установки Visual Studio
3. Убедитесь, что установлен компонент "Windows 10/11 SDK"

### Ошибка: "Cannot open include file: 'windows.h'"

**Решение:** Убедитесь, что установлен Windows SDK.

### Ошибка: "Unresolved external symbol"

**Решение:** Проверьте, что все необходимые библиотеки указаны в CMakeLists.txt:
- ws2_32 (Winsock)
- winhttp (WinHTTP)
- wbemuuid (WMI)
- ole32 (COM)
- oleaut32 (COM)

## Создание установочного пакета

### Простое копирование

После сборки скопируйте `HDDCollector.exe` в нужную папку.

### Использование NSIS или Inno Setup

Создайте установочный скрипт, который копирует:
- `HDDCollector.exe`
- `config.example.json` (опционально)

## Проверка работоспособности

После сборки протестируйте программу:

```cmd
HDDCollector.exe --help
```

Должна вывестись справка по использованию.

Затем попробуйте запустить:

```cmd
HDDCollector.exe --host localhost --port 5000 --save test.json
```

Это должно собрать информацию о дисках и сохранить в файл.

