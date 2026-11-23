#include "stdafx.h"
#include "ChaosCastle.h"
#include "DSProtocol.h"
#include "Duel.h"
#include "EffectManager.h"
#include "ItemManager.h"
#include "Map.h"
#include "Monster.h"
#include "MonsterManager.h"
#include "Notice.h"
#include "ObjectManager.h"
#include "Party.h"
#include "Protocol.h"
#include "ReadScript.h"
#include "ScheduleManager.h"
#include "ServerInfo.h"
#include "Util.h"
#include <stdarg.h>

CChaosCastle gChaosCastle;

static const int gCCLevelRange[MAX_CC_LEVEL][2] = {
	{ 15,  80 },
	{ 81,  160 },
	{ 161, 260 },
	{ 261, 400 },
	{ 401, 650 },
	{ 651, 1000 },
};

static const int gCCMonsterClass[MAX_CC_LEVEL][2] = {
	{ 84,  86 },  // CC1
	{ 90,  94 },  // CC2
	{ 114, 117 }, // CC3
	{ 125, 127 }, // CC4
	{ 63,  72 },  // CC5
	{ 58,  74 },  // CC6
};

static const BYTE gCCSafeZone[4] = { 23, 75, 44, 108 };

static const BYTE gCCHollowZone[4][MAX_CC_HOLLOW_ZONE][4] = {
	{ { 23, 75, 44, 76 }, { 43, 77, 44, 108 }, { 23, 107, 42, 108 }, { 23, 77, 24, 106 } },
	{ { 23, 75, 44, 76 }, { 43, 77, 44, 108 }, { 23, 107, 42, 108 }, { 23, 77, 24, 106 } },
	{ { 25, 77, 42, 78 }, { 41, 79, 42, 106 }, { 25, 105, 40, 106 }, { 25, 79, 26, 104 } },
	{ { 27, 79, 40, 80 }, { 39, 81, 40, 104 }, { 27, 103, 38, 104 }, { 27, 81, 28, 102 } },
};

static const BYTE gCCCenterHollow[MAX_CC_CENTER_HOLLOW_ZONE][4] = {
	{ 32, 84, 35, 87 },
	{ 32, 96, 35, 99 },
};

CChaosCastle::CChaosCastle()
{
	this->m_WarningTime = 3;
	this->m_NotifyTime = 1;
	this->m_EventTime = 10;
	this->m_CloseTime = 2;

	static const int defFee[MAX_CC_LEVEL] = { 25000, 80000, 150000, 250000, 400000, 650000 };
	static const int defZen[MAX_CC_LEVEL] = { 200000, 500000, 1000000, 2000000, 4000000, 8000000 };
	for (int n = 0; n < MAX_CC_LEVEL; n++)
	{
		this->m_EnterFee[n] = defFee[n];
		this->m_RewardZen[n] = defZen[n];
		this->m_ExpTable[n][0] = 25000 * (n + 1);
		this->m_ExpTable[n][1] = 8000 * (n + 1);

		CHAOS_CASTLE_LEVEL* lp = &this->m_Level[n];
		lp->Level = n;
		lp->State = CC_STATE_BLANK;
		lp->Map = MAP_CHAOS_CASTLE1 + n;
		lp->RemainTime = 0;
		lp->TargetTime = 0;
		lp->TickCount = GetTickCount();
		lp->EnterEnabled = 0;
		lp->MinutesLeft = -1;
		lp->Stage = 0;
		lp->MaxMonster = 0;
		lp->WinnerIndex = -1;
		this->CleanUser(lp);
		this->CleanMonster(lp);
	}
}

CChaosCastle::~CChaosCastle()
{
}

void CChaosCastle::Init()
{
	for (int n = 0; n < MAX_CC_LEVEL; n++)
	{
		this->SetState(&this->m_Level[n], CC_STATE_EMPTY);
	}
}

void CChaosCastle::Load(char* path)
{
	CReadScript* lpReadScript = new CReadScript;
	if (!lpReadScript) return;

	if (!lpReadScript->Load(path))
	{
		delete lpReadScript;
		return;
	}

	this->m_StartTime.clear();

	try
	{
		eTokenResult token;
		while (true)
		{
			token = lpReadScript->GetToken();
			if (token == TOKEN_END || token == TOKEN_END_SECTION) break;

			int section = lpReadScript->GetNumber();
			while (true)
			{
				if (section == 0)
				{
					if (strcmp("end", lpReadScript->GetAsString()) == 0) break;
					this->m_WarningTime = lpReadScript->GetNumber();
					this->m_NotifyTime = lpReadScript->GetAsNumber();
					this->m_EventTime = lpReadScript->GetAsNumber();
					this->m_CloseTime = lpReadScript->GetAsNumber();
				}
				else if (section == 1)
				{
					if (strcmp("end", lpReadScript->GetAsString()) == 0) break;
					CHAOS_CASTLE_START_TIME info;
					info.Year = lpReadScript->GetNumber();
					info.Month = lpReadScript->GetAsNumber();
					info.Day = lpReadScript->GetAsNumber();
					info.DayOfWeek = lpReadScript->GetAsNumber();
					info.Hour = lpReadScript->GetAsNumber();
					info.Minute = lpReadScript->GetAsNumber();
					info.Second = lpReadScript->GetAsNumber();
					this->m_StartTime.push_back(info);
				}
				else if (section == 2)
				{
					if (strcmp("end", lpReadScript->GetAsString()) == 0) break;
					int lv = lpReadScript->GetNumber();
					int pExp = lpReadScript->GetAsNumber();
					int mExp = lpReadScript->GetAsNumber();
					if (CC_LEVEL_RANGE(lv))
					{
						this->m_ExpTable[lv][0] = pExp;
						this->m_ExpTable[lv][1] = mExp;
					}
				}
				else if (section == 3)
				{
					if (strcmp("end", lpReadScript->GetAsString()) == 0) break;
					int lv = lpReadScript->GetNumber();
					int winZen = lpReadScript->GetAsNumber();
					int fee = lpReadScript->GetAsNumber();
					if (CC_LEVEL_RANGE(lv))
					{
						this->m_RewardZen[lv] = winZen;
						this->m_EnterFee[lv] = fee;
					}
				}
				else
				{
					break;
				}
			}
		}
	}
	catch (...) {}

	delete lpReadScript;
}

