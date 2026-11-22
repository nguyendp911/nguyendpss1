#include "stdafx.h"
#include "MarketManager.h"
#include "QueryManager.h"
#include "SocketManager.h"
#include "Util.h"

CMarketManager gMarketManager;

CMarketManager::CMarketManager()
{
}

CMarketManager::~CMarketManager()
{
}

void CMarketManager::Init()
{
	this->CheckDatabaseSchema();
}

void CMarketManager::CheckDatabaseSchema()
{
#ifndef MYSQL
	gQueryManager.ExecQuery(
		"IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'MarketItems')\n"
		"BEGIN\n"
		"    CREATE TABLE [MarketItems] (\n"
		"        [Id] INT IDENTITY(1,1) PRIMARY KEY,\n"
		"        [AccountID] VARCHAR(10) NOT NULL,\n"
		"        [CharacterName] VARCHAR(10) NOT NULL,\n"
		"        [ItemByte] VARBINARY(10) NOT NULL,\n"
		"        [ItemIndex] INT NOT NULL,\n"
		"        [ItemLevel] INT NOT NULL,\n"
		"        [ItemCategory] INT NOT NULL,\n"
		"        [Price] INT NOT NULL,\n"
		"        [PriceType] TINYINT NOT NULL,\n"
		"        [Status] TINYINT NOT NULL DEFAULT 0,\n"
		"        [BuyerAccount] VARCHAR(10) NULL,\n"
		"        [BuyerName] VARCHAR(10) NULL,\n"
		"        [PostDate] DATETIME DEFAULT GETDATE(),\n"
		"        [SoldDate] DATETIME NULL\n"
		"    );\n"
		"    CREATE INDEX IX_MarketItems_Status ON [MarketItems]([Status], [ItemCategory]);\n"
		"    CREATE INDEX IX_MarketItems_Seller ON [MarketItems]([AccountID], [Status]);\n"
		"END"
	);
	gQueryManager.Close();

	gQueryManager.ExecQuery(
		"IF NOT EXISTS (SELECT * FROM sys.columns WHERE object_id = OBJECT_ID('MEMB_INFO') AND name = 'WCoin')\n"
		"BEGIN\n"
		"    ALTER TABLE [MEMB_INFO] ADD [WCoin] INT NOT NULL DEFAULT 0;\n"
		"END"
	);
	gQueryManager.Close();
#else
	gQueryManager.ExecUpdateQuery(
		"CREATE TABLE IF NOT EXISTS MarketItems ("
		"  Id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,"
		"  AccountID VARCHAR(10) NOT NULL,"
		"  CharacterName VARCHAR(10) NOT NULL,"
		"  ItemByte VARBINARY(10) NOT NULL,"
		"  ItemIndex INT NOT NULL,"
		"  ItemLevel INT NOT NULL,"
		"  ItemCategory INT NOT NULL,"
		"  Price INT NOT NULL,"
		"  PriceType TINYINT UNSIGNED NOT NULL,"
		"  Status TINYINT UNSIGNED NOT NULL DEFAULT 0,"
		"  BuyerAccount VARCHAR(10) NULL,"
		"  BuyerName VARCHAR(10) NULL,"
		"  PostDate DATETIME DEFAULT CURRENT_TIMESTAMP,"
		"  SoldDate DATETIME NULL,"
		"  INDEX IX_MarketItems_Status (Status, ItemCategory),"
		"  INDEX IX_MarketItems_Seller (AccountID, Status)"
		")"
	);
	gQueryManager.Close();

	bool hasWCoin = false;
	if (gQueryManager.ExecResultQuery(
		"SELECT COUNT(*) AS Cnt FROM information_schema.COLUMNS "
		"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'MEMB_INFO' AND COLUMN_NAME = 'WCoin'") && gQueryManager.Fetch())
	{
		hasWCoin = (gQueryManager.GetAsInteger("Cnt") > 0);
	}
	gQueryManager.Close();

	if (!hasWCoin)
	{
		gQueryManager.ExecUpdateQuery("ALTER TABLE MEMB_INFO ADD COLUMN WCoin INT NOT NULL DEFAULT 0");
		gQueryManager.Close();
	}
#endif
	LogAdd(LOG_BLUE, "[MarketManager] Database schema checked successfully");
}

