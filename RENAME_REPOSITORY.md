# Инструкция по переименованию репозитория на GitHub

Проект успешно переименован локально. Теперь нужно переименовать репозиторий на GitHub.

## Текущее состояние

- ✅ Локальная папка переименована: `C:\Users\Ez\Desktop\CrystalDiskInfo-master`
- ✅ Git remote URL обновлен: `https://github.com/ez-flight/CrystalDiskInfo-master.git`
- ⚠️ Репозиторий на GitHub еще нужно переименовать

## Шаги для переименования на GitHub

1. **Откройте репозиторий на GitHub:**
   - Перейдите по адресу: https://github.com/ez-flight/windows_hdd_collector

2. **Переименуйте репозиторий:**
   - Нажмите на кнопку **Settings** (Настройки) в правом верхнем углу репозитория
   - Прокрутите вниз до раздела **Repository name** (Название репозитория)
   - Измените название с `windows_hdd_collector` на `CrystalDiskInfo-master`
   - Нажмите **Rename** (Переименовать)

3. **Проверьте результат:**
   - После переименования репозиторий будет доступен по адресу: https://github.com/ez-flight/CrystalDiskInfo-master

4. **Отправьте изменения:**
   ```powershell
   cd C:\Users\Ez\Desktop\CrystalDiskInfo-master
   git add .
   git commit -m "Rename project to CrystalDiskInfo-master"
   git push -u origin main
   ```

## Итоговая структура проекта

```
CrystalDiskInfo-master/
  ├── DataCollector.cpp
  ├── DataCollector.h
  ├── DiskInfo.sln
  ├── cpp/                  # C++ консольная утилита
  ├── README.md
  ├── .gitignore
  └── ... (остальные файлы проекта)
```

## Проверка

После переименования на GitHub проверьте:
- [ ] Репозиторий доступен по новому адресу
- [ ] Remote URL указан правильно: `git remote -v`
- [ ] Проект компилируется: `msbuild DiskInfo.sln /p:Configuration=Release /p:Platform=x64`

