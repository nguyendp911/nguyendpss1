#pragma once

#include "ProtocolDefines.h"
#include "Util.h"

#define JEWELBANK_TYPES 5

struct PMSG_JEWELBANK_INFO_REQ
{
	PSBMSG_HEAD header; // C1:98:20
};

struct PMSG_JEWELBANK_DEPOSIT_REQ
{
	PSBMSG_HEAD header; // C1:98:21
	BYTE jewel;         // 0 ~ 4, 255 = all jewels
};

struct PMSG_JEWELBANK_WITHDRAW_REQ
{
	PSBMSG_HEAD header; // C1:98:22
	BYTE jewel;         // 0 ~ 4
	BYTE count;         // 0 = as many as possible
};

struct PMSG_JEWELBANK_STATE_ANS
{
	PSBMSG_HEAD header; // C1:98:20
	BYTE result;        // 0: none, 1: done, 2: inventory full, 3: bank empty, 4: nothing to deposit, 5: busy, 6: bank closed, 7: failed
	BYTE enable;
	WORD inventory[JEWELBANK_TYPES];
	DWORD bank[JEWELBANK_TYPES];
};

class CJewelBankUI
{
public:
	CJewelBankUI();

	void LoadImages();
	void Render();
	void UpdateMouse();

	void Toggle();
	void Close();
	bool IsOpen() const { return this->m_Open; }

	void OnStateRecv(BYTE* lpMsg);

private:
	void SendInfoRequest();
	void SendDeposit(BYTE jewel);
	void SendWithdraw(BYTE jewel, BYTE count);
	void SetResult(const char* text, bool good);

	bool m_Open;
	bool m_Enable;
	unsigned int m_TexBtnCS;
	DWORD m_Bank[JEWELBANK_TYPES];
	WORD m_Inventory[JEWELBANK_TYPES];
	ITEM m_Items[JEWELBANK_TYPES];
	DWORD m_WaitTick;
	DWORD m_RefreshTick;
	DWORD m_ResultTick;
	bool m_ResultGood;
	char m_ResultText[96];
};

extern CJewelBankUI gJewelBankUI;