void CMarketManager::GDMarketGetItemsRecv(SDHP_MARKET_GET_ITEMS_RECV* lpMsg, int index)
{
	BYTE sendBuffer[4096];
	SDHP_MARKET_GET_ITEMS_SEND* pMsg = (SDHP_MARKET_GET_ITEMS_SEND*)sendBuffer;
	pMsg->index = lpMsg->index;
	pMsg->category = lpMsg->category;
	pMsg->page = lpMsg->page;
	pMsg->totalPages = 1;
	pMsg->totalItems = 0;
	pMsg->count = 0;
	pMsg->myWCoin = 0;

	const int pageSize = 6;
	int page = lpMsg->page;
	if (page < 1) page = 1;

	char szWhere[128] = "WHERE Status = 0";
	if (lpMsg->category > 0)
	{
		sprintf_s(szWhere, sizeof(szWhere), "WHERE Status = 0 AND ItemCategory = %d", lpMsg->category);
	}

#ifndef MYSQL
	if (gQueryManager.ExecQuery("SELECT WCoin FROM MEMB_INFO WHERE memb___id = '%s'", lpMsg->account) != false && gQueryManager.Fetch() != SQL_NO_DATA)
	{
		pMsg->myWCoin = (DWORD)gQueryManager.GetAsInteger("WCoin");
	}
	gQueryManager.Close();

	if (gQueryManager.ExecQuery("SELECT COUNT(*) AS TotalCount FROM MarketItems %s", szWhere) && gQueryManager.Fetch() != SQL_NO_DATA)
	{
		pMsg->totalItems = (WORD)gQueryManager.GetAsInteger("TotalCount");
	}
	gQueryManager.Close();

	if (pMsg->totalItems > 0)
	{
		pMsg->totalPages = (BYTE)((pMsg->totalItems + pageSize - 1) / pageSize);
		if (page > pMsg->totalPages) page = pMsg->totalPages;
		pMsg->page = (BYTE)page;

		int offset = (page - 1) * pageSize;
		char szQuery[512];
		if (offset == 0)
		{
			sprintf_s(szQuery, sizeof(szQuery),
				"SELECT TOP %d Id, AccountID, CharacterName, ItemByte, ItemIndex, ItemLevel, ItemCategory, Price, PriceType, Status, ISNULL(BuyerName, '') AS BuyerName "
				"FROM MarketItems %s ORDER BY Id DESC",
				pageSize, szWhere);
		}
		else
		{
			sprintf_s(szQuery, sizeof(szQuery),
				"SELECT TOP %d Id, AccountID, CharacterName, ItemByte, ItemIndex, ItemLevel, ItemCategory, Price, PriceType, Status, ISNULL(BuyerName, '') AS BuyerName "
				"FROM MarketItems %s AND Id NOT IN (SELECT TOP %d Id FROM MarketItems %s ORDER BY Id DESC) ORDER BY Id DESC",
				pageSize, szWhere, offset, szWhere);
		}

		if (gQueryManager.ExecQuery(szQuery))
		{
			SDHP_MARKET_ITEM_INFO* itemInfoList = (SDHP_MARKET_ITEM_INFO*)(sendBuffer + sizeof(SDHP_MARKET_GET_ITEMS_SEND));
			while (gQueryManager.Fetch() != SQL_NO_DATA && pMsg->count < pageSize)
			{
				SDHP_MARKET_ITEM_INFO& info = itemInfoList[pMsg->count];
				memset(&info, 0, sizeof(info));
				info.id = gQueryManager.GetAsInteger("Id");
				gQueryManager.GetAsString("AccountID", info.sellerAccount, sizeof(info.sellerAccount));
				gQueryManager.GetAsString("CharacterName", info.sellerName, sizeof(info.sellerName));
				gQueryManager.GetAsBinary("ItemByte", info.itemByte, sizeof(info.itemByte));
				info.itemIndex = gQueryManager.GetAsInteger("ItemIndex");
				info.itemLevel = gQueryManager.GetAsInteger("ItemLevel");
				info.itemCategory = gQueryManager.GetAsInteger("ItemCategory");
				info.price = gQueryManager.GetAsInteger("Price");
				info.priceType = (BYTE)gQueryManager.GetAsInteger("PriceType");
				info.status = (BYTE)gQueryManager.GetAsInteger("Status");
				gQueryManager.GetAsString("BuyerName", info.buyerName, sizeof(info.buyerName));
				pMsg->count++;
			}
		}
		gQueryManager.Close();
	}
#else
	if (gQueryManager.ExecResultQuery("SELECT WCoin FROM MEMB_INFO WHERE memb___id = '%s'", lpMsg->account) && gQueryManager.Fetch())
	{
		pMsg->myWCoin = (DWORD)gQueryManager.GetAsInteger("WCoin");
	}
	gQueryManager.Close();

	if (gQueryManager.ExecResultQuery("SELECT COUNT(*) AS TotalCount FROM MarketItems %s", szWhere) && gQueryManager.Fetch())
	{
		pMsg->totalItems = (WORD)gQueryManager.GetAsInteger("TotalCount");
	}
	gQueryManager.Close();

	if (pMsg->totalItems > 0)
	{
		pMsg->totalPages = (BYTE)((pMsg->totalItems + pageSize - 1) / pageSize);
		if (page > pMsg->totalPages) page = pMsg->totalPages;
		pMsg->page = (BYTE)page;

		int offset = (page - 1) * pageSize;
		if (gQueryManager.ExecResultQuery(
			"SELECT Id, AccountID, CharacterName, ItemByte, ItemIndex, ItemLevel, ItemCategory, Price, PriceType, Status, COALESCE(BuyerName, '') AS BuyerName "
			"FROM MarketItems %s ORDER BY Id DESC LIMIT %d OFFSET %d",
			szWhere, pageSize, offset))
		{
			SDHP_MARKET_ITEM_INFO* itemInfoList = (SDHP_MARKET_ITEM_INFO*)(sendBuffer + sizeof(SDHP_MARKET_GET_ITEMS_SEND));
			while (gQueryManager.Fetch() && pMsg->count < pageSize)
			{
				SDHP_MARKET_ITEM_INFO& info = itemInfoList[pMsg->count];
				memset(&info, 0, sizeof(info));
				info.id = gQueryManager.GetAsInteger("Id");
				gQueryManager.GetAsString("AccountID", info.sellerAccount, sizeof(info.sellerAccount));
				gQueryManager.GetAsString("CharacterName", info.sellerName, sizeof(info.sellerName));
				gQueryManager.GetAsBinary("ItemByte", info.itemByte, sizeof(info.itemByte));
				info.itemIndex = gQueryManager.GetAsInteger("ItemIndex");
				info.itemLevel = gQueryManager.GetAsInteger("ItemLevel");
				info.itemCategory = gQueryManager.GetAsInteger("ItemCategory");
				info.price = gQueryManager.GetAsInteger("Price");
				info.priceType = (BYTE)gQueryManager.GetAsInteger("PriceType");
				info.status = (BYTE)gQueryManager.GetAsInteger("Status");
				gQueryManager.GetAsString("BuyerName", info.buyerName, sizeof(info.buyerName));
				pMsg->count++;
			}
		}
		gQueryManager.Close();
	}
#endif

	int totalPacketSize = sizeof(SDHP_MARKET_GET_ITEMS_SEND) + (pMsg->count * sizeof(SDHP_MARKET_ITEM_INFO));
	pMsg->header.set(0x80, 0x01, totalPacketSize);
	gSocketManager.DataSend(index, sendBuffer, totalPacketSize);
}

