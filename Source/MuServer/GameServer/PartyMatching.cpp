#include "stdafx.h"
#include "PartyMatching.h"
#include "Map.h"
#include "Notice.h"
#include "Party.h"
#include "ServerInfo.h"
#include "SocketManager.h"
#include "Util.h"
#include <vector>

static const char* PMGetMapName(int map)
{
	switch (map)
	{
		case MAP_LORENCIA: return "Lorencia";
		case MAP_DUNGEON: return "Dungeon";
		case MAP_DEVIAS: return "Devias";
		case MAP_NORIA: return "Noria";
		case MAP_LOST_TOWER: return "LostTower";
		case MAP_ARENA: return "Stadium";
		case MAP_ATLANS: return "Atlans";
		case MAP_TARKAN: return "Tarkan";
		case MAP_DEVIL_SQUARE1: return "DevilSquare";
		case MAP_ICARUS: return "Icarus";
		default:
			if (BC_MAP_RANGE(map)) return "BloodCastle";
			return "MU World";
	}
}

static int PMGetMinMapLevel(int map)
{
	switch (map)
	{
		case MAP_LORENCIA:
		case MAP_NORIA:
		case MAP_DEVIAS: return 1;
		case MAP_DUNGEON: return 20;
		case MAP_LOST_TOWER: return 40;
		case MAP_ARENA: return 50;
		case MAP_ATLANS: return 50;
		case MAP_TARKAN: return 100;
		case MAP_ICARUS: return 160;
		default: return 1;
	}
}

CPartyMatching gPartyMatching;

CPartyMatching::CPartyMatching()
{
}

CPartyMatching::~CPartyMatching()
{
}

void CPartyMatching::CleanupRooms()
{
	DWORD now = GetTickCount();
	for (std::map<std::string, PARTY_MATCHING_ROOM>::iterator it = this->m_Rooms.begin(); it != this->m_Rooms.end(); )
	{
		int idx = it->second.leaderIndex;
		bool valid = false;
		if (OBJECT_RANGE(idx) && gObjIsConnectedGP(idx) && _stricmp(gObj[idx].Name, it->second.leaderName) == 0)
		{
			LPOBJ lpLeader = &gObj[idx];
			if ((now - it->second.createTick) < PARTY_MATCHING_TTL_MS)
			{
				if (lpLeader->PartyNumber < 0 || gParty.IsLeader(lpLeader->PartyNumber, idx))
				{
					valid = true;
				}
			}
		}
		if (!valid) it = this->m_Rooms.erase(it);
		else ++it;
	}
}

WORD CPartyMatching::CalcPartyExpPct(int count, bool diverse)
{
	if (count <= 1) return 100;
	if (count > MAX_PARTY_USER) count = MAX_PARTY_USER;
	int idx = count - 1;
	int val = diverse ? gServerInfo.m_PartySpecialExperience[idx] : gServerInfo.m_PartyGeneralExperience[idx];
	return (WORD)(val > 0 ? val : 100);
}

bool CPartyMatching::IsPartyDiverse(LPOBJ lpLeader, int addClass)
{
	BYTE seen = 0;
	int distinct = 0;

	auto addCls = [&](int c)
	{
		if (c < 0 || c > 3) return;
		if ((seen & (1 << c)) == 0)
		{
			seen |= (1 << c);
			distinct++;
		}
	};

	if (lpLeader->PartyNumber >= 0 && gParty.IsParty(lpLeader->PartyNumber))
	{
		int cnt = gParty.GetMemberCount(lpLeader->PartyNumber);
		for (int i = 0; i < cnt; i++)
		{
			int mIdx = gParty.GetMemberIndex(lpLeader->PartyNumber, i);
			if (OBJECT_RANGE(mIdx) && gObjIsConnectedGP(mIdx))
			{
				addCls(gObj[mIdx].Class);
			}
		}
	}
	else
	{
		addCls(lpLeader->Class);
	}

	if (addClass >= 0) addCls(addClass);
	return (distinct >= 2);
}

