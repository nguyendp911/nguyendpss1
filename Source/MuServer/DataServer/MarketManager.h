#pragma once

#include "DataServerProtocol.h"

class CMarketManager
{
public:
	CMarketManager();
	~CMarketManager();

	void Init();
	void GDMarketGetItemsRecv(SDHP_MARKET_GET_ITEMS_RECV* lpMsg, int index);
	void GDMarketAddItemRecv(SDHP_MARKET_ADD_ITEM_RECV* lpMsg, int index);
	void GDMarketBuyItemRecv(SDHP_MARKET_BUY_ITEM_RECV* lpMsg, int index);
	void GDMarketGetMyItemsRecv(SDHP_MARKET_MY_ITEMS_RECV* lpMsg, int index);
	void GDMarketCancelItemRecv(SDHP_MARKET_CANCEL_ITEM_RECV* lpMsg, int index);
	void GDMarketClaimItemRecv(SDHP_MARKET_CLAIM_ITEM_RECV* lpMsg, int index);
	void GDBuyVipRecv(SDHP_BUY_VIP_RECV* lpMsg, int index);
	void GDRankingGetRecv(SDHP_RANKING_GET_RECV* lpMsg, int index);
	void GDCashShopBuyRecv(SDHP_CASHSHOP_BUY_RECV* lpMsg, int index);

private:
	void CheckDatabaseSchema();
};

extern CMarketManager gMarketManager;