void CMarketManager::GDMarketAddItemRecv(SDHP_MARKET_ADD_ITEM_RECV* lpMsg, int index)
{
	SDHP_MARKET_ADD_ITEM_SEND pMsg;
	pMsg.header.set(0x80, 0x02, sizeof(pMsg));
	pMsg.index = lpMsg->index;
	pMsg.result = 0;
	pMsg.marketId = 0;

#ifndef MYSQL
	gQueryManager.BindParameterAsBinary(1, lpMsg->itemByte, sizeof(lpMsg->itemByte));
	if (gQueryManager.ExecQuery(
		"INSERT INTO MarketItems (AccountID, CharacterName, ItemByte, ItemIndex, ItemLevel, ItemCategory, Price, PriceType, Status, PostDate) "
		"VALUES ('%s', '%s', ?, %d, %d, %d, %d, %d, 0, GETDATE())",
		lpMsg->account, lpMsg->name, lpMsg->itemIndex, lpMsg->itemLevel, lpMsg->itemCategory, lpMsg->price, lpMsg->priceType))
	{
		gQueryManager.Close();
		if (gQueryManager.ExecQuery("SELECT ISNULL(CAST(@@IDENTITY AS INT), 0) AS NewId") && gQueryManager.Fetch() != SQL_NO_DATA)
		{
			pMsg.marketId = gQueryManager.GetAsInteger("NewId");
		}
		pMsg.result = 1;
		gQueryManager.Close();
	}
	else
	{
		gQueryManager.Close();
	}
#else
	gQueryManager.PrepareQuery(
		"INSERT INTO MarketItems (AccountID, CharacterName, ItemByte, ItemIndex, ItemLevel, ItemCategory, Price, PriceType, Status, PostDate) "
		"VALUES ('%s', '%s', ?, %d, %d, %d, %u, %d, 0, NOW())",
		lpMsg->account, lpMsg->name, lpMsg->itemIndex, lpMsg->itemLevel, lpMsg->itemCategory, lpMsg->price, lpMsg->priceType);
	gQueryManager.SetAsBinary(1, lpMsg->itemByte, sizeof(lpMsg->itemByte));
	if (gQueryManager.ExecPreparedUpdateQuery())
	{
		gQueryManager.Close();
		if (gQueryManager.ExecResultQuery("SELECT LAST_INSERT_ID() AS NewId") && gQueryManager.Fetch())
		{
			pMsg.marketId = gQueryManager.GetAsInteger("NewId");
		}
		pMsg.result = 1;
		gQueryManager.Close();
	}
	else
	{
		gQueryManager.Close();
	}
#endif

	gSocketManager.DataSend(index, (BYTE*)&pMsg, sizeof(pMsg));
}

