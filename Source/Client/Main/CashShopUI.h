#pragma once

#include "ProtocolDefines.h"

#define CASHSHOP_MAX_PRODUCT 64

struct PMSG_CASHSHOP_INFO_REQ
{
	PSBMSG_HEAD header; // C1:98:11
};

struct PMSG_CASHSHOP_ENTRY
{
	BYTE type;          // 0: item, 1: VIP days
	WORD item;          // GET_ITEM(section, index)
	BYTE level;
	BYTE category;      // 1: Pets/Mounts, 2: Jewels/Items, 3: Event Tickets, 4: VIP Services
	DWORD price;
	char name[32];
	char description[60];
};

struct PMSG_CASHSHOP_LIST_ANS
{
	PSWMSG_HEAD header; // C2:98:11
	BYTE enable;
	DWORD wcoin;
	BYTE count;
};

struct PMSG_CASHSHOP_BUY_REQ
{
	PSBMSG_HEAD header; // C1:98:10
	BYTE product;       // 1-based product index
	BYTE param;
};

struct PMSG_CASHSHOP_BUY_ANS
{
	PSBMSG_HEAD header; // C1:98:10
	BYTE result;
	BYTE product;
	DWORD wcoin;
};

class CCashShopUI
{
public:
	CCashShopUI();
	~CCashShopUI();

	void LoadImages();
	void Toggle();
	void Open();
	void Close() { this->m_Open = false; }
	bool IsOpen() const { return this->m_Open; }

	void OnListRecv(BYTE* lpMsg);
	void OnBuyRecv(BYTE* lpMsg);

	void Render();
	void UpdateMouse();

private:
	void RequestInfo();
	void RequestBuy(BYTE productIdx, BYTE param = 0);

	bool m_Open;
	bool m_Enable;
	int m_Category; // 0: All, 2: Jewels, 4: VIP & Services
	int m_Page;     // 0-based
	DWORD m_WCoin;
	int m_Count;
	PMSG_CASHSHOP_ENTRY m_Products[CASHSHOP_MAX_PRODUCT];

	DWORD m_WaitTick;
	DWORD m_StatusTick;
	bool m_StatusGood;
	char m_StatusText[96];

	int m_ConfirmProductIdx; // -1 = none, 0..m_Count-1 = pending confirm
	int m_SelectedClass;     // 0: DW, 1: DK, 2: ELF, 3: MG

	unsigned int m_TexShopCS;
};

extern CCashShopUI gCashShopUI;