void CChaosCastle::MainProc()
{
	DWORD now = GetTickCount();
	for (int n = 0; n < MAX_CC_LEVEL; n++)
	{
		CHAOS_CASTLE_LEVEL* lpLevel = &this->m_Level[n];
		DWORD elapsed = now - lpLevel->TickCount;
		if (elapsed < 1000) continue;

		lpLevel->TickCount = now;
		lpLevel->RemainTime = (int)difftime(lpLevel->TargetTime, time(0));
		if (lpLevel->RemainTime < 0) lpLevel->RemainTime = 0;

		switch (lpLevel->State)
		{
			case CC_STATE_EMPTY: this->ProcState_EMPTY(lpLevel); break;
			case CC_STATE_STAND: this->ProcState_STAND(lpLevel); break;
			case CC_STATE_START: this->ProcState_START(lpLevel); break;
			case CC_STATE_CLEAN: this->ProcState_CLEAN(lpLevel); break;
		}
	}
}

void CChaosCastle::SetState(CHAOS_CASTLE_LEVEL* lpLevel, int state)
{
	lpLevel->State = state;
	switch (state)
	{
		case CC_STATE_EMPTY:
			lpLevel->EnterEnabled = 0;
			lpLevel->MinutesLeft = -1;
			lpLevel->Stage = 0;
			lpLevel->MaxMonster = 0;
			lpLevel->WinnerIndex = -1;
			this->ClearUser(lpLevel);
			this->ClearMonster(lpLevel);
			for (int s = 1; s <= 3; s++) this->DelHollowZone(lpLevel, s);
			this->DelCenterHollowZone(lpLevel);
			this->SetSafeZone(lpLevel);
			this->CheckSync(lpLevel);
			break;

		case CC_STATE_STAND:
			lpLevel->EnterEnabled = 0;
			lpLevel->MinutesLeft = -1;
			this->SetCenterHollowZone(lpLevel);
			lpLevel->RemainTime = this->m_NotifyTime * 60;
			lpLevel->TargetTime = (int)(time(0) + lpLevel->RemainTime);
			if (this->GetUserCount(lpLevel) > 0)
			{
				this->NoticeSendToAll(lpLevel, 0, "[Chaos Castle] Entry closed! Battle begins in %d minute(s)!", this->m_NotifyTime);
			}
			break;

		case CC_STATE_START:
			lpLevel->EnterEnabled = 0;
			lpLevel->Stage = 0;
			lpLevel->WinnerIndex = -1;
			lpLevel->RemainTime = this->m_EventTime * 60;
			lpLevel->TargetTime = (int)(time(0) + lpLevel->RemainTime);
			this->DelSafeZone(lpLevel);
			this->SpawnWave(lpLevel);
			this->NoticeSendToAll(lpLevel, 0, "[Chaos Castle] FIGHT! Defeat all enemies and survive the collapsing platform!");
			this->SendStateToAll(lpLevel);
			break;

		case CC_STATE_CLEAN:
			lpLevel->EnterEnabled = 0;
			lpLevel->RemainTime = this->m_CloseTime * 60;
			lpLevel->TargetTime = (int)(time(0) + lpLevel->RemainTime);
			this->ClearMonster(lpLevel);
			this->SetSafeZone(lpLevel);
			this->SendStateToAll(lpLevel);
			break;
	}
}

void CChaosCastle::CheckSync(CHAOS_CASTLE_LEVEL* lpLevel)
{
	if (this->m_StartTime.empty())
	{
		lpLevel->RemainTime = 3600;
		lpLevel->TargetTime = (int)(time(0) + lpLevel->RemainTime);
		return;
	}

	CTime ScheduleTime;
	CScheduleManager ScheduleManager;
	for (size_t i = 0; i < this->m_StartTime.size(); i++)
	{
		const CHAOS_CASTLE_START_TIME& st = this->m_StartTime[i];
		ScheduleManager.AddSchedule(st.Year, st.Month, st.Day, st.Hour, st.Minute, st.Second, st.DayOfWeek);
	}

	if (ScheduleManager.GetSchedule(&ScheduleTime) == 0)
	{
		lpLevel->RemainTime = 3600;
		lpLevel->TargetTime = (int)(time(0) + lpLevel->RemainTime);
		return;
	}

	lpLevel->RemainTime = (int)difftime(ScheduleTime.GetTime(), time(0));
	if (lpLevel->RemainTime < 1) lpLevel->RemainTime = 60;
	lpLevel->TargetTime = (int)(time(0) + lpLevel->RemainTime);
}