void CMarketManager::GDMarketBuyItemRecv(SDHP_MARKET_BUY_ITEM_RECV* lpMsg, int index)
{
	SDHP_MARKET_BUY_ITEM_SEND pMsg;
	pMsg.header.set(0x80, 0x03, sizeof(pMsg));
	pMsg.index = lpMsg->index;
	pMsg.result = 0;
	pMsg.marketId = lpMsg->marketId;
	memset(pMsg.sellerAccount, 0, sizeof(pMsg.sellerAccount));
	memset(pMsg.sellerName, 0, sizeof(pMsg.sellerName));
	memset(pMsg.itemByte, 0xFF, sizeof(pMsg.itemByte));
	pMsg.price = 0;
	pMsg.priceType = 0;

#ifndef MYSQL
	if (gQueryManager.ExecQuery("SELECT AccountID, CharacterName, ItemByte, Price, PriceType FROM MarketItems WHERE Id = %d AND Status = 0", lpMsg->marketId))
	{
		if (gQueryManager.Fetch() != SQL_NO_DATA)
		{
			gQueryManager.GetAsString("AccountID", pMsg.sellerAccount, sizeof(pMsg.sellerAccount));
			gQueryManager.GetAsString("CharacterName", pMsg.sellerName, sizeof(pMsg.sellerName));
			gQueryManager.GetAsBinary("ItemByte", pMsg.itemByte, sizeof(pMsg.itemByte));
			pMsg.price = gQueryManager.GetAsInteger("Price");
			pMsg.priceType = (BYTE)gQueryManager.GetAsInteger("PriceType");
			gQueryManager.Close();

			if (_stricmp(lpMsg->buyerAccount, pMsg.sellerAccount) == 0 || _stricmp(lpMsg->buyerName, pMsg.sellerName) == 0)
			{
				pMsg.result = 5;
				gSocketManager.DataSend(index, (BYTE*)&pMsg, sizeof(pMsg));
				return;
			}

			if (pMsg.priceType == 4)
			{
				int wcoin = 0;
				if (gQueryManager.ExecQuery("SELECT WCoin FROM MEMB_INFO WHERE memb___id = '%s'", lpMsg->buyerAccount) && gQueryManager.Fetch() != SQL_NO_DATA)
				{
					wcoin = gQueryManager.GetAsInteger("WCoin");
				}
				gQueryManager.Close();
				if (wcoin < (int)pMsg.price)
				{
					pMsg.result = 2;
					gSocketManager.DataSend(index, (BYTE*)&pMsg, sizeof(pMsg));
					return;
				}
			}

			if (gQueryManager.ExecQuery("UPDATE MarketItems SET Status = 2, BuyerAccount = '%s', BuyerName = '%s', SoldDate = GETDATE() WHERE Id = %d AND Status = 0",
				lpMsg->buyerAccount, lpMsg->buyerName, lpMsg->marketId))
			{
				pMsg.result = 1;
				gQueryManager.Close();

				if (pMsg.priceType == 0)
				{
					gQueryManager.ExecQuery("UPDATE Character SET Money = CASE WHEN CAST(Money AS BIGINT) + %d > 2000000000 THEN 2000000000 ELSE Money + %d END WHERE AccountID = '%s' AND Name = '%s'",
						pMsg.price, pMsg.price, pMsg.sellerAccount, pMsg.sellerName);
					gQueryManager.Close();
				}
				else if (pMsg.priceType == 4)
				{
					gQueryManager.ExecQuery("UPDATE MEMB_INFO SET WCoin = WCoin - %d WHERE memb___id = '%s'", pMsg.price, lpMsg->buyerAccount);
					gQueryManager.Close();
					gQueryManager.ExecQuery("UPDATE MEMB_INFO SET WCoin = WCoin + %d WHERE memb___id = '%s'", pMsg.price, pMsg.sellerAccount);
					gQueryManager.Close();
				}
			}
			else
			{
				gQueryManager.Close();
			}
		}
		else
		{
			gQueryManager.Close();
		}
	}
#else
	if (gQueryManager.ExecResultQuery("SELECT AccountID, CharacterName, ItemByte, Price, PriceType FROM MarketItems WHERE Id = %u AND Status = 0", lpMsg->marketId))
	{
		if (gQueryManager.Fetch())
		{
			gQueryManager.GetAsString("AccountID", pMsg.sellerAccount, sizeof(pMsg.sellerAccount));
			gQueryManager.GetAsString("CharacterName", pMsg.sellerName, sizeof(pMsg.sellerName));
			gQueryManager.GetAsBinary("ItemByte", pMsg.itemByte, sizeof(pMsg.itemByte));
			pMsg.price = (DWORD)gQueryManager.GetAsInteger("Price");
			pMsg.priceType = (BYTE)gQueryManager.GetAsInteger("PriceType");
			gQueryManager.Close();

			if (_stricmp(lpMsg->buyerAccount, pMsg.sellerAccount) == 0 || _stricmp(lpMsg->buyerName, pMsg.sellerName) == 0)
			{
				pMsg.result = 5;
				gSocketManager.DataSend(index, (BYTE*)&pMsg, sizeof(pMsg));
				return;
			}

			if (pMsg.priceType == 4)
			{
				int wcoin = 0;
				if (gQueryManager.ExecResultQuery("SELECT WCoin FROM MEMB_INFO WHERE memb___id = '%s'", lpMsg->buyerAccount) && gQueryManager.Fetch())
				{
					wcoin = gQueryManager.GetAsInteger("WCoin");
				}
				gQueryManager.Close();
				if (wcoin < (int)pMsg.price)
				{
					pMsg.result = 2;
					gSocketManager.DataSend(index, (BYTE*)&pMsg, sizeof(pMsg));
					return;
				}
			}

			if (gQueryManager.ExecUpdateQuery("UPDATE MarketItems SET Status = 2, BuyerAccount = '%s', BuyerName = '%s', SoldDate = NOW() WHERE Id = %u AND Status = 0",
				lpMsg->buyerAccount, lpMsg->buyerName, lpMsg->marketId))
			{
				pMsg.result = 1;
				gQueryManager.Close();

				DWORD netPrice = pMsg.price - (pMsg.price * 5) / 100;
				if (pMsg.priceType == 0)
				{
					gQueryManager.ExecUpdateQuery("UPDATE `Character` SET Money = LEAST(2000000000, Money + %u) WHERE AccountID = '%s' AND Name = '%s'",
						netPrice, pMsg.sellerAccount, pMsg.sellerName);
					gQueryManager.Close();
				}
				else if (pMsg.priceType == 4)
				{
					gQueryManager.ExecUpdateQuery("UPDATE MEMB_INFO SET WCoin = GREATEST(0, WCoin - %u) WHERE memb___id = '%s'", pMsg.price, lpMsg->buyerAccount);
					gQueryManager.Close();
					gQueryManager.ExecUpdateQuery("UPDATE MEMB_INFO SET WCoin = WCoin + %u WHERE memb___id = '%s'", netPrice, pMsg.sellerAccount);
					gQueryManager.Close();
				}
			}
			else
			{
				gQueryManager.Close();
			}
		}
		else
		{
			gQueryManager.Close();
		}
	}
#endif

	gSocketManager.DataSend(index, (BYTE*)&pMsg, sizeof(pMsg));
}

