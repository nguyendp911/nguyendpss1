#pragma once

#include "ProtocolDefines.h"
#include "Item.h"
#include "DSProtocol.h"

// Client <-> GameServer Protocol Head 0x98
struct CG_MARKET_PAGE_REQ
{
	PSBMSG_HEAD header; // C1:98:01
	BYTE category;      // 0: All, 1: Weapons, 2: Armor, 3: Wings/Accessories, 4: Jewels/Misc
	BYTE page;          // 1-indexed
};

struct MARKET_ITEM_ENTRY
{
	DWORD marketId;
	char sellerName[11];
	BYTE itemBytes[10];
	DWORD price;
	BYTE priceType;     // 0: Zen, 1: Bless, 2: Soul, 3: Chaos, 4: WCoin
};

struct GC_MARKET_PAGE_ANS
{
	PSWMSG_HEAD header; // C2:98:01
	BYTE category;
	BYTE currentPage;
	BYTE totalPages;
	WORD totalItems;
	BYTE itemCount;
	DWORD myWCoin;
};

struct CG_MARKET_BUY_REQ
{
	PSBMSG_HEAD header; // C1:98:02
	DWORD marketId;
};

struct GC_MARKET_BUY_ANS
{
	PSBMSG_HEAD header; // C1:98:02
	BYTE result;        // 0: Fail, 1: Success, 2: Not enough currency, 3: Inventory full, 4: Unavailable, 5: Own item
	DWORD marketId;
};

struct CG_MARKET_SELL_REQ
{
	PSBMSG_HEAD header; // C1:98:03
	BYTE slot;          // Inventory slot 12..75
	DWORD price;
	BYTE priceType;     // 0: Zen, 1: Bless, 2: Soul, 3: Chaos, 4: WCoin
};

struct GC_MARKET_SELL_ANS
{
	PSBMSG_HEAD header; // C1:98:03
	BYTE result;        // 0: Fail, 1: Success
};

struct CG_MARKET_MY_ITEMS_REQ
{
	PSBMSG_HEAD header; // C1:98:04
};

struct MY_MARKET_ITEM_ENTRY
{
	DWORD marketId;
	BYTE itemBytes[10];
	DWORD price;
	BYTE priceType;
	BYTE status;        // 0: Selling, 1: Sold (Wait claim), 2: Sold (Paid)
	char buyerName[11];
};

struct GC_MARKET_MY_ITEMS_ANS
{
	PSWMSG_HEAD header; // C2:98:04
	BYTE count;
	DWORD myWCoin;
};

struct GC_WCOIN_SEND
{
	PSBMSG_HEAD header; // C1:98:07
	DWORD wcoin;
};

struct CG_MARKET_CANCEL_SELL_REQ
{
	PSBMSG_HEAD header; // C1:98:05
	DWORD marketId;
};

struct GC_MARKET_CANCEL_SELL_ANS
{
	PSBMSG_HEAD header; // C1:98:05
	BYTE result;        // 0: Fail, 1: Success, 2: Inventory full
	DWORD marketId;
};

struct CG_MARKET_CLAIM_PAYMENT_REQ
{
	PSBMSG_HEAD header; // C1:98:06
	DWORD marketId;
};

struct GC_MARKET_CLAIM_PAYMENT_ANS
{
	PSBMSG_HEAD header; // C1:98:06
	BYTE result;        // 0: Fail, 1: Success, 2: Zen overflow
	DWORD marketId;
	DWORD price;
	BYTE priceType;
};

struct CG_RANKING_REQ
{
	PSBMSG_HEAD header; // C1:98:08
	BYTE type;          // 0: Top Level, 1: Top Guild, 2: DW, 3: DK, 4: ELF, 5: MG
};

struct GC_RANKING_ANS
{
	PSWMSG_HEAD header; // C2:98:08
	BYTE type;
	BYTE count;
	DWORD myWCoin;
	SDHP_RANKING_ENTRY entries[10];
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

class CMarket
{
public:
	CMarket();
	~CMarket();

	void CGMarketPageRecv(CG_MARKET_PAGE_REQ* lpMsg, int aIndex);
	void CGMarketBuyRecv(CG_MARKET_BUY_REQ* lpMsg, int aIndex);
	void CGMarketSellRecv(CG_MARKET_SELL_REQ* lpMsg, int aIndex);
	void CGMarketMyItemsRecv(CG_MARKET_MY_ITEMS_REQ* lpMsg, int aIndex);
	void CGMarketCancelSellRecv(CG_MARKET_CANCEL_SELL_REQ* lpMsg, int aIndex);
	void CGMarketClaimPaymentRecv(CG_MARKET_CLAIM_PAYMENT_REQ* lpMsg, int aIndex);
	void CGRankingReqRecv(CG_RANKING_REQ* lpMsg, int aIndex);
	void CGBuyVipReqRecv(CG_BUY_VIP_REQ* lpMsg, int aIndex);

	void DGMarketGetItemsRecv(SDHP_MARKET_GET_ITEMS_SEND* lpMsg);
	void DGMarketAddItemRecv(SDHP_MARKET_ADD_ITEM_SEND* lpMsg);
	void DGMarketBuyItemRecv(SDHP_MARKET_BUY_ITEM_SEND* lpMsg);
	void DGMarketMyItemsRecv(SDHP_MARKET_MY_ITEMS_SEND* lpMsg);
	void DGMarketCancelItemRecv(SDHP_MARKET_CANCEL_ITEM_SEND* lpMsg);
	void DGMarketClaimItemRecv(SDHP_MARKET_CLAIM_ITEM_SEND* lpMsg);
	void DGBuyVipRecv(SDHP_BUY_VIP_SEND* lpMsg);
	void DGRankingGetRecv(SDHP_RANKING_GET_SEND* lpMsg);

	void CommandBuyVip(LPOBJ lpObj, char* arg);
	void CommandTop(LPOBJ lpObj, char* arg);

	void GCWCoinSend(int aIndex, DWORD wcoin);

	int GetCategoryByItemIndex(int itemIndex);
	int GetJewelCount(LPOBJ lpObj, int jewelIndex);
	bool DeductJewels(LPOBJ lpObj, int jewelIndex, int count);
	bool AddJewelsToInventory(LPOBJ lpObj, int jewelIndex, int count);
};

extern CMarket gMarket;
