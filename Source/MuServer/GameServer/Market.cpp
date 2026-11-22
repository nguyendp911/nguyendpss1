#include "stdafx.h"
#include "Market.h"
#include "GameMain.h"
#include "ItemManager.h"
#include "ItemStack.h"
#include "JSProtocol.h"
#include "Notice.h"
#include "ObjectManager.h"
#include "Protocol.h"
#include "SocketManager.h"
#include "Util.h"

CMarket gMarket;

CMarket::CMarket()
{
}

CMarket::~CMarket()
{
}

int CMarket::GetCategoryByItemIndex(int itemIndex)
{
	int section = itemIndex / MAX_ITEM_TYPE;

	if (section >= 0 && section <= 5) return 1;      // Weapons
	else if (section >= 6 && section <= 11) return 2; // Armor & Shields
	else if (section >= 12 && section <= 13) return 3; // Wings, Orbs, Rings, Pendants
	else return 4;                                     // Jewels, Potions, Misc
}

int CMarket::GetJewelCount(LPOBJ lpObj, int jewelIndex)
{
	int count = 0;
	int maxSlot = gItemManager.GetInventoryMaxValue(lpObj);

	for (int n = INVENTORY_WEAR_SIZE; n < maxSlot; n++)
	{
		if (lpObj->Inventory[n].IsItem() && lpObj->Inventory[n].m_Index == jewelIndex)
		{
			if (gItemStack.GetItemMaxStack(lpObj->Inventory[n].m_Index, lpObj->Inventory[n].m_Level) <= 0)
			{
				count++;
			}
			else
			{
				count += (int)lpObj->Inventory[n].m_Durability;
			}
		}
	}
	return count;
}

bool CMarket::DeductJewels(LPOBJ lpObj, int jewelIndex, int count)
{
	if (this->GetJewelCount(lpObj, jewelIndex) < count)
	{
		return false;
	}

	gItemManager.DeleteInventoryItemCount(lpObj, jewelIndex, 0, count);
	return true;
}

bool CMarket::AddJewelsToInventory(LPOBJ lpObj, int jewelIndex, int count)
{
	for (int i = 0; i < count; i++)
	{
		CItem item;
		item.Convert(jewelIndex, 0, 0, 0, 0);
		if (gItemManager.InventoryInsertItem(lpObj->Index, item) == 0xFF)
		{
			return false;
		}
	}
	gItemManager.GCItemListSend(lpObj->Index);
	return true;
}

