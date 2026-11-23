#include "stdafx.h"
#include "CashShop.h"
#include "DefaultClassInfo.h"
#include "GameMain.h"
#include "ItemManager.h"
#include "JSProtocol.h"
#include "Market.h"
#include "Notice.h"
#include "ObjectManager.h"
#include "Path.h"
#include "Protocol.h"
#include "Quest.h"
#include "ResetTable.h"
#include "ServerInfo.h"
#include "SkillManager.h"
#include "User.h"
#include "Util.h"

CCashShop gCashShop;

CCashShop::CCashShop()
{
	this->m_Enable = false;
	this->m_Count = 0;
	memset(this->m_Products, 0, sizeof(this->m_Products));

	this->m_SePayEnable = false;
	this->m_SePayRateVnd = 100000;
	this->m_SePayRateWcoin = 100000;
	this->m_SePayMinAmount = 10000;
	memset(this->m_SePayBankCode, 0, sizeof(this->m_SePayBankCode));
	memset(this->m_SePayBankAccount, 0, sizeof(this->m_SePayBankAccount));
	memset(this->m_SePayBankOwner, 0, sizeof(this->m_SePayBankOwner));
}

CCashShop::~CCashShop()
{
}

void CCashShop::Load(const char* path)
{
	this->m_Enable = (GetPrivateProfileIntA("CashShop", "Enable", 1, path) != 0);
	this->m_Count = 0;
	memset(this->m_Products, 0, sizeof(this->m_Products));

	for (int i = 1; i <= CASHSHOP_MAX_PRODUCT; i++)
	{
		char sec[32];
		sprintf_s(sec, sizeof(sec), "Item%d", i);

		char name[32] = { 0 };
		GetPrivateProfileStringA(sec, "Name", "", name, sizeof(name), path);
		if (name[0] == '\0')
		{
			continue;
		}

		CASHSHOP_PRODUCT& p = this->m_Products[this->m_Count];
		p.type = (BYTE)GetPrivateProfileIntA(sec, "Type", 0, path);
		p.category = (BYTE)GetPrivateProfileIntA(sec, "Category", 1, path);
		int itemSec = GetPrivateProfileIntA(sec, "Section", 14, path);
		int itemIdx = GetPrivateProfileIntA(sec, "Index", 13, path);
		p.item = (WORD)GET_ITEM(itemSec, itemIdx);
		p.level = (BYTE)GetPrivateProfileIntA(sec, "Level", 0, path);
		p.dur = (BYTE)GetPrivateProfileIntA(sec, "Dur", 1, path);
		p.skill = (BYTE)GetPrivateProfileIntA(sec, "Skill", 0, path);
		p.luck = (BYTE)GetPrivateProfileIntA(sec, "Luck", 0, path);
		p.opt = (BYTE)GetPrivateProfileIntA(sec, "Opt", 0, path);
		p.exc = (BYTE)GetPrivateProfileIntA(sec, "Exc", 0, path);
		p.param = (WORD)GetPrivateProfileIntA(sec, "Param", 1, path);
		p.price = (DWORD)GetPrivateProfileIntA(sec, "Price", 100, path);
		strcpy_s(p.name, sizeof(p.name), name);
		GetPrivateProfileStringA(sec, "Desc", "", p.description, sizeof(p.description), path);

		this->m_Count++;
	}

	LogAdd(LOG_BLUE, "[CashShop] Loaded %d products (Enable=%d)", this->m_Count, this->m_Enable ? 1 : 0);

	// Load SePay QR Top-Up config from Data\Custom\SePay.ini
	const char* sepayPath = gPath.GetFullPath("Custom\\SePay.ini");
	this->m_SePayEnable = (GetPrivateProfileIntA("SePay", "Enable", 1, sepayPath) != 0);
	this->m_SePayRateVnd = (DWORD)GetPrivateProfileIntA("SePay", "RateVnd", 100000, sepayPath);
	this->m_SePayRateWcoin = (DWORD)GetPrivateProfileIntA("SePay", "RateWcoin", 100000, sepayPath);
	this->m_SePayMinAmount = (DWORD)GetPrivateProfileIntA("SePay", "MinAmount", 10000, sepayPath);
	if (this->m_SePayRateVnd == 0) this->m_SePayRateVnd = 100000;
	if (this->m_SePayRateWcoin == 0) this->m_SePayRateWcoin = 100000;
	GetPrivateProfileStringA("SePay", "BankCode", "MB", this->m_SePayBankCode, sizeof(this->m_SePayBankCode), sepayPath);
	GetPrivateProfileStringA("SePay", "BankAccount", "", this->m_SePayBankAccount, sizeof(this->m_SePayBankAccount), sepayPath);
	GetPrivateProfileStringA("SePay", "BankOwner", "", this->m_SePayBankOwner, sizeof(this->m_SePayBankOwner), sepayPath);
	LogAdd(LOG_BLUE, "[SePay] Enable=%d Bank=%s Acc=%s Rate=%uVND=%uWC",
		this->m_SePayEnable ? 1 : 0, this->m_SePayBankCode, this->m_SePayBankAccount, this->m_SePayRateVnd, this->m_SePayRateWcoin);
}

