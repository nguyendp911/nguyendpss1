#include "stdafx.h"
#include "Battlefield.h"
#include "DSProtocol.h"
#include "Duel.h"
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
#include <math.h>

CBattlefield gBattlefield;

// 3 Capture Flag Tower coordinates (Top / Mid / Bot bridges)
static const int gBFTowerPos[MAX_BF_TOWER][2] = {
	{ 74,  182 }, // TOP
	{ 128, 128 }, // MID
	{ 182, 74  }, // BOT
};

static const char* gBFLaneName[MAX_BF_TOWER] = { "TOP", "MID", "BOT" };

// 6 Defensive Watchtower coordinates (0..2 = BLUE, 3..5 = RED)
static const int gBFWatchPos[MAX_BF_WATCHTOWER][3] = {
	{ 54,  130, BF_TEAM_BLUE },
	{ 92,  92,  BF_TEAM_BLUE },
	{ 130, 54,  BF_TEAM_BLUE },
	{ 130, 202, BF_TEAM_RED  },
	{ 164, 164, BF_TEAM_RED  },
	{ 202, 130, BF_TEAM_RED  },
};

CBattlefield::CBattlefield()
{
	this->m_State = BF_STATE_BLANK;
	this->m_RemainTime = 0;
	this->m_TargetTime = 0;
	this->m_TickCount = GetTickCount();
	this->m_ScoreTick = 0;
	this->m_WatchTick = 0;
	this->m_EnterEnabled = 0;
	this->m_MinutesLeft = -1;

	this->m_WarningTime = 5;
	this->m_EventTime = 15;
	this->m_CloseTime = 2;
	this->m_WinScore = 1000;
	this->m_TowerHP = 150000;
	this->m_TowerDmgMin = 800;
	this->m_TowerDmgMax = 1500;
	this->m_WinExp = 500000;
	this->m_LoseExp = 150000;
	this->m_WinZen = 3000000;
	this->m_LoseZen = 1000000;
	this->m_WinJewels = 2;

	this->m_BlueScore = 0;
	this->m_RedScore = 0;
	for (int i = 0; i < MAX_BF_TOWER; i++)
	{
		this->m_TowerOwner[i] = BF_TEAM_NONE;
		this->m_TowerMonster[i] = -1;
		this->m_TowerRespawnTick[i] = 0;
	}
	this->CleanUsers();
}

CBattlefield::~CBattlefield()
{
}

void CBattlefield::Load(const char* path)
{
	CReadScript* lpReadScript = new CReadScript;
	if (!lpReadScript) return;
	if (!lpReadScript->Load((char*)path))
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
				token = lpReadScript->GetToken();
				if (token == TOKEN_END || token == TOKEN_END_SECTION) break;

				if (section == 0)
				{
					this->m_WarningTime = lpReadScript->GetNumber();
					this->m_EventTime   = lpReadScript->GetAsNumber();
					this->m_CloseTime   = lpReadScript->GetAsNumber();
					this->m_WinScore    = lpReadScript->GetAsNumber();
					this->m_TowerHP     = lpReadScript->GetAsNumber();
					this->m_TowerDmgMin = lpReadScript->GetAsNumber();
					this->m_TowerDmgMax = lpReadScript->GetAsNumber();
				}
				else if (section == 1)
				{
					BF_START_TIME st;
					st.Year      = lpReadScript->GetNumber();
					st.Month     = lpReadScript->GetAsNumber();
					st.Day       = lpReadScript->GetAsNumber();
					st.DayOfWeek = lpReadScript->GetAsNumber();
					st.Hour      = lpReadScript->GetAsNumber();
					st.Minute    = lpReadScript->GetAsNumber();
					st.Second    = lpReadScript->GetAsNumber();
					this->m_StartTime.push_back(st);
				}
				else if (section == 2)
				{
					this->m_WinExp    = (DWORD)lpReadScript->GetNumber();
					this->m_LoseExp   = (DWORD)lpReadScript->GetAsNumber();
					this->m_WinZen    = (DWORD)lpReadScript->GetAsNumber();
					this->m_LoseZen   = (DWORD)lpReadScript->GetAsNumber();
					this->m_WinJewels = (BYTE)lpReadScript->GetAsNumber();
				}
			}
		}
	}
	catch (...)
	{
	}

	delete lpReadScript;
	LogAdd(LOG_BLUE, "[Battlefield] Loaded %d schedules (11:00 & 19:00), WinScore=%d, TowerHP=%d",
		(int)this->m_StartTime.size(), this->m_WinScore, this->m_TowerHP);
}