void CChaosCastle::ProcState_EMPTY(CHAOS_CASTLE_LEVEL* lpLevel)
{
	if (lpLevel->RemainTime > 0 && lpLevel->RemainTime <= (this->m_WarningTime * 60))
	{
		if (lpLevel->EnterEnabled == 0)
		{
			lpLevel->EnterEnabled = 1;
			if (lpLevel->Level == 0)
			{
				gNotice.GCNoticeSendToAll(0, "[Chaos Castle] Entrance is now OPEN! Talk to Chaos Castle Guard in Lorencia or type /cc!");
			}
		}

		int mins = (lpLevel->RemainTime + 59) / 60;
		if (mins != lpLevel->MinutesLeft)
		{
			lpLevel->MinutesLeft = mins;
			if (lpLevel->Level == 0)
			{
				gNotice.GCNoticeSendToAll(0, "[Chaos Castle] %d minute(s) remaining until entry closes!", mins);
			}
		}

		this->CheckUser(lpLevel);
		if (this->GetUserCount(lpLevel) > 0) this->SendStateToAll(lpLevel);
	}

	if (lpLevel->RemainTime <= 0)
	{
		if (this->GetUserCount(lpLevel) <= 0)
		{
			this->SetState(lpLevel, CC_STATE_EMPTY);
		}
		else
		{
			this->SetState(lpLevel, CC_STATE_STAND);
		}
	}
}

void CChaosCastle::ProcState_STAND(CHAOS_CASTLE_LEVEL* lpLevel)
{
	this->CheckUser(lpLevel);
	if (this->GetUserCount(lpLevel) <= 0)
	{
		this->SetState(lpLevel, CC_STATE_EMPTY);
		return;
	}

	this->SendStateToAll(lpLevel);

	if (lpLevel->RemainTime <= 0)
	{
		this->SetState(lpLevel, CC_STATE_START);
	}
}

void CChaosCastle::ProcState_START(CHAOS_CASTLE_LEVEL* lpLevel)
{
	this->CheckUser(lpLevel);
	this->CheckUserPosition(lpLevel);
	this->CheckMonsterPosition(lpLevel);
	this->CheckItemPosition(lpLevel);

	int users = this->GetUserCount(lpLevel);
	int mobs = this->GetMonsterCount(lpLevel);
	int alive = users + mobs;

	// Progressive platform collapse based on remaining enemies
	int totalInit = lpLevel->MaxMonster + 1;
	int targetStage = 0;
	if (alive <= (totalInit * 65) / 100) targetStage = 1;
	if (alive <= (totalInit * 40) / 100) targetStage = 2;
	if (alive <= (totalInit * 20) / 100) targetStage = 3;

	while (lpLevel->Stage < targetStage && lpLevel->Stage < 3)
	{
		lpLevel->Stage++;
		this->SetHollowZone(lpLevel, lpLevel->Stage);
		if (lpLevel->Stage == 2) this->SetCenterHollowZone(lpLevel);
		this->NoticeSendToAll(lpLevel, 0, "[Chaos Castle] WARNING! Platform edge collapsed (Stage %d/3)!", lpLevel->Stage);
	}

	this->SendStateToAll(lpLevel);

	// Check win condition: 1 user left & 0 monsters, OR 0 users left, OR timer expired
	if (users == 0)
	{
		this->SetState(lpLevel, CC_STATE_CLEAN);
		return;
	}

	if (users == 1 && mobs == 0)
	{
		for (int n = 0; n < MAX_CC_USER; n++)
		{
			if (OBJECT_USER_RANGE(lpLevel->User[n].Index))
			{
				this->GiveWinnerReward(lpLevel, lpLevel->User[n].Index);
				break;
			}
		}
		this->SetState(lpLevel, CC_STATE_CLEAN);
		return;
	}

	if (lpLevel->RemainTime <= 0)
	{
		// Time up: grant accumulated kill EXP (capped by MaxLevelUpEvent) and end match
		for (int n = 0; n < MAX_CC_USER; n++)
		{
			if (OBJECT_USER_RANGE(lpLevel->User[n].Index))
			{
				this->GiveUserRewardExperience(lpLevel, lpLevel->User[n].Index, false);
				this->SendResult(lpLevel, lpLevel->User[n].Index, false);
			}
		}
		this->NoticeSendToAll(lpLevel, 0, "[Chaos Castle] Time expired! No sole survivor.");
		this->SetState(lpLevel, CC_STATE_CLEAN);
	}
}

void CChaosCastle::ProcState_CLEAN(CHAOS_CASTLE_LEVEL* lpLevel)
{
	this->CheckUser(lpLevel);
	if (lpLevel->RemainTime <= 0 || this->GetUserCount(lpLevel) <= 0)
	{
		this->SetState(lpLevel, CC_STATE_EMPTY);
	}
}

int CChaosCastle::GetState(int level)
{
	if (!CC_LEVEL_RANGE(level)) return CC_STATE_BLANK;
	return this->m_Level[level].State;
}

int CChaosCastle::GetUserAbleLevel(LPOBJ lpObj)
{
	if (!lpObj) return -1;
	for (int n = 0; n < MAX_CC_LEVEL; n++)
	{
		if (lpObj->Level >= gCCLevelRange[n][0] && lpObj->Level <= gCCLevelRange[n][1])
		{
			return n;
		}
	}
	return -1;
}

bool CChaosCastle::IsUserInCC(int aIndex)
{
	if (!OBJECT_USER_RANGE(aIndex)) return false;
	return (CC_MAP_RANGE(gObj[aIndex].Map) != 0);
}

void CChaosCastle::CleanUser(CHAOS_CASTLE_LEVEL* lpLevel)
{
	for (int n = 0; n < MAX_CC_USER; n++)
	{
		lpLevel->User[n].Index = -1;
		lpLevel->User[n].UserKillCount = 0;
		lpLevel->User[n].MonsterKillCount = 0;
		lpLevel->User[n].RewardExperience = 0;
		lpLevel->User[n].RewardMoney = 0;
		lpLevel->User[n].Sent = false;
	}
}