void CCashShop::SendSePayInfo(int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpObj = &gObj[aIndex];

	PMSG_SEPAY_INFO pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.header.set(0x99, 0x30, sizeof(pMsg));
	pMsg.enable = this->m_SePayEnable ? 1 : 0;
	pMsg.rateVnd = this->m_SePayRateVnd;
	pMsg.rateWcoin = this->m_SePayRateWcoin;
	pMsg.minAmount = this->m_SePayMinAmount;
	strcpy_s(pMsg.bankCode, sizeof(pMsg.bankCode), this->m_SePayBankCode);
	strcpy_s(pMsg.bankAccount, sizeof(pMsg.bankAccount), this->m_SePayBankAccount);
	strcpy_s(pMsg.bankOwner, sizeof(pMsg.bankOwner), this->m_SePayBankOwner);
	strcpy_s(pMsg.account, sizeof(pMsg.account), lpObj->Account);
	DataSend(aIndex, (BYTE*)&pMsg, sizeof(pMsg));
}

void CCashShop::SendList(int aIndex, DWORD wcoin)
{
	if (!gObjIsConnectedGP(aIndex)) return;

	this->SendSePayInfo(aIndex);

	BYTE sendBuf[8192];
	PMSG_CASHSHOP_LIST_ANS* pMsg = (PMSG_CASHSHOP_LIST_ANS*)sendBuf;
	pMsg->enable = this->m_Enable ? 1 : 0;
	pMsg->wcoin = wcoin;
	pMsg->count = (BYTE)this->m_Count;

	PMSG_CASHSHOP_ENTRY* entries = (PMSG_CASHSHOP_ENTRY*)(sendBuf + sizeof(PMSG_CASHSHOP_LIST_ANS));
	for (int i = 0; i < this->m_Count; i++)
	{
		entries[i].type = this->m_Products[i].type;
		entries[i].item = this->m_Products[i].item;
		entries[i].level = this->m_Products[i].level;
		entries[i].category = this->m_Products[i].category;
		entries[i].price = this->m_Products[i].price;
		memcpy(entries[i].name, this->m_Products[i].name, sizeof(entries[i].name));
		memcpy(entries[i].description, this->m_Products[i].description, sizeof(entries[i].description));
	}

	int totalSize = sizeof(PMSG_CASHSHOP_LIST_ANS) + (this->m_Count * sizeof(PMSG_CASHSHOP_ENTRY));
	pMsg->header.set(0x98, 0x11, totalSize);
	DataSend(aIndex, sendBuf, totalSize);
}