void CBattlefield::Init()
{
	this->SetState(BF_STATE_EMPTY);
}

void CBattlefield::StartNow()
{
	this->ClearUsers();
	this->ClearTowers();
	this->m_BlueScore = 0;
	this->m_RedScore = 0;
	for (int i = 0; i < MAX_BF_TOWER; i++) this->m_TowerOwner[i] = BF_TEAM_NONE;

	this->m_State = BF_STATE_OPEN;
	this->m_EnterEnabled = 1;
	this->m_RemainTime = 60; // 1 min quick open for GM test
	this->m_TargetTime = (int)(time(0) + this->m_RemainTime);
	this->m_MinutesLeft = 1;
	gNotice.GCNoticeSendToAll(0, "[Battlefield] Instant Faction War OPEN! 1 minute to join via /bf or Lorencia Commander!");
}

void CBattlefield::CheckSync()
{
	if (this->m_StartTime.empty())
	{
		this->m_RemainTime = 3600;
		this->m_TargetTime = (int)(time(0) + this->m_RemainTime);
		return;
	}

	CTime ScheduleTime;
	CScheduleManager sm;
	for (size_t i = 0; i < this->m_StartTime.size(); i++)
	{
		const BF_START_TIME& s = this->m_StartTime[i];
		sm.AddSchedule(s.Year, s.Month, s.Day, s.Hour, s.Minute, s.Second, s.DayOfWeek);
	}

	if (sm.GetSchedule(&ScheduleTime) == 0)
	{
		this->m_RemainTime = 3600;
		this->m_TargetTime = (int)(time(0) + this->m_RemainTime);
		return;
	}

	this->m_RemainTime = (int)difftime(ScheduleTime.GetTime(), time(0));
	if (this->m_RemainTime < 1) this->m_RemainTime = 60;
	this->m_TargetTime = (int)ScheduleTime.GetTime();
}

void CBattlefield::SetState(int state)
{
	this->m_State = state;
	switch (state)
	{
		case BF_STATE_EMPTY:
			this->m_EnterEnabled = 0;
			this->m_MinutesLeft = -1;
			this->m_BlueScore = 0;
			this->m_RedScore = 0;
			for (int i = 0; i < MAX_BF_TOWER; i++) this->m_TowerOwner[i] = BF_TEAM_NONE;
			this->ClearUsers();
			this->ClearTowers();
			this->CheckSync();
			break;

		case BF_STATE_OPEN:
			this->m_EnterEnabled = 1;
			this->m_MinutesLeft = -1;
			this->m_RemainTime = this->m_WarningTime * 60;
			this->m_TargetTime = (int)(time(0) + this->m_RemainTime);
			gNotice.GCNoticeSendToAll(0, "[Battlefield] 2-Faction War is OPEN! Type /bf or talk to Commander in Lorencia (140, 127)!");
			break;

		case BF_STATE_START:
			this->m_EnterEnabled = 1; // Allow late join during battle to keep teams active
			this->m_BlueScore = 0;
			this->m_RedScore = 0;
			this->m_RemainTime = this->m_EventTime * 60;
			this->m_TargetTime = (int)(time(0) + this->m_RemainTime);
			this->m_ScoreTick = GetTickCount();
			this->m_WatchTick = GetTickCount();
			this->SpawnTowers();
			this->NoticeSendToAll(0, "[Battlefield] BATTLE STARTED! Destroy & capture the 3 Lane Towers (TOP/MID/BOT)! First to %d pts wins!", this->m_WinScore);
			this->SendTeamsToAll();
			this->SendStateToAll();
			break;

		case BF_STATE_CLEAN:
			this->m_EnterEnabled = 0;
			this->m_RemainTime = this->m_CloseTime * 60;
			this->m_TargetTime = (int)(time(0) + this->m_RemainTime);
			this->ClearTowers();
			this->SendStateToAll();
			break;
	}
}

