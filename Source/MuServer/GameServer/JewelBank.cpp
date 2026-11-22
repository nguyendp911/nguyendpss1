#include "stdafx.h"
#include "JewelBank.h"
#include "DSProtocol.h"
#include "ItemManager.h"
#include "Log.h"
#include "Notice.h"
#include "Protocol.h"
#include "Util.h"

CJewelBank gJewelBank;

static const int s_JewelItem[JEWELBANK_TYPES] = { GET_ITEM(12, 15), GET_ITEM(14, 13), GET_ITEM(14, 14), GET_ITEM(14, 16), GET_ITEM(14, 22) };

static const char* s_JewelName[JEWELBANK_TYPES] = { "Chaos", "Bless", "Soul", "Life", "Creation" };

CJewelBank::CJewelBank()
{
	this->m_Enable = 1;
	this->m_DataPath[0] = 0;
	this->m_DataLoaded = false;
	this->m_Accounts.clear();

	memset(this->m_BusyTick, 0, sizeof(this->m_BusyTick));
}

void CJewelBank::Load(char* iniPath, char* dataPath)
{
	this->m_Enable = GetPrivateProfileInt("JewelBank", "Enable", 1, iniPath);

	strncpy_s(this->m_DataPath, sizeof(this->m_DataPath), dataPath, _TRUNCATE);

	if (this->m_DataLoaded == false)
	{
		this->LoadData();

		this->m_DataLoaded = true;
	}

	LogAdd(LOG_BLUE, "[JewelBank] Loaded (enable: %d, accounts: %d)", this->m_Enable, (int)this->m_Accounts.size());
}

static std::string AccountKey(LPOBJ lpObj)
{
	char account[11] = { 0 };

	memcpy(account, lpObj->Account, 10);

	for (int n = 0; account[n] != 0; n++)
	{
		account[n] = (char)tolower((unsigned char)account[n]);
	}

	return std::string(account);
}

void CJewelBank::LoadData()
{
	this->m_Accounts.clear();

	FILE* file = NULL;
	if (fopen_s(&file, this->m_DataPath, "r") != 0 || file == NULL)
	{
		return;
	}

	char account[64];

	JEWELBANK_ACCOUNT bank;

	while (true)
	{
		if (fscanf_s(file, "%63s %u %u %u %u %u", account, (unsigned)_countof(account), &bank.count[0], &bank.count[1], &bank.count[2], &bank.count[3], &bank.count[4]) != 6)
		{
			break;
		}

		this->m_Accounts[std::string(account)] = bank;
	}

	fclose(file);
}

void CJewelBank::SaveData()
{
	char temp[MAX_PATH];

	sprintf_s(temp, sizeof(temp), "%s.tmp", this->m_DataPath);

	FILE* file = NULL;
	if (fopen_s(&file, temp, "w") != 0 || file == NULL)
	{
		LogAdd(LOG_RED, "[JewelBank] cannot write %s", temp);

		return;
	}

	for (std::map<std::string, JEWELBANK_ACCOUNT>::iterator it = this->m_Accounts.begin(); it != this->m_Accounts.end(); it++)
	{
		fprintf(file, "%s %u %u %u %u %u\n", it->first.c_str(), it->second.count[0], it->second.count[1], it->second.count[2], it->second.count[3], it->second.count[4]);
	}

	fclose(file);

	MoveFileExA(temp, this->m_DataPath, MOVEFILE_REPLACE_EXISTING);
}

JEWELBANK_ACCOUNT* CJewelBank::GetBank(LPOBJ lpObj, bool create)
{
	std::string key = AccountKey(lpObj);

	std::map<std::string, JEWELBANK_ACCOUNT>::iterator it = this->m_Accounts.find(key);

	if (it != this->m_Accounts.end())
	{
		return &it->second;
	}

	if (create == false)
	{
		return 0;
	}

	JEWELBANK_ACCOUNT bank;

	memset(&bank, 0, sizeof(bank));

	this->m_Accounts[key] = bank;

	return &this->m_Accounts[key];
}