void CMarket::CGMarketPageRecv(CG_MARKET_PAGE_REQ* lpMsg, int aIndex)
{
	LPOBJ lpObj = &gObj[aIndex];
	if (!gObjIsConnectedGP(aIndex)) return;

	SDHP_MARKET_GET_ITEMS_RECV pMsg;
	pMsg.header.set(0x80, 0x01, sizeof(pMsg));
	pMsg.index = aIndex;
	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));
	pMsg.category = lpMsg->category;
	pMsg.page = lpMsg->page;

	gDataServerConnection.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CMarket::CGMarketSellRecv(CG_MARKET_SELL_REQ* lpMsg, int aIndex)
{
	LPOBJ lpObj = &gObj[aIndex];
	if (!gObjIsConnectedGP(aIndex)) return;

	GC_MARKET_SELL_ANS ans;
	ans.header.set(0x98, 0x03, sizeof(ans));
	ans.result = 0;

	if (lpObj->Interface.use != 0 || lpObj->State != OBJECT_PLAYING)
	{
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		return;
	}

	if (!INVENTORY_MAIN_RANGE(lpMsg->slot))
	{
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		return;
	}

	CItem* lpItem = &lpObj->Inventory[lpMsg->slot];
	if (!lpItem->IsItem())
	{
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		return;
	}

	if (lpMsg->price <= 0 || lpMsg->price > 2000000000 || lpMsg->priceType > 4)
	{
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		return;
	}

	SDHP_MARKET_ADD_ITEM_RECV pMsg;
	pMsg.header.set(0x80, 0x02, sizeof(pMsg));
	pMsg.index = aIndex;
	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));
	memcpy(pMsg.name, lpObj->Name, sizeof(pMsg.name));
	gItemManager.DBItemByteConvert(pMsg.itemByte, lpItem);
	pMsg.itemIndex = lpItem->m_Index;
	pMsg.itemLevel = lpItem->m_Level;
	pMsg.itemCategory = this->GetCategoryByItemIndex(lpItem->m_Index);
	pMsg.price = lpMsg->price;
	pMsg.priceType = lpMsg->priceType;

	gItemManager.InventoryDelItem(aIndex, lpMsg->slot);
	gItemManager.GCItemDeleteSend(aIndex, lpMsg->slot, 1);

	gDataServerConnection.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CMarket::CGMarketBuyRecv(CG_MARKET_BUY_REQ* lpMsg, int aIndex)
{
	LPOBJ lpObj = &gObj[aIndex];
	if (!gObjIsConnectedGP(aIndex)) return;

	GC_MARKET_BUY_ANS ans;
	ans.header.set(0x98, 0x02, sizeof(ans));
	ans.result = 0;
	ans.marketId = lpMsg->marketId;

	if (lpObj->Interface.use != 0 || lpObj->State != OBJECT_PLAYING)
	{
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		return;
	}

	SDHP_MARKET_BUY_ITEM_RECV pMsg;
	pMsg.header.set(0x80, 0x03, sizeof(pMsg));
	pMsg.index = aIndex;
	pMsg.marketId = lpMsg->marketId;
	memcpy(pMsg.buyerAccount, lpObj->Account, sizeof(pMsg.buyerAccount));
	memcpy(pMsg.buyerName, lpObj->Name, sizeof(pMsg.buyerName));

	gDataServerConnection.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CMarket::CGMarketMyItemsRecv(CG_MARKET_MY_ITEMS_REQ* lpMsg, int aIndex)
{
	LPOBJ lpObj = &gObj[aIndex];
	if (!gObjIsConnectedGP(aIndex)) return;

	SDHP_MARKET_MY_ITEMS_RECV pMsg;
	pMsg.header.set(0x80, 0x04, sizeof(pMsg));
	pMsg.index = aIndex;
	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));

	gDataServerConnection.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CMarket::CGMarketCancelSellRecv(CG_MARKET_CANCEL_SELL_REQ* lpMsg, int aIndex)
{
	LPOBJ lpObj = &gObj[aIndex];
	if (!gObjIsConnectedGP(aIndex)) return;

	SDHP_MARKET_CANCEL_ITEM_RECV pMsg;
	pMsg.header.set(0x80, 0x05, sizeof(pMsg));
	pMsg.index = aIndex;
	pMsg.marketId = lpMsg->marketId;
	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));

	gDataServerConnection.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CMarket::CGMarketClaimPaymentRecv(CG_MARKET_CLAIM_PAYMENT_REQ* lpMsg, int aIndex)
{
	LPOBJ lpObj = &gObj[aIndex];
	if (!gObjIsConnectedGP(aIndex)) return;

	SDHP_MARKET_CLAIM_ITEM_RECV pMsg;
	pMsg.header.set(0x80, 0x06, sizeof(pMsg));
	pMsg.index = aIndex;
	pMsg.marketId = lpMsg->marketId;
	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));

	gDataServerConnection.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CMarket::DGMarketGetItemsRecv(SDHP_MARKET_GET_ITEMS_SEND* lpMsg)
{
	if (!gObjIsConnectedGP(lpMsg->index)) return;

	BYTE sendBuffer[4096];
	GC_MARKET_PAGE_ANS* pMsg = (GC_MARKET_PAGE_ANS*)sendBuffer;
	pMsg->category = lpMsg->category;
	pMsg->currentPage = lpMsg->page;
	pMsg->totalPages = lpMsg->totalPages;
	pMsg->totalItems = lpMsg->totalItems;
	pMsg->itemCount = lpMsg->count;
	pMsg->myWCoin = lpMsg->myWCoin;

	gObj[lpMsg->index].WCoin = lpMsg->myWCoin;

	SDHP_MARKET_ITEM_INFO* dsItems = (SDHP_MARKET_ITEM_INFO*)(((BYTE*)lpMsg) + sizeof(SDHP_MARKET_GET_ITEMS_SEND));
	MARKET_ITEM_ENTRY* clientItems = (MARKET_ITEM_ENTRY*)(sendBuffer + sizeof(GC_MARKET_PAGE_ANS));

	for (int i = 0; i < lpMsg->count; i++)
	{
		clientItems[i].marketId = dsItems[i].id;
		memcpy(clientItems[i].sellerName, dsItems[i].sellerName, sizeof(clientItems[i].sellerName));
		memcpy(clientItems[i].itemBytes, dsItems[i].itemByte, sizeof(clientItems[i].itemBytes));
		clientItems[i].price = dsItems[i].price;
		clientItems[i].priceType = dsItems[i].priceType;
	}

	int totalSize = sizeof(GC_MARKET_PAGE_ANS) + (lpMsg->count * sizeof(MARKET_ITEM_ENTRY));
	pMsg->header.set(0x98, 0x01, totalSize);
	DataSend(lpMsg->index, sendBuffer, totalSize);
}