void CPartyMatching::CGListRecv(CG_PARTY_MATCHING_LIST_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpObj = &gObj[aIndex];

	this->CleanupRooms();

	std::vector<PARTY_MATCHING_ENTRY> list;
	for (std::map<std::string, PARTY_MATCHING_ROOM>::iterator it = this->m_Rooms.begin(); it != this->m_Rooms.end(); ++it)
	{
		const PARTY_MATCHING_ROOM& rm = it->second;
		if (lpMsg->objectiveFilter != PARTY_OBJECTIVE_ALL && rm.objectiveType != lpMsg->objectiveFilter)
		{
			continue;
		}

		LPOBJ lpLeader = &gObj[rm.leaderIndex];
		int memCount = (lpLeader->PartyNumber >= 0 && gParty.IsParty(lpLeader->PartyNumber)) ? gParty.GetMemberCount(lpLeader->PartyNumber) : 1;

		PARTY_MATCHING_ENTRY e;
		memset(&e, 0, sizeof(e));
		strcpy_s(e.leaderName, sizeof(e.leaderName), lpLeader->Name);
		e.leaderLevel = (WORD)lpLeader->Level;
		e.leaderClass = (BYTE)lpLeader->Class;
		strcpy_s(e.title, sizeof(e.title), rm.title);
		e.mapNumber = (BYTE)lpLeader->Map;
		e.minLevel = rm.minLevel;
		e.maxLevel = rm.maxLevel;
		e.memberCount = (BYTE)memCount;
		e.hasPassword = (rm.password[0] != '\0') ? 1 : 0;
		e.autoAccept = rm.autoAccept;
		e.wantedClasses = rm.wantedClasses;
		e.objectiveType = rm.objectiveType;
		e.expNow = this->CalcPartyExpPct(memCount, this->IsPartyDiverse(lpLeader, -1));
		e.expJoin = (memCount < MAX_PARTY_USER) ? this->CalcPartyExpPct(memCount + 1, this->IsPartyDiverse(lpLeader, lpObj->Class)) : 0;
		list.push_back(e);
	}

	const int perPage = 6;
	int total = (int)list.size();
	int totalPages = (total + perPage - 1) / perPage;
	if (totalPages < 1) totalPages = 1;
	int page = lpMsg->page;
	if (page < 1) page = 1;
	if (page > totalPages) page = totalPages;

	BYTE sendBuf[4096];
	GC_PARTY_MATCHING_LIST_ANS* pAns = (GC_PARTY_MATCHING_LIST_ANS*)sendBuf;
	pAns->currentPage = (BYTE)page;
	pAns->totalPages = (BYTE)totalPages;
	pAns->myRegistered = (this->m_Rooms.find(lpObj->Name) != this->m_Rooms.end()) ? 1 : 0;
	pAns->count = 0;

	PARTY_MATCHING_ENTRY* out = (PARTY_MATCHING_ENTRY*)(sendBuf + sizeof(GC_PARTY_MATCHING_LIST_ANS));
	int start = (page - 1) * perPage;
	for (int i = start; i < total && pAns->count < perPage; i++)
	{
		out[pAns->count++] = list[i];
	}

	int totalSize = sizeof(GC_PARTY_MATCHING_LIST_ANS) + (pAns->count * sizeof(PARTY_MATCHING_ENTRY));
	pAns->header.set(0x99, 0x01, totalSize);
	DataSend(aIndex, sendBuf, totalSize);
}