void CChaosCastle::ClearUser(CHAOS_CASTLE_LEVEL* lpLevel)
{
	for (int n = 0; n < MAX_CC_USER; n++)
	{
		int idx = lpLevel->User[n].Index;
		if (OBJECT_USER_RANGE(idx) && gObjIsConnectedGP(idx) && gObj[idx].Map == lpLevel->Map)
		{
			gObjMoveGate(idx, 17); // Lorencia
		}
	}
	this->CleanUser(lpLevel);
}

bool CChaosCastle::AddUser(CHAOS_CASTLE_LEVEL* lpLevel, int aIndex)
{
	if (this->GetUser(lpLevel, aIndex) != NULL) return false;
	for (int n = 0; n < MAX_CC_USER; n++)
	{
		if (OBJECT_USER_RANGE(lpLevel->User[n].Index) == 0)
		{
			lpLevel->User[n].Index = aIndex;
			lpLevel->User[n].UserKillCount = 0;
			lpLevel->User[n].MonsterKillCount = 0;
			lpLevel->User[n].RewardExperience = 0;
			lpLevel->User[n].RewardMoney = 0;
			lpLevel->User[n].Sent = false;
			return true;
		}
	}
	return false;
}

bool CChaosCastle::DelUser(CHAOS_CASTLE_LEVEL* lpLevel, int aIndex)
{
	CHAOS_CASTLE_USER* u = this->GetUser(lpLevel, aIndex);
	if (!u) return false;
	u->Index = -1;
	return true;
}

CHAOS_CASTLE_USER* CChaosCastle::GetUser(CHAOS_CASTLE_LEVEL* lpLevel, int aIndex)
{
	if (!OBJECT_USER_RANGE(aIndex)) return NULL;
	for (int n = 0; n < MAX_CC_USER; n++)
	{
		if (lpLevel->User[n].Index == aIndex) return &lpLevel->User[n];
	}
	return NULL;
}

int CChaosCastle::GetUserCount(CHAOS_CASTLE_LEVEL* lpLevel)
{
	int cnt = 0;
	for (int n = 0; n < MAX_CC_USER; n++)
	{
		int idx = lpLevel->User[n].Index;
		if (OBJECT_USER_RANGE(idx) && gObjIsConnectedGP(idx) && gObj[idx].Map == lpLevel->Map)
		{
			cnt++;
		}
	}
	return cnt;
}

void CChaosCastle::CheckUser(CHAOS_CASTLE_LEVEL* lpLevel)
{
	for (int n = 0; n < MAX_CC_USER; n++)
	{
		int idx = lpLevel->User[n].Index;
		if (OBJECT_USER_RANGE(idx) == 0) continue;
		if (!gObjIsConnectedGP(idx) || gObj[idx].Map != lpLevel->Map)
		{
			lpLevel->User[n].Index = -1;
		}
	}
}

void CChaosCastle::CleanMonster(CHAOS_CASTLE_LEVEL* lpLevel)
{
	for (int n = 0; n < MAX_CC_MONSTER; n++) lpLevel->MonsterIndex[n] = -1;
}

void CChaosCastle::ClearMonster(CHAOS_CASTLE_LEVEL* lpLevel)
{
	for (int n = 0; n < MAX_CC_MONSTER; n++)
	{
		if (OBJECT_RANGE(lpLevel->MonsterIndex[n]))
		{
			gObjDel(lpLevel->MonsterIndex[n]);
			lpLevel->MonsterIndex[n] = -1;
		}
	}
}

int CChaosCastle::GetMonsterCount(CHAOS_CASTLE_LEVEL* lpLevel)
{
	int cnt = 0;
	for (int n = 0; n < MAX_CC_MONSTER; n++)
	{
		int mIdx = lpLevel->MonsterIndex[n];
		if (OBJECT_RANGE(mIdx) && gObj[mIdx].Connected == OBJECT_ONLINE && gObj[mIdx].Live != 0)
		{
			cnt++;
		}
	}
	return cnt;
}

void CChaosCastle::SpawnWave(CHAOS_CASTLE_LEVEL* lpLevel)
{
	this->ClearMonster(lpLevel);
	int count = 36 + (lpLevel->Level * 4);
	if (count > MAX_CC_MONSTER) count = MAX_CC_MONSTER;

	int spawned = 0;
	for (int i = 0; i < count; i++)
	{
		int x = 33, y = 90;
		if (!this->GetFreePosition(lpLevel, &x, &y)) continue;

		int mClass = gCCMonsterClass[lpLevel->Level][i & 1];
		int idx = gObjAddMonster(lpLevel->Map);
		if (!OBJECT_RANGE(idx)) continue;

		LPOBJ lpMon = &gObj[idx];
		lpMon->PosNum = -1;
		lpMon->X = (short)x;
		lpMon->Y = (short)y;
		lpMon->Map = lpLevel->Map;
		lpMon->TX = lpMon->X;
		lpMon->TY = lpMon->Y;
		lpMon->OldX = lpMon->X;
		lpMon->OldY = lpMon->Y;
		lpMon->StartX = (BYTE)x;
		lpMon->StartY = (BYTE)y;
		lpMon->Dir = (BYTE)(GetLargeRand() % 8);

		if (gObjSetMonster(idx, mClass) == 0)
		{
			gObjDel(idx);
			continue;
		}

		lpMon->MaxRegenTime = 0;
		lpLevel->MonsterIndex[spawned++] = idx;
	}
	lpLevel->MaxMonster = spawned;
}