void CCashShop::CGCashShopInfoRecv(PMSG_CASHSHOP_INFO_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpObj = &gObj[aIndex];

	SDHP_CASHSHOP_BUY_RECV pMsg;
	pMsg.header.set(0x80, 0x09, sizeof(pMsg));
	pMsg.index = aIndex;
	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));
	pMsg.product = 0; // query WCoin & send list
	pMsg.param = 0;
	pMsg.cost = 0;
	gDataServerConnection.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CCashShop::CGCashShopBuyRecv(PMSG_CASHSHOP_BUY_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpObj = &gObj[aIndex];

	PMSG_CASHSHOP_BUY_ANS ans;
	ans.header.set(0x98, 0x10, sizeof(ans));
	ans.result = 0;
	ans.product = lpMsg->product;
	ans.wcoin = lpObj->WCoin;

	if (!this->m_Enable)
	{
		ans.result = 4;
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		return;
	}

	if (lpMsg->product < 1 || lpMsg->product > this->m_Count)
	{
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		return;
	}

	const CASHSHOP_PRODUCT& prod = this->m_Products[lpMsg->product - 1];

	if (prod.type == 0)
	{
		if (gItemManager.CheckItemInventorySpace(lpObj, prod.item) == 0)
		{
			ans.result = 3;
			DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
			gNotice.GCNoticeSend(aIndex, 1, "[CashShop] Not enough space in your inventory!");
			return;
		}
	}
	else if (prod.type == 2)
	{
		if (lpMsg->param > 3 || lpMsg->param == lpObj->Class)
		{
			ans.result = 5;
			DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
			gNotice.GCNoticeSend(aIndex, 1, "[CashShop] Please select a different class!");
			return;
		}

		for (int i = 0; i < INVENTORY_WEAR_SIZE; i++)
		{
			if (lpObj->Inventory[i].IsItem() != 0)
			{
				ans.result = 6;
				DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
				gNotice.GCNoticeSend(aIndex, 1, "[CashShop] Please unequip all items before changing class!");
				return;
			}
		}
	}

	SDHP_CASHSHOP_BUY_RECV pMsg;
	pMsg.header.set(0x80, 0x09, sizeof(pMsg));
	pMsg.index = aIndex;
	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));
	pMsg.product = lpMsg->product;
	pMsg.param = lpMsg->param;
	pMsg.cost = prod.price;
	gDataServerConnection.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CCashShop::ApplyClassChange(LPOBJ lpObj, int targetClass)
{
	lpObj->Class = targetClass;
	lpObj->ChangeUp = (targetClass == CLASS_MG) ? 0 : lpObj->ChangeUp;
	lpObj->DBClass = (BYTE)(targetClass * 16 + lpObj->ChangeUp);

	int point = gResetTable.GetResetPoint(lpObj);
	point = (point * gServerInfo.m_CommandResetPointRate[lpObj->Class]) / 100;
	point += (lpObj->Level - 1) * gServerInfo.m_LevelUpPoint[lpObj->Class];
	point += ((gQuest.CheckQuestListState(lpObj, 2, QUEST_FINISH) == 0) ? 0 : ((lpObj->Level > 220) ? ((lpObj->Level - 220) * gServerInfo.m_PlusStatPoint) : 0));
	point += gQuest.GetQuestRewardLevelUpPoint(lpObj);
	point += lpObj->FruitAddPoint;

	lpObj->LevelUpPoint = point;
	lpObj->Strength = gDefaultClassInfo.m_DefaultClassInfo[lpObj->Class].Strength;
	lpObj->Dexterity = gDefaultClassInfo.m_DefaultClassInfo[lpObj->Class].Dexterity;
	lpObj->Vitality = gDefaultClassInfo.m_DefaultClassInfo[lpObj->Class].Vitality;
	lpObj->Energy = gDefaultClassInfo.m_DefaultClassInfo[lpObj->Class].Energy;

	for (int n = 0; n < MAX_SKILL_LIST; n++)
	{
		lpObj->Skill[n].Clear();
	}

	gSkillManager.GCSkillListSend(lpObj);
	gObjectManager.CharacterCalcAttribute(lpObj->Index);
	GCNewCharacterInfoSend(lpObj);
	GDCharacterInfoSaveSend(lpObj->Index);

	gNotice.GCNoticeSend(lpObj->Index, 1, "[CashShop] Class changed! Returning to character select...");
	lpObj->CloseCount = 3;
	lpObj->CloseType = 1;
}

void CCashShop::DGCashShopBuyRecv(SDHP_CASHSHOP_BUY_SEND* lpMsg)
{
	if (!gObjIsConnectedGP(lpMsg->index)) return;
	LPOBJ lpObj = &gObj[lpMsg->index];

	lpObj->WCoin = lpMsg->remainWCoin;
	gMarket.GCWCoinSend(lpObj->Index, lpObj->WCoin);

	// Product 0 = Info/List refresh
	if (lpMsg->product == 0)
	{
		this->SendList(lpObj->Index, lpMsg->remainWCoin);
		return;
	}

	PMSG_CASHSHOP_BUY_ANS ans;
	ans.header.set(0x98, 0x10, sizeof(ans));
	ans.result = lpMsg->result;
	ans.product = lpMsg->product;
	ans.wcoin = lpMsg->remainWCoin;

	if (lpMsg->result == 1 && lpMsg->product >= 1 && lpMsg->product <= this->m_Count)
	{
		const CASHSHOP_PRODUCT& prod = this->m_Products[lpMsg->product - 1];

		if (prod.type == 0)
		{
			GDCreateItemSend(lpObj->Index, 0xEB, 0, 0, prod.item, prod.level, prod.dur, prod.skill, prod.luck, prod.opt, -1, prod.exc);
			gNotice.GCNoticeSend(lpObj->Index, 1, "[CashShop] Purchased %s! Remaining: %u WCoin.", prod.name, lpMsg->remainWCoin);
		}
		else if (prod.type == 1)
		{
			GJAccountLevelSaveSend(lpObj->Index, 1, (int)prod.param * 86400);
			GJAccountLevelSend(lpObj->Index);
			GCFireworksSend(lpObj, lpObj->X, lpObj->Y);
			gNotice.GCNoticeSend(lpObj->Index, 1, "[CashShop] Activated %s! Remaining: %u WCoin.", prod.name, lpMsg->remainWCoin);
		}
		else if (prod.type == 2)
		{
			this->ApplyClassChange(lpObj, lpMsg->param);
		}
	}
	else if (lpMsg->result == 2)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, "[CashShop] Not enough WCoin! Balance: %u WCoin.", lpMsg->remainWCoin);
	}

	DataSend(lpObj->Index, (BYTE*)&ans, sizeof(ans));
}