void CMarket::DGMarketAddItemRecv(SDHP_MARKET_ADD_ITEM_SEND* lpMsg)
{
	if (!gObjIsConnectedGP(lpMsg->index)) return;

	GC_MARKET_SELL_ANS ans;
	ans.header.set(0x98, 0x03, sizeof(ans));
	ans.result = lpMsg->result;

	DataSend(lpMsg->index, (BYTE*)&ans, sizeof(ans));

	if (lpMsg->result == 1)
	{
		gNotice.GCNoticeSend(lpMsg->index, 1, "[Market] Item listed for sale!");
	}
}

void CMarket::DGMarketBuyItemRecv(SDHP_MARKET_BUY_ITEM_SEND* lpMsg)
{
	if (!gObjIsConnectedGP(lpMsg->index)) return;

	LPOBJ lpObj = &gObj[lpMsg->index];
	GC_MARKET_BUY_ANS ans;
	ans.header.set(0x98, 0x02, sizeof(ans));
	ans.result = 0;
	ans.marketId = lpMsg->marketId;

	if (lpMsg->result == 5 || _stricmp(lpObj->Account, lpMsg->sellerAccount) == 0 || _stricmp(lpObj->Name, lpMsg->sellerName) == 0)
	{
		ans.result = 5;
		DataSend(lpMsg->index, (BYTE*)&ans, sizeof(ans));
		gNotice.GCNoticeSend(lpMsg->index, 1, "[Market] You cannot buy your own item. Cancel it in My Listings.");
		return;
	}

	if (lpMsg->result == 2)
	{
		ans.result = 2;
		DataSend(lpMsg->index, (BYTE*)&ans, sizeof(ans));
		return;
	}

	if (lpMsg->result != 1)
	{
		ans.result = 4;
		DataSend(lpMsg->index, (BYTE*)&ans, sizeof(ans));
		return;
	}

	bool hasEnoughCurrency = false;
	if (lpMsg->priceType == 0) hasEnoughCurrency = (lpObj->Money >= lpMsg->price);
	else if (lpMsg->priceType == 1) hasEnoughCurrency = (this->GetJewelCount(lpObj, GET_ITEM(14, 13)) >= (int)lpMsg->price);
	else if (lpMsg->priceType == 2) hasEnoughCurrency = (this->GetJewelCount(lpObj, GET_ITEM(14, 14)) >= (int)lpMsg->price);
	else if (lpMsg->priceType == 3) hasEnoughCurrency = (this->GetJewelCount(lpObj, GET_ITEM(12, 15)) >= (int)lpMsg->price);
	else if (lpMsg->priceType == 4) hasEnoughCurrency = true;

	if (!hasEnoughCurrency)
	{
		ans.result = 2;
		DataSend(lpMsg->index, (BYTE*)&ans, sizeof(ans));
		return;
	}

	CItem item;
	if (!gItemManager.ConvertItemByte(&item, lpMsg->itemByte))
	{
		ans.result = 0;
		DataSend(lpMsg->index, (BYTE*)&ans, sizeof(ans));
		return;
	}

	if (!gItemManager.CheckItemInventorySpace(lpObj, item.m_Index))
	{
		ans.result = 3;
		DataSend(lpMsg->index, (BYTE*)&ans, sizeof(ans));
		return;
	}

	if (lpMsg->priceType == 0)
	{
		lpObj->Money -= lpMsg->price;
		GCMoneySend(lpObj->Index, lpObj->Money);
	}
	else if (lpMsg->priceType == 1) this->DeductJewels(lpObj, GET_ITEM(14, 13), lpMsg->price);
	else if (lpMsg->priceType == 2) this->DeductJewels(lpObj, GET_ITEM(14, 14), lpMsg->price);
	else if (lpMsg->priceType == 3) this->DeductJewels(lpObj, GET_ITEM(12, 15), lpMsg->price);
	else if (lpMsg->priceType == 4)
	{
		lpObj->WCoin = (lpObj->WCoin >= lpMsg->price) ? (lpObj->WCoin - lpMsg->price) : 0;
		this->GCWCoinSend(lpObj->Index, lpObj->WCoin);
	}

	BYTE targetSlot = gItemManager.InventoryInsertItem(lpObj->Index, item);
	if (targetSlot != 0xFF)
	{
		gItemManager.GCItemListSend(lpObj->Index);
		ans.result = 1;
		DataSend(lpObj->Index, (BYTE*)&ans, sizeof(ans));
		gNotice.GCNoticeSend(lpObj->Index, 1, "[Market] Item purchased successfully!");

		for (int n = OBJECT_START_USER; n < MAX_OBJECT_USER; n++)
		{
			if (gObjIsConnectedGP(n) && strcmp(gObj[n].Account, lpMsg->sellerAccount) == 0)
			{
				LPOBJ lpSeller = &gObj[n];
				DWORD netPrice = lpMsg->price - (lpMsg->price * 5) / 100;
				if (lpMsg->priceType == 0)
				{
					lpSeller->Money = ((QWORD)lpSeller->Money + netPrice > MAX_ITEM_PRICE) ? MAX_ITEM_PRICE : (lpSeller->Money + netPrice);
					GCMoneySend(lpSeller->Index, lpSeller->Money);
					gNotice.GCNoticeSend(lpSeller->Index, 1, "[Market] Item sold! Received %u Zen (5%% tax).", netPrice);
				}
				else if (lpMsg->priceType == 1)
				{
					this->AddJewelsToInventory(lpSeller, GET_ITEM(14, 13), netPrice);
					gNotice.GCNoticeSend(lpSeller->Index, 1, "[Market] Item sold! Received %u Bless (5%% tax).", netPrice);
				}
				else if (lpMsg->priceType == 2)
				{
					this->AddJewelsToInventory(lpSeller, GET_ITEM(14, 14), netPrice);
					gNotice.GCNoticeSend(lpSeller->Index, 1, "[Market] Item sold! Received %u Soul (5%% tax).", netPrice);
				}
				else if (lpMsg->priceType == 3)
				{
					this->AddJewelsToInventory(lpSeller, GET_ITEM(12, 15), netPrice);
					gNotice.GCNoticeSend(lpSeller->Index, 1, "[Market] Item sold! Received %u Chaos (5%% tax).", netPrice);
				}
				else if (lpMsg->priceType == 4)
				{
					lpSeller->WCoin += netPrice;
					this->GCWCoinSend(lpSeller->Index, lpSeller->WCoin);
					gNotice.GCNoticeSend(lpSeller->Index, 1, "[Market] Item sold! Received %u WCoin (5%% tax).", netPrice);
				}
				break;
			}
		}
	}
	else
	{
		ans.result = 3;
		DataSend(lpObj->Index, (BYTE*)&ans, sizeof(ans));
	}
}

