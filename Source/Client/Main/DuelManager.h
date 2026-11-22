#pragma once

#include "ProtocolDefines.h"

enum eDuelRules
{
	DUEL_RULE_NONE    = 0x00,
	DUEL_RULE_NO_HP   = 0x01,
	DUEL_RULE_NO_BUFF = 0x04,
	DUEL_RULE_NO_MANA = 0x08,
};

struct DUEL_BET_ITEM_INFO
{
	WORD index;
	BYTE level;
	BYTE dur;
	BYTE skill;
	BYTE luck;
	BYTE add;
	BYTE exce;
};

struct PMSG_DUEL_REQUEST_SEND
{
	PSBMSG_HEAD header; // C3:AA:01
	BYTE index[2];
	BYTE rules;
	BYTE betItemCount;
	BYTE betSlots[4];
};

struct PMSG_DUEL_REQUEST_RECV
{
	PSBMSG_HEAD header; // C1:AA:01
	BYTE index[2];
	char name[11];
	BYTE rules;
	BYTE betItemCount;
	DUEL_BET_ITEM_INFO betItems[4];
};

struct PMSG_DUEL_ANSWER_SEND
{
	PSBMSG_HEAD header; // C1:AA:02
	BYTE result; // 1 = Accept, 0 = Decline
	BYTE index[2];
	BYTE betItemCount;
	BYTE betSlots[4];
};

struct PMSG_DUEL_START_RECV
{
	PSBMSG_HEAD header; // C1:AA:03
	BYTE index[2];
	char name[11];
	BYTE rules;
};

struct PMSG_DUEL_END_RECV
{
	PSBMSG_HEAD header; // C1:AA:04
	BYTE result; // 0 = Canceled, 1 = Winner, 2 = Loser
	char winner[11];
	char loser[11];
};

struct PMSG_DUEL_SCORE_RECV
{
	PSBMSG_HEAD header; // C1:AA:05
	BYTE myScore;
	BYTE targetScore;
};

struct PMSG_DUEL_OPEN_SETUP_SEND
{
	PSBMSG_HEAD header; // C1:AA:06
	BYTE index[2];
	char name[11];
};

class CDuelManager
{
public:
	CDuelManager();
	~CDuelManager();

	void Reset();
	void Render();
	void UpdateMouse();

	void OpenSetupWindow(WORD targetIndex, const char* targetName);
	void CloseSetupWindow();
	bool IsOpen() const { return (this->m_SetupOpen || this->m_RequestReceived); }
	bool IsInDuel() const { return this->m_InDuel; }

	void OnDuelOpenSetupRecv(BYTE* lpMsg);
	void OnDuelRequestRecv(BYTE* lpMsg);
	void OnDuelStartRecv(BYTE* lpMsg);
	void OnDuelEndRecv(BYTE* lpMsg);
	void OnDuelScoreRecv(BYTE* lpMsg);

private:
	void SendSetupRequest();
	void SendAnswer(bool accept);
	void RenderSetupModal();
	void RenderRequestModal();
	void RenderScoreboard();
	bool UpdateSetupMouse();
	bool UpdateRequestMouse();

	// Challenger Setup Modal
	bool m_SetupOpen;
	WORD m_TargetIndex;
	char m_TargetName[11];
	BYTE m_SelectedRules;
	BYTE m_BetCount;
	BYTE m_BetSlots[4]; // 0..63 main inventory index
	int  m_InvScroll;

	// Challenged Request Modal
	bool m_RequestReceived;
	WORD m_ChallengerIndex;
	char m_ChallengerName[11];
	DWORD m_RequestTick;
	BYTE m_RequestedRules;
	BYTE m_RequestedBetCount;
	DUEL_BET_ITEM_INFO m_RequestedBetItems[4];
	BYTE m_TargetBetCount;
	BYTE m_TargetBetSlots[4];
	int  m_ReqInvScroll;

	// Active Duel Scoreboard
	bool m_InDuel;
	WORD m_OpponentIndex;
	char m_OpponentName[11];
	BYTE m_ActiveRules;
	int  m_MyScore;
	int  m_OpponentScore;
};

extern CDuelManager gDuelManager;
