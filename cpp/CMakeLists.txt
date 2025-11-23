cmake_minimum_required(VERSION 3.10)
project(HDDCollector VERSION 1.0.0 LANGUAGES CXX)

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Параметры компиляции
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    # Используем динамическое связывание библиотек времени выполнения для совместимости
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Исходные файлы
set(SOURCES
    src/main.cpp
    src/hdd_collector.cpp
    src/wmi_helper.cpp
    src/http_client.cpp
    src/json_helper.cpp
    src/smart_helper.cpp
)

# Заголовочные файлы
set(HEADERS
    src/hdd_collector.h
    src/wmi_helper.h
    src/http_client.h
    src/json_helper.h
    src/smart_helper.h
)

# Создание исполняемого файла
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Связывание библиотек Windows
if(WIN32)
    target_link_libraries(${PROJECT_NAME}
        ws2_32          # Winsock для работы с сетью
        winhttp         # WinHTTP для HTTP запросов
        wbemuuid        # WMI для получения информации о дисках
        ole32           # COM для WMI
        oleaut32        # COM для WMI
        iphlpapi        # IP Helper API для получения IP и MAC адресов
    )
endif()

# Установка для Release
set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE FALSE
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Копирование конфигурационного файла примера
if(EXISTS "${CMAKE_SOURCE_DIR}/../config.example.json")
    configure_file(
        "${CMAKE_SOURCE_DIR}/../config.example.json"
        "${CMAKE_BINARY_DIR}/bin/config.example.json"
        COPYONLY
    )
endif()

# Информация о сборке
message(STATUS "Проект: ${PROJECT_NAME}")
message(STATUS "Версия: ${PROJECT_VERSION}")
message(STATUS "Компилятор: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Стандарт C++: ${CMAKE_CXX_STANDARD}")