void CMarketManager::GDMarketGetMyItemsRecv(SDHP_MARKET_MY_ITEMS_RECV* lpMsg, int index)
{
	BYTE sendBuffer[4096];
	SDHP_MARKET_MY_ITEMS_SEND* pMsg = (SDHP_MARKET_MY_ITEMS_SEND*)sendBuffer;
	pMsg->index = lpMsg->index;
	pMsg->count = 0;
	pMsg->myWCoin = 0;

#ifndef MYSQL
	if (gQueryManager.ExecQuery("SELECT WCoin FROM MEMB_INFO WHERE memb___id = '%s'", lpMsg->account) != false && gQueryManager.Fetch() != SQL_NO_DATA)
	{
		pMsg->myWCoin = (DWORD)gQueryManager.GetAsInteger("WCoin");
	}
	gQueryManager.Close();

	if (gQueryManager.ExecQuery(
		"SELECT TOP 20 Id, AccountID, CharacterName, ItemByte, ItemIndex, ItemLevel, ItemCategory, Price, PriceType, Status, ISNULL(BuyerName, '') AS BuyerName "
		"FROM MarketItems WHERE AccountID = '%s' AND (Status = 0 OR Status = 1 OR Status = 2) ORDER BY Status ASC, Id DESC",
		lpMsg->account))
	{
		SDHP_MARKET_ITEM_INFO* itemInfoList = (SDHP_MARKET_ITEM_INFO*)(sendBuffer + sizeof(SDHP_MARKET_MY_ITEMS_SEND));
		while (gQueryManager.Fetch() != SQL_NO_DATA && pMsg->count < 20)
		{
			SDHP_MARKET_ITEM_INFO& info = itemInfoList[pMsg->count];
			memset(&info, 0, sizeof(info));
			info.id = gQueryManager.GetAsInteger("Id");
			gQueryManager.GetAsString("AccountID", info.sellerAccount, sizeof(info.sellerAccount));
			gQueryManager.GetAsString("CharacterName", info.sellerName, sizeof(info.sellerName));
			gQueryManager.GetAsBinary("ItemByte", info.itemByte, sizeof(info.itemByte));
			info.itemIndex = gQueryManager.GetAsInteger("ItemIndex");
			info.itemLevel = gQueryManager.GetAsInteger("ItemLevel");
			info.itemCategory = gQueryManager.GetAsInteger("ItemCategory");
			info.price = gQueryManager.GetAsInteger("Price");
			info.priceType = (BYTE)gQueryManager.GetAsInteger("PriceType");
			info.status = (BYTE)gQueryManager.GetAsInteger("Status");
			gQueryManager.GetAsString("BuyerName", info.buyerName, sizeof(info.buyerName));
			pMsg->count++;
		}
	}
	gQueryManager.Close();
#else
	if (gQueryManager.ExecResultQuery("SELECT WCoin FROM MEMB_INFO WHERE memb___id = '%s'", lpMsg->account) && gQueryManager.Fetch())
	{
		pMsg->myWCoin = (DWORD)gQueryManager.GetAsInteger("WCoin");
	}
	gQueryManager.Close();

	if (gQueryManager.ExecResultQuery(
		"SELECT Id, AccountID, CharacterName, ItemByte, ItemIndex, ItemLevel, ItemCategory, Price, PriceType, Status, COALESCE(BuyerName, '') AS BuyerName "
		"FROM MarketItems WHERE AccountID = '%s' AND (Status = 0 OR Status = 1 OR Status = 2) ORDER BY Status ASC, Id DESC LIMIT 20",
		lpMsg->account))
	{
		SDHP_MARKET_ITEM_INFO* itemInfoList = (SDHP_MARKET_ITEM_INFO*)(sendBuffer + sizeof(SDHP_MARKET_MY_ITEMS_SEND));
		while (gQueryManager.Fetch() && pMsg->count < 20)
		{
			SDHP_MARKET_ITEM_INFO& info = itemInfoList[pMsg->count];
			memset(&info, 0, sizeof(info));
			info.id = gQueryManager.GetAsInteger("Id");
			gQueryManager.GetAsString("AccountID", info.sellerAccount, sizeof(info.sellerAccount));
			gQueryManager.GetAsString("CharacterName", info.sellerName, sizeof(info.sellerName));
			gQueryManager.GetAsBinary("ItemByte", info.itemByte, sizeof(info.itemByte));
			info.itemIndex = gQueryManager.GetAsInteger("ItemIndex");
			info.itemLevel = gQueryManager.GetAsInteger("ItemLevel");
			info.itemCategory = gQueryManager.GetAsInteger("ItemCategory");
			info.price = (DWORD)gQueryManager.GetAsInteger("Price");
			info.priceType = (BYTE)gQueryManager.GetAsInteger("PriceType");
			info.status = (BYTE)gQueryManager.GetAsInteger("Status");
			gQueryManager.GetAsString("BuyerName", info.buyerName, sizeof(info.buyerName));
			pMsg->count++;
		}
	}
	gQueryManager.Close();
#endif

	int totalPacketSize = sizeof(SDHP_MARKET_MY_ITEMS_SEND) + (pMsg->count * sizeof(SDHP_MARKET_ITEM_INFO));
	pMsg->header.set(0x80, 0x04, totalPacketSize);
	gSocketManager.DataSend(index, sendBuffer, totalPacketSize);
}