void CPartyMatching::CGRegisterRecv(CG_PARTY_MATCHING_REGISTER_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpObj = &gObj[aIndex];

	GC_PARTY_MATCHING_REGISTER_ANS ans;
	ans.header.set(0x99, 0x02, sizeof(ans));
	ans.result = 0;

	if (lpObj->PartyNumber >= 0 && gParty.IsParty(lpObj->PartyNumber))
	{
		if (!gParty.IsLeader(lpObj->PartyNumber, aIndex))
		{
			ans.result = 2;
			DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
			return;
		}
		if (gParty.GetMemberCount(lpObj->PartyNumber) >= MAX_PARTY_USER)
		{
			ans.result = 3;
			DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
			return;
		}
	}

	WORD minLv = lpMsg->minLevel;
	WORD maxLv = lpMsg->maxLevel;
	if (minLv < 1) minLv = 1;
	if (maxLv < minLv || maxLv > MAX_CHARACTER_LEVEL) maxLv = MAX_CHARACTER_LEVEL;

	PARTY_MATCHING_ROOM rm;
	memset(&rm, 0, sizeof(rm));
	rm.leaderIndex = aIndex;
	strcpy_s(rm.leaderName, sizeof(rm.leaderName), lpObj->Name);
	strncpy_s(rm.title, sizeof(rm.title), lpMsg->title, _TRUNCATE);
	if (rm.title[0] == '\0') sprintf_s(rm.title, sizeof(rm.title), "%s's Party", lpObj->Name);
	rm.minLevel = minLv;
	rm.maxLevel = maxLv;
	strncpy_s(rm.password, sizeof(rm.password), lpMsg->password, _TRUNCATE);
	rm.autoAccept = lpMsg->autoAccept ? 1 : 0;
	rm.wantedClasses = (lpMsg->wantedClasses == 0) ? 0x0F : (lpMsg->wantedClasses & 0x0F);
	rm.objectiveType = (lpMsg->objectiveType >= 1 && lpMsg->objectiveType <= 4) ? lpMsg->objectiveType : PARTY_OBJECTIVE_TRAIN;
	rm.createTick = GetTickCount();

	this->m_Rooms[lpObj->Name] = rm;

	ans.result = 1;
	DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
	gNotice.GCNoticeSend(aIndex, 1, "[Party Matching] Party room registered successfully!");

	// Global Recruit Broadcast (60s cooldown per leader to prevent spam)
	DWORD now = GetTickCount();
	std::map<std::string, DWORD>::iterator bit = this->m_BroadcastCooldown.find(lpObj->Name);
	if (bit == this->m_BroadcastCooldown.end() || (now - bit->second) >= 60000)
	{
		this->m_BroadcastCooldown[lpObj->Name] = now;
		int cnt = (lpObj->PartyNumber >= 0 && gParty.IsParty(lpObj->PartyNumber)) ? gParty.GetMemberCount(lpObj->PartyNumber) : 1;
		gNotice.GCNoticeSendToAll(1, "[Party] %s recruiting Lv.%u-%u (%s | %d/5) - Press P to Join!", lpObj->Name, rm.minLevel, rm.maxLevel, PMGetMapName(lpObj->Map), cnt);
	}
}

void CPartyMatching::CGCancelRecv(CG_PARTY_MATCHING_CANCEL_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpObj = &gObj[aIndex];

	GC_PARTY_MATCHING_CANCEL_ANS ans;
	ans.header.set(0x99, 0x03, sizeof(ans));
	ans.result = (this->m_Rooms.erase(lpObj->Name) > 0) ? 1 : 0;
	DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
	if (ans.result == 1)
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Party Matching] Party listing cancelled.");
	}
}

void CPartyMatching::CGJoinRecv(CG_PARTY_MATCHING_JOIN_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpObj = &gObj[aIndex];

	GC_PARTY_MATCHING_JOIN_ANS ans;
	ans.header.set(0x99, 0x04, sizeof(ans));
	ans.result = 0;

	if (lpObj->PartyNumber >= 0 && gParty.IsParty(lpObj->PartyNumber))
	{
		ans.result = 6; // Already in party
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		return;
	}

	this->CleanupRooms();
	char leaderKey[11] = { 0 };
	strncpy_s(leaderKey, sizeof(leaderKey), lpMsg->leaderName, _TRUNCATE);

	std::map<std::string, PARTY_MATCHING_ROOM>::iterator it = this->m_Rooms.find(leaderKey);
	if (it == this->m_Rooms.end())
	{
		ans.result = 7; // Not found
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		return;
	}

	PARTY_MATCHING_ROOM& rm = it->second;
	LPOBJ lpLeader = &gObj[rm.leaderIndex];
	if (lpLeader->Index == aIndex)
	{
		ans.result = 6;
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		return;
	}

	if (rm.password[0] != '\0' && strcmp(rm.password, lpMsg->password) != 0)
	{
		ans.result = 3; // Wrong password
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		return;
	}

	if (lpObj->Level < rm.minLevel || lpObj->Level > rm.maxLevel)
	{
		ans.result = 4; // Level mismatch
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		return;
	}

	if ((rm.wantedClasses & (1 << lpObj->Class)) == 0)
	{
		ans.result = 8; // Class not wanted
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		return;
	}

	if (lpLeader->PartyNumber >= 0 && gParty.IsParty(lpLeader->PartyNumber) && gParty.GetMemberCount(lpLeader->PartyNumber) >= MAX_PARTY_USER)
	{
		ans.result = 5; // Party full
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		return;
	}

	if (rm.autoAccept)
	{
		if (lpLeader->PartyNumber < 0 || !gParty.IsParty(lpLeader->PartyNumber))
		{
			if (!gParty.Create(lpLeader->Index))
			{
				DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
				return;
			}
		}

		if (gParty.AddMember(lpLeader->PartyNumber, aIndex))
		{
			gParty.GCPartyListSend(lpLeader->PartyNumber);
			ans.result = 1;
			DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
			gNotice.GCNoticeSend(aIndex, 1, "[Party Matching] Joined %s's party!", lpLeader->Name);
			gNotice.GCNoticeSend(lpLeader->Index, 1, "[Party Matching] %s joined your party!", lpObj->Name);
			if (gParty.GetMemberCount(lpLeader->PartyNumber) >= MAX_PARTY_USER)
			{
				this->m_Rooms.erase(it);
			}
			return;
		}
		ans.result = 5;
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
	}
	else
	{
		GC_PARTY_MATCHING_NOTIFY_LEADER notify;
		notify.header.set(0x99, 0x05, sizeof(notify));
		strcpy_s(notify.applicantName, sizeof(notify.applicantName), lpObj->Name);
		notify.applicantLevel = (WORD)lpObj->Level;
		notify.applicantClass = (BYTE)lpObj->Class;
		DataSend(lpLeader->Index, (BYTE*)&notify, sizeof(notify));

		ans.result = 2; // Pending approval
		DataSend(aIndex, (BYTE*)&ans, sizeof(ans));
		gNotice.GCNoticeSend(aIndex, 1, "[Party Matching] Join request sent to %s.", lpLeader->Name);
	}
}

