#pragma once

#include "ProtocolDefines.h"

#define MAX_PARTY_MATCHING_TITLE 32
#define MAX_PARTY_MATCHING_PASSWORD 10

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
	BYTE wantedClasses;
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
	BYTE result;
};

struct CG_PARTY_MATCHING_CANCEL_REQ
{
	PSBMSG_HEAD header; // C1:99:03
};

struct GC_PARTY_MATCHING_CANCEL_ANS
{
	PSBMSG_HEAD header; // C1:99:03
	BYTE result;
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
	BYTE result;
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
	BYTE accept;
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

struct PARTY_PENDING_APP
{
	char name[11];
	WORD level;
	BYTE classCode;
	DWORD tick;
};

struct PMSG_PARTY_DEL_MEMBER_SEND
{
	PBMSG_HEAD header; // C1:43
	BYTE number;
};

class CPartyMatchingUI
{
public:
	CPartyMatchingUI();
	~CPartyMatchingUI();

	void Toggle();
	void Open(int tab = -1);
	void Close();
	bool IsOpen() const { return this->m_Open; }
	bool IsEditingInput() const { return this->m_Open && (this->m_FocusField != 0 || this->m_ShowPassModal); }
	void OnChar(wchar_t ch);

	void OnListRecv(BYTE* lpMsg);
	void OnRegisterRecv(BYTE* lpMsg);
	void OnCancelRecv(BYTE* lpMsg);
	void OnJoinRecv(BYTE* lpMsg);
	void OnNotifyLeaderRecv(BYTE* lpMsg);
	void OnExtInfoRecv(BYTE* lpMsg);

	void Render();
	void UpdateMouse();
	void RequestMoveToLeader();

private:
	void RenderMiniHud();
	bool UpdateMiniHudMouse();
	void RequestList(int page);
	void RequestRegister();
	void RequestCancel();
	void RequestJoin(const char* leaderName, const char* password);
	void RequestAccept(const char* appName, bool accept);
	void RequestSetLeader(const char* targetName);
	void SendPartyDelMember(BYTE number);
	void SetStatus(const char* text, bool good);
	const PARTY_EXT_MEMBER* FindExtMember(const char* name) const;
	void PopFrontPendingApp();

	bool m_Open;
	int m_Tab; // 0: My Party, 1: Find Party, 2: Recruit
	int m_FilterObj; // 0: All, 1: Train, 2: BC, 3: DS, 4: Boss
	int m_Page;
	int m_TotalPages;
	bool m_MyRegistered;
	int m_Count;
	PARTY_MATCHING_ENTRY m_Entries[6];

	// Recruit form fields
	int m_FocusField; // 0=none, 1=title, 2=minLv, 3=maxLv, 4=password
	char m_Title[MAX_PARTY_MATCHING_TITLE];
	char m_MinLvStr[8];
	char m_MaxLvStr[8];
	char m_Pass[MAX_PARTY_MATCHING_PASSWORD];
	bool m_AutoAccept;
	BYTE m_WantedClasses;
	BYTE m_Objective;

	// Password join modal
	bool m_ShowPassModal;
	char m_JoinLeader[11];
	char m_JoinPass[MAX_PARTY_MATCHING_PASSWORD];

	// Incoming join request queue (for Leader, up to 5)
	int m_PendingCount;
	PARTY_PENDING_APP m_PendingApps[5];

	// Live extended party info (EXP %, Diverse flag, Class, Level, In-Range)
	WORD m_PartyExpPct;
	BYTE m_PartyIsDiverse;
	int m_ExtCount;
	PARTY_EXT_MEMBER m_ExtMembers[5];

	DWORD m_StatusTick;
	bool m_StatusGood;
	char m_StatusText[96];
};

extern CPartyMatchingUI gPartyMatchingUI;
