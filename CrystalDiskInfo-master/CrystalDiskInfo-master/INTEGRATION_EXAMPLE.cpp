/*---------------------------------------------------------------------------*/
// Пример интеграции отправки данных на сервер в CrystalDiskInfo
// Добавьте этот код в соответствующие файлы проекта
/*---------------------------------------------------------------------------*/

// ============================================
// 1. В DiskInfoDlg.h добавить:
// ============================================
#include "DataCollector.h"

class CDiskInfoDlg : public CMainDialogFx
{
    // ... существующий код ...
    
    CDataCollector m_DataCollector;  // Добавить эту строку
    
    // ... существующий код ...
    
    afx_msg void OnSendToServer();  // Добавить объявление обработчика
};

// ============================================
// 2. В Resource.h добавить ID:
// ============================================
#define ID_SEND_TO_SERVER           0xE100
#define ID_SERVER_SETTINGS          0xE101

// ============================================
// 3. В DiskInfo.rc добавить пункт меню:
// ============================================
// В меню "Function" или создать новое меню "Server":
POPUP "&Server"
BEGIN
    MENUITEM "Отправить данные на сервер...", ID_SEND_TO_SERVER
    MENUITEM SEPARATOR
    MENUITEM "Настройки сервера...", ID_SERVER_SETTINGS
END

// ============================================
// 4. В DiskInfoDlg.cpp добавить в карту сообщений:
// ============================================
BEGIN_MESSAGE_MAP(CDiskInfoDlg, CMainDialogFx)
    // ... существующие обработчики ...
    ON_COMMAND(ID_SEND_TO_SERVER, &CDiskInfoDlg::OnSendToServer)
    ON_COMMAND(ID_SERVER_SETTINGS, &CDiskInfoDlg::OnServerSettings)
END_MESSAGE_MAP()

// ============================================
// 5. Реализация обработчика OnSendToServer:
// ============================================
void CDiskInfoDlg::OnSendToServer()
{
    // Чтение настроек сервера из ini файла
    CString serverHost = GetPrivateProfileString(_T("Server"), _T("Host"), _T("10.3.3.7"), m_Ini);
    int serverPort = GetPrivateProfileInt(_T("Server"), _T("Port"), 5000, m_Ini);
    
    CString serverUrl;
    serverUrl.Format(_T("http://%s:%d/api/hdd_collect"), serverHost, serverPort);
    
    // Показываем диалог подтверждения
    CString message;
    message.Format(_T("Отправить данные о %d дисках на сервер %s?"), 
                   (int)m_Ata.vars.GetCount(), serverUrl);
    
    if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }
    
    // Отправка данных (передаем путь к ini файлу)
    CString response;
    CWaitCursor wait;
    
    if (m_DataCollector.SendToServer(m_Ini, m_Ata, response)) {
        AfxMessageBox(_T("Данные успешно отправлены на сервер!"), MB_OK | MB_ICONINFORMATION);
    } else {
        CString errorMsg;
        errorMsg.Format(_T("Ошибка при отправке данных на сервер:\n\nURL: %s\n\nОтвет: %s"), 
                        serverUrl, response);
        AfxMessageBox(errorMsg, MB_OK | MB_ICONERROR);
    }
}

// ============================================
// 6. Реализация обработчика OnServerSettings (опционально):
// ============================================
void CDiskInfoDlg::OnServerSettings()
{
    // Открываем диалог настроек сервера
    CServerSettingsDlg dlg(this);
    dlg.m_Ini = m_Ini;
    
    // Чтение текущих настроек
    dlg.m_ServerHost = GetPrivateProfileString(_T("Server"), _T("Host"), _T("10.3.3.7"), m_Ini);
    CString portStr;
    portStr.Format(_T("%d"), GetPrivateProfileInt(_T("Server"), _T("Port"), 5000, m_Ini));
    dlg.m_ServerPort = portStr;
    
    if (dlg.DoModal() == IDOK) {
        // Сохранение настроек в ini файл
        WritePrivateProfileString(_T("Server"), _T("Host"), dlg.m_ServerHost, m_Ini);
        WritePrivateProfileString(_T("Server"), _T("Port"), dlg.m_ServerPort, m_Ini);
    }
}

// ============================================
// 7. Автоматическая отправка при обновлении (опционально):
// ============================================
// В функции обновления данных (например, в OnTimer или после UpdateSmartInfo)
// можно добавить автоматическую отправку:

void CDiskInfoDlg::OnAutoSendToServer()
{
    BOOL autoSend = GetPrivateProfileInt(_T("Server"), _T("AutoSend"), 0, m_Ini);
    if (autoSend) {
        CString serverHost = GetPrivateProfileString(_T("Server"), _T("Host"), _T("10.3.3.7"), m_Ini);
        int serverPort = GetPrivateProfileInt(_T("Server"), _T("Port"), 5000, m_Ini);
        
        CString serverUrl;
        serverUrl.Format(_T("http://%s:%d/api/hdd_collect"), serverHost, serverPort);
        
        CString response;
        m_DataCollector.SendToServer(serverUrl, m_Ata, response);
        // Без показа сообщений для автоматической отправки
    }
}