void CMarketManager::GDMarketCancelItemRecv(SDHP_MARKET_CANCEL_ITEM_RECV* lpMsg, int index)
{
	SDHP_MARKET_CANCEL_ITEM_SEND pMsg;
	pMsg.header.set(0x80, 0x05, sizeof(pMsg));
	pMsg.index = lpMsg->index;
	pMsg.result = 0;
	pMsg.marketId = lpMsg->marketId;
	memset(pMsg.itemByte, 0xFF, sizeof(pMsg.itemByte));

#ifndef MYSQL
	if (gQueryManager.ExecQuery("SELECT ItemByte FROM MarketItems WHERE Id = %d AND AccountID = '%s' AND Status = 0", lpMsg->marketId, lpMsg->account))
	{
		if (gQueryManager.Fetch() != SQL_NO_DATA)
		{
			gQueryManager.GetAsBinary("ItemByte", pMsg.itemByte, sizeof(pMsg.itemByte));
			gQueryManager.Close();

			if (gQueryManager.ExecQuery("UPDATE MarketItems SET Status = 3 WHERE Id = %d AND AccountID = '%s' AND Status = 0", lpMsg->marketId, lpMsg->account))
			{
				pMsg.result = 1;
			}
			gQueryManager.Close();
		}
		else
		{
			gQueryManager.Close();
		}
	}
#else
	if (gQueryManager.ExecResultQuery("SELECT ItemByte FROM MarketItems WHERE Id = %u AND AccountID = '%s' AND Status = 0", lpMsg->marketId, lpMsg->account))
	{
		if (gQueryManager.Fetch())
		{
			gQueryManager.GetAsBinary("ItemByte", pMsg.itemByte, sizeof(pMsg.itemByte));
			gQueryManager.Close();

			if (gQueryManager.ExecUpdateQuery("UPDATE MarketItems SET Status = 3 WHERE Id = %u AND AccountID = '%s' AND Status = 0", lpMsg->marketId, lpMsg->account))
			{
				pMsg.result = 1;
			}
			gQueryManager.Close();
		}
		else
		{
			gQueryManager.Close();
		}
	}
#endif

	gSocketManager.DataSend(index, (BYTE*)&pMsg, sizeof(pMsg));
}


void CMarketManager::GDMarketClaimItemRecv(SDHP_MARKET_CLAIM_ITEM_RECV* lpMsg, int index)
{
	SDHP_MARKET_CLAIM_ITEM_SEND pMsg;
	pMsg.header.set(0x80, 0x06, sizeof(pMsg));
	pMsg.index = lpMsg->index;
	pMsg.result = 0;
	pMsg.marketId = lpMsg->marketId;
	pMsg.price = 0;
	pMsg.priceType = 0;

#ifndef MYSQL
	if (gQueryManager.ExecQuery("SELECT Price, PriceType FROM MarketItems WHERE Id = %d AND AccountID = '%s' AND Status = 1", lpMsg->marketId, lpMsg->account))
	{
		if (gQueryManager.Fetch() != SQL_NO_DATA)
		{
			pMsg.price = gQueryManager.GetAsInteger("Price");
			pMsg.priceType = (BYTE)gQueryManager.GetAsInteger("PriceType");
			gQueryManager.Close();

			if (gQueryManager.ExecQuery("UPDATE MarketItems SET Status = 2 WHERE Id = %d AND AccountID = '%s' AND Status = 1", lpMsg->marketId, lpMsg->account))
			{
				pMsg.result = 1;
				if (pMsg.priceType == 4)
				{
					gQueryManager.ExecQuery("UPDATE MEMB_INFO SET WCoin = WCoin + %d WHERE memb___id = '%s'", pMsg.price, lpMsg->account);
					gQueryManager.Close();
				}
			}
			gQueryManager.Close();
		}
		else
		{
			gQueryManager.Close();
		}
	}
#else
	if (gQueryManager.ExecResultQuery("SELECT Price, PriceType FROM MarketItems WHERE Id = %u AND AccountID = '%s' AND Status = 1", lpMsg->marketId, lpMsg->account))
	{
		if (gQueryManager.Fetch())
		{
			pMsg.price = (DWORD)gQueryManager.GetAsInteger("Price");
			pMsg.priceType = (BYTE)gQueryManager.GetAsInteger("PriceType");
			gQueryManager.Close();

			if (gQueryManager.ExecUpdateQuery("UPDATE MarketItems SET Status = 2 WHERE Id = %u AND AccountID = '%s' AND Status = 1", lpMsg->marketId, lpMsg->account))
			{
				pMsg.result = 1;
				gQueryManager.Close();
				if (pMsg.priceType == 4)
				{
					DWORD netPrice = pMsg.price - (pMsg.price * 5) / 100;
					gQueryManager.ExecUpdateQuery("UPDATE MEMB_INFO SET WCoin = WCoin + %u WHERE memb___id = '%s'", netPrice, lpMsg->account);
					gQueryManager.Close();
				}
			}
			else
			{
				gQueryManager.Close();
			}
		}
		else
		{
			gQueryManager.Close();
		}
	}
#endif

	gSocketManager.DataSend(index, (BYTE*)&pMsg, sizeof(pMsg));
}

