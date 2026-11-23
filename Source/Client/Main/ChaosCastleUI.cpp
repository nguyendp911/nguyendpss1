#include "stdafx.h"
#include "ChaosCastleUI.h"
#include "Font.h"
#include "Protocol.h"
#include "Sound.h"
#include <stdio.h>
#include <string.h>

CChaosCastleUI gChaosCastleUI;

static const float CC_W = 436.0f;
static const float CC_H = 278.0f;
static const float CC_X = (640.0f - CC_W) * 0.5f;
static const float CC_Y = 76.0f;

static const char* gCCRanges[CC_LEVELS] = {
	"Lv. 15 - 80",
	"Lv. 81 - 160",
	"Lv. 161 - 260",
	"Lv. 261 - 400",
	"Lv. 401 - 650",
	"Lv. 651 - 1000",
};

static bool CCHit(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static DWORD CCColor(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static void CCSetFont(HFONT f)
{
	if (f) SelectObject(m_hFontDC, f);
}

static float CCMeasureW(const char* text)
{
	if (!text || !text[0]) return 0.0f;
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (float)sz.cx / (g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f);
}

static void CCRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

static void CCText(float x, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	CCSetFont(bold ? g_hFontBold : g_hFont);
	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	SetBackgroundTextColor = 0;
	SetTextColor = CCColor(0, 0, 0, 220);
	RenderText((int)x + 1, (int)y + 1, (char*)text, 0, RT3_SORT_LEFT, NULL);
	SetTextColor = color;
	RenderText((int)x, (int)y, (char*)text, 0, RT3_SORT_LEFT, NULL);
}

static void CCTextCentered(float cx, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	CCSetFont(bold ? g_hFontBold : g_hFont);
	float w = CCMeasureW(text);
	CCText(cx - w * 0.5f, y, text, color, bold);
}

static void CCDrawButton(float x, float y, float w, float h, const char* text, int style, bool enabled = true)
{
	bool hov = enabled && CCHit(x, y, w, h);
	float br = 0.24f, bg = 0.30f, bb = 0.37f;
	float fr = 0.08f, fg = 0.11f, fb = 0.15f;
	DWORD tc = CCColor(220, 236, 246, 255);

	if (!enabled)
	{
		br = 0.16f; bg = 0.19f; bb = 0.22f;
		fr = 0.06f; fg = 0.075f; fb = 0.095f;
		tc = CCColor(105, 118, 130, 255);
	}
	else if (style == 1)
	{
		br = 0.42f; bg = 0.80f; bb = 0.90f;
		fr = 0.11f; fg = 0.22f; fb = 0.30f;
		tc = CCColor(235, 250, 255, 255);
	}
	else if (style == 2)
	{
		br = 0.24f; bg = 0.82f; bb = 0.48f;
		fr = 0.08f; fg = 0.25f; fb = 0.15f;
		tc = CCColor(195, 255, 215, 255);
	}
	else if (style == 3)
	{
		br = 0.84f; bg = 0.28f; bb = 0.28f;
		fr = 0.26f; fg = 0.08f; fb = 0.09f;
		tc = CCColor(255, 210, 210, 255);
	}

	if (hov) { br *= 1.15f; bg *= 1.15f; bb *= 1.15f; fr *= 1.25f; fg *= 1.25f; fb *= 1.25f; }
	CCRect(x, y, w, h, br, bg, bb, 0.96f);
	CCRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fr, fg, fb, 0.96f);
	CCTextCentered(x + w * 0.5f, y + (h - 10.0f) * 0.5f, text, tc, style != 0);
}

CChaosCastleUI::CChaosCastleUI()
{
	this->m_Open = false;
	memset(&this->m_Info, 0, sizeof(this->m_Info));
	this->m_InfoTick = 0;
	this->m_HasState = false;
	memset(&this->m_State, 0, sizeof(this->m_State));
	this->m_StateTick = 0;
	this->m_HasResult = false;
	memset(&this->m_Result, 0, sizeof(this->m_Result));
	this->m_ResultTick = 0;
}

CChaosCastleUI::~CChaosCastleUI()
{
}

void CChaosCastleUI::Close()
{
	this->m_Open = false;
	this->m_HasResult = false;
	PlayBuffer(29, 0, 0);
}

void CChaosCastleUI::SendInfo()
{
	CG_CHAOS_CASTLE_REQ req;
	req.header.set(0x99, 0x20, sizeof(req));
	req.level = 0;
	gProtocol.DataSend((BYTE*)&req, sizeof(req));
}

void CChaosCastleUI::SendEnter()
{
	CG_CHAOS_CASTLE_REQ req;
	req.header.set(0x99, 0x21, sizeof(req));
	req.level = this->m_Info.able;
	gProtocol.DataSend((BYTE*)&req, sizeof(req));
	this->m_Open = false;
	PlayBuffer(25, 0, 0);
}

void CChaosCastleUI::OnInfoRecv(BYTE* lpMsg)
{
	memcpy(&this->m_Info, lpMsg, sizeof(GC_CHAOS_CASTLE_INFO));
	this->m_InfoTick = GetTickCount();
	this->m_Open = true;
	PlayBuffer(25, 0, 0);
}

void CChaosCastleUI::OnStateRecv(BYTE* lpMsg)
{
	memcpy(&this->m_State, lpMsg, sizeof(GC_CHAOS_CASTLE_STATE));
	this->m_HasState = true;
	this->m_StateTick = GetTickCount();
}

void CChaosCastleUI::OnResultRecv(BYTE* lpMsg)
{
	memcpy(&this->m_Result, lpMsg, sizeof(GC_CHAOS_CASTLE_RESULT));
	this->m_HasResult = true;
	this->m_ResultTick = GetTickCount();
	this->m_HasState = false;
	PlayBuffer(25, 0, 0);
}

void ChaosCastleDress(DWORD c, bool self)
{
	if (c == 0 || Hero == 0) return;

	// Authentic Main5.2 Chaos Castle look (ZzzCharacter.cpp:9221 + CSChaosCastle.cpp:55):
	// Full-body Chaos Castle Guard armor (Data\Player\Angel.bmd), Hero has +9 sheen to distinguish self,
	// all other players & monsters wear identical matte Guard armor + class weapons, no wings/pet/name.
	*(short*)(c + 0x1F8) = (short)MODEL_CC_GUARD;
	*(BYTE*)(c + 0x1F8 + 2) = (BYTE)(self ? 7 : 0);
	*(BYTE*)(c + 0x1F8 + 3) = 0;

	for (int part = 1; part < 5; part++)
	{
		DWORD p = c + 0x1F8 + 24 * part;
		if (part == 4)
		{
			// Main.exe 0x44396f requires Boots.Type != -1 AND Boots.Level >= 5 to RUN (PLAYER_RUN).
			// Use empty BMD slot (MODEL_CC_GUARD - 1, NumMeshs == 0) so nothing renders over Angel.bmd while character runs!
			*(short*)p = (short)(MODEL_CC_GUARD - 1);
			*(BYTE*)(p + 2) = 7;
		}
		else
		{
			*(short*)p = -1;
			*(BYTE*)(p + 2) = 0;
		}
		*(BYTE*)(p + 3) = 0;
	}

	int cls = *(BYTE*)(c + 0x1BC) & 7;
	short weapon0 = (short)GET_ITEM_MODEL(0, 16); // Sword of Destruction (DK/MG)
	short weapon1 = (short)GET_ITEM_MODEL(0, 16);
	if (cls == 2) // Elf: preserve Bow (left hand + Arrows) vs Crossbow (right hand + Bolts) for Triple Shot!
	{
		short curW1 = *(short*)(c + 0x270 + 24);
		bool hasBowLeft = (curW1 >= (short)GET_ITEM_MODEL(4, 0) && curW1 <= (short)GET_ITEM_MODEL(4, 20) && curW1 != (short)GET_ITEM_MODEL(4, 7));
		if (hasBowLeft)
		{
			weapon0 = (short)GET_ITEM_MODEL(4, 15); // Arrows (right hand)
			weapon1 = (short)GET_ITEM_MODEL(4, 17); // Celestial Bow (left hand)
		}
		else
		{
			weapon0 = (short)GET_ITEM_MODEL(4, 16); // Saint Crossbow (right hand)
			weapon1 = (short)GET_ITEM_MODEL(4, 7);  // Bolts (left hand)
		}
	}
	else if (cls == 0) // DW
	{
		weapon0 = (short)GET_ITEM_MODEL(5, 5); // Legendary Staff
		weapon1 = -1;
	}

	*(short*)(c + 0x270) = weapon0;
	*(BYTE*)(c + 0x272) = 0;
	*(short*)(c + 0x270 + 24) = weapon1;
	*(BYTE*)(c + 0x272 + 24) = 0;

	*(short*)(c + 0x2A0) = -1; // Hide wings
	*(short*)(c + 0x2B8) = -1; // Hide pet

	if (!self)
	{
		*(char*)(c + 0x1C1) = 0; // Hide name so players & monsters are indistinguishable
	}
}

void CChaosCastleUI::DressAll()
{
	if (World < 18 || World > 23 || !Hero) return;

	for (int i = 0; i < MAX_MAIN_VIEWPORT; i++)
	{
		DWORD c = CharactersClient + (i * 916);
		if (*(BYTE*)c == 0 || *(WORD*)(c + 2) != 390) continue;

		ChaosCastleDress(c, (c == Hero));
	}
}

void CChaosCastleUI::Render()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		this->m_Open = false;
		this->m_HasState = false;
		this->m_HasResult = false;
		return;
	}

	// Clear stale HUD if player left Chaos Castle maps (18..23)
	if (World < 18 || World > 23)
	{
		this->m_HasState = false;
	}
	else
	{
		this->DressAll();
	}

	if (this->m_Open && (GetTickCount() - this->m_InfoTick) > 3000)
	{
		this->m_InfoTick = GetTickCount();
		this->SendInfo();
	}

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTC = SetTextColor;
	const DWORD oldBG = SetBackgroundTextColor;

	if (this->m_HasState) this->RenderBattleHud();
	if (this->m_Open) this->RenderEntryWindow();
	if (this->m_HasResult) this->RenderResultModal();

	CCSetFont(oldFont ? oldFont : g_hFont);
	SetTextColor = oldTC;
	SetBackgroundTextColor = oldBG;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