void CPartyMatching::CGAcceptRecv(CG_PARTY_MATCHING_ACCEPT_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpLeader = &gObj[aIndex];

	char appName[11] = { 0 };
	strncpy_s(appName, sizeof(appName), lpMsg->applicantName, _TRUNCATE);

	LPOBJ lpApp = gObjFind(appName);
	if (lpApp == NULL || !gObjIsConnectedGP(lpApp->Index))
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Party Matching] Player %s is no longer online.", appName);
		return;
	}

	if (!lpMsg->accept)
	{
		gNotice.GCNoticeSend(lpApp->Index, 1, "[Party Matching] %s declined your party request.", lpLeader->Name);
		return;
	}

	if (lpApp->PartyNumber >= 0 && gParty.IsParty(lpApp->PartyNumber))
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Party Matching] %s is already in another party.", lpApp->Name);
		return;
	}

	if (lpLeader->PartyNumber < 0 || !gParty.IsParty(lpLeader->PartyNumber))
	{
		if (!gParty.Create(lpLeader->Index)) return;
	}

	if (gParty.GetMemberCount(lpLeader->PartyNumber) >= MAX_PARTY_USER)
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Party Matching] Your party is already full.");
		return;
	}

	if (gParty.AddMember(lpLeader->PartyNumber, lpApp->Index))
	{
		gParty.GCPartyListSend(lpLeader->PartyNumber);
		gNotice.GCNoticeSend(lpApp->Index, 1, "[Party Matching] Joined %s's party!", lpLeader->Name);
		gNotice.GCNoticeSend(aIndex, 1, "[Party Matching] %s joined your party!", lpApp->Name);
		if (gParty.GetMemberCount(lpLeader->PartyNumber) >= MAX_PARTY_USER)
		{
			this->m_Rooms.erase(lpLeader->Name);
		}
	}
}

