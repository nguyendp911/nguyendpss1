#pragma once

#include "ProtocolDefines.h"

struct CG_REGISTER_ACCOUNT_REQ
{
	PSBMSG_HEAD header; // C1:99:10
	char account[11];
	char password[11];
	char securityCode[8]; // 7 digits Personal ID (sno__numb)
};

struct GC_REGISTER_ACCOUNT_ANS
{
	PSBMSG_HEAD header; // C1:99:10
	BYTE result; // 1=OK, 2=Exists, 3=Invalid, 4=DB Error, 5=Rate Limited
};

class CRegisterUI
{
public:
	CRegisterUI();
	~CRegisterUI();

	void Open();
	void Close();
	bool IsOpen() const { return this->m_Open; }
	void OnChar(wchar_t ch);
	void OnRegisterAnsRecv(BYTE* lpMsg);

	// Called on Login scene (LOGIN_STATE == 2 || 3)
	void Render(int loginState);
	bool UpdateMouse(int loginState, bool click);

private:
	void GenerateCaptcha();
	void Submit();
	void SetStatus(const char* text, bool good);

	bool m_Open;
	int  m_Focus; // 1=account, 2=pass, 3=confirm, 4=secCode(7 digits), 5=captcha(4 digits)
	bool m_Pending;

	char m_Account[11];
	char m_Password[11];
	char m_Confirm[11];
	char m_SecCode[8];
	char m_CaptchaInput[6];
	char m_CaptchaCode[6];

	DWORD m_StatusTick;
	bool  m_StatusGood;
	char  m_StatusText[96];
};

extern CRegisterUI gRegisterUI;