void CBattlefield::MainProc()
{
	DWORD now = GetTickCount();
	if ((now - this->m_TickCount) < 1000) return;
	this->m_TickCount = now;
	this->m_RemainTime = (int)difftime(this->m_TargetTime, time(0));
	if (this->m_RemainTime < 0) this->m_RemainTime = 0;

	switch (this->m_State)
	{
		case BF_STATE_EMPTY: this->ProcState_EMPTY(); break;
		case BF_STATE_OPEN:  this->ProcState_OPEN();  break;
		case BF_STATE_START: this->ProcState_START(); break;
		case BF_STATE_CLEAN: this->ProcState_CLEAN(); break;
	}
}

void CBattlefield::ProcState_EMPTY()
{
	if (this->m_RemainTime > 0 && this->m_RemainTime <= (this->m_WarningTime * 60))
	{
		this->SetState(BF_STATE_OPEN);
		return;
	}
	if (this->m_RemainTime <= 0)
	{
		this->SetState(BF_STATE_OPEN);
	}
}

void CBattlefield::ProcState_OPEN()
{
	this->CheckUsers();

	int mins = (this->m_RemainTime + 59) / 60;
	if (mins > 0 && mins != this->m_MinutesLeft)
	{
		this->m_MinutesLeft = mins;
		gNotice.GCNoticeSendToAll(0, "[Battlefield] %d minute(s) until Faction War begins! Type /bf to join!", mins);
	}

	this->SendStateToAll();

	if (this->m_RemainTime <= 0)
	{
		if ((this->GetTeamCount(BF_TEAM_BLUE) + this->GetTeamCount(BF_TEAM_RED)) <= 0)
		{
			this->SetState(BF_STATE_EMPTY);
		}
		else
		{
			this->SetState(BF_STATE_START);
		}
	}
}

void CBattlefield::ProcState_START()
{
	this->CheckUsers();

	DWORD now = GetTickCount();

	// Delayed Flag Tower respawn (2s after destroyed) so client finishes die animation cleanly!
	for (int i = 0; i < MAX_BF_TOWER; i++)
	{
		if (this->m_TowerRespawnTick[i] != 0 && now >= this->m_TowerRespawnTick[i])
		{
			this->m_TowerRespawnTick[i] = 0;
			this->SpawnSingleTower(i);
		}
	}

	// Every 2s: Watchtowers shoot nearby enemy players!
	if ((now - this->m_WatchTick) >= 2000)
	{
		this->m_WatchTick = now;
		this->ProcWatchtowers();
	}

	// Every 5s: Captured 3-lane Flag Towers generate +5 points each!
	if ((now - this->m_ScoreTick) >= 5000)
	{
		this->m_ScoreTick = now;
		for (int i = 0; i < MAX_BF_TOWER; i++)
		{
			if (this->m_TowerOwner[i] == BF_TEAM_BLUE) this->m_BlueScore += 5;
			else if (this->m_TowerOwner[i] == BF_TEAM_RED) this->m_RedScore += 5;
		}
	}

	this->SendStateToAll();
	this->SendTeamsToAll();

	if (this->m_BlueScore >= this->m_WinScore || this->m_RedScore >= this->m_WinScore || this->m_RemainTime <= 0)
	{
		this->FinishMatch();
		this->SetState(BF_STATE_CLEAN);
	}
}

void CBattlefield::ProcState_CLEAN()
{
	this->CheckUsers();
	if (this->m_RemainTime <= 0 || (this->GetTeamCount(BF_TEAM_BLUE) + this->GetTeamCount(BF_TEAM_RED)) <= 0)
	{
		this->SetState(BF_STATE_EMPTY);
	}
}

void CBattlefield::CleanUsers()
{
	for (int i = 0; i < MAX_BF_USER; i++)
	{
		this->m_Users[i].Index = -1;
		this->m_Users[i].Team = BF_TEAM_NONE;
		this->m_Users[i].Kills = 0;
		this->m_Users[i].Captures = 0;
	}
}

void CBattlefield::ClearUsers()
{
	for (int i = 0; i < MAX_BF_USER; i++)
	{
		int idx = this->m_Users[i].Index;
		if (OBJECT_USER_RANGE(idx) && gObjIsConnectedGP(idx) && gObj[idx].Map == MAP_BATTLEFIELD)
		{
			gObjMoveGate(idx, 17);
		}
	}
	this->CleanUsers();
}

