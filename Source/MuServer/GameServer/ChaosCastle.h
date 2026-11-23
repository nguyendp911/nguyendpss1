#pragma once

#include "ProtocolDefines.h"
#include "User.h"
#include <vector>

#define CHAOS_CASTLE_NPC 247 // Lorencia Chaos Castle Guard (136, 127)
#define MAX_CC_LEVEL 6
#define MAX_CC_USER 70
#define MAX_CC_MONSTER 60
#define MAX_CC_HOLLOW_ZONE 4
#define MAX_CC_CENTER_HOLLOW_ZONE 2

#define GET_CC_LEVEL(x) (((x) < MAP_CHAOS_CASTLE1) ? -1 : ((x) > MAP_CHAOS_CASTLE6) ? -1 : ((x) - MAP_CHAOS_CASTLE1))
#define CC_LEVEL_RANGE(x) (((x) < 0) ? 0 : ((x) >= MAX_CC_LEVEL) ? 0 : 1)

enum eChaosCastleState
{
	CC_STATE_BLANK = 0,
	CC_STATE_EMPTY = 1,
	CC_STATE_STAND = 2,
	CC_STATE_START = 3,
	CC_STATE_CLEAN = 4,
};

#pragma pack(push, 1)

struct CG_CHAOS_CASTLE_REQ
{
	PSBMSG_HEAD header; // C1:99:20 (info), C1:99:21 (enter)
	BYTE level;
};

struct GC_CHAOS_CASTLE_LEVEL
{
	BYTE state;
	BYTE enter;
	WORD remain;
	BYTE users;
};

struct GC_CHAOS_CASTLE_INFO
{
	PSBMSG_HEAD header; // C1:99:20
	BYTE able;          // 0..5 or 0xFF if too low
	DWORD cost[MAX_CC_LEVEL];
	GC_CHAOS_CASTLE_LEVEL level[MAX_CC_LEVEL];
};

struct GC_CHAOS_CASTLE_STATE
{
	PSBMSG_HEAD header; // C1:99:22
	BYTE state;
	WORD remain;
	WORD alive;
	WORD monsters;
	WORD players;
	BYTE hollow;
};

struct GC_CHAOS_CASTLE_RESULT
{
	PSBMSG_HEAD header; // C1:99:23
	BYTE win;
	WORD monsters;
	WORD players;
	DWORD experience;
	DWORD zen;
};

#pragma pack(pop)

struct CHAOS_CASTLE_START_TIME
{
	int Year;
	int Month;
	int Day;
	int DayOfWeek;
	int Hour;
	int Minute;
	int Second;
};

struct CHAOS_CASTLE_USER
{
	int Index;
	int UserKillCount;
	int MonsterKillCount;
	DWORD RewardExperience;
	DWORD RewardMoney;
	bool Sent;
};

struct CHAOS_CASTLE_LEVEL
{
	int Level;
	int State;
	int Map;
	int RemainTime;
	int TargetTime;
	DWORD TickCount;
	int EnterEnabled;
	int MinutesLeft;
	int Stage; // Collapse stage 0..3
	int MaxMonster;
	int WinnerIndex;
	CHAOS_CASTLE_USER User[MAX_CC_USER];
	int MonsterIndex[MAX_CC_MONSTER];
};

class CChaosCastle
{
public:
	CChaosCastle();
	~CChaosCastle();

	void Init();
	void Load(char* path);
	void MainProc();

	int GetState(int level);
	int GetUserAbleLevel(LPOBJ lpObj);
	bool IsUserInCC(int aIndex);

	void NpcChaosCastle(LPOBJ lpNpc, LPOBJ lpObj);
	void CGChaosCastleInfoRecv(CG_CHAOS_CASTLE_REQ* lpMsg, int aIndex);
	void CGChaosCastleEnterRecv(CG_CHAOS_CASTLE_REQ* lpMsg, int aIndex);

	void UserDieProc(LPOBJ lpObj, LPOBJ lpTarget);
	void MonsterDieProc(LPOBJ lpObj, LPOBJ lpTarget);
	void OnUserDisconnect(LPOBJ lpObj);

