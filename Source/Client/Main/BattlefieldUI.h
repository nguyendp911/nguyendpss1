#pragma once

#include "ProtocolDefines.h"

#define BF_MAP_INDEX 24
#define BF_MAX_TOWERS 3

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
	BYTE towerOwner[BF_MAX_TOWERS];
	WORD myKills;
	WORD myCaptures;
};

struct GC_BF_RESULT
{
	PSBMSG_HEAD header; // C1:99:43
	BYTE winnerTeam;
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

class CBattlefieldUI
{
public:
	CBattlefieldUI();
	~CBattlefieldUI();

	bool IsOpen() const { return this->m_Open || this->m_HasResult; }
	bool IsInBattle() const { return (World == BF_MAP_INDEX && this->m_HasState && this->m_State.state == 3); }
	void Close();

	void SendInfo();
	void SendEnter();

	void OnInfoRecv(BYTE* lpMsg);
	void OnStateRecv(BYTE* lpMsg);
	void OnResultRecv(BYTE* lpMsg);
	void OnTeamsRecv(BYTE* lpMsg);

	void Render();
	void UpdateMouse();

private:
	void RenderEntryWindow();
	void RenderBattleHud();
	void RenderTeamBadges();
	void RenderResultModal();
	BYTE GetTeamByKey(WORD key) const;

	bool m_Open;
	GC_BF_INFO m_Info;
	DWORD m_InfoTick;

	bool m_HasState;
	GC_BF_STATE m_State;

	bool m_HasResult;
	GC_BF_RESULT m_Result;

	int m_TeamCount;
	BF_TEAM_ENTRY m_Teams[100];
};

extern CBattlefieldUI gBattlefieldUI;