void CBattlefield::CheckUsers()
{
	for (int i = 0; i < MAX_BF_USER; i++)
	{
		int idx = this->m_Users[i].Index;
		if (!OBJECT_USER_RANGE(idx)) continue;
		if (!gObjIsConnectedGP(idx) || gObj[idx].Map != MAP_BATTLEFIELD)
		{
			this->m_Users[i].Index = -1;
			this->m_Users[i].Team = BF_TEAM_NONE;
		}
	}
}

bool CBattlefield::AddUser(int aIndex, BYTE team)
{
	if (this->GetUser(aIndex) != NULL) return false;
	for (int i = 0; i < MAX_BF_USER; i++)
	{
		if (!OBJECT_USER_RANGE(this->m_Users[i].Index))
		{
			this->m_Users[i].Index = aIndex;
			this->m_Users[i].Team = team;
			this->m_Users[i].Kills = 0;
			this->m_Users[i].Captures = 0;
			return true;
		}
	}
	return false;
}

void CBattlefield::DelUser(int aIndex)
{
	BF_USER* u = this->GetUser(aIndex);
	if (u) { u->Index = -1; u->Team = BF_TEAM_NONE; }
}

BF_USER* CBattlefield::GetUser(int aIndex)
{
	if (!OBJECT_USER_RANGE(aIndex)) return NULL;
	for (int i = 0; i < MAX_BF_USER; i++)
	{
		if (this->m_Users[i].Index == aIndex) return &this->m_Users[i];
	}
	return NULL;
}

int CBattlefield::GetTeamCount(BYTE team)
{
	int c = 0;
	for (int i = 0; i < MAX_BF_USER; i++)
	{
		int idx = this->m_Users[i].Index;
		if (OBJECT_USER_RANGE(idx) && gObjIsConnectedGP(idx) && gObj[idx].Map == MAP_BATTLEFIELD && this->m_Users[i].Team == team)
			c++;
	}
	return c;
}

bool CBattlefield::IsUserInBF(int aIndex)
{
	if (!OBJECT_USER_RANGE(aIndex)) return false;
	return (gObj[aIndex].Map == MAP_BATTLEFIELD && this->GetUser(aIndex) != NULL);
}

BYTE CBattlefield::GetUserTeam(int aIndex)
{
	BF_USER* u = this->GetUser(aIndex);
	return u ? u->Team : BF_TEAM_NONE;
}

bool CBattlefield::CanAttack(LPOBJ lpObj, LPOBJ lpTarget)
{
	if (!lpObj || !lpTarget) return false;
	if (lpObj->Map != MAP_BATTLEFIELD || lpTarget->Map != MAP_BATTLEFIELD) return false;
	if (this->m_State != BF_STATE_START) return false;

	BYTE atkTeam = this->GetUserTeam(lpObj->Index);
	if (atkTeam == BF_TEAM_NONE) return false;

	// Player vs Player: only allow opposite factions!
	if (lpTarget->Type == OBJECT_USER)
	{
		BYTE tgtTeam = this->GetUserTeam(lpTarget->Index);
		if (tgtTeam == BF_TEAM_NONE || tgtTeam == atkTeam) return false;
		// Respect base SafeZone
		if (gMap[MAP_BATTLEFIELD].CheckAttr(lpObj->X, lpObj->Y, 1) != 0 ||
			gMap[MAP_BATTLEFIELD].CheckAttr(lpTarget->X, lpTarget->Y, 1) != 0)
			return false;
		return true;
	}

	// Player vs Capture Flag Tower: block attacking own team's tower!
	int lane = this->GetTowerLaneByMonster(lpTarget->Index);
	if (lane >= 0)
	{
		if (this->m_TowerOwner[lane] == atkTeam) return false;
		return true;
	}

	return true;
}