void CPartyMatching::CGMoveToLeaderRecv(CG_PARTY_MATCHING_MOVE_LEADER_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpObj = &gObj[aIndex];

	if (lpObj->PartyNumber < 0 || !gParty.IsParty(lpObj->PartyNumber))
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Party] You are not in a party.");
		return;
	}

	if (gParty.IsLeader(lpObj->PartyNumber, aIndex))
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Party] You are already the Party Leader.");
		return;
	}

	int leaderIdx = gParty.GetMemberIndex(lpObj->PartyNumber, 0);
	if (!OBJECT_RANGE(leaderIdx) || !gObjIsConnectedGP(leaderIdx))
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Party] Party Leader is offline.");
		return;
	}

	LPOBJ lpLeader = &gObj[leaderIdx];

	// Block event maps (BC / DS)
	if (BC_MAP_RANGE(lpLeader->Map) || DS_MAP_RANGE(lpLeader->Map) || BC_MAP_RANGE(lpObj->Map) || DS_MAP_RANGE(lpObj->Map))
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Party] Cannot warp into or out of Blood Castle / Devil Square.");
		return;
	}

	// Level check for destination map
	int reqLv = PMGetMinMapLevel(lpLeader->Map);
	if (lpObj->Level < reqLv)
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Party] Requires Level %d to warp to %s.", reqLv, PMGetMapName(lpLeader->Map));
		return;
	}

	// 30s cooldown per player
	DWORD now = GetTickCount();
	std::map<std::string, DWORD>::iterator mit = this->m_MoveCooldown.find(lpObj->Name);
	if (mit != this->m_MoveCooldown.end() && (now - mit->second) < 30000)
	{
		DWORD rem = (30000 - (now - mit->second) + 999) / 1000;
		gNotice.GCNoticeSend(aIndex, 1, "[Party] Move to Leader cooldown: %us remaining.", rem);
		return;
	}

	// Small Zen fee (10,000 Zen for normal accounts, FREE for VIP)
	const DWORD moveCost = (lpObj->AccountLevel > 0) ? 0 : 10000;
	if (moveCost > 0)
	{
		if (lpObj->Money < moveCost)
		{
			gNotice.GCNoticeSend(aIndex, 1, "[Party] Requires 10,000 Zen to warp to Leader (FREE for VIP).");
			return;
		}
		lpObj->Money -= moveCost;
		GCMoneySend(aIndex, lpObj->Money);
	}

	this->m_MoveCooldown[lpObj->Name] = now;
	gObjTeleport(aIndex, lpLeader->Map, lpLeader->X, lpLeader->Y);
	gNotice.GCNoticeSend(aIndex, 1, "[Party] Warped to Leader %s (%s %d,%d).", lpLeader->Name, PMGetMapName(lpLeader->Map), lpLeader->X, lpLeader->Y);
}

void CPartyMatching::CGSetLeaderRecv(CG_PARTY_MATCHING_SET_LEADER_REQ* lpMsg, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpObj = &gObj[aIndex];

	if (lpObj->PartyNumber < 0 || !gParty.IsParty(lpObj->PartyNumber))
	{
		return;
	}

	if (!gParty.IsLeader(lpObj->PartyNumber, aIndex))
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Party] Only the Party Leader can transfer leadership.");
		return;
	}

	char targetName[11] = { 0 };
	strncpy_s(targetName, sizeof(targetName), lpMsg->targetName, _TRUNCATE);

	LPOBJ lpTarget = gObjFind(targetName);
	if (lpTarget == NULL || !gObjIsConnectedGP(lpTarget->Index) || lpTarget->PartyNumber != lpObj->PartyNumber)
	{
		gNotice.GCNoticeSend(aIndex, 1, "[Party] Target member not found in your party.");
		return;
	}

	if (gParty.SetLeader(lpObj->PartyNumber, lpTarget->Index))
	{
		// Transfer active Party Matching room ownership if one exists
		std::map<std::string, PARTY_MATCHING_ROOM>::iterator it = this->m_Rooms.find(lpObj->Name);
		if (it != this->m_Rooms.end())
		{
			PARTY_MATCHING_ROOM rm = it->second;
			this->m_Rooms.erase(it);
			rm.leaderIndex = lpTarget->Index;
			strcpy_s(rm.leaderName, sizeof(rm.leaderName), lpTarget->Name);
			this->m_Rooms[lpTarget->Name] = rm;
		}

		gNotice.GCNoticeSend(aIndex, 1, "[Party] Transferred Party Leadership to %s.", lpTarget->Name);
		gNotice.GCNoticeSend(lpTarget->Index, 1, "[Party] %s promoted you to Party Leader!", lpObj->Name);
	}
}

