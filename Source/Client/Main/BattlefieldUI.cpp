#include "stdafx.h"
#include "BattlefieldUI.h"
#include "Font.h"
#include "Protocol.h"
#include "Sound.h"
#include <stdio.h>
#include <string.h>

CBattlefieldUI gBattlefieldUI;

static const float BF_W = 410.0f;
static const float BF_H = 216.0f;
static const float BF_X = (640.0f - BF_W) * 0.5f;
static const float BF_Y = 96.0f;

static bool BFHit(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static DWORD BFColor(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static void BFSetFont(HFONT f)
{
	if (f) SelectObject(m_hFontDC, f);
}

static float BFMeasureW(const char* text)
{
	if (!text || !text[0]) return 0.0f;
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (float)sz.cx / (g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f);
}

static void BFRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

static void BFText(float x, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	BFSetFont(bold ? g_hFontBold : g_hFont);
	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	SetBackgroundTextColor = 0;
	SetTextColor = BFColor(0, 0, 0, 220);
	RenderText((int)x + 1, (int)y + 1, (char*)text, 0, RT3_SORT_LEFT, NULL);
	SetTextColor = color;
	RenderText((int)x, (int)y, (char*)text, 0, RT3_SORT_LEFT, NULL);
}

static void BFTextCentered(float cx, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	BFSetFont(bold ? g_hFontBold : g_hFont);
	float w = BFMeasureW(text);
	BFText(cx - w * 0.5f, y, text, color, bold);
}

static void BFDrawButton(float x, float y, float w, float h, const char* text, int style, bool enabled = true)
{
	bool hov = enabled && BFHit(x, y, w, h);
	float br = 0.24f, bg = 0.30f, bb = 0.37f;
	float fr = 0.08f, fg = 0.11f, fb = 0.15f;
	DWORD tc = BFColor(220, 236, 246, 255);

	if (!enabled)
	{
		br = 0.16f; bg = 0.19f; bb = 0.22f;
		fr = 0.06f; fg = 0.075f; fb = 0.095f;
		tc = BFColor(105, 118, 130, 255);
	}
	else if (style == 2)
	{
		br = 0.24f; bg = 0.82f; bb = 0.48f;
		fr = 0.08f; fg = 0.25f; fb = 0.15f;
		tc = BFColor(195, 255, 215, 255);
	}

	if (hov) { br *= 1.15f; bg *= 1.15f; bb *= 1.15f; fr *= 1.25f; fg *= 1.25f; fb *= 1.25f; }
	BFRect(x, y, w, h, br, bg, bb, 0.96f);
	BFRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fr, fg, fb, 0.96f);
	BFTextCentered(x + w * 0.5f, y + (h - 10.0f) * 0.5f, text, tc, style != 0);
}

CBattlefieldUI::CBattlefieldUI()
{
	this->m_Open = false;
	memset(&this->m_Info, 0, sizeof(this->m_Info));
	this->m_InfoTick = 0;
	this->m_HasState = false;
	memset(&this->m_State, 0, sizeof(this->m_State));
	this->m_HasResult = false;
	memset(&this->m_Result, 0, sizeof(this->m_Result));
	this->m_TeamCount = 0;
	memset(this->m_Teams, 0, sizeof(this->m_Teams));
}

CBattlefieldUI::~CBattlefieldUI()
{
}

void CBattlefieldUI::Close()
{
	this->m_Open = false;
	this->m_HasResult = false;
	PlayBuffer(29, 0, 0);
}

void CBattlefieldUI::SendInfo()
{
	CG_BF_REQ req;
	req.header.set(0x99, 0x40, sizeof(req));
	req.action = 0;
	gProtocol.DataSend((BYTE*)&req, sizeof(req));
}

void CBattlefieldUI::SendEnter()
{
	CG_BF_REQ req;
	req.header.set(0x99, 0x41, sizeof(req));
	req.action = 1;
	gProtocol.DataSend((BYTE*)&req, sizeof(req));
	this->m_Open = false;
	PlayBuffer(25, 0, 0);
}

void CBattlefieldUI::OnInfoRecv(BYTE* lpMsg)
{
	memcpy(&this->m_Info, lpMsg, sizeof(GC_BF_INFO));
	this->m_InfoTick = GetTickCount();
	this->m_Open = true;
	PlayBuffer(25, 0, 0);
}

void CBattlefieldUI::OnStateRecv(BYTE* lpMsg)
{
	memcpy(&this->m_State, lpMsg, sizeof(GC_BF_STATE));
	this->m_HasState = true;
}

void CBattlefieldUI::OnResultRecv(BYTE* lpMsg)
{
	memcpy(&this->m_Result, lpMsg, sizeof(GC_BF_RESULT));
	this->m_HasResult = true;
	this->m_HasState = false;
	PlayBuffer(25, 0, 0);
}

void CBattlefieldUI::OnTeamsRecv(BYTE* lpMsg)
{
	GC_BF_TEAMS* head = (GC_BF_TEAMS*)lpMsg;
	int cnt = head->count;
	if (cnt > 100) cnt = 100;
	this->m_TeamCount = cnt;
	BF_TEAM_ENTRY* list = (BF_TEAM_ENTRY*)(lpMsg + sizeof(GC_BF_TEAMS));
	for (int i = 0; i < cnt; i++) this->m_Teams[i] = list[i];
}

BYTE CBattlefieldUI::GetTeamByKey(WORD key) const
{
	for (int i = 0; i < this->m_TeamCount; i++)
	{
		if (this->m_Teams[i].index == key) return this->m_Teams[i].team;
	}
	return 0;
}

void CBattlefieldUI::Render()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		this->m_Open = false;
		this->m_HasState = false;
		this->m_HasResult = false;
		return;
	}

	if (World != BF_MAP_INDEX)
	{
		this->m_HasState = false;
	}

	if (this->m_Open && (GetTickCount() - this->m_InfoTick) > 3000)
	{
		this->m_InfoTick = GetTickCount();
		this->SendInfo();
	}

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTC = SetTextColor;
	const DWORD oldBG = SetBackgroundTextColor;

	if (World == BF_MAP_INDEX && this->m_HasState)
	{
		this->RenderBattleHud();
	}
	if (this->m_Open) this->RenderEntryWindow();
	if (this->m_HasResult) this->RenderResultModal();

	BFSetFont(oldFont ? oldFont : g_hFont);
	SetTextColor = oldTC;
	SetBackgroundTextColor = oldBG;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