bool CBattlefield::CheckRespawn(LPOBJ lpObj)
{
	if (!lpObj || lpObj->Map != MAP_BATTLEFIELD) return false;
	BYTE team = this->GetUserTeam(lpObj->Index);
	if (team == BF_TEAM_NONE) return false;

	int rx = (team == BF_TEAM_BLUE) ? (44 + (GetLargeRand() % 8)) : (204 + (GetLargeRand() % 8));
	int ry = (team == BF_TEAM_BLUE) ? (44 + (GetLargeRand() % 8)) : (204 + (GetLargeRand() % 8));
	lpObj->X = (short)rx;
	lpObj->Y = (short)ry;
	lpObj->TX = lpObj->X;
	lpObj->TY = lpObj->Y;
	lpObj->Life = lpObj->MaxLife + lpObj->AddLife;
	lpObj->Mana = lpObj->MaxMana + lpObj->AddMana;
	return true;
}

void CBattlefield::SpawnTowers()
{
	this->ClearTowers();
	for (int lane = 0; lane < MAX_BF_TOWER; lane++)
	{
		this->m_TowerOwner[lane] = BF_TEAM_NONE;
		this->SpawnSingleTower(lane);
	}
}

void CBattlefield::SpawnSingleTower(int lane)
{
	if (lane < 0 || lane >= MAX_BF_TOWER) return;
	if (OBJECT_RANGE(this->m_TowerMonster[lane]))
	{
		gObjDel(this->m_TowerMonster[lane]);
		this->m_TowerMonster[lane] = -1;
	}

	int idx = gObjAddMonster(MAP_BATTLEFIELD);
	if (!OBJECT_RANGE(idx)) return;

	LPOBJ lpMon = &gObj[idx];
	lpMon->PosNum = -1;
	lpMon->X = (short)gBFTowerPos[lane][0];
	lpMon->Y = (short)gBFTowerPos[lane][1];
	lpMon->Map = MAP_BATTLEFIELD;
	lpMon->TX = lpMon->X;
	lpMon->TY = lpMon->Y;
	lpMon->OldX = lpMon->X;
	lpMon->OldY = lpMon->Y;
	lpMon->StartX = (BYTE)lpMon->X;
	lpMon->StartY = (BYTE)lpMon->Y;
	lpMon->Dir = 2;

	if (gObjSetMonster(idx, 132) == 0) // Statue of Saint (stationary pillar)
	{
		gObjDel(idx);
		return;
	}

	lpMon->Life = (float)this->m_TowerHP;
	lpMon->MaxLife = (float)this->m_TowerHP;
	lpMon->Defense = 200;
	lpMon->MoveRange = 0;
	lpMon->MaxRegenTime = 0;
	this->m_TowerMonster[lane] = idx;
	gObjViewportListProtocolCreate(lpMon);
	LogAdd(LOG_BLUE, "[Battlefield] Spawned %s Flag Tower (idx=%d) at (%d,%d)", gBFLaneName[lane], idx, lpMon->X, lpMon->Y);
}

void CBattlefield::ClearTowers()
{
	for (int i = 0; i < MAX_BF_TOWER; i++)
	{
		this->m_TowerRespawnTick[i] = 0;
		if (OBJECT_RANGE(this->m_TowerMonster[i]))
		{
			gObjDel(this->m_TowerMonster[i]);
			this->m_TowerMonster[i] = -1;
		}
	}
}

int CBattlefield::GetTowerLaneByMonster(int mIndex)
{
	for (int i = 0; i < MAX_BF_TOWER; i++)
	{
		if (this->m_TowerMonster[i] == mIndex) return i;
	}
	return -1;
}