void CChaosCastle::SetSafeZone(CHAOS_CASTLE_LEVEL* lpLevel)
{
	for (int x = gCCSafeZone[0]; x <= gCCSafeZone[2]; x++)
		for (int y = gCCSafeZone[1]; y <= gCCSafeZone[3]; y++)
			gMap[lpLevel->Map].SetAttr(x, y, 1);

	for (int n = 0; n < MAX_CC_USER; n++)
	{
		if (OBJECT_USER_RANGE(lpLevel->User[n].Index) && gObjIsConnectedGP(lpLevel->User[n].Index))
		{
			GCMapAttrSend(lpLevel->User[n].Index, 0, 1, 0, 1, (PMSG_MAP_ATTR*)gCCSafeZone);
		}
	}
}

void CChaosCastle::DelSafeZone(CHAOS_CASTLE_LEVEL* lpLevel)
{
	for (int x = gCCSafeZone[0]; x <= gCCSafeZone[2]; x++)
		for (int y = gCCSafeZone[1]; y <= gCCSafeZone[3]; y++)
			gMap[lpLevel->Map].DelAttr(x, y, 1);

	for (int n = 0; n < MAX_CC_USER; n++)
	{
		if (OBJECT_USER_RANGE(lpLevel->User[n].Index) && gObjIsConnectedGP(lpLevel->User[n].Index))
		{
			GCMapAttrSend(lpLevel->User[n].Index, 0, 1, 1, 1, (PMSG_MAP_ATTR*)gCCSafeZone);
		}
	}
}

void CChaosCastle::SetHollowZone(CHAOS_CASTLE_LEVEL* lpLevel, int stage)
{
	if (stage < 1 || stage > 3) return;
	for (int n = 0; n < MAX_CC_HOLLOW_ZONE; n++)
		for (int x = gCCHollowZone[stage][n][0]; x <= gCCHollowZone[stage][n][2]; x++)
			for (int y = gCCHollowZone[stage][n][1]; y <= gCCHollowZone[stage][n][3]; y++)
				gMap[lpLevel->Map].SetAttr(x, y, 8);

	for (int n = 0; n < MAX_CC_USER; n++)
	{
		if (OBJECT_USER_RANGE(lpLevel->User[n].Index) && gObjIsConnectedGP(lpLevel->User[n].Index))
		{
			GCMapAttrSend(lpLevel->User[n].Index, 0, 8, 0, MAX_CC_HOLLOW_ZONE, (PMSG_MAP_ATTR*)gCCHollowZone[stage]);
		}
	}
}

void CChaosCastle::DelHollowZone(CHAOS_CASTLE_LEVEL* lpLevel, int stage)
{
	if (stage < 1 || stage > 3) return;
	for (int n = 0; n < MAX_CC_HOLLOW_ZONE; n++)
		for (int x = gCCHollowZone[stage][n][0]; x <= gCCHollowZone[stage][n][2]; x++)
			for (int y = gCCHollowZone[stage][n][1]; y <= gCCHollowZone[stage][n][3]; y++)
				gMap[lpLevel->Map].DelAttr(x, y, 8);
}

void CChaosCastle::SetCenterHollowZone(CHAOS_CASTLE_LEVEL* lpLevel)
{
	for (int n = 0; n < MAX_CC_CENTER_HOLLOW_ZONE; n++)
		for (int x = gCCCenterHollow[n][0]; x <= gCCCenterHollow[n][2]; x++)
			for (int y = gCCCenterHollow[n][1]; y <= gCCCenterHollow[n][3]; y++)
				gMap[lpLevel->Map].SetAttr(x, y, 8);

	for (int n = 0; n < MAX_CC_USER; n++)
	{
		if (OBJECT_USER_RANGE(lpLevel->User[n].Index) && gObjIsConnectedGP(lpLevel->User[n].Index))
		{
			GCMapAttrSend(lpLevel->User[n].Index, 0, 8, 0, MAX_CC_CENTER_HOLLOW_ZONE, (PMSG_MAP_ATTR*)gCCCenterHollow);
		}
	}
}

void CChaosCastle::DelCenterHollowZone(CHAOS_CASTLE_LEVEL* lpLevel)
{
	for (int n = 0; n < MAX_CC_CENTER_HOLLOW_ZONE; n++)
		for (int x = gCCCenterHollow[n][0]; x <= gCCCenterHollow[n][2]; x++)
			for (int y = gCCCenterHollow[n][1]; y <= gCCCenterHollow[n][3]; y++)
				gMap[lpLevel->Map].DelAttr(x, y, 8);
}

void CChaosCastle::CheckUserPosition(CHAOS_CASTLE_LEVEL* lpLevel)
{
	for (int n = 0; n < MAX_CC_USER; n++)
	{
		int idx = lpLevel->User[n].Index;
		if (!OBJECT_USER_RANGE(idx) || !gObjIsConnectedGP(idx)) continue;
		LPOBJ lpObj = &gObj[idx];
		if (lpObj->Map != lpLevel->Map || lpObj->Live == 0) continue;

		if (gMap[lpLevel->Map].CheckAttr(lpObj->X, lpObj->Y, 8) != 0)
		{
			// Fell off the collapsing platform!
			gNotice.GCNoticeSend(idx, 1, "[Chaos Castle] You fell off the collapsed platform!");
			this->GiveUserRewardExperience(lpLevel, idx, false);
			this->SendResult(lpLevel, idx, false);
			this->DelUser(lpLevel, idx);
			gObjMoveGate(idx, 17);
		}
	}
}