void CMarket::DGMarketMyItemsRecv(SDHP_MARKET_MY_ITEMS_SEND* lpMsg)
{
	if (!gObjIsConnectedGP(lpMsg->index)) return;

	LPOBJ lpObj = &gObj[lpMsg->index];
	BYTE sendBuffer[4096];
	GC_MARKET_MY_ITEMS_ANS* pMsg = (GC_MARKET_MY_ITEMS_ANS*)sendBuffer;
	pMsg->count = lpMsg->count;
	pMsg->myWCoin = lpMsg->myWCoin;

	lpObj->WCoin = lpMsg->myWCoin;

	SDHP_MARKET_ITEM_INFO* dsItems = (SDHP_MARKET_ITEM_INFO*)(((BYTE*)lpMsg) + sizeof(SDHP_MARKET_MY_ITEMS_SEND));
	MY_MARKET_ITEM_ENTRY* clientItems = (MY_MARKET_ITEM_ENTRY*)(sendBuffer + sizeof(GC_MARKET_MY_ITEMS_ANS));

	for (int i = 0; i < lpMsg->count; i++)
	{
		clientItems[i].marketId = dsItems[i].id;
		memcpy(clientItems[i].itemBytes, dsItems[i].itemByte, sizeof(clientItems[i].itemBytes));
		clientItems[i].price = dsItems[i].price;
		clientItems[i].priceType = dsItems[i].priceType;
		clientItems[i].status = dsItems[i].status;
		memcpy(clientItems[i].buyerName, dsItems[i].buyerName, sizeof(clientItems[i].buyerName));

		if (dsItems[i].status == 1)
		{
			DWORD netPrice = dsItems[i].price - (dsItems[i].price * 5) / 100;
			if (dsItems[i].priceType == 1) this->AddJewelsToInventory(lpObj, GET_ITEM(14, 13), netPrice);
			else if (dsItems[i].priceType == 2) this->AddJewelsToInventory(lpObj, GET_ITEM(14, 14), netPrice);
			else if (dsItems[i].priceType == 3) this->AddJewelsToInventory(lpObj, GET_ITEM(12, 15), netPrice);

			SDHP_MARKET_CLAIM_ITEM_RECV claimReq;
			claimReq.header.set(0x80, 0x06, sizeof(claimReq));
			claimReq.index = lpObj->Index;
			claimReq.marketId = dsItems[i].id;
			memcpy(claimReq.account, lpObj->Account, sizeof(claimReq.account));
			gDataServerConnection.DataSend((BYTE*)&claimReq, sizeof(claimReq));

			clientItems[i].status = 2;
			gNotice.GCNoticeSend(lpObj->Index, 1, "[Market] Automatically claimed payment for sold item!");
		}
	}

	int totalSize = sizeof(GC_MARKET_MY_ITEMS_ANS) + (lpMsg->count * sizeof(MY_MARKET_ITEM_ENTRY));
	pMsg->header.set(0x98, 0x04, totalSize);
	DataSend(lpMsg->index, sendBuffer, totalSize);
}