void CChaosCastleUI::RenderBattleHud()
{
	const float hw = 276.0f, hh = 42.0f;
	const float hx = 320.0f - hw * 0.5f, hy = 8.0f;

	bool danger = (this->m_State.hollow >= 2);
	CCRect(hx - 1.0f, hy - 1.0f, hw + 2.0f, hh + 2.0f, 0.0f, 0.0f, 0.0f, 0.50f);
	CCRect(hx, hy, hw, hh, danger ? 0.88f : 0.435f, danger ? 0.28f : 0.714f, danger ? 0.24f : 0.769f, 0.95f);
	CCRect(hx + 1.0f, hy + 1.0f, hw - 2.0f, hh - 2.0f, 0.045f, 0.065f, 0.092f, 0.92f);

	int mm = this->m_State.remain / 60;
	int ss = this->m_State.remain % 60;
	const char* stName = (this->m_State.state == 3) ? "BATTLE" : (this->m_State.state == 2) ? "STAND-BY" : (this->m_State.state == 4) ? "ENDING" : "WAITING";

	char line1[96];
	sprintf_s(line1, sizeof(line1), "CHAOS CASTLE [%s]  |  Time %02d:%02d  |  Alive: %u", stName, mm, ss, this->m_State.alive);
	CCTextCentered(hx + hw * 0.5f, hy + 5.0f, line1, CCColor(150, 232, 248, 255), true);

	char line2[96];
	sprintf_s(line2, sizeof(line2), "Kills: %u Mobs / %u Players  |  Collapse Stage: %u / 3",
		this->m_State.monsters, this->m_State.players, this->m_State.hollow);
	CCTextCentered(hx + hw * 0.5f, hy + 22.0f, line2, danger ? CCColor(255, 145, 110, 255) : CCColor(255, 224, 120, 255), false);
}

