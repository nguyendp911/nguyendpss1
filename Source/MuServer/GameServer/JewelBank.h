#pragma once

#include <map>
#include <string>
#include "ProtocolDefines.h"
#include "User.h"
#include "CriticalSection.h"

#define JEWELBANK_TYPES 5

// Client <-> GameServer protocol, head 0x98
struct CG_JEWELBANK_INFO_REQ
{
	PSBMSG_HEAD header; // C1:98:20
};

struct CG_JEWELBANK_DEPOSIT_REQ
{
	PSBMSG_HEAD header; // C1:98:21
	BYTE jewel;         // 0 ~ 4, 255 = all jewels
};

struct CG_JEWELBANK_WITHDRAW_REQ
{
	PSBMSG_HEAD header; // C1:98:22
	BYTE jewel;         // 0 ~ 4
	BYTE count;         // 0 = as many as possible
};

struct GC_JEWELBANK_STATE
{
	PSBMSG_HEAD header; // C1:98:20
	BYTE result;        // 0: none, 1: done, 2: inventory full, 3: bank empty, 4: nothing to deposit, 5: busy, 6: bank closed, 7: failed
	BYTE enable;
	WORD inventory[JEWELBANK_TYPES]; // jewels in inventory
	DWORD bank[JEWELBANK_TYPES];     // jewels in account bank
};

struct JEWELBANK_ACCOUNT
{
	DWORD count[JEWELBANK_TYPES];
};

class CJewelBank
{
public:
	CJewelBank();

	void Load(char* iniPath, char* dataPath);

	void CGInfoRecv(CG_JEWELBANK_INFO_REQ* lpMsg, int aIndex);
	void CGDepositRecv(CG_JEWELBANK_DEPOSIT_REQ* lpMsg, int aIndex);
	void CGWithdrawRecv(CG_JEWELBANK_WITHDRAW_REQ* lpMsg, int aIndex);

	// Helper API: type 0 Chaos, 1 Bless, 2 Soul, 3 Life, 4 Creation
	DWORD Count(LPOBJ lpObj, int type);
	bool IsEnabled() { return (this->m_Enable != 0); }

	bool Take(LPOBJ lpObj, int type, DWORD count);
	void Give(LPOBJ lpObj, int type, DWORD count);

private:
	bool CanUse(LPOBJ lpObj);
	void SendState(int aIndex, BYTE result);
	int CountInventory(LPOBJ lpObj, int type);
	int CountFreeCells(LPOBJ lpObj);
	int Deposit(LPOBJ lpObj, int type, JEWELBANK_ACCOUNT* bank);
	JEWELBANK_ACCOUNT* GetBank(LPOBJ lpObj, bool create);
	void LoadData();
	void SaveData();

	int m_Enable;
	char m_DataPath[MAX_PATH];
	bool m_DataLoaded;
	std::map<std::string, JEWELBANK_ACCOUNT> m_Accounts;
	CCriticalSection m_Critical;
	DWORD m_BusyTick[MAX_OBJECT];
};

extern CJewelBank gJewelBank;
