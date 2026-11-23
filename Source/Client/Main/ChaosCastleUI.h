#pragma once

#include "ProtocolDefines.h"

#define CC_LEVELS 6

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
	BYTE able;
	DWORD cost[CC_LEVELS];
	GC_CHAOS_CASTLE_LEVEL level[CC_LEVELS];
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

void ChaosCastleDress(DWORD c, bool self);

class CChaosCastleUI
{
public:
	CChaosCastleUI();
	~CChaosCastleUI();

	void Close();
	bool IsOpen() const { return (this->m_Open || this->m_HasResult); }

	void OnInfoRecv(BYTE* lpMsg);
	void OnStateRecv(BYTE* lpMsg);
	void OnResultRecv(BYTE* lpMsg);

	void Render();
	void UpdateMouse();
	bool IsInBattle() const { return (this->m_HasState && this->m_State.state == 3 && World >= 18 && World <= 23); }

private:
	void SendInfo();
	void SendEnter();
	void DressAll();
	void RenderEntryWindow();
	void RenderBattleHud();
	void RenderResultModal();

	bool m_Open;
	GC_CHAOS_CASTLE_INFO m_Info;
	DWORD m_InfoTick;

	bool m_HasState;
	GC_CHAOS_CASTLE_STATE m_State;
	DWORD m_StateTick;

	bool m_HasResult;
	GC_CHAOS_CASTLE_RESULT m_Result;
	DWORD m_ResultTick;
};

extern CChaosCastleUI gChaosCastleUI;