void CBattlefield::ProcWatchtowers()
{
	for (int t = 0; t < MAX_BF_WATCHTOWER; t++)
	{
		int tx = gBFWatchPos[t][0];
		int ty = gBFWatchPos[t][1];
		BYTE towerTeam = (BYTE)gBFWatchPos[t][2];

		for (int i = 0; i < MAX_BF_USER; i++)
		{
			int aIndex = this->m_Users[i].Index;
			if (!OBJECT_USER_RANGE(aIndex) || !gObjIsConnectedGP(aIndex)) continue;
			if (this->m_Users[i].Team == BF_TEAM_NONE || this->m_Users[i].Team == towerTeam) continue;

			LPOBJ lpObj = &gObj[aIndex];
			if (lpObj->Map != MAP_BATTLEFIELD || lpObj->Live == 0 || lpObj->DieRegen != 0) continue;

			int dx = lpObj->X - tx;
			int dy = lpObj->Y - ty;
			if ((dx * dx + dy * dy) <= 36) // Within 6 tiles of enemy watchtower!
			{
				int span = (this->m_TowerDmgMax > this->m_TowerDmgMin) ? (this->m_TowerDmgMax - this->m_TowerDmgMin + 1) : 1;
				int dmg = this->m_TowerDmgMin + (GetLargeRand() % span);
				if (dmg < 100) dmg = 100;

				lpObj->Life -= (float)dmg;
				if (lpObj->Life < 0) lpObj->Life = 0;
				GCDamageSend(aIndex, aIndex, 0, dmg, 0);
				GCLifeSend(aIndex, 0xFF, (int)lpObj->Life);

				if (lpObj->Life <= 0)
				{
					lpObj->Live = 0;
					lpObj->State = OBJECT_DYING;
					lpObj->RegenTime = GetTickCount();
					lpObj->DieRegen = 1;
					lpObj->PathCount = 0;
					GCUserDieSend(lpObj, aIndex, 0, aIndex);
					gNotice.GCNoticeSend(aIndex, 1, "[Battlefield] You were eliminated by an enemy Watchtower!");
				}
				break; // 1 target per shot per watchtower
			}
		}
	}
}

void CBattlefield::UserDieProc(LPOBJ lpObj, LPOBJ lpTarget)
{
	if (!lpObj || lpObj->Map != MAP_BATTLEFIELD) return;
	if (this->m_State != BF_STATE_START) return;

	if (lpTarget && lpTarget->Type == OBJECT_USER)
	{
		BF_USER* killer = this->GetUser(lpTarget->Index);
		if (killer)
		{
			killer->Kills++;
			if (killer->Team == BF_TEAM_BLUE) this->m_BlueScore += 10;
			else if (killer->Team == BF_TEAM_RED) this->m_RedScore += 10;
		}
	}
}

void CBattlefield::MonsterDieProc(LPOBJ lpObj, LPOBJ lpTarget)
{
	if (!lpObj || lpObj->Map != MAP_BATTLEFIELD) return;
	if (this->m_State != BF_STATE_START) return;

	int lane = this->GetTowerLaneByMonster(lpObj->Index);
	if (lane < 0) return;

	this->m_TowerMonster[lane] = -1;

	if (lpTarget && lpTarget->Type == OBJECT_USER)
	{
		BF_USER* capturer = this->GetUser(lpTarget->Index);
		if (capturer && (capturer->Team == BF_TEAM_BLUE || capturer->Team == BF_TEAM_RED))
		{
			this->m_TowerOwner[lane] = capturer->Team;
			capturer->Captures++;
			if (capturer->Team == BF_TEAM_BLUE) this->m_BlueScore += 50;
			else this->m_RedScore += 50;

			const char* teamStr = (capturer->Team == BF_TEAM_BLUE) ? "BLUE" : "RED";
			this->NoticeSendToAll(0, "[Battlefield] [%s] (%s) destroyed & captured %s TOWER! (+50 pts)",
				lpTarget->Name, teamStr, gBFLaneName[lane]);
		}
	}

	// Schedule Flag Tower respawn in 2 seconds under the new owner's control (avoids client die-animation race!)
	this->m_TowerRespawnTick[lane] = GetTickCount() + 2000;
	this->SendStateToAll();
}

void CBattlefield::OnUserDisconnect(LPOBJ lpObj)
{
	if (!lpObj || lpObj->Map != MAP_BATTLEFIELD) return;
	this->DelUser(lpObj->Index);
}

