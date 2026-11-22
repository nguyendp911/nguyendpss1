#include "stdafx.h"
#include "DuelManager.h"
#include "CommandWindow.h"
#include "Font.h"
#include "Item.h"
#include "ItemManager.h"
#include "Protocol.h"
#include "Sound.h"
#include <stdio.h>
#include <string.h>
#include <vector>

CDuelManager gDuelManager;

static bool DMHit(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static DWORD DMColor(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static void DMSetFont(HFONT f)
{
	if (f) SelectObject(m_hFontDC, f);
}

static float DMMeasureW(const char* text)
{
	if (!text || !text[0]) return 0.0f;
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (float)sz.cx / (g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f);
}

static void DMRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

static void DMText(float x, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	DMSetFont(bold ? g_hFontBold : g_hFont);
	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	SetBackgroundTextColor = 0;
	SetTextColor = DMColor(0, 0, 0, 220);
	RenderText((int)x + 1, (int)y + 1, (char*)text, 0, RT3_SORT_LEFT, NULL);
	SetTextColor = color;
	RenderText((int)x, (int)y, (char*)text, 0, RT3_SORT_LEFT, NULL);
}

static void DMTextCentered(float cx, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	DMSetFont(bold ? g_hFontBold : g_hFont);
	float w = DMMeasureW(text);
	DMText(cx - w * 0.5f, y, text, color, bold);
}

static void DMDrawButton(float x, float y, float w, float h, const char* text, int style, bool enabled = true)
{
	bool hov = enabled && DMHit(x, y, w, h);
	float br = 0.24f, bg = 0.29f, bb = 0.35f;
	float fr = 0.085f, fg = 0.115f, fb = 0.155f;
	DWORD tc = DMColor(220, 234, 244, 255);

	if (!enabled)
	{
		br = 0.16f; bg = 0.19f; bb = 0.22f;
		fr = 0.06f; fg = 0.075f; fb = 0.095f;
		tc = DMColor(105, 118, 130, 255);
	}
	else if (style == 1) // Cyan
	{
		br = 0.42f; bg = 0.80f; bb = 0.90f;
		fr = 0.12f; fg = 0.24f; fb = 0.32f;
		tc = DMColor(235, 250, 255, 255);
	}
	else if (style == 2) // Green confirm
	{
		br = 0.24f; bg = 0.82f; bb = 0.48f;
		fr = 0.08f; fg = 0.25f; fb = 0.15f;
		tc = DMColor(195, 255, 215, 255);
	}
	else if (style == 3) // Red decline/cancel
	{
		br = 0.84f; bg = 0.28f; bb = 0.28f;
		fr = 0.26f; fg = 0.08f; fb = 0.09f;
		tc = DMColor(255, 210, 210, 255);
	}

	if (hov) { br *= 1.15f; bg *= 1.15f; bb *= 1.15f; fr *= 1.25f; fg *= 1.25f; fb *= 1.25f; }
	DMRect(x, y, w, h, br, bg, bb, 0.95f);
	DMRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fr, fg, fb, 0.95f);
	DMTextCentered(x + w * 0.5f, y + (h - 10.0f) * 0.5f, text, tc, style != 0);
}

struct DM_INV_ENTRY
{
	BYTE slot; // 0..63 main inventory index
	char label[56];
};

static void DMCollectInventoryItems(std::vector<DM_INV_ENTRY>& out)
{
	out.clear();

	for (int i = 0; i < INVENTORY_ITEM_SIZE && i < 64; i++)
	{
		ITEM* pItem = GetInventoryItem(i);
		if (!pItem || pItem->Type < 0 || pItem->Type >= MAX_ITEM)
		{
			continue;
		}

		ITEM_ATTRIBUTE* attr = (ITEM_ATTRIBUTE*)(ItemAttribute + pItem->Type * sizeof(ITEM_ATTRIBUTE));
		int lv = (pItem->Level >> 3) & 0x0F;
		bool isExc = ((pItem->Option1 & 0x3F) != 0);

		DM_INV_ENTRY e;
		e.slot = (BYTE)i;
		if (lv > 0)
			sprintf_s(e.label, sizeof(e.label), "%s%s +%d", isExc ? "Exc " : "", attr->Name, lv);
		else
			sprintf_s(e.label, sizeof(e.label), "%s%s", isExc ? "Exc " : "", attr->Name);
		out.push_back(e);
	}
}

static bool DMIsSlotSelected(const BYTE* slots, BYTE count, BYTE slot)
{
	for (BYTE i = 0; i < count; i++)
	{
		if (slots[i] == slot) return true;
	}
	return false;
}

static void DMToggleSlot(BYTE* slots, BYTE& count, BYTE slot)
{
	for (BYTE i = 0; i < count; i++)
	{
		if (slots[i] == slot)
		{
			for (BYTE j = i + 1; j < count; j++) slots[j - 1] = slots[j];
			slots[--count] = 0xFF;
			return;
		}
	}
	if (count < 4)
	{
		slots[count++] = slot;
	}
}

static void DMFormatRules(BYTE rules, char* out, size_t outSize)
{
	out[0] = 0;
	if (rules == 0)
	{
		strcpy_s(out, outSize, "Free PvP (All Allowed)");
		return;
	}
	if (rules & DUEL_RULE_NO_HP) strcat_s(out, outSize, "[No HP] ");
	if (rules & DUEL_RULE_NO_BUFF) strcat_s(out, outSize, "[No Buff] ");
	if (rules & DUEL_RULE_NO_MANA) strcat_s(out, outSize, "[No Mana]");
}

CDuelManager::CDuelManager()
{
	this->Reset();
}

CDuelManager::~CDuelManager()
{
}

void CDuelManager::Reset()
{
	this->m_SetupOpen = false;
	this->m_TargetIndex = 0;
	memset(this->m_TargetName, 0, sizeof(this->m_TargetName));
	this->m_SelectedRules = DUEL_RULE_NO_BUFF;
	this->m_BetCount = 0;
	memset(this->m_BetSlots, 0xFF, sizeof(this->m_BetSlots));
	this->m_InvScroll = 0;

	this->m_RequestReceived = false;
	this->m_ChallengerIndex = 0;
	memset(this->m_ChallengerName, 0, sizeof(this->m_ChallengerName));
	this->m_RequestTick = 0;
	this->m_RequestedRules = 0;
	this->m_RequestedBetCount = 0;
	memset(this->m_RequestedBetItems, 0, sizeof(this->m_RequestedBetItems));
	this->m_TargetBetCount = 0;
	memset(this->m_TargetBetSlots, 0xFF, sizeof(this->m_TargetBetSlots));
	this->m_ReqInvScroll = 0;

	this->m_InDuel = false;
	this->m_OpponentIndex = 0;
	memset(this->m_OpponentName, 0, sizeof(this->m_OpponentName));
	this->m_ActiveRules = 0;
	this->m_MyScore = 0;
	this->m_OpponentScore = 0;
}

void CDuelManager::OpenSetupWindow(WORD targetIndex, const char* targetName)
{
	this->m_SetupOpen = true;
	this->m_TargetIndex = targetIndex;
	memset(this->m_TargetName, 0, sizeof(this->m_TargetName));
	if (targetName) strncpy_s(this->m_TargetName, sizeof(this->m_TargetName), targetName, 10);
	this->m_SelectedRules = DUEL_RULE_NO_BUFF;
	this->m_BetCount = 0;
	memset(this->m_BetSlots, 0xFF, sizeof(this->m_BetSlots));
	this->m_InvScroll = 0;

	gCommandWindow.Show(false);
	PlayBuffer(25, 0, 0);
}

void CDuelManager::CloseSetupWindow()
{
	this->m_SetupOpen = false;
	this->m_TargetIndex = 0;
	this->m_BetCount = 0;
	memset(this->m_BetSlots, 0xFF, sizeof(this->m_BetSlots));
}

void CDuelManager::SendSetupRequest()
{
	if (!Hero || this->m_TargetIndex == 0)
	{
		this->CloseSetupWindow();
		return;
	}

	PMSG_DUEL_REQUEST_SEND pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.header.setE(0xAA, 0x01, sizeof(pMsg));
	pMsg.index[0] = SET_NUMBERHB(this->m_TargetIndex);
	pMsg.index[1] = SET_NUMBERLB(this->m_TargetIndex);
	pMsg.rules = this->m_SelectedRules;
	pMsg.betItemCount = this->m_BetCount;
	memset(pMsg.betSlots, 0xFF, sizeof(pMsg.betSlots));
	for (BYTE i = 0; i < this->m_BetCount; i++)
	{
		pMsg.betSlots[i] = 12 + this->m_BetSlots[i]; // INVENTORY_WEAR_SIZE (12) + main slot
	}

	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
	this->CloseSetupWindow();
	PlayBuffer(25, 0, 0);
}

void CDuelManager::SendAnswer(bool accept)
{
	if (!this->m_RequestReceived) return;

	PMSG_DUEL_ANSWER_SEND pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.header.setE(0xAA, 0x02, sizeof(pMsg));
	pMsg.result = accept ? 1 : 0;
	pMsg.index[0] = SET_NUMBERHB(this->m_ChallengerIndex);
	pMsg.index[1] = SET_NUMBERLB(this->m_ChallengerIndex);
	pMsg.betItemCount = accept ? this->m_TargetBetCount : 0;
	memset(pMsg.betSlots, 0xFF, sizeof(pMsg.betSlots));
	if (accept)
	{
		for (BYTE i = 0; i < this->m_TargetBetCount; i++)
		{
			pMsg.betSlots[i] = 12 + this->m_TargetBetSlots[i];
		}
	}

	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
	this->m_RequestReceived = false;
	PlayBuffer(25, 0, 0);
}

void CDuelManager::OnDuelOpenSetupRecv(BYTE* lpMsg)
{
	PMSG_DUEL_OPEN_SETUP_SEND* p = (PMSG_DUEL_OPEN_SETUP_SEND*)lpMsg;
	WORD idx = MAKE_NUMBERW(p->index[0], p->index[1]);
	this->OpenSetupWindow(idx, p->name);
}

void CDuelManager::OnDuelRequestRecv(BYTE* lpMsg)
{
	PMSG_DUEL_REQUEST_RECV* p = (PMSG_DUEL_REQUEST_RECV*)lpMsg;
	this->m_RequestReceived = true;
	this->m_ChallengerIndex = MAKE_NUMBERW(p->index[0], p->index[1]);
	memset(this->m_ChallengerName, 0, sizeof(this->m_ChallengerName));
	strncpy_s(this->m_ChallengerName, sizeof(this->m_ChallengerName), p->name, 10);
	this->m_RequestTick = GetTickCount();
	this->m_RequestedRules = p->rules;
	this->m_RequestedBetCount = (p->betItemCount > 4) ? 4 : p->betItemCount;
	memcpy(this->m_RequestedBetItems, p->betItems, sizeof(this->m_RequestedBetItems));
	this->m_TargetBetCount = 0;
	memset(this->m_TargetBetSlots, 0xFF, sizeof(this->m_TargetBetSlots));
	this->m_ReqInvScroll = 0;
	PlayBuffer(25, 0, 0);
}

void CDuelManager::OnDuelStartRecv(BYTE* lpMsg)
{
	PMSG_DUEL_START_RECV* p = (PMSG_DUEL_START_RECV*)lpMsg;
	this->m_SetupOpen = false;
	this->m_RequestReceived = false;
	this->m_InDuel = true;
	this->m_OpponentIndex = MAKE_NUMBERW(p->index[0], p->index[1]);
	memset(this->m_OpponentName, 0, sizeof(this->m_OpponentName));
	strncpy_s(this->m_OpponentName, sizeof(this->m_OpponentName), p->name, 10);
	this->m_ActiveRules = p->rules;
	this->m_MyScore = 0;
	this->m_OpponentScore = 0;
	PlayBuffer(25, 0, 0);
}

void CDuelManager::OnDuelEndRecv(BYTE* lpMsg)
{
	PMSG_DUEL_END_RECV* p = (PMSG_DUEL_END_RECV*)lpMsg;
	this->m_InDuel = false;
	this->m_OpponentIndex = 0;
	this->m_MyScore = 0;
	this->m_OpponentScore = 0;
	if (p->result == 1) CreateNotice("[DUEL] VICTORY! You won the PvP Duel!", 1);
	else if (p->result == 2) CreateNotice("[DUEL] DEFEAT! Better luck next time.", 1);
	PlayBuffer(25, 0, 0);
}

void CDuelManager::OnDuelScoreRecv(BYTE* lpMsg)
{
	PMSG_DUEL_SCORE_RECV* p = (PMSG_DUEL_SCORE_RECV*)lpMsg;
	this->m_MyScore = p->myScore;
	this->m_OpponentScore = p->targetScore;
}

void CDuelManager::Render()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		if (this->m_InDuel || this->m_SetupOpen || this->m_RequestReceived) this->Reset();
		return;
	}

	if (this->m_RequestReceived && (GetTickCount() - this->m_RequestTick) > 20000)
	{
		this->m_RequestReceived = false;
	}

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTC = SetTextColor;
	const DWORD oldBG = SetBackgroundTextColor;

	if (this->m_InDuel) this->RenderScoreboard();
	if (this->m_SetupOpen) this->RenderSetupModal();
	if (this->m_RequestReceived) this->RenderRequestModal();

	DMSetFont(oldFont ? oldFont : g_hFont);
	SetTextColor = oldTC;
	SetBackgroundTextColor = oldBG;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