void CMarketManager::GDBuyVipRecv(SDHP_BUY_VIP_RECV* lpMsg, int index)
{
	SDHP_BUY_VIP_SEND pMsg;
	pMsg.header.set(0x80, 0x07, sizeof(pMsg));
	pMsg.index = lpMsg->index;
	pMsg.result = 0;
	pMsg.days = lpMsg->days;
	pMsg.remainWCoin = 0;

#ifndef MYSQL
	if (gQueryManager.ExecQuery("SELECT ISNULL(WCoin, 0) AS WCoin FROM MEMB_INFO WHERE memb___id = '%s'", lpMsg->account) && gQueryManager.Fetch() != SQL_NO_DATA)
	{
		DWORD cur = (DWORD)gQueryManager.GetAsInteger("WCoin");
		gQueryManager.Close();
		if (cur < lpMsg->cost)
		{
			pMsg.result = 2;
			pMsg.remainWCoin = cur;
		}
		else if (gQueryManager.ExecQuery("UPDATE MEMB_INFO SET WCoin = WCoin - %u WHERE memb___id = '%s'", lpMsg->cost, lpMsg->account))
		{
			gQueryManager.Close();
			pMsg.result = 1;
			pMsg.remainWCoin = cur - lpMsg->cost;
		}
		else
		{
			gQueryManager.Close();
		}
	}
	else
	{
		gQueryManager.Close();
	}
#else
	if (gQueryManager.ExecResultQuery("SELECT COALESCE(WCoin, 0) AS WCoin FROM MEMB_INFO WHERE memb___id = '%s'", lpMsg->account) && gQueryManager.Fetch())
	{
		DWORD cur = (DWORD)gQueryManager.GetAsInteger("WCoin");
		gQueryManager.Close();
		if (cur < lpMsg->cost)
		{
			pMsg.result = 2;
			pMsg.remainWCoin = cur;
		}
		else if (gQueryManager.ExecUpdateQuery("UPDATE MEMB_INFO SET WCoin = GREATEST(0, WCoin - %u) WHERE memb___id = '%s'", lpMsg->cost, lpMsg->account))
		{
			gQueryManager.Close();
			pMsg.result = 1;
			pMsg.remainWCoin = cur - lpMsg->cost;
		}
		else
		{
			gQueryManager.Close();
		}
	}
	else
	{
		gQueryManager.Close();
	}
#endif

	gSocketManager.DataSend(index, (BYTE*)&pMsg, sizeof(pMsg));
}