void CBattlefieldUI::RenderTeamBadges()
{
	if (this->m_TeamCount <= 0) return;

	for (int n = 0; n < MAX_MAIN_VIEWPORT; n++)
	{
		DWORD vp = CharactersClient + (n * 916);
		if (*(BYTE*)vp == 0 || *(BYTE*)(vp + 0x84) != 1) continue; // Players only

		WORD key = *(WORD*)(vp + 0x1DC);
		BYTE team = this->GetTeamByKey(key);
		if (team != 1 && team != 2) continue;

		vec3_t pos;
		pos[0] = *(float*)(vp + 0x10);
		pos[1] = *(float*)(vp + 0x14);
		pos[2] = *(float*)(vp + 0x18) + *(float*)(vp + 0x9C) + 120.0f;

		int sx = 0, sy = 0;
		Projection(pos, &sx, &sy);
		if (sx < 0 || sx > 640 || sy < 0 || sy > 480) continue;

		const char* tag = (team == 1) ? "[BLUE]" : "[RED]";
		DWORD col = (team == 1) ? BFColor(95, 200, 255, 255) : BFColor(255, 95, 95, 255);
		BFTextCentered((float)sx, (float)sy - 14.0f, tag, col, true);
	}
}

void CBattlefieldUI::RenderBattleHud()
{
	const float hw = 344.0f, hh = 46.0f;
	const float hx = 320.0f - hw * 0.5f, hy = 8.0f;

	BFRect(hx - 1.0f, hy - 1.0f, hw + 2.0f, hh + 2.0f, 0.0f, 0.0f, 0.0f, 0.52f);
	BFRect(hx, hy, hw, hh, 0.42f, 0.72f, 0.84f, 0.95f);
	BFRect(hx + 1.0f, hy + 1.0f, hw - 2.0f, hh - 2.0f, 0.045f, 0.065f, 0.092f, 0.92f);

	int mm = this->m_State.remain / 60;
	int ss = this->m_State.remain % 60;

	char l1[112];
	sprintf_s(l1, sizeof(l1), "BLUE: %u   VS   RED: %u   |   Goal: %u   |   %02d:%02d",
		this->m_State.blueScore, this->m_State.redScore, this->m_State.winScore, mm, ss);
	BFTextCentered(hx + hw * 0.5f, hy + 6.0f, l1, BFColor(255, 228, 120, 255), true);

	auto towStr = [](BYTE o) -> const char* { return (o == 1) ? "BLUE" : (o == 2) ? "RED" : "FREE"; };
	const char* myTeamStr = (this->m_State.myTeam == 1) ? "BLUE" : "RED";
	char l2[128];
	sprintf_s(l2, sizeof(l2), "TOP:%s  MID:%s  BOT:%s   |   Team:%s  K:%u  Cap:%u",
		towStr(this->m_State.towerOwner[0]), towStr(this->m_State.towerOwner[1]), towStr(this->m_State.towerOwner[2]),
		myTeamStr, this->m_State.myKills, this->m_State.myCaptures);
	BFTextCentered(hx + hw * 0.5f, hy + 25.0f, l2,
		(this->m_State.myTeam == 1) ? BFColor(125, 215, 255, 255) : BFColor(255, 145, 135, 255), false);
}

