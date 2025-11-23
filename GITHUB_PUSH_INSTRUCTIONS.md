# Инструкция по отправке проекта на GitHub

## Шаг 1: Создайте репозиторий на GitHub

1. Откройте [GitHub.com](https://github.com)
2. Нажмите кнопку **"+"** в правом верхнем углу
3. Выберите **"New repository"**
4. Заполните форму:
   - **Repository name**: `windows_hdd_collector` (или другое имя)
   - **Description**: "Сбор информации о жестких дисках и отправка данных на сервер через API v2"
   - Выберите **Public** или **Private**
   - **НЕ** добавляйте README, .gitignore или лицензию (они уже есть)
5. Нажмите **"Create repository"**

## Шаг 2: Добавьте файлы в Git

```powershell
cd C:\Users\Ez\Desktop\windows_hdd_collector

# Добавить все файлы (кроме тех, что в .gitignore)
git add .

# Проверить статус
git status
```

## Шаг 3: Сделайте первый коммит

```powershell
git commit -m "Initial commit: Windows HDD Collector with API v2 support"
```

## Шаг 4: Подключите удаленный репозиторий

Замените `YOUR_USERNAME` на ваш GitHub username и `REPOSITORY_NAME` на имя репозитория:

```powershell
git remote add origin https://github.com/YOUR_USERNAME/REPOSITORY_NAME.git
```

Например:
```powershell
git remote add origin https://github.com/username/windows_hdd_collector.git
```

## Шаг 5: Отправьте проект на GitHub

```powershell
# Для первого пуша используйте -u (upstream)
git push -u origin main
```

Если ваша ветка называется `master` вместо `main`:
```powershell
git branch -M main  # Переименовать ветку в main
git push -u origin main
```

## Альтернатива: Использование SSH

Если вы настроили SSH ключ на GitHub:

```powershell
git remote set-url origin git@github.com:YOUR_USERNAME/REPOSITORY_NAME.git
git push -u origin main
```

## Проблемы с аутентификацией

Если запрашивается пароль:

1. **Для HTTPS**: Используйте Personal Access Token вместо пароля
   - GitHub -> Settings -> Developer settings -> Personal access tokens -> Generate new token
   - Создайте токен с правами `repo`
   - Используйте токен как пароль

2. **Для SSH**: Настройте SSH ключ
   ```powershell
   ssh-keygen -t ed25519 -C "your_email@example.com"
   # Скопируйте публичный ключ в GitHub -> Settings -> SSH and GPG keys
   ```

## Проверка

После успешного пуша откройте ваш репозиторий на GitHub и убедитесь, что все файлы загружены.