void CChaosCastleUI::RenderEntryWindow()
{
	CCRect(CC_X - 2.0f, CC_Y - 2.0f, CC_W + 4.0f, CC_H + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
	CCRect(CC_X, CC_Y, CC_W, CC_H, 0.24f, 0.30f, 0.37f, 0.98f);
	CCRect(CC_X + 1.0f, CC_Y + 1.0f, CC_W - 2.0f, CC_H - 2.0f, 0.045f, 0.064f, 0.088f, 0.96f);

	// Title bar
	CCRect(CC_X + 1.0f, CC_Y + 1.0f, CC_W - 2.0f, 24.0f, 0.075f, 0.105f, 0.145f, 0.98f);
	CCRect(CC_X + 1.0f, CC_Y + 25.0f, CC_W - 2.0f, 1.0f, 0.435f, 0.714f, 0.769f, 0.85f);
	CCText(CC_X + 10.0f, CC_Y + 7.0f, "CHAOS CASTLE - SURVIVAL ARENA", CCColor(150, 234, 248, 255), true);
	CCDrawButton(CC_X + CC_W - 24.0f, CC_Y + 4.0f, 18.0f, 17.0f, "X", 0, true);

	// Column Headers
	const float bx = CC_X + 12.0f;
	const float bw = CC_W - 24.0f;
	float y = CC_Y + 34.0f;
	CCText(bx + 6.0f, y, "Tier", CCColor(150, 175, 195, 255), true);
	CCText(bx + 62.0f, y, "Level Range", CCColor(150, 175, 195, 255), true);
	CCText(bx + 162.0f, y, "Status", CCColor(150, 175, 195, 255), true);
	CCText(bx + 232.0f, y, "Timer", CCColor(150, 175, 195, 255), true);
	CCText(bx + 292.0f, y, "Users", CCColor(150, 175, 195, 255), true);
	CCText(bx + 342.0f, y, "Fee (Zen)", CCColor(150, 175, 195, 255), true);
	y += 16.0f;

	for (int i = 0; i < CC_LEVELS; i++)
	{
		bool isMine = (this->m_Info.able == i);
		const GC_CHAOS_CASTLE_LEVEL& lv = this->m_Info.level[i];

		CCRect(bx, y, bw, 24.0f, isMine ? 0.435f : 0.18f, isMine ? 0.78f : 0.23f, isMine ? 0.88f : 0.29f, 0.92f);
		CCRect(bx + 1.0f, y + 1.0f, bw - 2.0f, 22.0f, isMine ? 0.08f : 0.052f, isMine ? 0.14f : 0.072f, isMine ? 0.19f : 0.095f, 0.95f);

		char tierBuf[16];
		sprintf_s(tierBuf, sizeof(tierBuf), "CC %d%s", i + 1, isMine ? "*" : "");
		CCText(bx + 6.0f, y + 6.5f, tierBuf, isMine ? CCColor(255, 226, 115, 255) : CCColor(225, 238, 248, 255), isMine);
		CCText(bx + 62.0f, y + 6.5f, gCCRanges[i], CCColor(185, 205, 220, 255), false);

		const char* stText = lv.enter ? "OPEN" : (lv.state == 3 ? "FIGHT" : (lv.state == 2 ? "STAND" : "WAIT"));
		DWORD stCol = lv.enter ? CCColor(115, 248, 165, 255) : (lv.state == 3 ? CCColor(255, 125, 115, 255) : CCColor(165, 185, 200, 255));
		CCText(bx + 162.0f, y + 6.5f, stText, stCol, lv.enter != 0);

		char tBuf[16];
		sprintf_s(tBuf, sizeof(tBuf), "%02d:%02d", lv.remain / 60, lv.remain % 60);
		CCText(bx + 232.0f, y + 6.5f, tBuf, CCColor(215, 230, 242, 255), false);

		char uBuf[16];
		sprintf_s(uBuf, sizeof(uBuf), "%u/70", lv.users);
		CCText(bx + 292.0f, y + 6.5f, uBuf, CCColor(175, 215, 235, 255), false);

		char fBuf[24];
		sprintf_s(fBuf, sizeof(fBuf), "%u", this->m_Info.cost[i]);
		CCText(bx + 342.0f, y + 6.5f, fBuf, CCColor(255, 214, 110, 255), false);

		y += 27.0f;
	}

	// Hint + Enter button
	y += 4.0f;
	bool canEnter = (this->m_Info.able < CC_LEVELS && this->m_Info.level[this->m_Info.able].enter != 0);
	char hint[96];
	if (this->m_Info.able >= CC_LEVELS)
		strcpy_s(hint, sizeof(hint), "You need at least Level 15 to join Chaos Castle.");
	else if (canEnter)
		sprintf_s(hint, sizeof(hint), "Chaos Castle %d is OPEN! Winner gets Bonus EXP + Zen + 2 Jewels!", this->m_Info.able + 1);
	else
		sprintf_s(hint, sizeof(hint), "Your Tier: Chaos Castle %d (Wait for entrance to open)", this->m_Info.able + 1);
	CCTextCentered(CC_X + CC_W * 0.5f, y, hint, canEnter ? CCColor(125, 248, 175, 255) : CCColor(175, 195, 212, 255), canEnter);

	const float btnY = CC_Y + CC_H - 34.0f;
	CCDrawButton(CC_X + 70.0f, btnY, 152.0f, 24.0f, "Enter Chaos Castle", 2, canEnter);
	CCDrawButton(CC_X + CC_W - 202.0f, btnY, 132.0f, 24.0f, "Close", 3, true);
}

void CChaosCastleUI::RenderResultModal()
{
	const float rw = 300.0f, rh = 162.0f;
	const float rx = 320.0f - rw * 0.5f, ry = 136.0f;

	CCRect(rx - 2.0f, ry - 2.0f, rw + 4.0f, rh + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
	CCRect(rx, ry, rw, rh, this->m_Result.win ? 0.88f : 0.435f, this->m_Result.win ? 0.72f : 0.714f, this->m_Result.win ? 0.24f : 0.769f, 0.98f);
	CCRect(rx + 1.0f, ry + 1.0f, rw - 2.0f, rh - 2.0f, 0.048f, 0.066f, 0.090f, 0.96f);

	CCTextCentered(rx + rw * 0.5f, ry + 14.0f,
		this->m_Result.win ? "CHAOS CASTLE VICTORY!" : "CHAOS CASTLE ENDED",
		this->m_Result.win ? CCColor(255, 226, 110, 255) : CCColor(160, 225, 245, 255), true);

	char l1[64], l2[64], l3[64];
	sprintf_s(l1, sizeof(l1), "Monsters Killed: %u   |   Players Killed: %u", this->m_Result.monsters, this->m_Result.players);
	sprintf_s(l2, sizeof(l2), "Total EXP Earned: +%u", this->m_Result.experience);
	sprintf_s(l3, sizeof(l3), this->m_Result.win ? "Reward: +%u Zen + 2 Random Jewels!" : "Reward Zen: %u", this->m_Result.zen);

	CCTextCentered(rx + rw * 0.5f, ry + 44.0f, l1, CCColor(225, 238, 248, 255), false);
	CCTextCentered(rx + rw * 0.5f, ry + 66.0f, l2, CCColor(135, 248, 175, 255), true);
	CCTextCentered(rx + rw * 0.5f, ry + 88.0f, l3, CCColor(255, 218, 115, 255), true);

	CCDrawButton(rx + (rw - 104.0f) * 0.5f, ry + rh - 34.0f, 104.0f, 24.0f, "Close", 1, true);
}

void CChaosCastleUI::UpdateMouse()
{
	if (SceneFlag != MAIN_SCENE || !Hero) return;

	if (this->m_HasResult)
	{
		const float rw = 300.0f, rh = 162.0f;
		const float rx = 320.0f - rw * 0.5f, ry = 136.0f;
		if (CCHit(rx, ry, rw, rh))
		{
			MouseOnWindow = true;
			if (MouseLButtonPush)
			{
				MouseLButtonPush = false; MouseLButton = false; MouseLButtonPop = false;
				if (CCHit(rx + (rw - 104.0f) * 0.5f, ry + rh - 34.0f, 104.0f, 24.0f))
				{
					this->m_HasResult = false;
					PlayBuffer(25, 0, 0);
				}
			}
			return;
		}
	}

	if (!this->m_Open || !CCHit(CC_X, CC_Y, CC_W, CC_H)) return;

	MouseOnWindow = true;
	if (!MouseLButtonPush) return;
	MouseLButtonPush = false; MouseLButton = false; MouseLButtonPop = false;
	PlayBuffer(25, 0, 0);

	if (CCHit(CC_X + CC_W - 24.0f, CC_Y + 4.0f, 18.0f, 17.0f)) { this->Close(); return; }

	const float btnY = CC_Y + CC_H - 34.0f;
	bool canEnter = (this->m_Info.able < CC_LEVELS && this->m_Info.level[this->m_Info.able].enter != 0);
	if (canEnter && CCHit(CC_X + 70.0f, btnY, 152.0f, 24.0f)) { this->SendEnter(); return; }
	if (CCHit(CC_X + CC_W - 202.0f, btnY, 132.0f, 24.0f)) { this->Close(); return; }
}