bool CJewelBank::CanUse(LPOBJ lpObj)
{
	return (lpObj->State == OBJECT_PLAYING && lpObj->Interface.use == 0 && lpObj->DieRegen == 0 && lpObj->Teleport == 0);
}

int CJewelBank::CountInventory(LPOBJ lpObj, int type)
{
	int count = 0;

	int max = gItemManager.GetInventoryMaxValue(lpObj);

	for (int n = INVENTORY_WEAR_SIZE; n < max; n++)
	{
		if (lpObj->Inventory[n].IsItem() != false && lpObj->Inventory[n].m_Index == s_JewelItem[type] && lpObj->Inventory[n].m_Level == 0)
		{
			count++;
		}
	}

	return count;
}

int CJewelBank::CountFreeCells(LPOBJ lpObj)
{
	int count = 0;

	int cells = gItemManager.GetInventoryMaxValue(lpObj) - INVENTORY_WEAR_SIZE;

	for (int n = 0; n < cells; n++)
	{
		if (lpObj->InventoryMap[n] == 0xFF)
		{
			count++;
		}
	}

	return count;
}

void CJewelBank::SendState(int aIndex, BYTE result)
{
	LPOBJ lpObj = &gObj[aIndex];

	GC_JEWELBANK_STATE pMsg;

	memset(&pMsg, 0, sizeof(pMsg));

	pMsg.header.set(0x98, 0x20, sizeof(pMsg));

	pMsg.result = result;

	pMsg.enable = (BYTE)this->m_Enable;

	JEWELBANK_ACCOUNT* bank = this->GetBank(lpObj, false);

	for (int n = 0; n < JEWELBANK_TYPES; n++)
	{
		pMsg.inventory[n] = (WORD)this->CountInventory(lpObj, n);

		pMsg.bank[n] = (bank != 0) ? bank->count[n] : 0;
	}

	DataSend(aIndex, (BYTE*)&pMsg, pMsg.header.size);
}