void CMarketManager::GDRankingGetRecv(SDHP_RANKING_GET_RECV* lpMsg, int index)
{
	SDHP_RANKING_GET_SEND pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.header.set(0x80, 0x08, sizeof(pMsg));
	pMsg.index = lpMsg->index;
	pMsg.type = lpMsg->type;
	pMsg.count = 0;
	pMsg.myWCoin = 0;

	const char* classWhere = "";
	if (lpMsg->type == 2) classWhere = "WHERE Class < 16 ";
	else if (lpMsg->type == 3) classWhere = "WHERE Class >= 16 AND Class < 32 ";
	else if (lpMsg->type == 4) classWhere = "WHERE Class >= 32 AND Class < 48 ";
	else if (lpMsg->type == 5) classWhere = "WHERE Class >= 48 ";

#ifndef MYSQL
	if (lpMsg->account[0] != '\0')
	{
		if (gQueryManager.ExecQuery("SELECT ISNULL(WCoin, 0) AS WCoin FROM MEMB_INFO WHERE memb___id = '%s'", lpMsg->account) && gQueryManager.Fetch() != SQL_NO_DATA)
		{
			pMsg.myWCoin = (DWORD)gQueryManager.GetAsInteger("WCoin");
		}
		gQueryManager.Close();
	}

	if (lpMsg->type == 1)
	{
		if (gQueryManager.ExecQuery("SELECT TOP 10 G_Name, ISNULL(G_Score, 0) AS G_Score FROM Guild ORDER BY G_Score DESC"))
		{
			while (gQueryManager.Fetch() != SQL_NO_DATA && pMsg.count < 10)
			{
				gQueryManager.GetAsString("G_Name", pMsg.entries[pMsg.count].name, sizeof(pMsg.entries[pMsg.count].name));
				pMsg.entries[pMsg.count].value = (DWORD)gQueryManager.GetAsInteger("G_Score");
				pMsg.count++;
			}
		}
		gQueryManager.Close();
	}
	else
	{
		if (gQueryManager.ExecQuery("SELECT TOP 10 Name, cLevel, Class FROM Character %sORDER BY cLevel DESC, Experience DESC", classWhere))
		{
			while (gQueryManager.Fetch() != SQL_NO_DATA && pMsg.count < 10)
			{
				gQueryManager.GetAsString("Name", pMsg.entries[pMsg.count].name, sizeof(pMsg.entries[pMsg.count].name));
				pMsg.entries[pMsg.count].value = (DWORD)gQueryManager.GetAsInteger("cLevel");
				pMsg.entries[pMsg.count].extra = (BYTE)gQueryManager.GetAsInteger("Class");
				pMsg.count++;
			}
		}
		gQueryManager.Close();
	}
#else
	if (lpMsg->account[0] != '\0')
	{
		if (gQueryManager.ExecResultQuery("SELECT COALESCE(WCoin, 0) AS WCoin FROM MEMB_INFO WHERE memb___id = '%s'", lpMsg->account) && gQueryManager.Fetch())
		{
			pMsg.myWCoin = (DWORD)gQueryManager.GetAsInteger("WCoin");
		}
		gQueryManager.Close();
	}

	if (lpMsg->type == 1)
	{
		if (gQueryManager.ExecResultQuery("SELECT G_Name, COALESCE(G_Score, 0) AS G_Score FROM Guild ORDER BY G_Score DESC LIMIT 10"))
		{
			while (gQueryManager.Fetch() && pMsg.count < 10)
			{
				gQueryManager.GetAsString("G_Name", pMsg.entries[pMsg.count].name, sizeof(pMsg.entries[pMsg.count].name));
				pMsg.entries[pMsg.count].value = (DWORD)gQueryManager.GetAsInteger("G_Score");
				pMsg.count++;
			}
		}
		gQueryManager.Close();
	}
	else
	{
		if (gQueryManager.ExecResultQuery("SELECT Name, cLevel, Class FROM `Character` %sORDER BY cLevel DESC, Experience DESC LIMIT 10", classWhere))
		{
			while (gQueryManager.Fetch() && pMsg.count < 10)
			{
				gQueryManager.GetAsString("Name", pMsg.entries[pMsg.count].name, sizeof(pMsg.entries[pMsg.count].name));
				pMsg.entries[pMsg.count].value = (DWORD)gQueryManager.GetAsInteger("cLevel");
				pMsg.entries[pMsg.count].extra = (BYTE)gQueryManager.GetAsInteger("Class");
				pMsg.count++;
			}
		}
		gQueryManager.Close();
	}
#endif

	gSocketManager.DataSend(index, (BYTE*)&pMsg, sizeof(pMsg));
}

void CMarketManager::GDCashShopBuyRecv(SDHP_CASHSHOP_BUY_RECV* lpMsg, int index)
{
	SDHP_CASHSHOP_BUY_SEND pMsg;
	pMsg.header.set(0x80, 0x09, sizeof(pMsg));
	pMsg.index = lpMsg->index;
	pMsg.result = 0;
	pMsg.product = lpMsg->product;
	pMsg.param = lpMsg->param;
	pMsg.remainWCoin = 0;

#ifndef MYSQL
	if (gQueryManager.ExecQuery("SELECT ISNULL(WCoin, 0) AS WCoin FROM MEMB_INFO WHERE memb___id = '%s'", lpMsg->account) && gQueryManager.Fetch() != SQL_NO_DATA)
	{
		DWORD cur = (DWORD)gQueryManager.GetAsInteger("WCoin");
		gQueryManager.Close();
		if (lpMsg->product == 0 || lpMsg->cost == 0)
		{
			pMsg.result = 1;
			pMsg.remainWCoin = cur;
		}
		else if (cur < lpMsg->cost)
		{
			pMsg.result = 2;
			pMsg.remainWCoin = cur;
		}
		else if (gQueryManager.ExecQuery("UPDATE MEMB_INFO SET WCoin = WCoin - %u WHERE memb___id = '%s'", lpMsg->cost, lpMsg->account))
		{
			gQueryManager.Close();
			pMsg.result = 1;
			pMsg.remainWCoin = cur - lpMsg->cost;
		}
		else
		{
			gQueryManager.Close();
		}
	}
	else
	{
		gQueryManager.Close();
	}
#else
	if (gQueryManager.ExecResultQuery("SELECT COALESCE(WCoin, 0) AS WCoin FROM MEMB_INFO WHERE memb___id = '%s'", lpMsg->account) && gQueryManager.Fetch())
	{
		DWORD cur = (DWORD)gQueryManager.GetAsInteger("WCoin");
		gQueryManager.Close();
		if (lpMsg->product == 0 || lpMsg->cost == 0)
		{
			pMsg.result = 1;
			pMsg.remainWCoin = cur;
		}
		else if (cur < lpMsg->cost)
		{
			pMsg.result = 2;
			pMsg.remainWCoin = cur;
		}
		else if (gQueryManager.ExecUpdateQuery("UPDATE MEMB_INFO SET WCoin = GREATEST(0, WCoin - %u) WHERE memb___id = '%s'", lpMsg->cost, lpMsg->account))
		{
			gQueryManager.Close();
			pMsg.result = 1;
			pMsg.remainWCoin = cur - lpMsg->cost;
		}
		else
		{
			gQueryManager.Close();
		}
	}
	else
	{
		gQueryManager.Close();
	}
#endif

	gSocketManager.DataSend(index, (BYTE*)&pMsg, sizeof(pMsg));
}