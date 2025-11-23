/*---------------------------------------------------------------------------*/
//       Author : Modified for 51 кафедра
//      License : MIT License
/*---------------------------------------------------------------------------*/
#pragma once

#include "DialogFx.h"
#include "EditFx.h"
#include "StaticFx.h"
#include "ButtonFx.h"

class CServerSettingsDlg : public CDialogFx
{
	DECLARE_DYNCREATE(CServerSettingsDlg)

	static const int SIZE_X = 400;
	static const int SIZE_Y = 200;

public:
	CServerSettingsDlg(CWnd* pParent = NULL);
	virtual ~CServerSettingsDlg();

	enum { IDD = IDD_SERVER_SETTINGS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void UpdateDialogSize();
	virtual void OnOK();

	DECLARE_MESSAGE_MAP()

	CString m_ServerHost;
	CString m_ServerPort;
	CString m_LabelServerHost;
	CString m_LabelServerPort;

	CEditFx m_CtrlServerHost;
	CEditFx m_CtrlServerPort;
	CStaticFx m_CtrlLabelServerHost;
	CStaticFx m_CtrlLabelServerPort;
	CButtonFx m_CtrlOK;
	CButtonFx m_CtrlCancel;

	CString m_Ini;
};