void CBattlefield::FinishMatch()
{
	BYTE winner = BF_TEAM_NONE;
	if (this->m_BlueScore > this->m_RedScore) winner = BF_TEAM_BLUE;
	else if (this->m_RedScore > this->m_BlueScore) winner = BF_TEAM_RED;

	const char* winName = (winner == BF_TEAM_BLUE) ? "BLUE VANGUARD" : (winner == BF_TEAM_RED) ? "RED LEGION" : "DRAW";
	gNotice.GCNoticeSendToAll(0, "[Battlefield] War Ended! Winner: %s (BLUE %u - %u RED)!", winName, this->m_BlueScore, this->m_RedScore);

	static const WORD jewelIds[4] = { GET_ITEM(12, 15), GET_ITEM(14, 13), GET_ITEM(14, 14), GET_ITEM(14, 16) };

	for (int i = 0; i < MAX_BF_USER; i++)
	{
		int aIndex = this->m_Users[i].Index;
		if (!OBJECT_USER_RANGE(aIndex) || !gObjIsConnectedGP(aIndex)) continue;
		LPOBJ lpObj = &gObj[aIndex];
		if (lpObj->Map != MAP_BATTLEFIELD) continue;

		bool won = (winner != BF_TEAM_NONE && this->m_Users[i].Team == winner);
		DWORD exp = won ? this->m_WinExp : this->m_LoseExp;
		DWORD zen = won ? this->m_WinZen : this->m_LoseZen;
		BYTE jewels = won ? this->m_WinJewels : 0;

		if (exp > 0)
		{
			if (gObjectManager.CharacterLevelUp(lpObj, exp, gServerInfo.m_MaxLevelUpEvent, EXPERIENCE_COMMON) == 0)
			{
				GCRewardExperienceSend(aIndex, exp);
			}
		}
		if (zen > 0 && gObjCheckMaxMoney(aIndex, zen))
		{
			lpObj->Money += zen;
			GCMoneySend(aIndex, lpObj->Money);
		}
		for (BYTE j = 0; j < jewels; j++)
		{
			GDCreateItemSend(aIndex, 0xEB, 0, 0, jewelIds[GetLargeRand() % 4], 0, 1, 0, 0, 0, aIndex, 0);
		}

		GC_BF_RESULT res;
		memset(&res, 0, sizeof(res));
		res.header.set(0x99, 0x43, sizeof(res));
		res.winnerTeam = winner;
		res.myTeam = this->m_Users[i].Team;
		res.blueScore = this->m_BlueScore;
		res.redScore = this->m_RedScore;
		res.myKills = this->m_Users[i].Kills;
		res.myCaptures = this->m_Users[i].Captures;
		res.rewardExp = exp;
		res.rewardZen = zen;
		res.rewardJewels = jewels;
		DataSend(aIndex, (BYTE*)&res, sizeof(res));
	}
}

void CBattlefield::NpcBattlefield(LPOBJ lpNpc, LPOBJ lpObj)
{
	CG_BF_REQ req;
	memset(&req, 0, sizeof(req));
	this->CGBattlefieldInfoRecv(&req, lpObj->Index);
}

void CBattlefield::CGBattlefieldInfoRecv(CG_BF_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;

	GC_BF_INFO pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.header.set(0x99, 0x40, sizeof(pMsg));
	pMsg.state = (BYTE)this->m_State;
	pMsg.enterEnabled = (BYTE)this->m_EnterEnabled;
	pMsg.remain = (WORD)(this->m_RemainTime > 65535 ? 65535 : this->m_RemainTime);
	pMsg.blueCount = (BYTE)this->GetTeamCount(BF_TEAM_BLUE);
	pMsg.redCount = (BYTE)this->GetTeamCount(BF_TEAM_RED);
	pMsg.winScore = (WORD)this->m_WinScore;
	DataSend(aIndex, (BYTE*)&pMsg, sizeof(pMsg));
}

void CBattlefield::CGBattlefieldEnterRecv(CG_BF_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpObj = &gObj[aIndex];

	if (lpObj->DuelUser != -1 || BC_MAP_RANGE(lpObj->Map) || DS_MAP_RANGE(lpObj->Map) || CC_MAP_RANGE(lpObj->Map) || lpObj->Map == MAP_BATTLEFIELD)
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Battlefield] Cannot join from your current event/duel!");
		return;
	}

	if (this->m_EnterEnabled == 0 || (this->m_State != BF_STATE_OPEN && this->m_State != BF_STATE_START))
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Battlefield] Opens daily at 11:00 and 19:00! Next in %d:%02d.", this->m_RemainTime / 60, this->m_RemainTime % 60);
		return;
	}

	int blueCnt = this->GetTeamCount(BF_TEAM_BLUE);
	int redCnt  = this->GetTeamCount(BF_TEAM_RED);
	BYTE team = (blueCnt <= redCnt) ? BF_TEAM_BLUE : BF_TEAM_RED;

	if (!this->AddUser(aIndex, team))
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Battlefield] Arena is full!");
		return;
	}

	if (lpObj->PartyNumber >= 0 && gParty.IsParty(lpObj->PartyNumber))
	{
		gParty.DelMember(lpObj->PartyNumber, aIndex);
	}

	int sx = (team == BF_TEAM_BLUE) ? (45 + (GetLargeRand() % 8)) : (203 + (GetLargeRand() % 8));
	int sy = (team == BF_TEAM_BLUE) ? (45 + (GetLargeRand() % 8)) : (203 + (GetLargeRand() % 8));
	gObjTeleport(aIndex, MAP_BATTLEFIELD, sx, sy);

	gNotice.GCNoticeSend(aIndex, 1, "[Battlefield] Joined %s! Destroy & capture the 3 Lane Towers!",
		(team == BF_TEAM_BLUE) ? "BLUE VANGUARD" : "RED LEGION");
	this->SendTeamsToAll();
	this->SendStateToAll();
}