void CMarket::DGMarketCancelItemRecv(SDHP_MARKET_CANCEL_ITEM_SEND* lpMsg)
{
	if (!gObjIsConnectedGP(lpMsg->index)) return;

	LPOBJ lpObj = &gObj[lpMsg->index];
	GC_MARKET_CANCEL_SELL_ANS ans;
	ans.header.set(0x98, 0x05, sizeof(ans));
	ans.result = lpMsg->result;
	ans.marketId = lpMsg->marketId;

	if (lpMsg->result == 1)
	{
		CItem item;
		if (gItemManager.ConvertItemByte(&item, lpMsg->itemByte))
		{
			if (gItemManager.InventoryInsertItem(lpObj->Index, item) != 0xFF)
			{
				gItemManager.GCItemListSend(lpObj->Index);
				gNotice.GCNoticeSend(lpObj->Index, 1, "[Market] Listing cancelled and item returned to inventory.");
			}
			else
			{
				ans.result = 2;
			}
		}
	}

	DataSend(lpObj->Index, (BYTE*)&ans, sizeof(ans));
}

void CMarket::DGMarketClaimItemRecv(SDHP_MARKET_CLAIM_ITEM_SEND* lpMsg)
{
	if (!gObjIsConnectedGP(lpMsg->index)) return;

	LPOBJ lpObj = &gObj[lpMsg->index];
	GC_MARKET_CLAIM_PAYMENT_ANS ans;
	ans.header.set(0x98, 0x06, sizeof(ans));
	ans.result = lpMsg->result;
	ans.marketId = lpMsg->marketId;
	ans.price = lpMsg->price;
	ans.priceType = lpMsg->priceType;

	if (lpMsg->result == 1)
	{
		DWORD netPrice = lpMsg->price - (lpMsg->price * 5) / 100;
		if (lpMsg->priceType == 0)
		{
			lpObj->Money = ((QWORD)lpObj->Money + netPrice > MAX_ITEM_PRICE) ? MAX_ITEM_PRICE : (lpObj->Money + netPrice);
			GCMoneySend(lpObj->Index, lpObj->Money);
			gNotice.GCNoticeSend(lpObj->Index, 1, "[Market] Claimed %u Zen (5%% tax)!", netPrice);
		}
		else if (lpMsg->priceType == 1)
		{
			this->AddJewelsToInventory(lpObj, GET_ITEM(14, 13), netPrice);
			gNotice.GCNoticeSend(lpObj->Index, 1, "[Market] Claimed %u Bless (5%% tax)!", netPrice);
		}
		else if (lpMsg->priceType == 2)
		{
			this->AddJewelsToInventory(lpObj, GET_ITEM(14, 14), netPrice);
			gNotice.GCNoticeSend(lpObj->Index, 1, "[Market] Claimed %u Soul (5%% tax)!", netPrice);
		}
		else if (lpMsg->priceType == 3)
		{
			this->AddJewelsToInventory(lpObj, GET_ITEM(12, 15), netPrice);
			gNotice.GCNoticeSend(lpObj->Index, 1, "[Market] Claimed %u Chaos (5%% tax)!", netPrice);
		}
		else if (lpMsg->priceType == 4)
		{
			lpObj->WCoin += netPrice;
			this->GCWCoinSend(lpObj->Index, lpObj->WCoin);
			gNotice.GCNoticeSend(lpObj->Index, 1, "[Market] Claimed %u WCoin (5%% tax)!", netPrice);
		}
	}

	DataSend(lpObj->Index, (BYTE*)&ans, sizeof(ans));
}

