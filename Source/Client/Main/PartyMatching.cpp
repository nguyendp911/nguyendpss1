#include "stdafx.h"
#include "PartyMatching.h"
#include "AutoHelper.h"
#include "Font.h"
#include "MapManager.h"
#include "MiniMap.h"
#include "Protocol.h"
#include "Sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CPartyMatchingUI gPartyMatchingUI;

static const float PM_PW = 468.0f;
static const float PM_PH = 352.0f;
static const float PM_PX = (640.0f - PM_PW) * 0.5f; // 86
static const float PM_PY = 48.0f;

static bool PMHit(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static DWORD PMColor(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static void PMSetFont(HFONT f)
{
	if (f) SelectObject(m_hFontDC, f);
}

static float PMMeasureW(const char* text)
{
	if (!text || !text[0]) return 0.0f;
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (float)sz.cx / (g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f);
}

static void PMRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

static void PMText(float x, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	PMSetFont(bold ? g_hFontBold : g_hFont);
	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	SetBackgroundTextColor = 0;
	SetTextColor = PMColor(0, 0, 0, 220);
	RenderText((int)x + 1, (int)y + 1, (char*)text, 0, RT3_SORT_LEFT, NULL);
	SetTextColor = color;
	RenderText((int)x, (int)y, (char*)text, 0, RT3_SORT_LEFT, NULL);
}

static void PMTextCentered(float cx, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	PMSetFont(bold ? g_hFontBold : g_hFont);
	float w = PMMeasureW(text);
	PMText(cx - w * 0.5f, y, text, color, bold);
}

static void PMDrawButton(float x, float y, float w, float h, const char* text, int style, bool enabled = true)
{
	bool hov = enabled && PMHit(x, y, w, h);
	float br = 0.24f, bg = 0.29f, bb = 0.35f;
	float fr = 0.085f, fg = 0.115f, fb = 0.155f;
	DWORD tc = PMColor(220, 234, 244, 255);

	if (!enabled)
	{
		br = 0.16f; bg = 0.19f; bb = 0.22f;
		fr = 0.06f; fg = 0.075f; fb = 0.095f;
		tc = PMColor(105, 118, 130, 255);
	}
	else if (style == 1) // Active tab / cyan
	{
		br = 0.42f; bg = 0.80f; bb = 0.90f;
		fr = 0.12f; fg = 0.24f; fb = 0.32f;
		tc = PMColor(235, 250, 255, 255);
	}
	else if (style == 2) // Green confirm / Join
	{
		br = hov ? 0.35f : 0.22f; bg = hov ? 0.85f : 0.65f; bb = hov ? 0.48f : 0.34f;
		fr = hov ? 0.10f : 0.06f; fg = hov ? 0.28f : 0.20f; fb = hov ? 0.14f : 0.10f;
		tc = PMColor(215, 255, 225, 255);
	}
	else if (style == 3) // Red Kick / Leave / Cancel
	{
		br = hov ? 0.88f : 0.66f; bg = hov ? 0.32f : 0.22f; bb = hov ? 0.32f : 0.22f;
		fr = hov ? 0.28f : 0.20f; fg = hov ? 0.08f : 0.06f; fb = hov ? 0.08f : 0.06f;
		tc = PMColor(255, 215, 215, 255);
	}
	else if (hov)
	{
		br = 0.38f; bg = 0.68f; bb = 0.78f;
		fr = 0.11f; fg = 0.18f; fb = 0.24f;
	}

	PMRect(x, y, w, h, br, bg, bb, 0.96f);
	PMRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fr, fg, fb, 0.95f);
	PMTextCentered(x + w * 0.5f, y + (h - 9.0f) * 0.5f, text, tc, (style == 1 || style == 2 || style == 3));
}

static void PMDrawCheckbox(float x, float y, const char* label, bool checked)
{
	bool hov = PMHit(x, y, 12.0f + PMMeasureW(label) + 6.0f, 13.0f);
	PMRect(x, y, 11.0f, 11.0f, checked ? 0.42f : (hov ? 0.38f : 0.25f), checked ? 0.80f : (hov ? 0.62f : 0.32f), checked ? 0.90f : (hov ? 0.72f : 0.40f), 0.96f);
	PMRect(x + 1.0f, y + 1.0f, 9.0f, 9.0f, 0.05f, 0.075f, 0.105f, 0.96f);
	if (checked)
	{
		PMRect(x + 3.0f, y + 3.0f, 5.0f, 5.0f, 0.40f, 0.86f, 0.96f, 1.0f);
	}
	PMText(x + 15.0f, y + 1.0f, label, checked ? PMColor(230, 248, 255, 255) : PMColor(168, 188, 204, 255), false);
}

static const char* PMClassName(BYTE c)
{
	switch (c)
	{
	case 0: return "DW";
	case 1: return "DK";
	case 2: return "ELF";
	case 3: return "MG";
	default: return "?";
	}
}

static const char* PMObjName(BYTE obj)
{
	switch (obj)
	{
	case 1: return "Train";
	case 2: return "BC";
	case 3: return "DS";
	case 4: return "Boss";
	default: return "All";
	}
}

CPartyMatchingUI::CPartyMatchingUI()
{
	this->m_Open = false;
	this->m_Tab = 0;
	this->m_FilterObj = 0;
	this->m_Page = 1;
	this->m_TotalPages = 1;
	this->m_MyRegistered = false;
	this->m_Count = 0;
	memset(this->m_Entries, 0, sizeof(this->m_Entries));

	this->m_FocusField = 0;
	strcpy_s(this->m_Title, sizeof(this->m_Title), "Train Party - Diverse Class");
	strcpy_s(this->m_MinLvStr, sizeof(this->m_MinLvStr), "1");
	strcpy_s(this->m_MaxLvStr, sizeof(this->m_MaxLvStr), "1000");
	this->m_Pass[0] = 0;
	this->m_AutoAccept = true;
	this->m_WantedClasses = 0x0F;
	this->m_Objective = 1;

	this->m_ShowPassModal = false;
	this->m_JoinLeader[0] = 0;
	this->m_JoinPass[0] = 0;

	this->m_PendingCount = 0;
	memset(this->m_PendingApps, 0, sizeof(this->m_PendingApps));

	this->m_PartyExpPct = 100;
	this->m_PartyIsDiverse = 0;
	this->m_ExtCount = 0;
	memset(this->m_ExtMembers, 0, sizeof(this->m_ExtMembers));

	this->m_StatusTick = 0;
	this->m_StatusGood = true;
	this->m_StatusText[0] = 0;
}

CPartyMatchingUI::~CPartyMatchingUI()
{
}

void CPartyMatchingUI::SetStatus(const char* text, bool good)
{
	strcpy_s(this->m_StatusText, sizeof(this->m_StatusText), text);
	this->m_StatusGood = good;
	this->m_StatusTick = GetTickCount();
}

void CPartyMatchingUI::Toggle()
{
	if (this->m_Open) this->Close();
	else this->Open(-1);
}

void CPartyMatchingUI::Open(int tab)
{
	this->m_Open = true;
	this->m_FocusField = 0;
	this->m_ShowPassModal = false;
	if (tab >= 0 && tab <= 2) this->m_Tab = tab;
	else this->m_Tab = (PartyNumber > 0) ? 0 : 1; // If in party -> My Party, if solo -> Find Party
	PlayBuffer(25, 0, 0);
	this->RequestList(this->m_Page);
}

void CPartyMatchingUI::Close()
{
	this->m_Open = false;
	this->m_FocusField = 0;
	this->m_ShowPassModal = false;
	PlayBuffer(29, 0, 0);
}

void CPartyMatchingUI::OnChar(wchar_t ch)
{
	if (!this->m_Open) return;

	if (ch == 27) // ESC
	{
		if (this->m_ShowPassModal) this->m_ShowPassModal = false;
		else this->m_FocusField = 0;
		return;
	}

	char* buf = NULL;
	int maxLen = 0;
	bool digitsOnly = false;

	if (this->m_ShowPassModal)
	{
		buf = this->m_JoinPass; maxLen = MAX_PARTY_MATCHING_PASSWORD - 1;
	}
	else if (this->m_FocusField == 1) { buf = this->m_Title; maxLen = MAX_PARTY_MATCHING_TITLE - 1; }
	else if (this->m_FocusField == 2) { buf = this->m_MinLvStr; maxLen = 4; digitsOnly = true; }
	else if (this->m_FocusField == 3) { buf = this->m_MaxLvStr; maxLen = 4; digitsOnly = true; }
	else if (this->m_FocusField == 4) { buf = this->m_Pass; maxLen = MAX_PARTY_MATCHING_PASSWORD - 1; }

	if (!buf) return;

	int len = (int)strlen(buf);
	if (ch == 8) // Backspace
	{
		if (len > 0) buf[len - 1] = 0;
	}
	else if (ch >= 32 && ch < 127 && len < maxLen)
	{
		if (digitsOnly && (ch < '0' || ch > '9')) return;
		buf[len] = (char)ch;
		buf[len + 1] = 0;
	}
}

void CPartyMatchingUI::RequestList(int page)
{
	CG_PARTY_MATCHING_LIST_REQ pMsg;
	pMsg.header.set(0x99, 0x01, sizeof(pMsg));
	pMsg.page = (BYTE)page;
	pMsg.objectiveFilter = (BYTE)this->m_FilterObj;
	gProtocol.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CPartyMatchingUI::RequestRegister()
{
	CG_PARTY_MATCHING_REGISTER_REQ pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.header.set(0x99, 0x02, sizeof(pMsg));
	strncpy_s(pMsg.title, sizeof(pMsg.title), this->m_Title, _TRUNCATE);
	int mn = atoi(this->m_MinLvStr);
	int mx = atoi(this->m_MaxLvStr);
	if (mn < 1) mn = 1;
	if (mx < mn || mx > 1000) mx = 1000;
	pMsg.minLevel = (WORD)mn;
	pMsg.maxLevel = (WORD)mx;
	strncpy_s(pMsg.password, sizeof(pMsg.password), this->m_Pass, _TRUNCATE);
	pMsg.autoAccept = this->m_AutoAccept ? 1 : 0;
	pMsg.wantedClasses = this->m_WantedClasses;
	pMsg.objectiveType = this->m_Objective;
	gProtocol.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CPartyMatchingUI::RequestCancel()
{
	CG_PARTY_MATCHING_CANCEL_REQ pMsg;
	pMsg.header.set(0x99, 0x03, sizeof(pMsg));
	gProtocol.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CPartyMatchingUI::RequestJoin(const char* leaderName, const char* password)
{
	CG_PARTY_MATCHING_JOIN_REQ pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.header.set(0x99, 0x04, sizeof(pMsg));
	strncpy_s(pMsg.leaderName, sizeof(pMsg.leaderName), leaderName, _TRUNCATE);
	if (password) strncpy_s(pMsg.password, sizeof(pMsg.password), password, _TRUNCATE);
	gProtocol.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CPartyMatchingUI::RequestAccept(const char* appName, bool accept)
{
	CG_PARTY_MATCHING_ACCEPT_REQ pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.header.set(0x99, 0x06, sizeof(pMsg));
	strncpy_s(pMsg.applicantName, sizeof(pMsg.applicantName), appName, _TRUNCATE);
	pMsg.accept = accept ? 1 : 0;
	gProtocol.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CPartyMatchingUI::SendPartyDelMember(BYTE number)
{
	PMSG_PARTY_DEL_MEMBER_SEND pMsg;
	pMsg.header.set(0x43, sizeof(pMsg));
	pMsg.number = number;
	gProtocol.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CPartyMatchingUI::OnListRecv(BYTE* lpMsg)
{
	GC_PARTY_MATCHING_LIST_ANS* p = (GC_PARTY_MATCHING_LIST_ANS*)lpMsg;
	this->m_Page = p->currentPage;
	this->m_TotalPages = (p->totalPages < 1) ? 1 : p->totalPages;
	this->m_MyRegistered = (p->myRegistered != 0);
	this->m_Count = (p->count > 6) ? 6 : p->count;

	PARTY_MATCHING_ENTRY* entries = (PARTY_MATCHING_ENTRY*)(lpMsg + sizeof(GC_PARTY_MATCHING_LIST_ANS));
	for (int i = 0; i < this->m_Count; i++)
	{
		this->m_Entries[i] = entries[i];
	}
}

void CPartyMatchingUI::OnRegisterRecv(BYTE* lpMsg)
{
	GC_PARTY_MATCHING_REGISTER_ANS* p = (GC_PARTY_MATCHING_REGISTER_ANS*)lpMsg;
	if (p->result == 1)
	{
		this->m_MyRegistered = true;
		this->SetStatus("Party room registered! Players can now find & join you.", true);
		this->RequestList(1);
	}
	else if (p->result == 2) this->SetStatus("Only the Party Leader can register the party.", false);
	else if (p->result == 3) this->SetStatus("Your party is already full (5/5).", false);
	else this->SetStatus("Failed to register party room.", false);
}

void CPartyMatchingUI::OnCancelRecv(BYTE* lpMsg)
{
	GC_PARTY_MATCHING_CANCEL_ANS* p = (GC_PARTY_MATCHING_CANCEL_ANS*)lpMsg;
	if (p->result == 1)
	{
		this->m_MyRegistered = false;
		this->SetStatus("Party listing cancelled.", true);
		this->RequestList(1);
	}
}

void CPartyMatchingUI::OnJoinRecv(BYTE* lpMsg)
{
	GC_PARTY_MATCHING_JOIN_ANS* p = (GC_PARTY_MATCHING_JOIN_ANS*)lpMsg;
	switch (p->result)
	{
	case 1: this->SetStatus("Joined party successfully!", true); this->m_Tab = 0; break;
	case 2: this->SetStatus("Join request sent! Waiting for leader approval...", true); break;
	case 3: this->SetStatus("Incorrect room password!", false); break;
	case 4: this->SetStatus("Your level does not meet this room's requirement.", false); break;
	case 5: this->SetStatus("This party is already full.", false); break;
	case 6: this->SetStatus("You are already in a party.", false); break;
	case 7: this->SetStatus("Party room no longer exists.", false); this->RequestList(this->m_Page); break;
	case 8: this->SetStatus("This party is recruiting different classes.", false); break;
	default: this->SetStatus("Unable to join party.", false); break;
	}
}

void CPartyMatchingUI::RequestMoveToLeader()
{
	CG_PARTY_MATCHING_MOVE_LEADER_REQ p;
	p.header.set(0x99, 0x07, sizeof(p));
	gProtocol.DataSend((BYTE*)&p, sizeof(p));
}

void CPartyMatchingUI::RequestSetLeader(const char* targetName)
{
	CG_PARTY_MATCHING_SET_LEADER_REQ p;
	memset(&p, 0, sizeof(p));
	p.header.set(0x99, 0x08, sizeof(p));
	strncpy_s(p.targetName, sizeof(p.targetName), targetName, _TRUNCATE);
	gProtocol.DataSend((BYTE*)&p, sizeof(p));
}

const PARTY_EXT_MEMBER* CPartyMatchingUI::FindExtMember(const char* name) const
{
	if (!name || !name[0]) return NULL;
	for (int i = 0; i < this->m_ExtCount && i < 5; i++)
	{
		if (strcmp(this->m_ExtMembers[i].name, name) == 0)
		{
			return &this->m_ExtMembers[i];
		}
	}
	return NULL;
}

void CPartyMatchingUI::PopFrontPendingApp()
{
	if (this->m_PendingCount <= 0) return;
	for (int i = 1; i < this->m_PendingCount; i++)
	{
		this->m_PendingApps[i - 1] = this->m_PendingApps[i];
	}
	this->m_PendingCount--;
}

void CPartyMatchingUI::OnExtInfoRecv(BYTE* lpMsg)
{
	GC_PARTY_EXT_INFO_SEND* p = (GC_PARTY_EXT_INFO_SEND*)lpMsg;
	this->m_PartyExpPct = p->expPct;
	this->m_PartyIsDiverse = p->isDiverse;
	this->m_ExtCount = (p->count > 5) ? 5 : p->count;
	for (int i = 0; i < this->m_ExtCount; i++)
	{
		this->m_ExtMembers[i] = p->members[i];
		// Keep stock Party[] map/x/y live every 1-2s for Auto Follow & My Party UI
		for (int j = 0; j < PartyNumber && j < 5; j++)
		{
			if (strcmp(Party[j].Name, p->members[i].name) == 0)
			{
				Party[j].map = p->members[i].map;
				Party[j].x = p->members[i].x;
				Party[j].y = p->members[i].y;
				break;
			}
		}
	}
}

void CPartyMatchingUI::OnNotifyLeaderRecv(BYTE* lpMsg)
{
	GC_PARTY_MATCHING_NOTIFY_LEADER* p = (GC_PARTY_MATCHING_NOTIFY_LEADER*)lpMsg;
	DWORD now = GetTickCount();

	// Update existing entry if already queued
	for (int i = 0; i < this->m_PendingCount; i++)
	{
		if (_stricmp(this->m_PendingApps[i].name, p->applicantName) == 0)
		{
			this->m_PendingApps[i].level = p->applicantLevel;
			this->m_PendingApps[i].classCode = p->applicantClass;
			this->m_PendingApps[i].tick = now;
			PlayBuffer(25, 0, 0);
			return;
		}
	}

	if (this->m_PendingCount < 5)
	{
		PARTY_PENDING_APP& slot = this->m_PendingApps[this->m_PendingCount++];
		memset(&slot, 0, sizeof(slot));
		strcpy_s(slot.name, sizeof(slot.name), p->applicantName);
		slot.level = p->applicantLevel;
		slot.classCode = p->applicantClass;
		slot.tick = now;
		PlayBuffer(25, 0, 0);
	}
}

void CPartyMatchingUI::Render()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		this->m_Open = false;
		this->m_PendingCount = 0;
		return;
	}

	// Replace old stock Party window seamlessly!
	if (PartyOpened != 0)
	{
		PartyOpened = 0;
		this->Toggle();
	}

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTC = SetTextColor;
	const DWORD oldBG = SetBackgroundTextColor;

	// ---- Floating Incoming Join Request Queue Popup (top-center, auto-expires 25s) ----
	while (this->m_PendingCount > 0 && (GetTickCount() - this->m_PendingApps[0].tick) > 25000)
	{
		this->PopFrontPendingApp();
	}

	if (this->m_PendingCount > 0)
	{
		const PARTY_PENDING_APP& app = this->m_PendingApps[0];
		const float pw = 266.0f, ph = 58.0f;
		const float px = 320.0f - pw * 0.5f, py = 46.0f;
		PMRect(px, py, pw, ph, 0.42f, 0.78f, 0.90f, 0.98f);
		PMRect(px + 1.0f, py + 1.0f, pw - 2.0f, ph - 2.0f, 0.055f, 0.080f, 0.112f, 0.96f);

		char msg[96];
		if (this->m_PendingCount > 1)
			sprintf_s(msg, sizeof(msg), "[1/%d] %s (Lv.%u %s) wants to join", this->m_PendingCount, app.name, app.level, PMClassName(app.classCode));
		else
			sprintf_s(msg, sizeof(msg), "%s (Lv.%u %s) wants to join Party", app.name, app.level, PMClassName(app.classCode));
		PMTextCentered(px + pw * 0.5f, py + 9.0f, msg, PMColor(255, 232, 130, 255), true);

		PMDrawButton(px + 38.0f, py + 28.0f, 86.0f, 21.0f, "Accept", 2, true);
		PMDrawButton(px + pw - 124.0f, py + 28.0f, 86.0f, 21.0f, "Decline", 3, true);
	}

	// 2. Cold Steel Mini Party HUD (docked cleanly below Minimap, never overlapped)
	this->RenderMiniHud();

	if (!this->m_Open)
	{
		PMSetFont(oldFont ? oldFont : g_hFont);
		SetTextColor = oldTC;
		SetBackgroundTextColor = oldBG;
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		DisableAlphaBlend();
		return;
	}

	// ---- Main Centered Cold Steel HD Window ----
	PMRect(PM_PX - 2.0f, PM_PY - 2.0f, PM_PW + 4.0f, PM_PH + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
	PMRect(PM_PX, PM_PY, PM_PW, PM_PH, 0.23f, 0.27f, 0.32f, 0.98f);
	PMRect(PM_PX + 1.0f, PM_PY + 1.0f, PM_PW - 2.0f, PM_PH - 2.0f, 0.045f, 0.062f, 0.082f, 0.96f);

	// Title Bar
	PMRect(PM_PX + 1.0f, PM_PY + 1.0f, PM_PW - 2.0f, 24.0f, 0.075f, 0.105f, 0.140f, 0.98f);
	PMRect(PM_PX + 1.0f, PM_PY + 25.0f, PM_PW - 2.0f, 1.0f, 0.435f, 0.714f, 0.769f, 0.85f);
	PMText(PM_PX + 10.0f, PM_PY + 7.0f, "PARTY & PARTY MATCHING (P)", PMColor(150, 232, 248, 255), true);

	PMDrawButton(PM_PX + PM_PW - 24.0f, PM_PY + 4.0f, 18.0f, 17.0f, "X", 0, true);

	// 3 Main Tabs
	const char* tabNames[3] = { "My Party", "Find Party", "Recruit" };
	const float tabY = PM_PY + 31.0f;
	const float tabW = (PM_PW - 24.0f - 8.0f) / 3.0f;
	for (int i = 0; i < 3; i++)
	{
		float tx = PM_PX + 12.0f + i * (tabW + 4.0f);
		PMDrawButton(tx, tabY, tabW, 19.0f, tabNames[i], (this->m_Tab == i) ? 1 : 0, true);
	}

	const float bodyX = PM_PX + 12.0f;
	const float bodyW = PM_PW - 24.0f;

	if (this->m_Tab == 0)
	{
		// ---- TAB 0: MY PARTY (Replaces Old Stock P Window) ----
		float y = PM_PY + 58.0f;
		char* heroName = (char*)(Hero + 0x1C1);

		if (PartyNumber <= 0)
		{
			PMRect(bodyX, y, bodyW, 220.0f, 0.18f, 0.23f, 0.29f, 0.85f);
			PMRect(bodyX + 1.0f, y + 1.0f, bodyW - 2.0f, 218.0f, 0.055f, 0.075f, 0.100f, 0.94f);
			PMTextCentered(PM_PX + PM_PW * 0.5f, y + 54.0f, "You are not currently in a party.", PMColor(200, 218, 232, 255), true);
			PMTextCentered(PM_PX + PM_PW * 0.5f, y + 74.0f, "Diverse-class Party (DW + DK + ELF + MG) grants +25% to +45% EXP!", PMColor(125, 238, 175, 255), false);

			PMDrawButton(PM_PX + PM_PW * 0.5f - 146.0f, y + 110.0f, 136.0f, 28.0f, "Find Open Party", 1, true);
			PMDrawButton(PM_PX + PM_PW * 0.5f + 10.0f, y + 110.0f, 136.0f, 28.0f, "Recruit Party", 2, true);
		}
		else
		{
			bool amLeader = (heroName && strcmp(Party[0].Name, heroName) == 0);
			char hdr[112];
			if (this->m_PartyExpPct > 100 && this->m_PartyIsDiverse)
				sprintf_s(hdr, sizeof(hdr), "Party (%d/5)  |  Diverse Bonus: %u%% EXP", PartyNumber, this->m_PartyExpPct);
			else
				sprintf_s(hdr, sizeof(hdr), "Party (%d/5)  |  EXP Rate: %u%% (Mix classes for Bonus!)", PartyNumber, this->m_PartyExpPct);
			PMText(bodyX + 4.0f, y + 2.0f, hdr, this->m_PartyIsDiverse ? PMColor(115, 242, 168, 255) : PMColor(145, 228, 248, 255), true);

			if (!amLeader)
			{
				PMDrawButton(bodyX + bodyW - 102.0f, y - 1.0f, 102.0f, 17.0f, "Move to Leader", 1, true);
			}
			y += 20.0f;

			for (int i = 0; i < PartyNumber && i < 5; i++)
			{
				const PARTY_t& m = Party[i];
				bool isSelf = (heroName && strcmp(m.Name, heroName) == 0);
				bool isLeader = (i == 0);
				const PARTY_EXT_MEMBER* ext = this->FindExtMember(m.Name);
				bool outOfRange = (ext != NULL && ext->inExpRange == 0);

				const float rh = 42.0f;
				if (isLeader)
					PMRect(bodyX, y, bodyW, rh, 0.85f, 0.68f, 0.24f, 0.92f);
				else if (outOfRange)
					PMRect(bodyX, y, bodyW, rh, 0.78f, 0.48f, 0.14f, 0.90f);
				else
					PMRect(bodyX, y, bodyW, rh, 0.22f, 0.28f, 0.35f, 0.92f);
				PMRect(bodyX + 1.0f, y + 1.0f, bodyW - 2.0f, rh - 2.0f, 0.062f, 0.088f, 0.118f, 0.95f);

				// Name + Class + Level + Leader tag
				char nBuf[64];
				if (ext)
					sprintf_s(nBuf, sizeof(nBuf), "[%s] %s Lv.%u%s", PMClassName(ext->classCode), m.Name, ext->level, isLeader ? " [L]" : (isSelf ? " (You)" : ""));
				else
					sprintf_s(nBuf, sizeof(nBuf), "%s%s", m.Name, isLeader ? " [Leader]" : (isSelf ? " (You)" : ""));
				PMText(bodyX + 8.0f, y + 6.0f, nBuf, isLeader ? PMColor(255, 225, 110, 255) : PMColor(235, 245, 252, 255), true);

				// Map & Coords + Out of Range warning
				char locBuf[72];
				if (outOfRange)
					sprintf_s(locBuf, sizeof(locBuf), "%s (%d,%d) [FAR]", gMapManager.GetMapName(m.map), m.x, m.y);
				else
					sprintf_s(locBuf, sizeof(locBuf), "%s (%d, %d)", gMapManager.GetMapName(m.map), m.x, m.y);
				PMText(bodyX + 195.0f, y + 6.0f, locBuf, outOfRange ? PMColor(255, 178, 72, 255) : PMColor(168, 195, 215, 255), outOfRange);

				// HP Bar (leave space for Lead + Kick buttons when leader)
				float rightBtnsW = (amLeader && !isSelf) ? 120.0f : 76.0f;
				float barX = bodyX + 8.0f, barY = y + 23.0f, barW = bodyW - rightBtnsW - 14.0f, barH = 11.0f;
				float ratio = (m.MaxLife > 0) ? ((float)m.CurLife / (float)m.MaxLife) : 0.0f;
				if (ratio < 0.0f) ratio = 0.0f;
				if (ratio > 1.0f) ratio = 1.0f;

				PMRect(barX, barY, barW, barH, 0.20f, 0.24f, 0.28f, 0.95f);
				PMRect(barX + 1.0f, barY + 1.0f, barW - 2.0f, barH - 2.0f, 0.05f, 0.04f, 0.05f, 0.95f);
				if (ratio > 0.0f)
				{
					PMRect(barX + 1.0f, barY + 1.0f, (barW - 2.0f) * ratio, barH - 2.0f, 0.78f, 0.16f, 0.18f, 0.95f);
				}

				char hpBuf[32];
				sprintf_s(hpBuf, sizeof(hpBuf), "HP %u / %u", m.CurLife, m.MaxLife);
				PMTextCentered(barX + barW * 0.5f, barY + 1.5f, hpBuf, PMColor(248, 248, 248, 255), false);

				// Transfer Leader + Kick / Leave buttons
				if (amLeader && !isSelf)
				{
					PMDrawButton(bodyX + bodyW - 118.0f, y + 10.0f, 52.0f, 22.0f, "Lead", 1, true);
					PMDrawButton(bodyX + bodyW - 62.0f, y + 10.0f, 54.0f, 22.0f, "Kick", 3, true);
				}
				else if (isSelf || amLeader)
				{
					PMDrawButton(bodyX + bodyW - 68.0f, y + 10.0f, 60.0f, 22.0f, isSelf ? "Leave" : "Kick", 3, true);
				}

				y += rh + 5.0f;
			}
		}
	}
	else if (this->m_Tab == 1)
	{
		// ---- TAB 1: FIND PARTY ----
		const float fy = PM_PY + 56.0f;
		const char* fNames[5] = { "All", "Train", "Blood Castle", "Devil Square", "Boss" };
		const float fw = (bodyW - 16.0f) / 5.0f;
		for (int i = 0; i < 5; i++)
		{
			PMDrawButton(bodyX + i * (fw + 4.0f), fy, fw, 17.0f, fNames[i], (this->m_FilterObj == i) ? 1 : 0, true);
		}

		float y = fy + 23.0f;
		if (this->m_Count == 0)
		{
			PMTextCentered(PM_PX + PM_PW * 0.5f, y + 75.0f, "No open party rooms found. Create one in the Recruit tab!", PMColor(160, 180, 195, 255), false);
		}
		else
		{
			for (int i = 0; i < this->m_Count; i++)
			{
				const PARTY_MATCHING_ENTRY& e = this->m_Entries[i];
				const float rh = 34.0f;
				PMRect(bodyX, y, bodyW, rh, 0.22f, 0.28f, 0.35f, 0.92f);
				PMRect(bodyX + 1.0f, y + 1.0f, bodyW - 2.0f, rh - 2.0f, 0.062f, 0.088f, 0.118f, 0.95f);

				char tBuf[64];
				sprintf_s(tBuf, sizeof(tBuf), "[%s] %s%s", PMObjName(e.objectiveType), e.title, e.hasPassword ? " [LOCK]" : "");
				PMText(bodyX + 8.0f, y + 5.0f, tBuf, PMColor(240, 248, 255, 255), true);

				char subBuf[96];
				sprintf_s(subBuf, sizeof(subBuf), "Leader: %s (Lv.%u %s) | %s | Lv.%u-%u | %u/5",
					e.leaderName, e.leaderLevel, PMClassName(e.leaderClass), gMapManager.GetMapName(e.mapNumber), e.minLevel, e.maxLevel, e.memberCount);
				PMText(bodyX + 8.0f, y + 19.0f, subBuf, PMColor(160, 186, 204, 255), false);

				// EXP bonus preview
				if (e.expJoin > 0)
				{
					char expBuf[24];
					sprintf_s(expBuf, sizeof(expBuf), "EXP %u%%", e.expJoin);
					PMText(bodyX + bodyW - 122.0f, y + 12.0f, expBuf, PMColor(125, 242, 165, 255), true);
				}

				bool canJoin = (e.memberCount < 5 && PartyNumber <= 0);
				PMDrawButton(bodyX + bodyW - 62.0f, y + 7.0f, 54.0f, 20.0f, e.autoAccept ? "Join" : "Apply", 2, canJoin);
				y += rh + 4.0f;
			}
		}
	}
	else if (this->m_Tab == 2)
	{
		// ---- TAB 2: RECRUIT ----
		float y = PM_PY + 60.0f;

		// Title input
		PMText(bodyX, y + 4.0f, "Room Title:", PMColor(195, 215, 230, 255), true);
		PMRect(bodyX + 82.0f, y, bodyW - 82.0f, 20.0f, (this->m_FocusField == 1) ? 0.42f : 0.24f, (this->m_FocusField == 1) ? 0.80f : 0.30f, (this->m_FocusField == 1) ? 0.90f : 0.36f, 0.96f);
		PMRect(bodyX + 83.0f, y + 1.0f, bodyW - 84.0f, 18.0f, 0.035f, 0.050f, 0.070f, 0.96f);
		char titleDisp[40];
		sprintf_s(titleDisp, sizeof(titleDisp), "%s%s", this->m_Title, (this->m_FocusField == 1 && ((GetTickCount() / 450) % 2 == 0)) ? "|" : "");
		PMText(bodyX + 88.0f, y + 5.0f, titleDisp, PMColor(245, 248, 255, 255), false);
		y += 28.0f;

		// Objective
		PMText(bodyX, y + 4.0f, "Objective:", PMColor(195, 215, 230, 255), true);
		const char* oNames[4] = { "Train", "Blood Castle", "Devil Square", "Boss" };
		float ow = (bodyW - 82.0f - 12.0f) / 4.0f;
		for (int i = 0; i < 4; i++)
		{
			PMDrawButton(bodyX + 82.0f + i * (ow + 4.0f), y, ow, 19.0f, oNames[i], (this->m_Objective == (i + 1)) ? 1 : 0, true);
		}
		y += 28.0f;

		// Min / Max Level
		PMText(bodyX, y + 4.0f, "Level Range:", PMColor(195, 215, 230, 255), true);
		PMRect(bodyX + 82.0f, y, 64.0f, 20.0f, (this->m_FocusField == 2) ? 0.42f : 0.24f, (this->m_FocusField == 2) ? 0.80f : 0.30f, (this->m_FocusField == 2) ? 0.90f : 0.36f, 0.96f);
		PMRect(bodyX + 83.0f, y + 1.0f, 62.0f, 18.0f, 0.035f, 0.050f, 0.070f, 0.96f);
		PMText(bodyX + 88.0f, y + 5.0f, this->m_MinLvStr, PMColor(245, 248, 255, 255), false);

		PMText(bodyX + 154.0f, y + 5.0f, "to", PMColor(170, 190, 205, 255), false);

		PMRect(bodyX + 174.0f, y, 64.0f, 20.0f, (this->m_FocusField == 3) ? 0.42f : 0.24f, (this->m_FocusField == 3) ? 0.80f : 0.30f, (this->m_FocusField == 3) ? 0.90f : 0.36f, 0.96f);
		PMRect(bodyX + 175.0f, y + 1.0f, 62.0f, 18.0f, 0.035f, 0.050f, 0.070f, 0.96f);
		PMText(bodyX + 180.0f, y + 5.0f, this->m_MaxLvStr, PMColor(245, 248, 255, 255), false);

		// Optional Password
		PMText(bodyX + 252.0f, y + 4.0f, "Password:", PMColor(195, 215, 230, 255), true);
		PMRect(bodyX + 314.0f, y, bodyW - 314.0f, 20.0f, (this->m_FocusField == 4) ? 0.42f : 0.24f, (this->m_FocusField == 4) ? 0.80f : 0.30f, (this->m_FocusField == 4) ? 0.90f : 0.36f, 0.96f);
		PMRect(bodyX + 315.0f, y + 1.0f, bodyW - 316.0f, 18.0f, 0.035f, 0.050f, 0.070f, 0.96f);
		PMText(bodyX + 320.0f, y + 5.0f, this->m_Pass[0] ? this->m_Pass : "(Optional)", this->m_Pass[0] ? PMColor(245, 248, 255, 255) : PMColor(115, 130, 145, 255), false);
		y += 30.0f;

		// Wanted Classes checkboxes
		PMText(bodyX, y + 2.0f, "Wanted Classes:", PMColor(195, 215, 230, 255), true);
		const char* cLbls[4] = { "DW", "DK", "ELF", "MG" };
		for (int c = 0; c < 4; c++)
		{
			PMDrawCheckbox(bodyX + 100.0f + c * 62.0f, y + 2.0f, cLbls[c], (this->m_WantedClasses & (1 << c)) != 0);
		}
		y += 26.0f;

		// Auto-Accept checkbox
		PMDrawCheckbox(bodyX + 100.0f, y + 2.0f, "Auto-Accept applicants immediately", this->m_AutoAccept);
		y += 34.0f;

		// Register / Cancel buttons
		PMDrawButton(bodyX + 82.0f, y, 140.0f, 26.0f, this->m_MyRegistered ? "Update Listing" : "Register Party", 2, true);
		PMDrawButton(bodyX + 232.0f, y, 130.0f, 26.0f, "Cancel Listing", 3, this->m_MyRegistered);
	}

	// Status banner
	if (this->m_StatusText[0] != 0 && (GetTickCount() - this->m_StatusTick) < 5500)
	{
		DWORD sc = this->m_StatusGood ? PMColor(120, 242, 158, 255) : PMColor(255, 118, 118, 255);
		PMTextCentered(PM_PX + PM_PW * 0.5f, PM_PY + PM_PH - 42.0f, this->m_StatusText, sc, true);
	}

	// Footer Refresh + Pagination
	const float footY = PM_PY + PM_PH - 24.0f;
	PMDrawButton(bodyX, footY, 62.0f, 17.0f, "Refresh", 0, true);
	if (this->m_Tab == 1 && this->m_TotalPages > 1)
	{
		const float midX = PM_PX + PM_PW * 0.5f;
		PMDrawButton(midX - 84.0f, footY, 48.0f, 17.0f, "< Prev", 0, this->m_Page > 1);
		char pgBuf[24];
		sprintf_s(pgBuf, sizeof(pgBuf), "Page %d / %d", this->m_Page, this->m_TotalPages);
		PMTextCentered(midX, footY + 4.0f, pgBuf, PMColor(185, 205, 220, 255), false);
		PMDrawButton(midX + 36.0f, footY, 48.0f, 17.0f, "Next >", 0, this->m_Page < this->m_TotalPages);
	}
	else
	{
		PMText(bodyX + 74.0f, footY + 4.0f, "Hotkey: P (Party & Matching) | Diverse Party = +25% to +45% EXP", PMColor(135, 155, 170, 255), false);
	}

	// Password Modal
	if (this->m_ShowPassModal)
	{
		PMRect(PM_PX, PM_PY, PM_PW, PM_PH, 0.0f, 0.0f, 0.0f, 0.62f);
		const float mw = 260.0f, mh = 114.0f;
		const float mx = PM_PX + (PM_PW - mw) * 0.5f, my = PM_PY + (PM_PH - mh) * 0.5f;
		PMRect(mx, my, mw, mh, 0.42f, 0.78f, 0.90f, 0.98f);
		PMRect(mx + 1.0f, my + 1.0f, mw - 2.0f, mh - 2.0f, 0.055f, 0.080f, 0.112f, 0.98f);
		PMTextCentered(mx + mw * 0.5f, my + 10.0f, "ENTER ROOM PASSWORD", PMColor(150, 232, 248, 255), true);

		PMRect(mx + 22.0f, my + 36.0f, mw - 44.0f, 22.0f, 0.42f, 0.78f, 0.90f, 0.96f);
		PMRect(mx + 23.0f, my + 37.0f, mw - 46.0f, 20.0f, 0.035f, 0.050f, 0.070f, 0.96f);
		PMText(mx + 28.0f, my + 42.0f, this->m_JoinPass, PMColor(255, 255, 255, 255), false);

		PMDrawButton(mx + 26.0f, my + 74.0f, 96.0f, 24.0f, "Join", 2, true);
		PMDrawButton(mx + mw - 122.0f, my + 74.0f, 96.0f, 24.0f, "Cancel", 0, true);
	}

	PMSetFont(oldFont ? oldFont : g_hFont);
	SetTextColor = oldTC;
	SetBackgroundTextColor = oldBG;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

void CPartyMatchingUI::UpdateMouse()
{
	if (SceneFlag != MAIN_SCENE || !Hero) return;

	if (PartyOpened != 0)
	{
		PartyOpened = 0;
		this->Toggle();
	}

	// 1. Incoming Join Request Queue Popup (always clickable even if main window closed)
	if (this->m_PendingCount > 0)
	{
		const float pw = 266.0f, ph = 58.0f;
		const float px = 320.0f - pw * 0.5f, py = 46.0f;
		if (PMHit(px, py, pw, ph))
		{
			MouseOnWindow = true;
			if (MouseLButtonPush)
			{
				MouseLButtonPush = false; MouseLButton = false; MouseLButtonPop = false;
				PlayBuffer(25, 0, 0);
				if (PMHit(px + 38.0f, py + 28.0f, 86.0f, 21.0f))
				{
					char nameCopy[11];
					strcpy_s(nameCopy, sizeof(nameCopy), this->m_PendingApps[0].name);
					this->PopFrontPendingApp();
					this->RequestAccept(nameCopy, true);
				}
				else if (PMHit(px + pw - 124.0f, py + 28.0f, 86.0f, 21.0f))
				{
					char nameCopy[11];
					strcpy_s(nameCopy, sizeof(nameCopy), this->m_PendingApps[0].name);
					this->PopFrontPendingApp();
					this->RequestAccept(nameCopy, false);
				}
			}
			return;
		}
	}

	// 2. Cold Steel Mini Party HUD clicks
	if (this->UpdateMiniHudMouse())
	{
		return;
	}

	if (!this->m_Open || !PMHit(PM_PX, PM_PY, PM_PW, PM_PH)) return;

	MouseOnWindow = true;
	if (!MouseLButtonPush) return;

	MouseLButtonPush = false;
	MouseLButton = false;
	MouseLButtonPop = false;
	PlayBuffer(25, 0, 0);

	// Password Modal clicks
	if (this->m_ShowPassModal)
	{
		const float mw = 260.0f, mh = 114.0f;
		const float mx = PM_PX + (PM_PW - mw) * 0.5f, my = PM_PY + (PM_PH - mh) * 0.5f;
		if (PMHit(mx + 26.0f, my + 74.0f, 96.0f, 24.0f))
		{
			this->m_ShowPassModal = false;
			this->RequestJoin(this->m_JoinLeader, this->m_JoinPass);
			return;
		}
		if (PMHit(mx + mw - 122.0f, my + 74.0f, 96.0f, 24.0f))
		{
			this->m_ShowPassModal = false;
			return;
		}
		return;
	}

	// Close [X]
	if (PMHit(PM_PX + PM_PW - 24.0f, PM_PY + 4.0f, 18.0f, 17.0f))
	{
		this->Close();
		return;
	}

	// 3 Main Tabs
	const float tabY = PM_PY + 31.0f;
	const float tabW = (PM_PW - 24.0f - 8.0f) / 3.0f;
	for (int i = 0; i < 3; i++)
	{
		if (PMHit(PM_PX + 12.0f + i * (tabW + 4.0f), tabY, tabW, 19.0f))
		{
			this->m_Tab = i;
			this->m_FocusField = 0;
			if (i == 1) this->RequestList(1);
			return;
		}
	}

	const float bodyX = PM_PX + 12.0f;
	const float bodyW = PM_PW - 24.0f;

	// Footer Refresh + Pagination
	const float footY = PM_PY + PM_PH - 24.0f;
	if (PMHit(bodyX, footY, 62.0f, 17.0f))
	{
		this->RequestList(this->m_Page);
		return;
	}
	if (this->m_Tab == 1 && this->m_TotalPages > 1)
	{
		const float midX = PM_PX + PM_PW * 0.5f;
		if (this->m_Page > 1 && PMHit(midX - 84.0f, footY, 48.0f, 17.0f)) { this->RequestList(this->m_Page - 1); return; }
		if (this->m_Page < this->m_TotalPages && PMHit(midX + 36.0f, footY, 48.0f, 17.0f)) { this->RequestList(this->m_Page + 1); return; }
	}

	if (this->m_Tab == 0)
	{
		float y = PM_PY + 58.0f;
		if (PartyNumber <= 0)
		{
			if (PMHit(PM_PX + PM_PW * 0.5f - 146.0f, y + 110.0f, 136.0f, 28.0f)) { this->m_Tab = 1; this->RequestList(1); return; }
			if (PMHit(PM_PX + PM_PW * 0.5f + 10.0f, y + 110.0f, 136.0f, 28.0f)) { this->m_Tab = 2; return; }
		}
		else
		{
			char* heroName = (char*)(Hero + 0x1C1);
			bool amLeader = (heroName && strcmp(Party[0].Name, heroName) == 0);
			if (!amLeader && PMHit(bodyX + bodyW - 102.0f, y - 1.0f, 102.0f, 17.0f))
			{
				this->RequestMoveToLeader();
				return;
			}
			y += 20.0f;
			for (int i = 0; i < PartyNumber && i < 5; i++)
			{
				bool isSelf = (heroName && strcmp(Party[i].Name, heroName) == 0);
				if (amLeader && !isSelf)
				{
					if (PMHit(bodyX + bodyW - 118.0f, y + 10.0f, 52.0f, 22.0f))
					{
						this->RequestSetLeader(Party[i].Name);
						return;
					}
					if (PMHit(bodyX + bodyW - 62.0f, y + 10.0f, 54.0f, 22.0f))
					{
						this->SendPartyDelMember(Party[i].number);
						return;
					}
				}
				else if ((isSelf || amLeader) && PMHit(bodyX + bodyW - 68.0f, y + 10.0f, 60.0f, 22.0f))
				{
					this->SendPartyDelMember(Party[i].number);
					return;
				}
				y += 47.0f;
			}
		}
	}
	else if (this->m_Tab == 1)
	{
		const float fy = PM_PY + 56.0f;
		const float fw = (bodyW - 16.0f) / 5.0f;
		for (int i = 0; i < 5; i++)
		{
			if (PMHit(bodyX + i * (fw + 4.0f), fy, fw, 17.0f))
			{
				this->m_FilterObj = i;
				this->RequestList(1);
				return;
			}
		}

		float y = fy + 23.0f;
		for (int i = 0; i < this->m_Count; i++)
		{
			if (PMHit(bodyX + bodyW - 62.0f, y + 7.0f, 54.0f, 20.0f))
			{
				if (this->m_Entries[i].memberCount < 5 && PartyNumber <= 0)
				{
					if (this->m_Entries[i].hasPassword)
					{
						this->m_ShowPassModal = true;
						strcpy_s(this->m_JoinLeader, sizeof(this->m_JoinLeader), this->m_Entries[i].leaderName);
						this->m_JoinPass[0] = 0;
					}
					else
					{
						this->RequestJoin(this->m_Entries[i].leaderName, "");
					}
				}
				return;
			}
			y += 38.0f;
		}
	}
	else if (this->m_Tab == 2)
	{
		this->m_FocusField = 0;
		float y = PM_PY + 60.0f;

		if (PMHit(bodyX + 82.0f, y, bodyW - 82.0f, 20.0f)) { this->m_FocusField = 1; return; }
		y += 28.0f;

		float ow = (bodyW - 82.0f - 12.0f) / 4.0f;
		for (int i = 0; i < 4; i++)
		{
			if (PMHit(bodyX + 82.0f + i * (ow + 4.0f), y, ow, 19.0f)) { this->m_Objective = (BYTE)(i + 1); return; }
		}
		y += 28.0f;

		if (PMHit(bodyX + 82.0f, y, 64.0f, 20.0f)) { this->m_FocusField = 2; return; }
		if (PMHit(bodyX + 174.0f, y, 64.0f, 20.0f)) { this->m_FocusField = 3; return; }
		if (PMHit(bodyX + 314.0f, y, bodyW - 314.0f, 20.0f)) { this->m_FocusField = 4; return; }
		y += 30.0f;

		for (int c = 0; c < 4; c++)
		{
			if (PMHit(bodyX + 100.0f + c * 62.0f, y, 54.0f, 16.0f))
			{
				this->m_WantedClasses ^= (1 << c);
				if (this->m_WantedClasses == 0) this->m_WantedClasses = (1 << c);
				return;
			}
		}
		y += 26.0f;

		if (PMHit(bodyX + 100.0f, y, 220.0f, 16.0f)) { this->m_AutoAccept = !this->m_AutoAccept; return; }
		y += 34.0f;

		if (PMHit(bodyX + 82.0f, y, 140.0f, 26.0f)) { this->RequestRegister(); return; }
		if (this->m_MyRegistered && PMHit(bodyX + 232.0f, y, 130.0f, 26.0f)) { this->RequestCancel(); return; }
	}
}

void CPartyMatchingUI::RenderMiniHud()
{
	if (SceneFlag != 5 || !Hero || PartyNumber <= 0 || CheckRightInterfaces())
	{
		return;
	}

	const float hx = 528.0f; // Aligned flush with Cold Steel Minimap (640 - 104 - 8)
	const float hw = 104.0f;
	float hy = gMiniMap.GetBottomY();

	char* heroName = (char*)(Hero + 0x1C1);
	bool amLeader = (heroName && strcmp(Party[0].Name, heroName) == 0);

	// Header strip (click to open full Party & Party Matching window)
	const float hdrH = 14.0f;
	bool hdrHov = PMHit(hx, hy, hw, hdrH);
	PMRect(hx, hy, hw, hdrH, hdrHov ? 0.435f : 0.24f, hdrHov ? 0.714f : 0.30f, hdrHov ? 0.769f : 0.37f, 0.92f);
	PMRect(hx + 1.0f, hy + 1.0f, hw - 2.0f, hdrH - 2.0f, 0.06f, 0.09f, 0.13f, 0.90f);

	char hdr[32];
	if (this->m_PartyExpPct > 100)
		sprintf_s(hdr, sizeof(hdr), "PT %d/5 %u%%", PartyNumber, this->m_PartyExpPct);
	else
		sprintf_s(hdr, sizeof(hdr), "PARTY (%d/5)", PartyNumber);
	PMText(hx + 4.0f, hy + 2.0f, hdr, this->m_PartyIsDiverse ? PMColor(115, 242, 168, 255) : PMColor(145, 226, 245, 255), true);
	PMText(hx + hw - 16.0f, hy + 2.0f, "[P]", PMColor(195, 215, 230, 255), false);

	hy += hdrH + 2.0f;

	// Quick Action Strip for non-leaders: [Go] (Warp to Leader) + [Follow] (Auto Helper Follow Leader)
	if (!amLeader)
	{
		const float qh = 13.0f;
		const float qw = (hw - 2.0f) * 0.5f;
		bool followOn = gAutoHelper.GetConfig().PartyFollowLeader;

		bool goHov = PMHit(hx, hy, qw, qh);
		PMRect(hx, hy, qw, qh, goHov ? 0.44f : 0.24f, goHov ? 0.78f : 0.42f, goHov ? 0.90f : 0.52f, 0.90f);
		PMRect(hx + 1.0f, hy + 1.0f, qw - 2.0f, qh - 2.0f, 0.07f, 0.14f, 0.20f, 0.90f);
		PMTextCentered(hx + qw * 0.5f, hy + 1.5f, "Go Lead", PMColor(170, 236, 255, 255), false);

		float fx = hx + qw + 2.0f;
		bool flwHov = PMHit(fx, hy, qw, qh);
		if (followOn)
		{
			PMRect(fx, hy, qw, qh, 0.24f, 0.84f, 0.46f, 0.95f);
			PMRect(fx + 1.0f, hy + 1.0f, qw - 2.0f, qh - 2.0f, 0.08f, 0.25f, 0.14f, 0.92f);
			PMTextCentered(fx + qw * 0.5f, hy + 1.5f, "Follow ON", PMColor(145, 255, 185, 255), true);
		}
		else
		{
			PMRect(fx, hy, qw, qh, flwHov ? 0.42f : 0.24f, flwHov ? 0.52f : 0.30f, flwHov ? 0.62f : 0.36f, 0.90f);
			PMRect(fx + 1.0f, hy + 1.0f, qw - 2.0f, qh - 2.0f, 0.07f, 0.09f, 0.12f, 0.88f);
			PMTextCentered(fx + qw * 0.5f, hy + 1.5f, "Follow", PMColor(185, 200, 212, 255), false);
		}

		hy += qh + 2.0f;
	}

	for (int i = 0; i < PartyNumber && i < 5; i++)
	{
		const float rh = 22.0f;
		bool isLeader = (i == 0);
		bool isSelf = (heroName && strcmp(Party[i].Name, heroName) == 0);
		bool canAct = (isSelf || amLeader);
		bool rowHov = PMHit(hx, hy, hw, rh);
		const PARTY_EXT_MEMBER* ext = this->FindExtMember(Party[i].Name);
		bool outOfRange = (ext != NULL && ext->inExpRange == 0);

		// Card frame + background (gold=Leader, amber=Out of EXP range, steel=normal)
		if (isLeader)
		{
			PMRect(hx, hy, hw, rh, 0.78f, 0.62f, 0.22f, 0.90f);
		}
		else if (outOfRange)
		{
			PMRect(hx, hy, hw, rh, 0.80f, 0.48f, 0.14f, 0.88f);
		}
		else
		{
			PMRect(hx, hy, hw, rh, 0.23f, 0.29f, 0.36f, 0.88f);
		}
		PMRect(hx + 1.0f, hy + 1.0f, hw - 2.0f, rh - 2.0f, 0.045f, 0.065f, 0.090f, outOfRange ? 0.72f : 0.86f);

		// Name (gold for Leader, muted amber if out of range, crisp white for members)
		DWORD nameCol = isLeader ? PMColor(255, 224, 118, 255) : (outOfRange ? PMColor(240, 185, 110, 255) : PMColor(232, 242, 250, 255));
		PMText(hx + 4.0f, hy + 2.0f, Party[i].Name, nameCol, isLeader);

		// HP ratio (stepHP is 0..10 in 0.97k)
		int hpPct = (int)Party[i].stepHP * 10;
		if (hpPct < 0) hpPct = 0;
		if (hpPct > 100) hpPct = 100;

		if (canAct && rowHov)
		{
			bool xHov = PMHit(hx + hw - 14.0f, hy + 1.0f, 13.0f, 11.0f);
			PMRect(hx + hw - 13.0f, hy + 2.0f, 11.0f, 10.0f, xHov ? 0.75f : 0.45f, 0.14f, 0.14f, 0.92f);
			PMText(hx + hw - 10.0f, hy + 1.0f, "x", PMColor(255, 225, 225, 255), true);
		}
		else if (outOfRange)
		{
			PMText(hx + hw - 22.0f, hy + 2.0f, "FAR", PMColor(255, 168, 65, 255), true);
		}
		else
		{
			char pctStr[12];
			sprintf_s(pctStr, sizeof(pctStr), "%d%%", hpPct);
			PMText(hx + hw - 22.0f, hy + 2.0f, pctStr, PMColor(180, 198, 212, 255), false);
		}

		// HP Bar
		const float bx = hx + 4.0f;
		const float by = hy + 14.0f;
		const float bw = hw - 8.0f;
		const float bh = 5.0f;
		PMRect(bx, by, bw, bh, 0.12f, 0.15f, 0.19f, 0.95f);
		if (hpPct > 0)
		{
			float fillW = bw * (hpPct / 100.0f);
			if (hpPct > 50)
				PMRect(bx, by, fillW, bh, 0.18f, 0.74f, 0.36f, 0.96f);
			else if (hpPct > 25)
				PMRect(bx, by, fillW, bh, 0.88f, 0.56f, 0.14f, 0.96f);
			else
				PMRect(bx, by, fillW, bh, 0.86f, 0.20f, 0.20f, 0.96f);
		}

		hy += rh + 2.0f;
	}
}

bool CPartyMatchingUI::UpdateMiniHudMouse()
{
	if (SceneFlag != 5 || !Hero || PartyNumber <= 0 || CheckRightInterfaces())
	{
		return false;
	}

	char* heroName = (char*)(Hero + 0x1C1);
	bool amLeader = (heroName && strcmp(Party[0].Name, heroName) == 0);

	const float hx = 528.0f;
	const float hw = 104.0f;
	float hy = gMiniMap.GetBottomY();
	const float hdrH = 14.0f;
	const float quickH = amLeader ? 0.0f : 15.0f;
	float totalH = hdrH + 2.0f + quickH + (float)(PartyNumber < 5 ? PartyNumber : 5) * 24.0f;

	if (!PMHit(hx, hy, hw, totalH))
	{
		return false;
	}

	MouseOnWindow = true;
	if (!MouseLButtonPush)
	{
		return true;
	}

	MouseLButtonPush = false;
	MouseLButton = false;
	MouseLButtonPop = false;
	PlayBuffer(25, 0, 0);

	// Header click -> toggle full Party & Party Matching window
	if (PMHit(hx, hy, hw, hdrH))
	{
		this->Toggle();
		return true;
	}

	hy += hdrH + 2.0f;

	if (!amLeader)
	{
		const float qh = 13.0f;
		const float qw = (hw - 2.0f) * 0.5f;
		if (PMHit(hx, hy, qw, qh))
		{
			this->RequestMoveToLeader();
			return true;
		}
		if (PMHit(hx + qw + 2.0f, hy, qw, qh))
		{
			AutoHelperConfig& cfg = gAutoHelper.GetConfig();
			cfg.PartyFollowLeader = !cfg.PartyFollowLeader;
			if (cfg.PartyFollowLeader && !gAutoHelper.IsActive())
			{
				gAutoHelper.Start();
			}
			CreateNotice(cfg.PartyFollowLeader ? "[Party] Auto Follow Leader: ON" : "[Party] Auto Follow Leader: OFF", 1);
			return true;
		}
		hy += qh + 2.0f;
	}

	for (int i = 0; i < PartyNumber && i < 5; i++)
	{
		const float rh = 22.0f;
		bool isSelf = (heroName && strcmp(Party[i].Name, heroName) == 0);
		if ((isSelf || amLeader) && PMHit(hx + hw - 14.0f, hy + 1.0f, 13.0f, 11.0f))
		{
			this->SendPartyDelMember(Party[i].number);
			return true;
		}
		hy += rh + 2.0f;
	}

	return true;
}
