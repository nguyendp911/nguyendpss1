#pragma once

#include <vector>
#include <string>
#include "Defines.h"
#include "Structs.h"
#include "ProtocolDefines.h"

enum MARKET_TAB
{
	MARKET_TAB_BROWSE = 0,
	MARKET_TAB_SELL = 1,
	MARKET_TAB_MY_ITEMS = 2,
};

enum MARKET_CATEGORY
{
	MARKET_CAT_ALL = 0,
	MARKET_CAT_WEAPONS = 1,
	MARKET_CAT_ARMOR = 2,
	MARKET_CAT_WINGS_ACC = 3,
	MARKET_CAT_JEWELS_MISC = 4,
};

enum MARKET_PRICE_TYPE
{
	MARKET_PRICE_ZEN = 0,
	MARKET_PRICE_BLESS = 1,
	MARKET_PRICE_SOUL = 2,
	MARKET_PRICE_CHAOS = 3,
	MARKET_PRICE_WCOIN = 4,
};

struct CLIENT_MARKET_ITEM
{
	DWORD marketId;
	char sellerName[11];
	BYTE itemBytes[10];
	DWORD price;
	BYTE priceType;
	ITEM item;
};

struct CLIENT_MY_MARKET_ITEM
{
	DWORD marketId;
	BYTE itemBytes[10];
	DWORD price;
	BYTE priceType;
	BYTE status;
	char buyerName[11];
	ITEM item;
};

// Client <-> GameServer Protocol Head 0x98
struct CG_MARKET_PAGE_REQ
{
	PSBMSG_HEAD header; // C1:98:01
	BYTE category;
	BYTE page;
};

struct MARKET_ITEM_ENTRY
{
	DWORD marketId;
	char sellerName[11];
	BYTE itemBytes[10];
	DWORD price;
	BYTE priceType;
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
	BYTE result;
	DWORD marketId;
};

struct CG_MARKET_SELL_REQ
{
	PSBMSG_HEAD header; // C1:98:03
	BYTE slot;
	DWORD price;
	BYTE priceType;
};

struct GC_MARKET_SELL_ANS
{
	PSBMSG_HEAD header; // C1:98:03
	BYTE result;
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
	BYTE status;
	char buyerName[11];
};

struct GC_MARKET_MY_ITEMS_ANS
{
	PSWMSG_HEAD header; // C2:98:04
	BYTE count;
	DWORD myWCoin;
};

struct CG_MARKET_CANCEL_SELL_REQ
{
	PSBMSG_HEAD header; // C1:98:05
	DWORD marketId;
};

struct GC_MARKET_CANCEL_SELL_ANS
{
	PSBMSG_HEAD header; // C1:98:05
	BYTE result;
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
	BYTE result;
	DWORD marketId;
	DWORD price;
	BYTE priceType;
};

struct GC_WCOIN_SEND
{
	PSBMSG_HEAD header; // C1:98:07
	DWORD wcoin;
};

class CMarketWindow
{
public:
	CMarketWindow();
	~CMarketWindow();

	void LoadImages();
	void Open();
	void Close();
	void Toggle();

	bool IsOpen() const { return this->m_bIsOpen; }
	bool IsEditingPrice() const { return this->m_bIsOpen && this->m_bEditingPrice; }
	DWORD GetMyWCoin() const { return this->m_dwMyWCoin; }
	void SetMyWCoin(DWORD wcoin) { this->m_dwMyWCoin = wcoin; }

	void Render();
	void UpdateMouse();
	void OnChar(wchar_t ch);

	void RequestPage(int category, int page);
	void RequestBuy(DWORD marketId);
	void RequestSell(int slot, DWORD price, int priceType);
	void RequestMyItems();
	void RequestCancel(DWORD marketId);
	void RequestClaim(DWORD marketId);

	void OnReceivePage(BYTE* lpMsg);
	void OnReceiveBuyResult(BYTE result, DWORD marketId);
	void OnReceiveSellResult(BYTE result);
	void OnReceiveMyItems(BYTE* lpMsg);
	void OnReceiveCancelResult(BYTE result, DWORD marketId);
	void OnReceiveClaimResult(BYTE result, DWORD marketId, DWORD price, BYTE priceType);

private:
	void ConvertBytesToItem(BYTE* data, ITEM* pOutItem);
	void RenderBrowseTab();
	void RenderSellTab();
	void RenderMyItemsTab();
	void RenderConfirmBuyModal();
	void UpdateBrowseMouse();
	void UpdateSellMouse();
	void UpdateMyItemsMouse();
	void UpdateConfirmBuyMouse();

	bool m_bIsOpen;
	unsigned int m_TexBtnCS;
	DWORD m_dwMyWCoin;

	int m_CurrentTab;
	int m_SelectedCategory;
	int m_CurrentPage;
	int m_TotalPages;
	int m_TotalItems;
	int m_MyItemsPage;

	std::vector<CLIENT_MARKET_ITEM> m_MarketItems;
	std::vector<CLIENT_MY_MARKET_ITEM> m_MyItems;

	bool m_bHoveringItem;
	ITEM m_HoverItem;

	int m_SelectedInventorySlot;
	char m_szPriceInput[16];
	bool m_bEditingPrice;
	int m_SelectedPriceType;

	bool m_bShowConfirmBuy;
	DWORD m_ConfirmBuyMarketId;
	char m_ConfirmBuyItemName[64];
	DWORD m_ConfirmBuyPrice;
	BYTE m_ConfirmBuyPriceType;
};

extern CMarketWindow gMarketWindow;