void CChaosCastle::CheckMonsterPosition(CHAOS_CASTLE_LEVEL* lpLevel)
{
	for (int n = 0; n < MAX_CC_MONSTER; n++)
	{
		int mIdx = lpLevel->MonsterIndex[n];
		if (!OBJECT_RANGE(mIdx) || gObj[mIdx].Connected != OBJECT_ONLINE || gObj[mIdx].Live == 0) continue;

		if (gMap[lpLevel->Map].CheckAttr(gObj[mIdx].X, gObj[mIdx].Y, 8) != 0)
		{
			int nx = 33, ny = 90;
			if (this->GetFreePosition(lpLevel, &nx, &ny))
			{
				gObjSetPosition(mIdx, nx, ny);
			}
		}
	}
}

void CChaosCastle::CheckItemPosition(CHAOS_CASTLE_LEVEL* lpLevel)
{
	for (int n = 0; n < MAX_MAP_ITEM; n++)
	{
		if (gMap[lpLevel->Map].m_Item[n].IsItem() == 0) continue;
		if (gMap[lpLevel->Map].CheckAttr(gMap[lpLevel->Map].m_Item[n].m_X, gMap[lpLevel->Map].m_Item[n].m_Y, 8) != 0)
		{
			gMap[lpLevel->Map].m_Item[n].m_Live = 0;
			gMap[lpLevel->Map].m_Item[n].m_Give = 1;
			gMap[lpLevel->Map].m_Item[n].m_State = OBJECT_DIECMD;
		}
	}
}

void CChaosCastle::SearchUserAndBlow(CHAOS_CASTLE_LEVEL* lpLevel, int x, int y)
{
	for (int n = 0; n < MAX_CC_USER; n++)
	{
		int idx = lpLevel->User[n].Index;
		if (!OBJECT_USER_RANGE(idx) || !gObjIsConnectedGP(idx)) continue;
		if (gObj[idx].DieRegen != 0 || gObj[idx].Teleport != 0 || gObj[idx].Map != lpLevel->Map) continue;

		if (gObj[idx].X >= (x - 3) && gObj[idx].X <= (x + 3) && gObj[idx].Y >= (y - 3) && gObj[idx].Y <= (y + 3))
		{
			this->BlowUserFromPoint(lpLevel, idx, x, y);
		}
	}
}

void CChaosCastle::BlowUserFromPoint(CHAOS_CASTLE_LEVEL* lpLevel, int aIndex, int x, int y)
{
	int px = gObj[aIndex].X - x;
	int py = gObj[aIndex].Y - y;

	int signX = (px < 0) ? -1 : ((px > 0) ? 1 : ((GetLargeRand() % 2) ? 1 : -1));
	int signY = (py < 0) ? -1 : ((py > 0) ? 1 : ((GetLargeRand() % 2) ? 1 : -1));
	int dis = (int)sqrt((float)(px * px + py * py));
	if (dis > 3) dis = 3;

	int tx = gObj[aIndex].X + ((4 - dis) + (GetLargeRand() % 2)) * signX;
	int ty = gObj[aIndex].Y + ((4 - dis) + (GetLargeRand() % 2)) * signY;
	if (tx < 20) tx = 20; if (tx > 47) tx = 47;
	if (ty < 72) ty = 72; if (ty > 111) ty = 111;

	gObjSetPosition(aIndex, tx, ty);
}

bool CChaosCastle::GetFreePosition(CHAOS_CASTLE_LEVEL* lpLevel, int* ox, int* oy)
{
	for (int tries = 0; tries < 100; tries++)
	{
		int x = 26 + (GetLargeRand() % 15);
		int y = 78 + (GetLargeRand() % 27);

		// Never spawn inside or on the 1-tile rim of the 2 black center holes (32..35, 84..87) & (32..35, 96..99)
		bool inHole = false;
		for (int h = 0; h < MAX_CC_CENTER_HOLLOW_ZONE; h++)
		{
			if (x >= (gCCCenterHollow[h][0] - 1) && x <= (gCCCenterHollow[h][2] + 1) &&
				y >= (gCCCenterHollow[h][1] - 1) && y <= (gCCCenterHollow[h][3] + 1))
			{
				inHole = true;
				break;
			}
		}
		if (inHole) continue;

		if (gMap[lpLevel->Map].CheckAttr(x, y, 2) == 0 && gMap[lpLevel->Map].CheckAttr(x, y, 4) == 0 && gMap[lpLevel->Map].CheckAttr(x, y, 8) == 0)
		{
			*ox = x;
			*oy = y;
			return true;
		}
	}
	*ox = 33;
	*oy = 91;
	return true;
}

void CChaosCastle::GiveUserRewardExperience(CHAOS_CASTLE_LEVEL* lpLevel, int aIndex, bool isWinner)
{
	if (!OBJECT_USER_RANGE(aIndex) || !gObjIsConnectedGP(aIndex)) return;
	CHAOS_CASTLE_USER* u = this->GetUser(lpLevel, aIndex);
	if (!u || u->Sent) return;

	LPOBJ lpObj = &gObj[aIndex];
	unsigned __int64 totalExp =
		((unsigned __int64)u->UserKillCount * (unsigned __int64)this->m_ExpTable[lpLevel->Level][0]) +
		((unsigned __int64)u->MonsterKillCount * (unsigned __int64)this->m_ExpTable[lpLevel->Level][1]);

	if (isWinner)
	{
		totalExp += (unsigned __int64)this->m_ExpTable[lpLevel->Level][0];
	}

	int rate = gServerInfo.m_AddEventExperienceRate[lpObj->AccountLevel];
	if (rate > 1) totalExp *= (unsigned __int64)rate;
	if (totalExp > 50000000ULL) totalExp = 50000000ULL;

	DWORD exp = (DWORD)totalExp;
	u->RewardExperience = exp;

	if (exp > 0)
	{
		if (gObjectManager.CharacterLevelUp(lpObj, exp, gServerInfo.m_MaxLevelUpEvent, EXPERIENCE_COMMON) == 0)
		{
			GCRewardExperienceSend(aIndex, exp);
		}
	}
}