void CDuelManager::RenderScoreboard()
{
	const float sw = 248.0f, sh = 42.0f;
	const float sx = 320.0f - sw * 0.5f, sy = 8.0f;

	DMRect(sx - 1.0f, sy - 1.0f, sw + 2.0f, sh + 2.0f, 0.0f, 0.0f, 0.0f, 0.50f);
	DMRect(sx, sy, sw, sh, 0.435f, 0.714f, 0.769f, 0.95f);
	DMRect(sx + 1.0f, sy + 1.0f, sw - 2.0f, sh - 2.0f, 0.045f, 0.065f, 0.092f, 0.92f);

	char rulesStr[64];
	DMFormatRules(this->m_ActiveRules, rulesStr, sizeof(rulesStr));
	char topLine[96];
	sprintf_s(topLine, sizeof(topLine), "DUEL PVP (FIRST TO 5)  -  %s", rulesStr);
	DMTextCentered(sx + sw * 0.5f, sy + 4.0f, topLine, DMColor(145, 226, 245, 255), false);

	char* heroName = (char*)(Hero + 0x1C1);
	char scoreLine[96];
	sprintf_s(scoreLine, sizeof(scoreLine), "%s   [ %d ]  VS  [ %d ]   %s",
		heroName ? heroName : "You", this->m_MyScore, this->m_OpponentScore, this->m_OpponentName);
	DMTextCentered(sx + sw * 0.5f, sy + 20.0f, scoreLine, DMColor(255, 224, 112, 255), true);
}