	void StartCC(); // Instant open for GM / testing

private:
	void SetState(CHAOS_CASTLE_LEVEL* lpLevel, int state);
	void ProcState_EMPTY(CHAOS_CASTLE_LEVEL* lpLevel);
	void ProcState_STAND(CHAOS_CASTLE_LEVEL* lpLevel);
	void ProcState_START(CHAOS_CASTLE_LEVEL* lpLevel);
	void ProcState_CLEAN(CHAOS_CASTLE_LEVEL* lpLevel);

	void CheckSync(CHAOS_CASTLE_LEVEL* lpLevel);
	void CleanUser(CHAOS_CASTLE_LEVEL* lpLevel);
	void ClearUser(CHAOS_CASTLE_LEVEL* lpLevel);
	bool AddUser(CHAOS_CASTLE_LEVEL* lpLevel, int aIndex);
	bool DelUser(CHAOS_CASTLE_LEVEL* lpLevel, int aIndex);
	CHAOS_CASTLE_USER* GetUser(CHAOS_CASTLE_LEVEL* lpLevel, int aIndex);
	int GetUserCount(CHAOS_CASTLE_LEVEL* lpLevel);
	void CheckUser(CHAOS_CASTLE_LEVEL* lpLevel);

	void CleanMonster(CHAOS_CASTLE_LEVEL* lpLevel);
	void ClearMonster(CHAOS_CASTLE_LEVEL* lpLevel);
	int GetMonsterCount(CHAOS_CASTLE_LEVEL* lpLevel);
	void SpawnWave(CHAOS_CASTLE_LEVEL* lpLevel);

	void SetSafeZone(CHAOS_CASTLE_LEVEL* lpLevel);
	void DelSafeZone(CHAOS_CASTLE_LEVEL* lpLevel);
	void SetHollowZone(CHAOS_CASTLE_LEVEL* lpLevel, int stage);
	void DelHollowZone(CHAOS_CASTLE_LEVEL* lpLevel, int stage);
	void SetCenterHollowZone(CHAOS_CASTLE_LEVEL* lpLevel);
	void DelCenterHollowZone(CHAOS_CASTLE_LEVEL* lpLevel);
	void CheckUserPosition(CHAOS_CASTLE_LEVEL* lpLevel);
	void CheckMonsterPosition(CHAOS_CASTLE_LEVEL* lpLevel);
	void CheckItemPosition(CHAOS_CASTLE_LEVEL* lpLevel);
	void SearchUserAndBlow(CHAOS_CASTLE_LEVEL* lpLevel, int x, int y);
	void BlowUserFromPoint(CHAOS_CASTLE_LEVEL* lpLevel, int aIndex, int x, int y);
	bool GetFreePosition(CHAOS_CASTLE_LEVEL* lpLevel, int* ox, int* oy);

	void GiveUserRewardExperience(CHAOS_CASTLE_LEVEL* lpLevel, int aIndex, bool isWinner);
	void GiveWinnerReward(CHAOS_CASTLE_LEVEL* lpLevel, int winnerIndex);
	void SendStateToAll(CHAOS_CASTLE_LEVEL* lpLevel);
	void SendResult(CHAOS_CASTLE_LEVEL* lpLevel, int aIndex, bool win);
	void NoticeSendToAll(CHAOS_CASTLE_LEVEL* lpLevel, int type, char* message, ...);

	CHAOS_CASTLE_LEVEL m_Level[MAX_CC_LEVEL];
	int m_WarningTime;
	int m_NotifyTime;
	int m_EventTime;
	int m_CloseTime;
	std::vector<CHAOS_CASTLE_START_TIME> m_StartTime;
	int m_ExpTable[MAX_CC_LEVEL][2]; // [0]=player kill, [1]=monster kill
	int m_RewardZen[MAX_CC_LEVEL];
	int m_EnterFee[MAX_CC_LEVEL];
};

extern CChaosCastle gChaosCastle;