void CMarket::GCWCoinSend(int aIndex, DWORD wcoin)
{
	if (!gObjIsConnectedGP(aIndex)) return;

	GC_WCOIN_SEND pMsg;
	pMsg.header.set(0x98, 0x07, sizeof(pMsg));
	pMsg.wcoin = wcoin;
	DataSend(aIndex, (BYTE*)&pMsg, sizeof(pMsg));
}

void CMarket::CGRankingReqRecv(CG_RANKING_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpObj = &gObj[aIndex];

	SDHP_RANKING_GET_RECV pMsg;
	pMsg.header.set(0x80, 0x08, sizeof(pMsg));
	pMsg.index = aIndex;
	pMsg.type = lpMsg->type;
	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));
	gDataServerConnection.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CMarket::CGBuyVipReqRecv(CG_BUY_VIP_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	char buf[16];
	sprintf_s(buf, sizeof(buf), "%u", lpMsg->days);
	this->CommandBuyVip(&gObj[aIndex], buf);
}

void CMarket::CommandBuyVip(LPOBJ lpObj, char* arg)
{
	int days = atoi(arg);
	DWORD cost = 0;

	if (days == 1) cost = 1000;
	else if (days == 7) cost = 6000;
	else if (days == 30) cost = 20000;
	else
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, "[VIP] Usage: /buyvip <1|7|30> (1d=1000, 7d=6000, 30d=20000 WCoin)");
		return;
	}

	SDHP_BUY_VIP_RECV pMsg;
	pMsg.header.set(0x80, 0x07, sizeof(pMsg));
	pMsg.index = lpObj->Index;
	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));
	pMsg.cost = cost;
	pMsg.days = (WORD)days;
	gDataServerConnection.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CMarket::DGBuyVipRecv(SDHP_BUY_VIP_SEND* lpMsg)
{
	if (!gObjIsConnectedGP(lpMsg->index)) return;

	LPOBJ lpObj = &gObj[lpMsg->index];
	lpObj->WCoin = lpMsg->remainWCoin;
	this->GCWCoinSend(lpObj->Index, lpObj->WCoin);

	GC_BUY_VIP_ANS ans;
	ans.header.set(0x98, 0x09, sizeof(ans));
	ans.result = lpMsg->result;
	ans.days = lpMsg->days;
	ans.remainWCoin = lpMsg->remainWCoin;
	DataSend(lpObj->Index, (BYTE*)&ans, sizeof(ans));

	if (lpMsg->result == 2)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, "[VIP] Not enough WCoin! Current balance: %u WCoin.", lpMsg->remainWCoin);
		return;
	}
	if (lpMsg->result != 1)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, "[VIP] Purchase failed. Please try again later.");
		return;
	}

	GJAccountLevelSaveSend(lpObj->Index, 1, (int)lpMsg->days * 86400);
	GJAccountLevelSend(lpObj->Index);
	GCFireworksSend(lpObj, lpObj->X, lpObj->Y);
	gNotice.GCNoticeSend(lpObj->Index, 1, "[VIP] Activated %d days VIP! Remaining: %u WCoin.", lpMsg->days, lpMsg->remainWCoin);
}

void CMarket::CommandTop(LPOBJ lpObj, char* arg)
{
	SDHP_RANKING_GET_RECV pMsg;
	pMsg.header.set(0x80, 0x08, sizeof(pMsg));
	pMsg.index = lpObj->Index;
	pMsg.type = (arg && (_strnicmp(arg, "guild", 5) == 0 || _strnicmp(arg, "g", 1) == 0)) ? 1 : 0;
	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));
	gDataServerConnection.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CMarket::DGRankingGetRecv(SDHP_RANKING_GET_SEND* lpMsg)
{
	if (!gObjIsConnectedGP(lpMsg->index)) return;

	LPOBJ lpObj = &gObj[lpMsg->index];
	lpObj->WCoin = lpMsg->myWCoin;

	GC_RANKING_ANS ans;
	memset(&ans, 0, sizeof(ans));
	ans.header.set(0x98, 0x08, sizeof(ans));
	ans.type = lpMsg->type;
	ans.count = lpMsg->count;
	ans.myWCoin = lpMsg->myWCoin;
	memcpy(ans.entries, lpMsg->entries, sizeof(ans.entries));
	DataSend(lpObj->Index, (BYTE*)&ans, sizeof(ans));
}
