#pragma once

#include "ProtocolDefines.h"

struct CG_RANKING_REQ
{
	PSBMSG_HEAD header; // C1:98:08
	BYTE type;          // 0: Top Level, 1: Top Guild, 2: DW, 3: DK, 4: ELF, 5: MG
};

struct RANKING_ENTRY
{
	char name[11];
	DWORD value;
	BYTE extra;
};

struct GC_RANKING_ANS
{
	PSWMSG_HEAD header; // C2:98:08
	BYTE type;
	BYTE count;
	DWORD myWCoin;
	RANKING_ENTRY entries[10];
};

struct CG_BUY_VIP_REQ
{
	PSBMSG_HEAD header; // C1:98:09
	WORD days;          // 1, 7, 30
};

struct GC_BUY_VIP_ANS
{
	PSBMSG_HEAD header; // C1:98:09
	BYTE result;        // 0: Fail, 1: Success, 2: Not enough WCoin
	WORD days;
	DWORD remainWCoin;
};

class CRankingUI
{
public:
	CRankingUI();
	~CRankingUI();

	void Toggle();
	void ToggleVipShop();
	void Open(int tab = 0);
	void Close() { this->m_Open = false; }
	bool IsOpen() const { return this->m_Open; }

	void LoadImages();
	void RequestRanking(BYTE type);
	void RequestBuyVip(WORD days);
	void OnRankingRecv(BYTE* lpMsg);
	void OnBuyVipRecv(BYTE* lpMsg);

	void Render();
	void UpdateMouse();

private:
	bool m_Open;
	int m_Tab;          // 0: Top Level, 1: Top Class, 2: Top Guild, 3: VIP Shop
	int m_ClassFilter;  // 0: DW, 1: DK, 2: ELF, 3: MG
	BYTE m_DataType;
	BYTE m_Count;
	DWORD m_MyWCoin;
	RANKING_ENTRY m_Entries[10];

	DWORD m_StatusTick;
	bool m_StatusGood;
	char m_StatusText[96];
	int m_ConfirmVipIdx; // -1 = none, 0..2 = pending VIP confirm
	unsigned int m_TexRankCS;
};

extern CRankingUI gRankingUI;
