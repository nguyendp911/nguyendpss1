#pragma once

#include "ProtocolDefines.h"
#include "DSProtocol.h"

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
	PSWMSG_HEAD header; // C2:98:11, followed by <count> PMSG_CASHSHOP_ENTRY
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
	BYTE result;        // 0: Fail, 1: Success, 2: Not enough WCoin, 3: Inventory full, 4: Disabled
	BYTE product;
	DWORD wcoin;
};

struct PMSG_SEPAY_INFO
{
	PSBMSG_HEAD header; // C1:99:30
	BYTE enable;
	DWORD rateVnd;
	DWORD rateWcoin;
	DWORD minAmount;
	char bankCode[16];
	char bankAccount[32];
	char bankOwner[48];
	char account[16];
};

struct CASHSHOP_PRODUCT
{
	BYTE type;
	BYTE category;
	WORD item;
	BYTE level;
	BYTE dur;
	BYTE skill;
	BYTE luck;
	BYTE opt;
	BYTE exc;
	WORD param;
	DWORD price;
	char name[32];
	char description[60];
};

class CCashShop
{
public:
	CCashShop();
	~CCashShop();

	void Load(const char* path);
	void CGCashShopInfoRecv(PMSG_CASHSHOP_INFO_REQ* lpMsg, int aIndex);
	void CGCashShopBuyRecv(PMSG_CASHSHOP_BUY_REQ* lpMsg, int aIndex);
	void DGCashShopBuyRecv(SDHP_CASHSHOP_BUY_SEND* lpMsg);

private:
	void SendList(int aIndex, DWORD wcoin);
	void SendSePayInfo(int aIndex);
	void ApplyClassChange(LPOBJ lpObj, int targetClass);

	bool m_Enable;
	int m_Count;
	CASHSHOP_PRODUCT m_Products[CASHSHOP_MAX_PRODUCT];

	bool m_SePayEnable;
	DWORD m_SePayRateVnd;
	DWORD m_SePayRateWcoin;
	DWORD m_SePayMinAmount;
	char m_SePayBankCode[16];
	char m_SePayBankAccount[32];
	char m_SePayBankOwner[48];
};

extern CCashShop gCashShop;