void CJewelBank::CGInfoRecv(CG_JEWELBANK_INFO_REQ* lpMsg, int aIndex)
{
	if (gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	this->m_Critical.lock();

	this->SendState(aIndex, 0);

	this->m_Critical.unlock();
}

int CJewelBank::Deposit(LPOBJ lpObj, int type, JEWELBANK_ACCOUNT* bank)
{
	int moved = 0;

	int max = gItemManager.GetInventoryMaxValue(lpObj);

	for (int n = INVENTORY_WEAR_SIZE; n < max; n++)
	{
		if (lpObj->Inventory[n].IsItem() == false || lpObj->Inventory[n].m_Index != s_JewelItem[type] || lpObj->Inventory[n].m_Level != 0)
		{
			continue;
		}

		if (bank->count[type] >= 2000000000)
		{
			break;
		}

		gItemManager.InventoryDelItem(lpObj->Index, n);

		gItemManager.GCItemDeleteSend(lpObj->Index, n, 1);

		bank->count[type]++;

		moved++;
	}

	return moved;
}

void CJewelBank::CGDepositRecv(CG_JEWELBANK_DEPOSIT_REQ* lpMsg, int aIndex)
{
	if (gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	this->m_Critical.lock();

	DWORD now = GetTickCount();

	if (this->m_Enable == 0)
	{
		this->SendState(aIndex, 6);
	}
	else if (this->CanUse(lpObj) == false || (lpMsg->jewel >= JEWELBANK_TYPES && lpMsg->jewel != 0xFF) || (this->m_BusyTick[aIndex] != 0 && (now - this->m_BusyTick[aIndex]) < 300))
	{
		this->SendState(aIndex, 5);
	}
	else
	{
		this->m_BusyTick[aIndex] = now;

		JEWELBANK_ACCOUNT* bank = this->GetBank(lpObj, true);

		int total = 0;

		for (int type = 0; type < JEWELBANK_TYPES; type++)
		{
			if (lpMsg->jewel == 0xFF || lpMsg->jewel == type)
			{
				int moved = this->Deposit(lpObj, type, bank);

				if (moved > 0)
				{
					gLog.Output(LOG_GENERAL, "[JewelBank] %s (%s) deposited %d x %s, bank now %u", lpObj->Account, lpObj->Name, moved, s_JewelName[type], bank->count[type]);
				}

				total += moved;
			}
		}

		if (total > 0)
		{
			this->SaveData();

			GDCharacterInfoSaveSend(aIndex);
		}

		this->SendState(aIndex, (total > 0) ? 1 : 4);
	}

	this->m_Critical.unlock();
}

void CJewelBank::CGWithdrawRecv(CG_JEWELBANK_WITHDRAW_REQ* lpMsg, int aIndex)
{
	if (gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	this->m_Critical.lock();

	DWORD now = GetTickCount();

	if (this->m_Enable == 0)
	{
		this->SendState(aIndex, 6);
	}
	else if (this->CanUse(lpObj) == false || lpMsg->jewel >= JEWELBANK_TYPES || (this->m_BusyTick[aIndex] != 0 && (now - this->m_BusyTick[aIndex]) < 300))
	{
		this->SendState(aIndex, 5);
	}
	else
	{
		this->m_BusyTick[aIndex] = now;

		JEWELBANK_ACCOUNT* bank = this->GetBank(lpObj, false);

		int type = lpMsg->jewel;

		if (bank == 0 || bank->count[type] == 0)
		{
			this->SendState(aIndex, 3);
		}
		else
		{
			int free = this->CountFreeCells(lpObj);

			int amount = (lpMsg->count == 0) ? 255 : lpMsg->count;

			amount = (amount > free) ? free : amount;

			amount = (amount > (int)bank->count[type]) ? (int)bank->count[type] : amount;

			if (amount <= 0)
			{
				this->SendState(aIndex, 2);
			}
			else
			{
				bank->count[type] -= amount;

				this->SaveData();

				for (int n = 0; n < amount; n++)
				{
					GDCreateItemSend(aIndex, 0xEB, 0, 0, s_JewelItem[type], 0, 0, 0, 0, 0, -1, 0);
				}

				gLog.Output(LOG_GENERAL, "[JewelBank] %s (%s) withdrew %d x %s, bank now %u", lpObj->Account, lpObj->Name, amount, s_JewelName[type], bank->count[type]);

				this->SendState(aIndex, 1);
			}
		}
	}

	this->m_Critical.unlock();
}

DWORD CJewelBank::Count(LPOBJ lpObj, int type)
{
	if (type < 0 || type >= JEWELBANK_TYPES)
	{
		return 0;
	}

	this->m_Critical.lock();

	JEWELBANK_ACCOUNT* bank = this->GetBank(lpObj, false);

	DWORD count = (bank != 0) ? bank->count[type] : 0;

	this->m_Critical.unlock();

	return count;
}

bool CJewelBank::Take(LPOBJ lpObj, int type, DWORD count)
{
	if (type < 0 || type >= JEWELBANK_TYPES)
	{
		return false;
	}

	this->m_Critical.lock();

	JEWELBANK_ACCOUNT* bank = this->GetBank(lpObj, false);

	bool ok = (bank != 0 && bank->count[type] >= count);

	if (ok != false && count > 0)
	{
		bank->count[type] -= count;

		this->SaveData();
	}

	this->m_Critical.unlock();

	return ok;
}

void CJewelBank::Give(LPOBJ lpObj, int type, DWORD count)
{
	if (type < 0 || type >= JEWELBANK_TYPES || count == 0)
	{
		return;
	}

	this->m_Critical.lock();

	JEWELBANK_ACCOUNT* bank = this->GetBank(lpObj, true);

	bank->count[type] = ((bank->count[type] + count) > 2000000000) ? 2000000000 : (bank->count[type] + count);

	this->SaveData();

	this->m_Critical.unlock();
}