void CChaosCastle::UserDieProc(LPOBJ lpObj, LPOBJ lpTarget)
{
	int level = GET_CC_LEVEL(lpObj->Map);
	if (!CC_LEVEL_RANGE(level)) return;
	CHAOS_CASTLE_LEVEL* lpLevel = &this->m_Level[level];

	if (lpTarget && lpTarget->Type == OBJECT_USER)
	{
		CHAOS_CASTLE_USER* killer = this->GetUser(lpLevel, lpTarget->Index);
		if (killer)
		{
			killer->UserKillCount++;
		}
	}

	this->GiveUserRewardExperience(lpLevel, lpObj->Index, false);
	this->SendResult(lpLevel, lpObj->Index, false);
	this->DelUser(lpLevel, lpObj->Index);
}

void CChaosCastle::MonsterDieProc(LPOBJ lpObj, LPOBJ lpTarget)
{
	int level = GET_CC_LEVEL(lpObj->Map);
	if (!CC_LEVEL_RANGE(level)) return;
	CHAOS_CASTLE_LEVEL* lpLevel = &this->m_Level[level];

	for (int n = 0; n < MAX_CC_MONSTER; n++)
	{
		if (lpLevel->MonsterIndex[n] == lpObj->Index)
		{
			lpLevel->MonsterIndex[n] = -1;
			break;
		}
	}

	if (lpTarget && lpTarget->Type == OBJECT_USER)
	{
		CHAOS_CASTLE_USER* killer = this->GetUser(lpLevel, lpTarget->Index);
		if (killer)
		{
			killer->MonsterKillCount++;
		}
	}

	// Iconic Chaos Castle mechanic: dying monster explodes and knocks nearby players backward!
	this->SearchUserAndBlow(lpLevel, lpObj->X, lpObj->Y);
}

void CChaosCastle::OnUserDisconnect(LPOBJ lpObj)
{
	if (!lpObj || !CC_MAP_RANGE(lpObj->Map)) return;
	int level = GET_CC_LEVEL(lpObj->Map);
	if (CC_LEVEL_RANGE(level))
	{
		this->DelUser(&this->m_Level[level], lpObj->Index);
	}
}

void CChaosCastle::GiveWinnerReward(CHAOS_CASTLE_LEVEL* lpLevel, int winnerIndex)
{
	if (!OBJECT_USER_RANGE(winnerIndex) || !gObjIsConnectedGP(winnerIndex)) return;
	LPOBJ lpWinner = &gObj[winnerIndex];
	CHAOS_CASTLE_USER* u = this->GetUser(lpLevel, winnerIndex);
	if (!u || u->Sent) return;

	lpLevel->WinnerIndex = winnerIndex;

	DWORD winZen = (DWORD)this->m_RewardZen[lpLevel->Level];
	u->RewardMoney = winZen;

	this->GiveUserRewardExperience(lpLevel, winnerIndex, true);
	if (gObjCheckMaxMoney(winnerIndex, winZen))
	{
		lpWinner->Money += winZen;
		GCMoneySend(winnerIndex, lpWinner->Money);
	}

	// Reward 2 random Jewels (Chaos / Bless / Soul / Life)
	static const WORD jewelIds[4] = { GET_ITEM(12, 15), GET_ITEM(14, 13), GET_ITEM(14, 14), GET_ITEM(14, 16) };
	for (int i = 0; i < 2; i++)
	{
		WORD jId = jewelIds[GetLargeRand() % 4];
		GDCreateItemSend(winnerIndex, 0xEB, 0, 0, jId, 0, 1, 0, 0, 0, winnerIndex, 0);
	}

	gNotice.GCNoticeSendToAll(0, "[Chaos Castle %d] [%s] is the SOLE SURVIVOR! (+%u Zen & 2 Jewels)", lpLevel->Level + 1, lpWinner->Name, winZen);
	this->SendResult(lpLevel, winnerIndex, true);
}

void CChaosCastle::NpcChaosCastle(LPOBJ lpNpc, LPOBJ lpObj)
{
	CG_CHAOS_CASTLE_REQ req;
	memset(&req, 0, sizeof(req));
	this->CGChaosCastleInfoRecv(&req, lpObj->Index);
}

void CChaosCastle::CGChaosCastleInfoRecv(CG_CHAOS_CASTLE_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpObj = &gObj[aIndex];

	GC_CHAOS_CASTLE_INFO pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.header.set(0x99, 0x20, sizeof(pMsg));
	int able = this->GetUserAbleLevel(lpObj);
	pMsg.able = (able >= 0) ? (BYTE)able : 0xFF;

	for (int n = 0; n < MAX_CC_LEVEL; n++)
	{
		pMsg.cost[n] = (DWORD)this->m_EnterFee[n];
		pMsg.level[n].state = (BYTE)this->m_Level[n].State;
		pMsg.level[n].enter = (BYTE)this->m_Level[n].EnterEnabled;
		pMsg.level[n].remain = (WORD)(this->m_Level[n].RemainTime > 65535 ? 65535 : this->m_Level[n].RemainTime);
		pMsg.level[n].users = (BYTE)this->GetUserCount(&this->m_Level[n]);
	}

	DataSend(aIndex, (BYTE*)&pMsg, sizeof(pMsg));
}

