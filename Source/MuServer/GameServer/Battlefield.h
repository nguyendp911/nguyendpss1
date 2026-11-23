#pragma once

#include "ProtocolDefines.h"
#include "User.h"
#include <vector>

#define BATTLEFIELD_NPC 249 // Lorencia Berdysh Guard (140, 127)
#define MAX_BF_USER 100
#define MAX_BF_TOWER 3      // 0: TOP, 1: MID, 2: BOT
#define MAX_BF_WATCHTOWER 6 // 0..2: BLUE, 3..5: RED

enum eBattlefieldState
{
	BF_STATE_BLANK = 0,
	BF_STATE_EMPTY = 1,
	BF_STATE_OPEN  = 2,
	BF_STATE_START = 3,
	BF_STATE_CLEAN = 4,
};

enum eBattlefieldTeam
{
	BF_TEAM_NONE = 0,
	BF_TEAM_BLUE = 1,
	BF_TEAM_RED  = 2,
};

#pragma pack(push, 1)

struct CG_BF_REQ
{
	PSBMSG_HEAD header; // C1:99:40 (info), C1:99:41 (enter)
	BYTE action;
};

struct GC_BF_INFO
{
	PSBMSG_HEAD header; // C1:99:40
	BYTE state;
	BYTE enterEnabled;
	WORD remain;
	BYTE blueCount;
	BYTE redCount;
	WORD winScore;
};

struct GC_BF_STATE
{
	PSBMSG_HEAD header; // C1:99:42
	BYTE state;
	WORD remain;
	BYTE myTeam;
	WORD blueScore;
	WORD redScore;
	WORD winScore;
	BYTE towerOwner[MAX_BF_TOWER]; // 0=Neutral, 1=Blue, 2=Red
	WORD myKills;
	WORD myCaptures;
};

struct GC_BF_RESULT
{
	PSBMSG_HEAD header; // C1:99:43
	BYTE winnerTeam; // 0=Draw, 1=Blue, 2=Red
	BYTE myTeam;
	WORD blueScore;
	WORD redScore;
	WORD myKills;
	WORD myCaptures;
	DWORD rewardExp;
	DWORD rewardZen;
	BYTE rewardJewels;
};

struct BF_TEAM_ENTRY
{
	WORD index;
	BYTE team;
};

struct GC_BF_TEAMS
{
	PSWMSG_HEAD header; // C2:99:44
	BYTE count;
};

#pragma pack(pop)

struct BF_START_TIME
{
	int Year;
	int Month;
	int Day;
	int DayOfWeek;
	int Hour;
	int Minute;
	int Second;
};

struct BF_USER
{
	int Index;
	BYTE Team;
	WORD Kills;
	WORD Captures;
};

class CBattlefield
{
public:
	CBattlefield();
	~CBattlefield();

	void Init();
	void Load(const char* path);
	void MainProc();
	void StartNow();

	int GetState() const { return this->m_State; }
	bool IsUserInBF(int aIndex);
	BYTE GetUserTeam(int aIndex);
	bool CanAttack(LPOBJ lpObj, LPOBJ lpTarget);
	bool CheckRespawn(LPOBJ lpObj);

	void NpcBattlefield(LPOBJ lpNpc, LPOBJ lpObj);
	void CGBattlefieldInfoRecv(CG_BF_REQ* lpMsg, int aIndex);
	void CGBattlefieldEnterRecv(CG_BF_REQ* lpMsg, int aIndex);

	void UserDieProc(LPOBJ lpObj, LPOBJ lpTarget);
	void MonsterDieProc(LPOBJ lpObj, LPOBJ lpTarget);
	void OnUserDisconnect(LPOBJ lpObj);

private:
	void SetState(int state);
	void CheckSync();
	void ProcState_EMPTY();
	void ProcState_OPEN();
	void ProcState_START();
	void ProcState_CLEAN();

	void CleanUsers();
	void ClearUsers();
	void CheckUsers();
	bool AddUser(int aIndex, BYTE team);
	void DelUser(int aIndex);
	BF_USER* GetUser(int aIndex);
	int GetTeamCount(BYTE team);

	void SpawnTowers();
	void SpawnSingleTower(int lane);
	void ClearTowers();
	int GetTowerLaneByMonster(int mIndex);
	void ProcWatchtowers();

	void FinishMatch();
	void SendStateToAll();
	void SendTeamsToAll();
	void NoticeSendToAll(int type, const char* message, ...);

	int m_State;
	int m_RemainTime;
	int m_TargetTime;
	DWORD m_TickCount;
	DWORD m_ScoreTick;
	DWORD m_WatchTick;
	int m_EnterEnabled;
	int m_MinutesLeft;

	int m_WarningTime;
	int m_EventTime;
	int m_CloseTime;
	int m_WinScore;
	int m_TowerHP;
	int m_TowerDmgMin;
	int m_TowerDmgMax;
	DWORD m_WinExp;
	DWORD m_LoseExp;
	DWORD m_WinZen;
	DWORD m_LoseZen;
	BYTE m_WinJewels;

	WORD m_BlueScore;
	WORD m_RedScore;
	BYTE m_TowerOwner[MAX_BF_TOWER];
	int m_TowerMonster[MAX_BF_TOWER];
	DWORD m_TowerRespawnTick[MAX_BF_TOWER];

	BF_USER m_Users[MAX_BF_USER];
	std::vector<BF_START_TIME> m_StartTime;
};

extern CBattlefield gBattlefield;
