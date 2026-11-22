#pragma once

#include "ProtocolDefines.h"
#include "User.h"
#include <map>
#include <string>

#define MAX_PARTY_MATCHING_TITLE 32
#define MAX_PARTY_MATCHING_PASSWORD 10
#define PARTY_MATCHING_TTL_MS (60 * 60 * 1000)

enum PARTY_MATCHING_OBJECTIVE
{
	PARTY_OBJECTIVE_ALL = 0,
	PARTY_OBJECTIVE_TRAIN = 1,
	PARTY_OBJECTIVE_BLOOD_CASTLE = 2,
	PARTY_OBJECTIVE_DEVIL_SQUARE = 3,
	PARTY_OBJECTIVE_BOSS = 4,
};

struct CG_PARTY_MATCHING_LIST_REQ
{
	PSBMSG_HEAD header; // C1:99:01
	BYTE page;
	BYTE objectiveFilter;
};

struct PARTY_MATCHING_ENTRY
{
	char leaderName[11];
	WORD leaderLevel;
	BYTE leaderClass;
	char title[MAX_PARTY_MATCHING_TITLE];
	BYTE mapNumber;
	WORD minLevel;
	WORD maxLevel;
	BYTE memberCount;
	BYTE hasPassword;
	BYTE autoAccept;
	BYTE wantedClasses; // bit0=DW, bit1=DK, bit2=ELF, bit3=MG
	BYTE objectiveType;
	WORD expNow;
	WORD expJoin;
};

struct GC_PARTY_MATCHING_LIST_ANS
{
	PSWMSG_HEAD header; // C2:99:01
	BYTE currentPage;
	BYTE totalPages;
	BYTE myRegistered;
	BYTE count;
};

struct CG_PARTY_MATCHING_REGISTER_REQ
{
	PSBMSG_HEAD header; // C1:99:02
	char title[MAX_PARTY_MATCHING_TITLE];
	WORD minLevel;
	WORD maxLevel;
	char password[MAX_PARTY_MATCHING_PASSWORD];
	BYTE autoAccept;
	BYTE wantedClasses;
	BYTE objectiveType;
};

struct GC_PARTY_MATCHING_REGISTER_ANS
{
	PSBMSG_HEAD header; // C1:99:02
	BYTE result;        // 0: Fail, 1: Success, 2: Not leader, 3: Full, 4: Invalid level
};

struct CG_PARTY_MATCHING_CANCEL_REQ
{
	PSBMSG_HEAD header; // C1:99:03
};

struct GC_PARTY_MATCHING_CANCEL_ANS
{
	PSBMSG_HEAD header; // C1:99:03
	BYTE result;        // 0: Fail, 1: Success
};

struct CG_PARTY_MATCHING_JOIN_REQ
{
	PSBMSG_HEAD header; // C1:99:04
	char leaderName[11];
	char password[MAX_PARTY_MATCHING_PASSWORD];
};

struct GC_PARTY_MATCHING_JOIN_ANS
{
	PSBMSG_HEAD header; // C1:99:04
	BYTE result;        // 0: Fail, 1: Joined, 2: Pending Approval, 3: Wrong Password, 4: Level Mismatch, 5: Party Full, 6: Already in Party, 7: Not Found, 8: Class Not Wanted
};

struct GC_PARTY_MATCHING_NOTIFY_LEADER
{
	PSBMSG_HEAD header; // C1:99:05
	char applicantName[11];
	WORD applicantLevel;
	BYTE applicantClass;
};

struct CG_PARTY_MATCHING_ACCEPT_REQ
{
	PSBMSG_HEAD header; // C1:99:06
	char applicantName[11];
	BYTE accept;        // 1: Accept, 0: Decline
};

struct CG_PARTY_MATCHING_MOVE_LEADER_REQ
{
	PSBMSG_HEAD header; // C1:99:07
};

struct CG_PARTY_MATCHING_SET_LEADER_REQ
{
	PSBMSG_HEAD header; // C1:99:08
	char targetName[11];
};

struct PARTY_EXT_MEMBER
{
	char name[11];
	BYTE classCode;
	WORD level;
	BYTE inExpRange;
	BYTE map;
	BYTE x;
	BYTE y;
};

struct GC_PARTY_EXT_INFO_SEND
{
	PSBMSG_HEAD header; // C1:99:09
	WORD expPct;
	BYTE isDiverse;
	BYTE count;
	PARTY_EXT_MEMBER members[5];
};

struct PARTY_REJOIN_INFO
{
	int partyNumber;
	char leaderName[11];
	DWORD expireTick;
};

struct PARTY_MATCHING_ROOM
{
	int leaderIndex;
	char leaderName[11];
	char title[MAX_PARTY_MATCHING_TITLE];
	WORD minLevel;
	WORD maxLevel;
	char password[MAX_PARTY_MATCHING_PASSWORD];
	BYTE autoAccept;
	BYTE wantedClasses;
	BYTE objectiveType;
	DWORD createTick;
};

class CPartyMatching
{
public:
	CPartyMatching();
	~CPartyMatching();

	void CGListRecv(CG_PARTY_MATCHING_LIST_REQ* lpMsg, int aIndex);
	void CGRegisterRecv(CG_PARTY_MATCHING_REGISTER_REQ* lpMsg, int aIndex);
	void CGCancelRecv(CG_PARTY_MATCHING_CANCEL_REQ* lpMsg, int aIndex);
	void CGJoinRecv(CG_PARTY_MATCHING_JOIN_REQ* lpMsg, int aIndex);
	void CGAcceptRecv(CG_PARTY_MATCHING_ACCEPT_REQ* lpMsg, int aIndex);
	void CGMoveToLeaderRecv(CG_PARTY_MATCHING_MOVE_LEADER_REQ* lpMsg, int aIndex);
	void CGSetLeaderRecv(CG_PARTY_MATCHING_SET_LEADER_REQ* lpMsg, int aIndex);

	void SendPartyExtInfo(int partyIndex);
	void OnUserDisconnect(LPOBJ lpObj);
	void OnUserEnterGame(LPOBJ lpObj);

private:
	void CleanupRooms();
	WORD CalcPartyExpPct(int count, bool diverse);
	bool IsPartyDiverse(LPOBJ lpLeader, int addClass = -1);

	std::map<std::string, PARTY_MATCHING_ROOM> m_Rooms;
	std::map<std::string, DWORD> m_BroadcastCooldown;
	std::map<std::string, DWORD> m_MoveCooldown;
	std::map<std::string, PARTY_REJOIN_INFO> m_RejoinMap;
};

extern CPartyMatching gPartyMatching;