void CPartyMatching::SendPartyExtInfo(int partyIndex)
{
	if (!gParty.IsParty(partyIndex)) return;

	int leaderIdx = gParty.GetMemberIndex(partyIndex, 0);
	if (!OBJECT_RANGE(leaderIdx) || !gObjIsConnectedGP(leaderIdx)) return;
	LPOBJ lpLeader = &gObj[leaderIdx];

	int memCount = gParty.GetMemberCount(partyIndex);
	bool diverse = this->IsPartyDiverse(lpLeader, -1);

	GC_PARTY_EXT_INFO_SEND pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.header.set(0x99, 0x09, sizeof(pMsg));
	pMsg.expPct = this->CalcPartyExpPct(memCount, diverse);
	pMsg.isDiverse = diverse ? 1 : 0;
	pMsg.count = 0;

	for (int n = 0; n < MAX_PARTY_USER; n++)
	{
		int mIdx = gParty.GetMemberIndex(partyIndex, n);
		if (!OBJECT_RANGE(mIdx) || !gObjIsConnectedGP(mIdx)) continue;
		LPOBJ lpMem = &gObj[mIdx];

		PARTY_EXT_MEMBER& out = pMsg.members[pMsg.count++];
		strcpy_s(out.name, sizeof(out.name), lpMem->Name);
		out.classCode = (BYTE)lpMem->Class;
		out.level = (WORD)lpMem->Level;

		bool inRange = (lpMem->Map == lpLeader->Map && gObjCalcDistance(lpLeader, lpMem) <= MAX_PARTY_DISTANCE);
		out.inExpRange = inRange ? 1 : 0;
		out.map = (BYTE)lpMem->Map;
		out.x = (BYTE)lpMem->X;
		out.y = (BYTE)lpMem->Y;
	}

	for (int n = 0; n < MAX_PARTY_USER; n++)
	{
		int mIdx = gParty.GetMemberIndex(partyIndex, n);
		if (OBJECT_RANGE(mIdx) && gObjIsConnectedGP(mIdx))
		{
			DataSend(mIdx, (BYTE*)&pMsg, sizeof(pMsg));
		}
	}
}

void CPartyMatching::OnUserDisconnect(LPOBJ lpObj)
{
	if (!lpObj || lpObj->PartyNumber < 0 || !gParty.IsParty(lpObj->PartyNumber)) return;

	int leaderIdx = gParty.GetMemberIndex(lpObj->PartyNumber, 0);
	if (leaderIdx == lpObj->Index)
	{
		// If leader disconnects, pick next remaining member as anchor
		leaderIdx = -1;
		for (int n = 1; n < MAX_PARTY_USER; n++)
		{
			int mIdx = gParty.GetMemberIndex(lpObj->PartyNumber, n);
			if (OBJECT_RANGE(mIdx) && gObjIsConnectedGP(mIdx))
			{
				leaderIdx = mIdx;
				break;
			}
		}
	}
	if (!OBJECT_RANGE(leaderIdx) || !gObjIsConnectedGP(leaderIdx)) return;

	PARTY_REJOIN_INFO info;
	memset(&info, 0, sizeof(info));
	info.partyNumber = lpObj->PartyNumber;
	strcpy_s(info.leaderName, sizeof(info.leaderName), gObj[leaderIdx].Name);
	info.expireTick = GetTickCount() + 180000; // 3 minutes auto-rejoin window
	this->m_RejoinMap[lpObj->Name] = info;
}

void CPartyMatching::OnUserEnterGame(LPOBJ lpObj)
{
	if (!lpObj || !gObjIsConnectedGP(lpObj->Index)) return;
	if (lpObj->PartyNumber >= 0 && gParty.IsParty(lpObj->PartyNumber)) return;

	std::map<std::string, PARTY_REJOIN_INFO>::iterator it = this->m_RejoinMap.find(lpObj->Name);
	if (it == this->m_RejoinMap.end()) return;

	PARTY_REJOIN_INFO info = it->second;
	this->m_RejoinMap.erase(it);

	if (GetTickCount() > info.expireTick) return;

	int targetParty = -1;
	if (gParty.IsParty(info.partyNumber) && gParty.GetMemberCount(info.partyNumber) < MAX_PARTY_USER)
	{
		targetParty = info.partyNumber;
	}
	else
	{
		LPOBJ lpAnchor = gObjFind(info.leaderName);
		if (lpAnchor && gObjIsConnectedGP(lpAnchor->Index) && lpAnchor->PartyNumber >= 0 && gParty.IsParty(lpAnchor->PartyNumber) && gParty.GetMemberCount(lpAnchor->PartyNumber) < MAX_PARTY_USER)
		{
			targetParty = lpAnchor->PartyNumber;
		}
	}

	if (targetParty >= 0 && gParty.AddMember(targetParty, lpObj->Index))
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, "[Party] Automatically rejoined your party after reconnect!");
		int leaderIdx = gParty.GetMemberIndex(targetParty, 0);
		if (OBJECT_RANGE(leaderIdx) && leaderIdx != lpObj->Index && gObjIsConnectedGP(leaderIdx))
		{
			gNotice.GCNoticeSend(leaderIdx, 1, "[Party] %s reconnected and rejoined your party.", lpObj->Name);
		}
	}
}
