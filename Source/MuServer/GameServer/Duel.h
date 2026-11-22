#pragma once

#include "ProtocolDefines.h"
#include "User.h"

#define MAX_DUEL_SCORE 5
#define DUEL_REQUEST_TIMEOUT 20000

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

struct PMSG_DUEL_ANSWER_RECV
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

class CDuel
{
public:
	CDuel();
	virtual ~CDuel();

	void MainProc();

	bool RequestDuel(int aIndex, int bIndex);
	bool RequestDuelWithSetup(int aIndex, int bIndex, BYTE rules, BYTE betCount, BYTE* betSlots);
	void CGDuelRequestRecv(PMSG_DUEL_REQUEST_SEND* lpMsg, int aIndex);
	void CGDuelAnswerRecv(PMSG_DUEL_ANSWER_RECV* lpMsg, int aIndex);

	void StartDuel(int aIndex, int bIndex);
	void EndDuel(int aIndex, int bIndex, int reason);
	void CancelDuel(int aIndex);
	void ResetDuel(LPOBJ lpObj);

	bool CheckDuel(LPOBJ lpObj, LPOBJ lpTarget);
	void CheckDuelScore(LPOBJ lpObj, LPOBJ lpTarget);
	void RespawnDuelUser(LPOBJ lpObj);

	void CommandDuel(LPOBJ lpObj, char* arg);
	void CommandDuelAccept(LPOBJ lpObj);
	void CommandDuelRefuse(LPOBJ lpObj);
};

extern CDuel gDuel;