void CChaosCastle::CGChaosCastleEnterRecv(CG_CHAOS_CASTLE_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpObj = &gObj[aIndex];

	if (lpObj->DuelUser != -1 || BC_MAP_RANGE(lpObj->Map) || DS_MAP_RANGE(lpObj->Map) || CC_MAP_RANGE(lpObj->Map))
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Chaos Castle] Cannot enter from your current event/duel!");
		return;
	}

	int level = this->GetUserAbleLevel(lpObj);
	if (!CC_LEVEL_RANGE(level))
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Chaos Castle] Minimum Level 15 required to enter Chaos Castle!");
		return;
	}

	CHAOS_CASTLE_LEVEL* lpLevel = &this->m_Level[level];
	if (lpLevel->EnterEnabled == 0)
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Chaos Castle] Entrance is currently closed! Next opening in %d:%02d.", lpLevel->RemainTime / 60, lpLevel->RemainTime % 60);
		return;
	}

	if (this->GetUserCount(lpLevel) >= MAX_CC_USER)
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Chaos Castle] Chaos Castle %d is full!", level + 1);
		return;
	}

	DWORD fee = (DWORD)this->m_EnterFee[level];
	if (lpObj->Money < fee)
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Chaos Castle] Requires %u Zen to enter Chaos Castle %d!", fee, level + 1);
		return;
	}

	int x = 33, y = 90;
	this->GetFreePosition(lpLevel, &x, &y);

	// Leave party for fair Free-For-All survival
	if (lpObj->PartyNumber >= 0 && gParty.IsParty(lpObj->PartyNumber))
	{
		gParty.DelMember(lpObj->PartyNumber, aIndex);
	}

	lpObj->Money -= fee;
	GCMoneySend(aIndex, lpObj->Money);

	this->AddUser(lpLevel, aIndex);
	gObjTeleport(aIndex, lpLevel->Map, x, y);
	gNotice.GCNoticeSend(aIndex, 1, "[Chaos Castle] Entered Chaos Castle %d! Prepare for battle!", level + 1);
	this->SendStateToAll(lpLevel);
}

void CChaosCastle::SendStateToAll(CHAOS_CASTLE_LEVEL* lpLevel)
{
	int alive = this->GetUserCount(lpLevel) + this->GetMonsterCount(lpLevel);
	for (int n = 0; n < MAX_CC_USER; n++)
	{
		int idx = lpLevel->User[n].Index;
		if (!OBJECT_USER_RANGE(idx) || !gObjIsConnectedGP(idx) || gObj[idx].Map != lpLevel->Map) continue;

		GC_CHAOS_CASTLE_STATE pMsg;
		pMsg.header.set(0x99, 0x22, sizeof(pMsg));
		pMsg.state = (BYTE)lpLevel->State;
		pMsg.remain = (WORD)(lpLevel->RemainTime > 65535 ? 65535 : lpLevel->RemainTime);
		pMsg.alive = (WORD)alive;
		pMsg.monsters = (WORD)lpLevel->User[n].MonsterKillCount;
		pMsg.players = (WORD)lpLevel->User[n].UserKillCount;
		pMsg.hollow = (BYTE)lpLevel->Stage;
		DataSend(idx, (BYTE*)&pMsg, sizeof(pMsg));
	}
}

void CChaosCastle::SendResult(CHAOS_CASTLE_LEVEL* lpLevel, int aIndex, bool win)
{
	CHAOS_CASTLE_USER* u = this->GetUser(lpLevel, aIndex);
	if (!u || u->Sent || !gObjIsConnectedGP(aIndex)) return;
	u->Sent = true;

	GC_CHAOS_CASTLE_RESULT pMsg;
	pMsg.header.set(0x99, 0x23, sizeof(pMsg));
	pMsg.win = win ? 1 : 0;
	pMsg.monsters = (WORD)u->MonsterKillCount;
	pMsg.players = (WORD)u->UserKillCount;
	pMsg.experience = u->RewardExperience;
	pMsg.zen = u->RewardMoney;
	DataSend(aIndex, (BYTE*)&pMsg, sizeof(pMsg));
}

void CChaosCastle::NoticeSendToAll(CHAOS_CASTLE_LEVEL* lpLevel, int type, char* message, ...)
{
	char buff[256] = { 0 };
	va_list arg;
	va_start(arg, message);
	vsprintf_s(buff, sizeof(buff), message, arg);
	va_end(arg);

	for (int n = 0; n < MAX_CC_USER; n++)
	{
		int idx = lpLevel->User[n].Index;
		if (OBJECT_USER_RANGE(idx) && gObjIsConnectedGP(idx) && gObj[idx].Map == lpLevel->Map)
		{
			gNotice.GCNoticeSend(idx, type, buff);
		}
	}
}

void CChaosCastle::StartCC()
{
	for (int n = 0; n < MAX_CC_LEVEL; n++)
	{
		CHAOS_CASTLE_LEVEL* lp = &this->m_Level[n];
		this->SetState(lp, CC_STATE_EMPTY);
		lp->RemainTime = 60; // 1 minute to enter, then STAND -> START!
		lp->TargetTime = (int)(time(0) + 60);
		lp->EnterEnabled = 1;
		lp->MinutesLeft = 1;
	}
	gNotice.GCNoticeSendToAll(0, "[Chaos Castle] Instant Event OPEN! 1 minute to enter via Lorencia Guard or /cc!");
}