void CDuelManager::RenderSetupModal()
{
	const float mw = 356.0f, mh = 316.0f;
	const float mx = (640.0f - mw) * 0.5f, my = 56.0f;

	DMRect(mx - 2.0f, my - 2.0f, mw + 4.0f, mh + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
	DMRect(mx, my, mw, mh, 0.24f, 0.30f, 0.37f, 0.98f);
	DMRect(mx + 1.0f, my + 1.0f, mw - 2.0f, mh - 2.0f, 0.048f, 0.066f, 0.088f, 0.96f);

	// Title bar
	DMRect(mx + 1.0f, my + 1.0f, mw - 2.0f, 24.0f, 0.075f, 0.105f, 0.145f, 0.98f);
	DMRect(mx + 1.0f, my + 25.0f, mw - 2.0f, 1.0f, 0.435f, 0.714f, 0.769f, 0.85f);
	DMText(mx + 10.0f, my + 7.0f, "DUEL PVP SETUP", DMColor(150, 232, 248, 255), true);
	DMDrawButton(mx + mw - 24.0f, my + 4.0f, 18.0f, 17.0f, "X", 0, true);

	float y = my + 34.0f;
	char oppLine[64];
	sprintf_s(oppLine, sizeof(oppLine), "Opponent: %s   (Arena: Stadium Ring | First to 5)", this->m_TargetName);
	DMText(mx + 12.0f, y, oppLine, DMColor(255, 226, 115, 255), true);
	y += 20.0f;

	// Duel Rules checkboxes
	DMText(mx + 12.0f, y, "Duel Rules:", DMColor(180, 205, 220, 255), true);
	y += 16.0f;

	const char* ruleNames[3] = { "No HP Pot", "No Buff", "No Mana Pot" };
	const BYTE ruleBits[3] = { DUEL_RULE_NO_HP, DUEL_RULE_NO_BUFF, DUEL_RULE_NO_MANA };
	for (int i = 0; i < 3; i++)
	{
		float rx = mx + 12.0f + i * 112.0f;
		bool on = ((this->m_SelectedRules & ruleBits[i]) != 0);
		DMRect(rx, y, 12.0f, 12.0f, on ? 0.35f : 0.28f, on ? 0.82f : 0.34f, on ? 0.55f : 0.40f, 0.95f);
		DMRect(rx + 1.0f, y + 1.0f, 10.0f, 10.0f, on ? 0.12f : 0.05f, on ? 0.35f : 0.07f, on ? 0.20f : 0.09f, 0.95f);
		if (on) DMText(rx + 2.5f, y + 0.5f, "v", DMColor(160, 255, 195, 255), true);
		DMText(rx + 17.0f, y + 1.0f, ruleNames[i], on ? DMColor(235, 248, 255, 255) : DMColor(150, 168, 182, 255), false);
	}
	y += 22.0f;

	// Item Wager section
	char betHdr[64];
	sprintf_s(betHdr, sizeof(betHdr), "Optional Item Wager (%d / 4 Selected - Click to toggle):", this->m_BetCount);
	DMText(mx + 12.0f, y, betHdr, DMColor(180, 205, 220, 255), true);
	y += 16.0f;

	std::vector<DM_INV_ENTRY> items;
	DMCollectInventoryItems(items);

	const int maxVisible = 7;
	int maxScroll = ((int)items.size() > maxVisible) ? ((int)items.size() - maxVisible) : 0;
	if (this->m_InvScroll > maxScroll) this->m_InvScroll = maxScroll;
	if (this->m_InvScroll < 0) this->m_InvScroll = 0;

	const float listX = mx + 12.0f, listW = mw - 24.0f, listH = 148.0f;
	DMRect(listX, y, listW, listH, 0.18f, 0.24f, 0.30f, 0.92f);
	DMRect(listX + 1.0f, y + 1.0f, listW - 2.0f, listH - 2.0f, 0.038f, 0.052f, 0.070f, 0.96f);

	if (items.empty())
	{
		DMTextCentered(listX + listW * 0.5f, y + 64.0f, "Inventory is empty (No item wager).", DMColor(135, 152, 166, 255), false);
	}
	else
	{
		for (int i = 0; i < maxVisible && (this->m_InvScroll + i) < (int)items.size(); i++)
		{
			const DM_INV_ENTRY& it = items[this->m_InvScroll + i];
			bool sel = DMIsSlotSelected(this->m_BetSlots, this->m_BetCount, it.slot);
			float ry = y + 3.0f + i * 20.0f;
			if (sel)
			{
				DMRect(listX + 3.0f, ry, listW - 28.0f, 18.0f, 0.10f, 0.30f, 0.20f, 0.90f);
			}
			else if (DMHit(listX + 3.0f, ry, listW - 28.0f, 18.0f))
			{
				DMRect(listX + 3.0f, ry, listW - 28.0f, 18.0f, 0.10f, 0.15f, 0.21f, 0.85f);
			}
			char rowTxt[72];
			sprintf_s(rowTxt, sizeof(rowTxt), "%s %s", sel ? "[WAGER]" : "[  +  ]", it.label);
			DMText(listX + 8.0f, ry + 3.5f, rowTxt, sel ? DMColor(145, 255, 185, 255) : DMColor(215, 228, 238, 255), sel);
		}
		if (maxScroll > 0)
		{
			DMDrawButton(listX + listW - 22.0f, y + 4.0f, 18.0f, 18.0f, "^", 0, this->m_InvScroll > 0);
			DMDrawButton(listX + listW - 22.0f, y + listH - 22.0f, 18.0f, 18.0f, "v", 0, this->m_InvScroll < maxScroll);
		}
	}

	// Bottom buttons
	const float btnY = my + mh - 34.0f;
	DMDrawButton(mx + 44.0f, btnY, 126.0f, 24.0f, "Send Challenge", 2, true);
	DMDrawButton(mx + mw - 170.0f, btnY, 126.0f, 24.0f, "Cancel", 3, true);
}

void CDuelManager::RenderRequestModal()
{
	const float mw = 356.0f, mh = 290.0f;
	const float mx = (640.0f - mw) * 0.5f, my = 66.0f;

	DMRect(mx - 2.0f, my - 2.0f, mw + 4.0f, mh + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
	DMRect(mx, my, mw, mh, 0.435f, 0.714f, 0.769f, 0.98f);
	DMRect(mx + 1.0f, my + 1.0f, mw - 2.0f, mh - 2.0f, 0.048f, 0.066f, 0.088f, 0.96f);

	DMRect(mx + 1.0f, my + 1.0f, mw - 2.0f, 24.0f, 0.075f, 0.105f, 0.145f, 0.98f);
	DMRect(mx + 1.0f, my + 25.0f, mw - 2.0f, 1.0f, 0.435f, 0.714f, 0.769f, 0.85f);

	DWORD elapsed = GetTickCount() - this->m_RequestTick;
	int remSec = (elapsed < 20000) ? (int)((20000 - elapsed + 999) / 1000) : 0;
	char title[64];
	sprintf_s(title, sizeof(title), "DUEL PVP INVITATION (%ds)", remSec);
	DMText(mx + 10.0f, my + 7.0f, title, DMColor(255, 224, 115, 255), true);

	float y = my + 34.0f;
	char chalLine[80];
	sprintf_s(chalLine, sizeof(chalLine), "[%s] challenges you to a PvP Duel!", this->m_ChallengerName);
	DMText(mx + 12.0f, y, chalLine, DMColor(235, 248, 255, 255), true);
	y += 18.0f;

	char ruleStr[64], ruleLine[88];
	DMFormatRules(this->m_RequestedRules, ruleStr, sizeof(ruleStr));
	sprintf_s(ruleLine, sizeof(ruleLine), "Rules: %s", ruleStr);
	DMText(mx + 12.0f, y, ruleLine, DMColor(145, 228, 245, 255), false);
	y += 18.0f;

	// Challenger wager display
	char cBetHdr[64];
	sprintf_s(cBetHdr, sizeof(cBetHdr), "Challenger Wager (%d item(s)):", this->m_RequestedBetCount);
	DMText(mx + 12.0f, y, cBetHdr, DMColor(255, 210, 120, 255), true);
	y += 15.0f;
	if (this->m_RequestedBetCount == 0)
	{
		DMText(mx + 20.0f, y, "None (Friendly PvP Duel)", DMColor(150, 168, 182, 255), false);
		y += 16.0f;
	}
	else
	{
		for (BYTE i = 0; i < this->m_RequestedBetCount; i++)
		{
			const DUEL_BET_ITEM_INFO& bi = this->m_RequestedBetItems[i];
			ITEM_ATTRIBUTE* attr = (bi.index < MAX_ITEM) ? (ITEM_ATTRIBUTE*)(ItemAttribute + bi.index * sizeof(ITEM_ATTRIBUTE)) : NULL;
			char bLine[64];
			sprintf_s(bLine, sizeof(bLine), "- %s%s +%u", (bi.exce ? "Exc " : ""), (attr ? attr->Name : "Item"), bi.level);
			DMText(mx + 20.0f, y, bLine, DMColor(145, 255, 185, 255), false);
			y += 13.0f;
		}
		y += 4.0f;
	}

	// Target optional counter-wager
	char tBetHdr[64];
	sprintf_s(tBetHdr, sizeof(tBetHdr), "Your Optional Wager (%d / 4 Selected):", this->m_TargetBetCount);
	DMText(mx + 12.0f, y, tBetHdr, DMColor(180, 205, 220, 255), true);
	y += 15.0f;

	std::vector<DM_INV_ENTRY> items;
	DMCollectInventoryItems(items);
	const int maxVisible = 4;
	int maxScroll = ((int)items.size() > maxVisible) ? ((int)items.size() - maxVisible) : 0;
	if (this->m_ReqInvScroll > maxScroll) this->m_ReqInvScroll = maxScroll;
	if (this->m_ReqInvScroll < 0) this->m_ReqInvScroll = 0;

	const float listX = mx + 12.0f, listW = mw - 24.0f, listH = 84.0f;
	DMRect(listX, y, listW, listH, 0.18f, 0.24f, 0.30f, 0.92f);
	DMRect(listX + 1.0f, y + 1.0f, listW - 2.0f, listH - 2.0f, 0.038f, 0.052f, 0.070f, 0.96f);

	for (int i = 0; i < maxVisible && (this->m_ReqInvScroll + i) < (int)items.size(); i++)
	{
		const DM_INV_ENTRY& it = items[this->m_ReqInvScroll + i];
		bool sel = DMIsSlotSelected(this->m_TargetBetSlots, this->m_TargetBetCount, it.slot);
		float ry = y + 3.0f + i * 19.0f;
		if (sel) DMRect(listX + 3.0f, ry, listW - 28.0f, 17.0f, 0.10f, 0.30f, 0.20f, 0.90f);
		char rowTxt[72];
		sprintf_s(rowTxt, sizeof(rowTxt), "%s %s", sel ? "[WAGER]" : "[  +  ]", it.label);
		DMText(listX + 8.0f, ry + 3.0f, rowTxt, sel ? DMColor(145, 255, 185, 255) : DMColor(215, 228, 238, 255), sel);
	}
	if (maxScroll > 0)
	{
		DMDrawButton(listX + listW - 22.0f, y + 3.0f, 18.0f, 17.0f, "^", 0, this->m_ReqInvScroll > 0);
		DMDrawButton(listX + listW - 22.0f, y + listH - 20.0f, 18.0f, 17.0f, "v", 0, this->m_ReqInvScroll < maxScroll);
	}

	const float btnY = my + mh - 34.0f;
	DMDrawButton(mx + 44.0f, btnY, 126.0f, 24.0f, "Accept Duel", 2, true);
	DMDrawButton(mx + mw - 170.0f, btnY, 126.0f, 24.0f, "Decline", 3, true);
}

void CDuelManager::UpdateMouse()
{
	if (SceneFlag != MAIN_SCENE || !Hero) return;
	if (this->m_RequestReceived && this->UpdateRequestMouse()) return;
	if (this->m_SetupOpen && this->UpdateSetupMouse()) return;
}

bool CDuelManager::UpdateSetupMouse()
{
	const float mw = 356.0f, mh = 316.0f;
	const float mx = (640.0f - mw) * 0.5f, my = 56.0f;
	if (!DMHit(mx, my, mw, mh)) return false;

	MouseOnWindow = true;
	if (!MouseLButtonPush) return true;
	MouseLButtonPush = false; MouseLButton = false; MouseLButtonPop = false;
	PlayBuffer(25, 0, 0);

	if (DMHit(mx + mw - 24.0f, my + 4.0f, 18.0f, 17.0f)) { this->CloseSetupWindow(); return true; }

	const float btnY = my + mh - 34.0f;
	if (DMHit(mx + 44.0f, btnY, 126.0f, 24.0f)) { this->SendSetupRequest(); return true; }
	if (DMHit(mx + mw - 170.0f, btnY, 126.0f, 24.0f)) { this->CloseSetupWindow(); return true; }

	// Rules checkboxes
	float ry = my + 70.0f;
	const BYTE ruleBits[3] = { DUEL_RULE_NO_HP, DUEL_RULE_NO_BUFF, DUEL_RULE_NO_MANA };
	for (int i = 0; i < 3; i++)
	{
		if (DMHit(mx + 12.0f + i * 112.0f, ry, 106.0f, 15.0f))
		{
			this->m_SelectedRules ^= ruleBits[i];
			return true;
		}
	}

	// Inventory list
	float ly = my + 108.0f;
	const float listX = mx + 12.0f, listW = mw - 24.0f, listH = 148.0f;
	std::vector<DM_INV_ENTRY> items;
	DMCollectInventoryItems(items);
	const int maxVisible = 7;
	int maxScroll = ((int)items.size() > maxVisible) ? ((int)items.size() - maxVisible) : 0;

	if (maxScroll > 0)
	{
		if (this->m_InvScroll > 0 && DMHit(listX + listW - 22.0f, ly + 4.0f, 18.0f, 18.0f)) { this->m_InvScroll--; return true; }
		if (this->m_InvScroll < maxScroll && DMHit(listX + listW - 22.0f, ly + listH - 22.0f, 18.0f, 18.0f)) { this->m_InvScroll++; return true; }
	}

	for (int i = 0; i < maxVisible && (this->m_InvScroll + i) < (int)items.size(); i++)
	{
		float iy = ly + 3.0f + i * 20.0f;
		if (DMHit(listX + 3.0f, iy, listW - 28.0f, 18.0f))
		{
			DMToggleSlot(this->m_BetSlots, this->m_BetCount, items[this->m_InvScroll + i].slot);
			return true;
		}
	}

	return true;
}

bool CDuelManager::UpdateRequestMouse()
{
	const float mw = 356.0f, mh = 290.0f;
	const float mx = (640.0f - mw) * 0.5f, my = 66.0f;
	if (!DMHit(mx, my, mw, mh)) return false;

	MouseOnWindow = true;
	if (!MouseLButtonPush) return true;
	MouseLButtonPush = false; MouseLButton = false; MouseLButtonPop = false;
	PlayBuffer(25, 0, 0);

	const float btnY = my + mh - 34.0f;
	if (DMHit(mx + 44.0f, btnY, 126.0f, 24.0f)) { this->SendAnswer(true); return true; }
	if (DMHit(mx + mw - 170.0f, btnY, 126.0f, 24.0f)) { this->SendAnswer(false); return true; }

	// Compute listY matching RenderRequestModal
	float ly = my + 34.0f + 18.0f + 18.0f + 15.0f;
	if (this->m_RequestedBetCount == 0) ly += 16.0f;
	else ly += this->m_RequestedBetCount * 13.0f + 4.0f;
	ly += 15.0f;

	const float listX = mx + 12.0f, listW = mw - 24.0f, listH = 84.0f;
	std::vector<DM_INV_ENTRY> items;
	DMCollectInventoryItems(items);
	const int maxVisible = 4;
	int maxScroll = ((int)items.size() > maxVisible) ? ((int)items.size() - maxVisible) : 0;

	if (maxScroll > 0)
	{
		if (this->m_ReqInvScroll > 0 && DMHit(listX + listW - 22.0f, ly + 3.0f, 18.0f, 17.0f)) { this->m_ReqInvScroll--; return true; }
		if (this->m_ReqInvScroll < maxScroll && DMHit(listX + listW - 22.0f, ly + listH - 20.0f, 18.0f, 17.0f)) { this->m_ReqInvScroll++; return true; }
	}

	for (int i = 0; i < maxVisible && (this->m_ReqInvScroll + i) < (int)items.size(); i++)
	{
		float iy = ly + 3.0f + i * 19.0f;
		if (DMHit(listX + 3.0f, iy, listW - 28.0f, 17.0f))
		{
			DMToggleSlot(this->m_TargetBetSlots, this->m_TargetBetCount, items[this->m_ReqInvScroll + i].slot);
			return true;
		}
	}

	return true;
}