void CBattlefield::SendStateToAll()
{
	for (int i = 0; i < MAX_BF_USER; i++)
	{
		int aIndex = this->m_Users[i].Index;
		if (!OBJECT_USER_RANGE(aIndex) || !gObjIsConnectedGP(aIndex)) continue;
		if (gObj[aIndex].Map != MAP_BATTLEFIELD) continue;

		GC_BF_STATE st;
		memset(&st, 0, sizeof(st));
		st.header.set(0x99, 0x42, sizeof(st));
		st.state = (BYTE)this->m_State;
		st.remain = (WORD)(this->m_RemainTime > 65535 ? 65535 : this->m_RemainTime);
		st.myTeam = this->m_Users[i].Team;
		st.blueScore = this->m_BlueScore;
		st.redScore = this->m_RedScore;
		st.winScore = (WORD)this->m_WinScore;
		for (int t = 0; t < MAX_BF_TOWER; t++) st.towerOwner[t] = this->m_TowerOwner[t];
		st.myKills = this->m_Users[i].Kills;
		st.myCaptures = this->m_Users[i].Captures;
		DataSend(aIndex, (BYTE*)&st, sizeof(st));
	}
}

void CBattlefield::SendTeamsToAll()
{
	BYTE sendBuf[1024];
	GC_BF_TEAMS* head = (GC_BF_TEAMS*)sendBuf;
	BF_TEAM_ENTRY* entries = (BF_TEAM_ENTRY*)(sendBuf + sizeof(GC_BF_TEAMS));
	int cnt = 0;

	for (int i = 0; i < MAX_BF_USER && cnt < 100; i++)
	{
		int aIndex = this->m_Users[i].Index;
		if (!OBJECT_USER_RANGE(aIndex) || !gObjIsConnectedGP(aIndex)) continue;
		if (gObj[aIndex].Map != MAP_BATTLEFIELD) continue;
		entries[cnt].index = (WORD)aIndex;
		entries[cnt].team = this->m_Users[i].Team;
		cnt++;
	}

	int totalSize = sizeof(GC_BF_TEAMS) + cnt * sizeof(BF_TEAM_ENTRY);
	head->header.set(0x99, 0x44, totalSize);
	head->count = (BYTE)cnt;

	for (int i = 0; i < MAX_BF_USER; i++)
	{
		int aIndex = this->m_Users[i].Index;
		if (!OBJECT_USER_RANGE(aIndex) || !gObjIsConnectedGP(aIndex)) continue;
		if (gObj[aIndex].Map != MAP_BATTLEFIELD) continue;
		DataSend(aIndex, sendBuf, totalSize);
	}
}

void CBattlefield::NoticeSendToAll(int type, const char* message, ...)
{
	char buff[256];
	va_list arg;
	va_start(arg, message);
	vsprintf_s(buff, sizeof(buff), message, arg);
	va_end(arg);

	for (int i = 0; i < MAX_BF_USER; i++)
	{
		int aIndex = this->m_Users[i].Index;
		if (OBJECT_USER_RANGE(aIndex) && gObjIsConnectedGP(aIndex) && gObj[aIndex].Map == MAP_BATTLEFIELD)
		{
			gNotice.GCNoticeSend(aIndex, type, "%s", buff);
		}
	}
}