void CBattlefieldUI::RenderEntryWindow()
{
	BFRect(BF_X - 2.0f, BF_Y - 2.0f, BF_W + 4.0f, BF_H + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
	BFRect(BF_X, BF_Y, BF_W, BF_H, 0.24f, 0.30f, 0.37f, 0.98f);
	BFRect(BF_X + 1.0f, BF_Y + 1.0f, BF_W - 2.0f, BF_H - 2.0f, 0.045f, 0.064f, 0.088f, 0.96f);

	BFRect(BF_X + 1.0f, BF_Y + 1.0f, BF_W - 2.0f, 24.0f, 0.075f, 0.105f, 0.145f, 0.98f);
	BFRect(BF_X + 1.0f, BF_Y + 25.0f, BF_W - 2.0f, 1.0f, 0.435f, 0.714f, 0.769f, 0.85f);
	BFText(BF_X + 10.0f, BF_Y + 7.0f, "BATTLEFIELD - 2 FACTION 3-LANE WAR", BFColor(150, 234, 248, 255), true);
	BFDrawButton(BF_X + BF_W - 24.0f, BF_Y + 4.0f, 18.0f, 17.0f, "X", 0, true);

	float cx = BF_X + BF_W * 0.5f;
	BFTextCentered(cx, BF_Y + 38.0f, "Schedule: Daily at 11:00 and 19:00 (2 times/day)", BFColor(255, 224, 115, 255), true);

	const char* stStr = (this->m_Info.state == 3) ? "BATTLE IN PROGRESS" : (this->m_Info.state == 2) ? "ENTRANCE OPEN" : "WAITING FOR SCHEDULE";
	DWORD stCol = this->m_Info.enterEnabled ? BFColor(115, 248, 165, 255) : BFColor(180, 200, 215, 255);
	char buf[96];
	sprintf_s(buf, sizeof(buf), "Status: %s  |  Timer: %02d:%02d", stStr, this->m_Info.remain / 60, this->m_Info.remain % 60);
	BFTextCentered(cx, BF_Y + 58.0f, buf, stCol, true);

	sprintf_s(buf, sizeof(buf), "BLUE Vanguard: %u   VS   RED Legion: %u   |   Goal: %u pts",
		this->m_Info.blueCount, this->m_Info.redCount, this->m_Info.winScore);
	BFTextCentered(cx, BF_Y + 80.0f, buf, BFColor(210, 232, 248, 255), false);

	// Rules box
	BFRect(BF_X + 14.0f, BF_Y + 102.0f, BF_W - 28.0f, 64.0f, 0.18f, 0.24f, 0.30f, 0.92f);
	BFRect(BF_X + 15.0f, BF_Y + 103.0f, BF_W - 30.0f, 62.0f, 0.055f, 0.078f, 0.105f, 0.95f);
	BFText(BF_X + 24.0f, BF_Y + 110.0f, "- Auto-balanced 2 Factions (BLUE vs RED). No PK / No EXP loss.", BFColor(185, 210, 228, 255), false);
	BFText(BF_X + 24.0f, BF_Y + 127.0f, "- 3 Lanes (TOP / MID / BOT): Destroy Flag Towers to capture (+50 pts).", BFColor(185, 210, 228, 255), false);
	BFText(BF_X + 24.0f, BF_Y + 144.0f, "- Held towers grant +5 pts/5s. Enemy Watchtowers deal heavy damage!", BFColor(255, 195, 130, 255), false);

	bool canJoin = (this->m_Info.enterEnabled != 0);
	BFDrawButton(cx - 80.0f, BF_Y + BF_H - 36.0f, 160.0f, 24.0f, "Join Battlefield", 2, canJoin);
}

void CBattlefieldUI::RenderResultModal()
{
	const float rw = 310.0f, rh = 178.0f;
	const float rx = (640.0f - rw) * 0.5f, ry = 135.0f;

	bool win = (this->m_Result.winnerTeam != 0 && this->m_Result.winnerTeam == this->m_Result.myTeam);
	BFRect(rx - 2.0f, ry - 2.0f, rw + 4.0f, rh + 4.0f, 0.0f, 0.0f, 0.0f, 0.62f);
	BFRect(rx, ry, rw, rh, win ? 0.25f : 0.78f, win ? 0.85f : 0.32f, win ? 0.52f : 0.28f, 0.98f);
	BFRect(rx + 1.0f, ry + 1.0f, rw - 2.0f, rh - 2.0f, 0.048f, 0.068f, 0.095f, 0.97f);

	float cx = rx + rw * 0.5f;
	const char* title = (this->m_Result.winnerTeam == 0) ? "BATTLEFIELD - DRAW" : (win ? "BATTLEFIELD - VICTORY!" : "BATTLEFIELD - DEFEAT");
	BFTextCentered(cx, ry + 14.0f, title, win ? BFColor(120, 252, 168, 255) : BFColor(255, 145, 130, 255), true);

	char buf[96];
	sprintf_s(buf, sizeof(buf), "Final Score: BLUE %u - %u RED", this->m_Result.blueScore, this->m_Result.redScore);
	BFTextCentered(cx, ry + 42.0f, buf, BFColor(255, 226, 120, 255), true);

	sprintf_s(buf, sizeof(buf), "Your Kills: %u   |   Tower Captures: %u", this->m_Result.myKills, this->m_Result.myCaptures);
	BFTextCentered(cx, ry + 64.0f, buf, BFColor(215, 232, 245, 255), false);

	sprintf_s(buf, sizeof(buf), "Reward EXP: +%u", this->m_Result.rewardExp);
	BFTextCentered(cx, ry + 86.0f, buf, BFColor(140, 235, 255, 255), false);

	sprintf_s(buf, sizeof(buf), "Reward Zen: +%u  |  Jewels: +%u", this->m_Result.rewardZen, this->m_Result.rewardJewels);
	BFTextCentered(cx, ry + 106.0f, buf, BFColor(255, 215, 110, 255), false);

	BFDrawButton(cx - 46.0f, ry + rh - 34.0f, 92.0f, 22.0f, "Close", 0, true);
}

void CBattlefieldUI::UpdateMouse()
{
	if (SceneFlag != MAIN_SCENE || !Hero) return;

	if (this->m_HasResult)
	{
		const float rw = 310.0f, rh = 178.0f;
		const float rx = (640.0f - rw) * 0.5f, ry = 135.0f;
		if (BFHit(rx, ry, rw, rh))
		{
			MouseOnWindow = true;
			if (MouseLButtonPush)
			{
				MouseLButtonPush = false; MouseLButton = false; MouseLButtonPop = false;
				if (BFHit(rx + rw * 0.5f - 46.0f, ry + rh - 34.0f, 92.0f, 22.0f)) this->Close();
			}
		}
		return;
	}

	if (!this->m_Open || !BFHit(BF_X, BF_Y, BF_W, BF_H)) return;
	MouseOnWindow = true;
	if (!MouseLButtonPush) return;

	MouseLButtonPush = false; MouseLButton = false; MouseLButtonPop = false;

	if (BFHit(BF_X + BF_W - 24.0f, BF_Y + 4.0f, 18.0f, 17.0f))
	{
		this->Close();
		return;
	}

	float cx = BF_X + BF_W * 0.5f;
	if (this->m_Info.enterEnabled && BFHit(cx - 80.0f, BF_Y + BF_H - 36.0f, 160.0f, 24.0f))
	{
		this->SendEnter();
	}
}
