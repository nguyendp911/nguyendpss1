#include "stdafx.h"
#include "RankingUI.h"
#include "Interface.h"
#include "Font.h"
#include "Protocol.h"
#include "Sound.h"
#include "stb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CRankingUI gRankingUI;

static const float RK_PW = 396.0f;
static const float RK_PH = 324.0f;
static const float RK_PX = (640.0f - RK_PW) * 0.5f; // 122
static const float RK_PY = 60.0f;

static void GetRankingHudLayout(float* btnW, float* btnH, float* topX, float* vipX, float* y)
{
	const float asp = (g_fScreenRate_x > 0.0f) ? (g_fScreenRate_y / g_fScreenRate_x) : 0.75f;
	*btnH = 22.0f;
	*btnW = (*btnH) * asp;
	*y = 8.0f;
	// Collapsible slots 9 & 10: Pinned(0..5), PK(6), Market(7), Recorder(8), TOP(9), VIP(10)
	*topX = 8.0f + (*btnW + 3.0f) * 9.0f;
	*vipX = 8.0f + (*btnW + 3.0f) * 10.0f;
}

static bool RKHit(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static DWORD RKColor(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static void RKSetFont(HFONT f)
{
	if (f) SelectObject(m_hFontDC, f);
}

static float RKMeasureW(const char* text)
{
	if (!text || !text[0]) return 0.0f;
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (float)sz.cx / (g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f);
}

static void RKRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

static void RKText(float x, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	RKSetFont(bold ? g_hFontBold : g_hFont);
	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	SetBackgroundTextColor = 0;
	SetTextColor = RKColor(0, 0, 0, 220);
	RenderText((int)x + 1, (int)y + 1, (char*)text, 0, RT3_SORT_LEFT, NULL);
	SetTextColor = color;
	RenderText((int)x, (int)y, (char*)text, 0, RT3_SORT_LEFT, NULL);
}

static void RKTextCentered(float cx, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	RKSetFont(bold ? g_hFontBold : g_hFont);
	float w = RKMeasureW(text);
	RKText(cx - w * 0.5f, y, text, color, bold);
}

static void RKFormatComma(DWORD val, char* out, int size)
{
	char raw[32];
	sprintf_s(raw, sizeof(raw), "%u", val);
	int len = (int)strlen(raw);
	int o = 0;
	for (int i = 0; i < len && o < size - 2; i++)
	{
		out[o++] = raw[i];
		int left = len - i - 1;
		if (left > 0 && (left % 3) == 0 && o < size - 2) out[o++] = ',';
	}
	out[o] = 0;
}

static void RKDrawButton(float x, float y, float w, float h, const char* text, int style, bool enabled = true)
{
	bool hov = enabled && RKHit(x, y, w, h);
	float br = 0.24f, bg = 0.29f, bb = 0.35f;
	float fr = 0.085f, fg = 0.115f, fb = 0.155f;
	DWORD tc = RKColor(220, 234, 244, 255);

	if (!enabled)
	{
		br = 0.16f; bg = 0.19f; bb = 0.22f;
		fr = 0.06f; fg = 0.075f; fb = 0.095f;
		tc = RKColor(105, 118, 130, 255);
	}
	else if (style == 1) // Active tab / cyan accent
	{
		br = 0.42f; bg = 0.80f; bb = 0.90f;
		fr = 0.12f; fg = 0.24f; fb = 0.32f;
		tc = RKColor(235, 250, 255, 255);
	}
	else if (style == 2) // Gold / Buy VIP
	{
		br = hov ? 0.95f : 0.78f; bg = hov ? 0.80f : 0.62f; bb = hov ? 0.32f : 0.20f;
		fr = hov ? 0.26f : 0.18f; fg = hov ? 0.20f : 0.13f; fb = hov ? 0.07f : 0.05f;
		tc = RKColor(255, 236, 155, 255);
	}
	else if (hov)
	{
		br = 0.38f; bg = 0.68f; bb = 0.78f;
		fr = 0.11f; fg = 0.18f; fb = 0.24f;
	}

	RKRect(x, y, w, h, br, bg, bb, 0.96f);
	RKRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fr, fg, fb, 0.95f);
	RKTextCentered(x + w * 0.5f, y + (h - 9.0f) * 0.5f, text, tc, (style == 1 || style == 2));
}

static const char* GetClassShortName(BYTE c, DWORD* outColor)
{
	if (c >= 48)
	{
		if (outColor) *outColor = RKColor(215, 155, 255, 255);
		return "MG";
	}
	if (c >= 32)
	{
		if (outColor) *outColor = RKColor(135, 235, 165, 255);
		return "ELF";
	}
	if (c >= 16)
	{
		if (outColor) *outColor = RKColor(255, 130, 130, 255);
		return "DK";
	}
	if (outColor) *outColor = RKColor(120, 215, 255, 255);
	return "DW";
}

static void DrawRankIconQuad(GLuint texId, float x, float y, float w, float h, float u0, float u1, float r, float g, float b, float a)
{
	if (texId == 0) return;

	float rx = x * g_fScreenRate_x;
	float ry = (float)WindowHeight - y * g_fScreenRate_y;
	float rw = w * g_fScreenRate_x;
	float rh = h * g_fScreenRate_y;

	EnableAlphaTest(true);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texId);
	glColor4f(r, g, b, a);

	glBegin(GL_QUADS);
	glTexCoord2f(u0, 0.0f); glVertex2f(rx,      ry);
	glTexCoord2f(u1, 0.0f); glVertex2f(rx + rw, ry);
	glTexCoord2f(u1, 1.0f); glVertex2f(rx + rw, ry - rh);
	glTexCoord2f(u0, 1.0f); glVertex2f(rx,      ry - rh);
	glEnd();

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

CRankingUI::CRankingUI()
{
	this->m_Open = false;
	this->m_Tab = 0;
	this->m_ClassFilter = 0;
	this->m_DataType = 0;
	this->m_Count = 0;
	this->m_MyWCoin = 0;
	memset(this->m_Entries, 0, sizeof(this->m_Entries));
	this->m_StatusTick = 0;
	this->m_StatusGood = true;
	this->m_StatusText[0] = 0;
	this->m_ConfirmVipIdx = -1;
	this->m_TexRankCS = 0;
}

CRankingUI::~CRankingUI()
{
}

void CRankingUI::LoadImages()
{
	if (this->m_TexRankCS != 0) return;

	int w = 0, h = 0, ch = 0;
	unsigned char* data = stbi_load("Data\\Interface\\RankingCS.png", &w, &h, &ch, 4);
	if (data && w > 0 && h > 0)
	{
		GLuint tex = 0;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);

		int mw = w, mh = h;
		unsigned char* cur = data;
		for (int level = 0; ; level++)
		{
			glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, mw, mh, 0, GL_RGBA, GL_UNSIGNED_BYTE, cur);
			if (mw == 1 && mh == 1) break;
			int nw = (mw > 1) ? (mw / 2) : 1;
			int nh = (mh > 1) ? (mh / 2) : 1;
			unsigned char* dst = (unsigned char*)malloc((size_t)nw * nh * 4);
			if (!dst) break;
			for (int y = 0; y < nh; y++)
			{
				for (int x = 0; x < nw; x++)
				{
					int sx = x * 2, sy = y * 2;
					int sx1 = (sx + 1 < mw) ? sx + 1 : sx;
					int sy1 = (sy + 1 < mh) ? sy + 1 : sy;
					const unsigned char* p00 = cur + ((sy * mw + sx) * 4);
					const unsigned char* p10 = cur + ((sy * mw + sx1) * 4);
					const unsigned char* p01 = cur + ((sy1 * mw + sx) * 4);
					const unsigned char* p11 = cur + ((sy1 * mw + sx1) * 4);
					unsigned char* out = dst + ((y * nw + x) * 4);
					for (int c = 0; c < 4; c++)
					{
						out[c] = (unsigned char)((p00[c] + p10[c] + p01[c] + p11[c] + 2) >> 2);
					}
				}
			}
			if (cur != data) free(cur);
			cur = dst;
			mw = nw;
			mh = nh;
		}
		if (cur != data) free(cur);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		this->m_TexRankCS = (unsigned int)tex;
		stbi_image_free(data);
	}
}

void CRankingUI::Toggle()
{
	if (this->m_Open && this->m_Tab != 3)
	{
		this->m_Open = false;
	}
	else
	{
		this->Open(this->m_Tab == 3 ? 0 : this->m_Tab);
	}
}

void CRankingUI::ToggleVipShop()
{
	if (this->m_Open && this->m_Tab == 3)
	{
		this->m_Open = false;
	}
	else
	{
		this->Open(3);
	}
}

void CRankingUI::Open(int tab)
{
	this->m_Open = true;
	this->m_Tab = tab;
	if (tab == 0) this->RequestRanking(0);
	else if (tab == 1) this->RequestRanking((BYTE)(2 + this->m_ClassFilter));
	else if (tab == 2) this->RequestRanking(1);
	else this->RequestRanking(0); // refresh WCoin balance
}

void CRankingUI::RequestRanking(BYTE type)
{
	CG_RANKING_REQ pMsg;
	pMsg.header.set(0x98, 0x08, sizeof(pMsg));
	pMsg.type = type;
	gProtocol.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CRankingUI::RequestBuyVip(WORD days)
{
	CG_BUY_VIP_REQ pMsg;
	pMsg.header.set(0x98, 0x09, sizeof(pMsg));
	pMsg.days = days;
	gProtocol.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CRankingUI::OnRankingRecv(BYTE* lpMsg)
{
	GC_RANKING_ANS* p = (GC_RANKING_ANS*)lpMsg;
	this->m_DataType = p->type;
	this->m_Count = (p->count > 10) ? 10 : p->count;
	this->m_MyWCoin = p->myWCoin;
	memcpy(this->m_Entries, p->entries, sizeof(this->m_Entries));

	// Auto-open window if user typed /top or /ranking in chat
	if (!this->m_Open)
	{
		this->m_Open = true;
		if (p->type == 1) this->m_Tab = 2;
		else if (p->type >= 2 && p->type <= 5) { this->m_Tab = 1; this->m_ClassFilter = p->type - 2; }
		else this->m_Tab = 0;
	}
}

void CRankingUI::OnBuyVipRecv(BYTE* lpMsg)
{
	GC_BUY_VIP_ANS* p = (GC_BUY_VIP_ANS*)lpMsg;
	this->m_MyWCoin = p->remainWCoin;
	this->m_StatusTick = GetTickCount();

	if (p->result == 1)
	{
		this->m_StatusGood = true;
		sprintf_s(this->m_StatusText, sizeof(this->m_StatusText), "VIP %u Day(s) activated! You can now enter Server VIP.", p->days);
	}
	else if (p->result == 2)
	{
		this->m_StatusGood = false;
		sprintf_s(this->m_StatusText, sizeof(this->m_StatusText), "Not enough WCoin! Balance: %u WCoin.", p->remainWCoin);
	}
	else
	{
		this->m_StatusGood = false;
		strcpy_s(this->m_StatusText, sizeof(this->m_StatusText), "VIP purchase failed. Please try again.");
	}
}

void CRankingUI::Render()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		this->m_Open = false;
		return;
	}

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTC = SetTextColor;
	const DWORD oldBG = SetBackgroundTextColor;

	// ---- Collapsible Top-Left HUD Buttons (Slot 9: TOP, Slot 10: VIP) ----
	if (!HudIconBarCollapsed())
	{
		if (this->m_TexRankCS == 0) this->LoadImages();

		float btnW, btnH, topX, vipX, by;
		GetRankingHudLayout(&btnW, &btnH, &topX, &vipX, &by);

		bool hovTop = RKHit(topX, by, btnW, btnH);
		bool actTop = (this->m_Open && this->m_Tab != 3);
		bool hovVip = RKHit(vipX, by, btnW, btnH);
		bool actVip = (this->m_Open && this->m_Tab == 3);

		if (this->m_TexRankCS != 0)
		{
			float ct = (actTop || hovTop) ? 1.0f : 0.90f;
			DrawRankIconQuad((GLuint)this->m_TexRankCS, topX, by, btnW, btnH, actTop ? 0.25f : 0.00f, actTop ? 0.50f : 0.25f, ct, ct, ct, 0.98f);

			float cv = (actVip || hovVip) ? 1.0f : 0.90f;
			DrawRankIconQuad((GLuint)this->m_TexRankCS, vipX, by, btnW, btnH, actVip ? 0.75f : 0.50f, actVip ? 1.00f : 0.75f, cv, cv, cv, 0.98f);
		}
		else
		{
			RKRect(topX, by, btnW, btnH, 0.25f, 0.34f, 0.42f, 0.96f);
			RKRect(topX + 1.0f, by + 1.0f, btnW - 2.0f, btnH - 2.0f, 0.06f, 0.085f, 0.115f, 0.94f);
			RKTextCentered(topX + btnW * 0.5f, by + 6.0f, "TOP", RKColor(160, 235, 250, 255), true);

			RKRect(vipX, by, btnW, btnH, 0.68f, 0.54f, 0.18f, 0.96f);
			RKRect(vipX + 1.0f, by + 1.0f, btnW - 2.0f, btnH - 2.0f, 0.12f, 0.095f, 0.04f, 0.94f);
			RKTextCentered(vipX + btnW * 0.5f, by + 6.0f, "VIP", RKColor(255, 226, 115, 255), true);
		}

		if (hovTop || hovVip)
		{
			const char* tip = hovVip ? "Buy VIP Shop (F7)" : "Ranking (F6)";
			float tx = hovVip ? vipX : topX;
			RKSetFont(g_hFont);
			float tw = RKMeasureW(tip);
			float ty = by + btnH + 4.0f;
			RKRect(tx, ty, tw + 10.0f, 15.0f, 0.23f, 0.27f, 0.32f, 0.95f);
			RKRect(tx + 1.0f, ty + 1.0f, tw + 8.0f, 13.0f, 0.055f, 0.075f, 0.098f, 0.92f);
			RKText(tx + 5.0f, ty + 2.5f, tip, RKColor(230, 242, 248, 255), false);
		}
	}

	if (!this->m_Open)
	{
		RKSetFont(oldFont ? oldFont : g_hFont);
		SetTextColor = oldTC;
		SetBackgroundTextColor = oldBG;
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		DisableAlphaBlend();
		return;
	}

	// ---- Main Centered Cold Steel Window ----
	RKRect(RK_PX - 2.0f, RK_PY - 2.0f, RK_PW + 4.0f, RK_PH + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
	RKRect(RK_PX, RK_PY, RK_PW, RK_PH, 0.23f, 0.27f, 0.32f, 0.98f);
	RKRect(RK_PX + 1.0f, RK_PY + 1.0f, RK_PW - 2.0f, RK_PH - 2.0f, 0.045f, 0.062f, 0.082f, 0.96f);

	// Title Bar
	RKRect(RK_PX + 1.0f, RK_PY + 1.0f, RK_PW - 2.0f, 24.0f, 0.075f, 0.105f, 0.140f, 0.98f);
	RKRect(RK_PX + 1.0f, RK_PY + 25.0f, RK_PW - 2.0f, 1.0f, 0.435f, 0.714f, 0.769f, 0.85f);
	RKText(RK_PX + 10.0f, RK_PY + 7.0f, "RANKING & VIP SHOP", RKColor(150, 232, 248, 255), true);

	char wcNum[24], wcBuf[48];
	RKFormatComma(this->m_MyWCoin, wcNum, sizeof(wcNum));
	sprintf_s(wcBuf, sizeof(wcBuf), "WCoin: %s", wcNum);
	RKText(RK_PX + 178.0f, RK_PY + 7.5f, wcBuf, RKColor(255, 222, 105, 255), true);

	// Close button [X]
	RKDrawButton(RK_PX + RK_PW - 24.0f, RK_PY + 4.0f, 18.0f, 17.0f, "X", 0, true);

	// 4 Tabs
	const char* tabNames[4] = { "Top Level", "Top Class", "Top Guild", "VIP Shop" };
	const float tabY = RK_PY + 31.0f;
	const float tabW = (RK_PW - 24.0f - 12.0f) / 4.0f;
	for (int i = 0; i < 4; i++)
	{
		float tx = RK_PX + 12.0f + i * (tabW + 4.0f);
		int st = (this->m_Tab == i) ? (i == 3 ? 2 : 1) : 0;
		RKDrawButton(tx, tabY, tabW, 19.0f, tabNames[i], st, true);
	}

	const float bodyX = RK_PX + 12.0f;
	const float bodyW = RK_PW - 24.0f;

	if (this->m_Tab == 3)
	{
		// ---- VIP SHOP TAB ----
		float y = RK_PY + 58.0f;
		RKTextCentered(RK_PX + RK_PW * 0.5f, y, "Upgrade to VIP to access Server VIP (Non-PK, x2 EXP, High Drop)", RKColor(180, 206, 222, 255), false);
		y += 18.0f;

		struct VipPkg { WORD days; const char* title; const char* desc; DWORD cost; };
		const VipPkg pkgs[3] = {
			{ 1,  "1 DAY VIP PASS",   "Non-PK | x2 EXP | +Drop | 65% Soul/Life", 1000 },
			{ 7,  "7 DAYS VIP PASS",  "Save 14% - Full access to Server VIP",     6000 },
			{ 30, "30 DAYS VIP PASS", "Best Value - Save 33% for a full month",  20000 }
		};

		for (int i = 0; i < 3; i++)
		{
			const float cardH = 56.0f;
			RKRect(bodyX, y, bodyW, cardH, 0.24f, 0.30f, 0.36f, 0.92f);
			RKRect(bodyX + 1.0f, y + 1.0f, bodyW - 2.0f, cardH - 2.0f, 0.065f, 0.090f, 0.120f, 0.95f);
			RKRect(bodyX + 1.0f, y + 1.0f, 3.0f, cardH - 2.0f, 0.92f, 0.74f, 0.25f, 0.95f);

			RKText(bodyX + 12.0f, y + 8.0f, pkgs[i].title, RKColor(255, 226, 118, 255), true);
			RKText(bodyX + 12.0f, y + 23.0f, pkgs[i].desc, RKColor(175, 198, 214, 255), false);

			char cNum[24], cBuf[40];
			RKFormatComma(pkgs[i].cost, cNum, sizeof(cNum));
			sprintf_s(cBuf, sizeof(cBuf), "Price: %s WCoin", cNum);
			RKText(bodyX + 12.0f, y + 38.0f, cBuf, RKColor(135, 232, 248, 255), true);

			bool canAfford = (this->m_MyWCoin >= pkgs[i].cost);
			RKDrawButton(bodyX + bodyW - 92.0f, y + 16.0f, 82.0f, 24.0f, "Buy VIP", 2, canAfford);

			y += cardH + 8.0f;
		}

		// Status message banner
		if (this->m_StatusText[0] != 0 && (GetTickCount() - this->m_StatusTick) < 6000)
		{
			DWORD sc = this->m_StatusGood ? RKColor(120, 240, 155, 255) : RKColor(255, 115, 115, 255);
			RKTextCentered(RK_PX + RK_PW * 0.5f, RK_PY + RK_PH - 36.0f, this->m_StatusText, sc, true);
		}
	}
	else
	{
		// ---- RANKING TABLE TABS (Top Level / Top Class / Top Guild) ----
		float tableTop = RK_PY + 56.0f;

		// Class sub-filter pills when in Top Class tab
		if (this->m_Tab == 1)
		{
			const char* clsLabels[4] = { "Dark Wizard", "Dark Knight", "Fairy Elf", "Magic Glad." };
			float cw = (bodyW - 12.0f) / 4.0f;
			for (int c = 0; c < 4; c++)
			{
				float cx = bodyX + c * (cw + 4.0f);
				RKDrawButton(cx, tableTop, cw, 17.0f, clsLabels[c], (this->m_ClassFilter == c) ? 1 : 0, true);
			}
			tableTop += 22.0f;
		}

		// Column header
		RKRect(bodyX, tableTop, bodyW, 18.0f, 0.10f, 0.14f, 0.19f, 0.96f);
		RKRect(bodyX, tableTop + 18.0f, bodyW, 1.0f, 0.25f, 0.35f, 0.45f, 0.85f);
		RKText(bodyX + 10.0f, tableTop + 4.5f, "RANK", RKColor(145, 224, 242, 255), true);
		if (this->m_Tab == 2)
		{
			RKText(bodyX + 68.0f, tableTop + 4.5f, "GUILD NAME", RKColor(145, 224, 242, 255), true);
			RKText(bodyX + bodyW - 76.0f, tableTop + 4.5f, "SCORE", RKColor(145, 224, 242, 255), true);
		}
		else
		{
			RKText(bodyX + 68.0f, tableTop + 4.5f, "CHARACTER", RKColor(145, 224, 242, 255), true);
			RKText(bodyX + 225.0f, tableTop + 4.5f, "CLASS", RKColor(145, 224, 242, 255), true);
			RKText(bodyX + bodyW - 68.0f, tableTop + 4.5f, "LEVEL", RKColor(145, 224, 242, 255), true);
		}

		float rowY = tableTop + 21.0f;
		const float rowH = (this->m_Tab == 1) ? 18.5f : 20.5f;

		if (this->m_Count == 0)
		{
			RKTextCentered(RK_PX + RK_PW * 0.5f, rowY + 60.0f, "No ranking entries found.", RKColor(150, 168, 182, 255), false);
		}
		else
		{
			for (int i = 0; i < this->m_Count; i++)
			{
				if (i % 2 == 0)
				{
					RKRect(bodyX, rowY, bodyW, rowH - 1.0f, 0.065f, 0.088f, 0.115f, 0.80f);
				}

				DWORD rankCol = RKColor(185, 200, 212, 255);
				if (i == 0) rankCol = RKColor(255, 215, 70, 255);      // Gold
				else if (i == 1) rankCol = RKColor(215, 225, 235, 255); // Silver
				else if (i == 2) rankCol = RKColor(225, 155, 95, 255);  // Bronze

				char rBuf[12];
				sprintf_s(rBuf, sizeof(rBuf), "#%d", i + 1);
				RKText(bodyX + 14.0f, rowY + 4.5f, rBuf, rankCol, (i < 3));
				RKText(bodyX + 68.0f, rowY + 4.5f, this->m_Entries[i].name, (i == 0) ? RKColor(255, 238, 165, 255) : RKColor(232, 240, 248, 255), (i < 3));

				if (this->m_Tab == 2)
				{
					char vBuf[24];
					RKFormatComma(this->m_Entries[i].value, vBuf, sizeof(vBuf));
					RKText(bodyX + bodyW - 76.0f, rowY + 4.5f, vBuf, RKColor(255, 215, 105, 255), true);
				}
				else
				{
					DWORD clsCol = RKColor(180, 210, 230, 255);
					const char* clsName = GetClassShortName(this->m_Entries[i].extra, &clsCol);
					RKText(bodyX + 230.0f, rowY + 4.5f, clsName, clsCol, true);

					char lvBuf[24];
					sprintf_s(lvBuf, sizeof(lvBuf), "Lv.%u", this->m_Entries[i].value);
					RKText(bodyX + bodyW - 68.0f, rowY + 4.5f, lvBuf, RKColor(135, 235, 185, 255), true);
				}

				rowY += rowH;
			}
		}
	}

	// Footer Refresh + Hint
	RKDrawButton(bodyX, RK_PY + RK_PH - 24.0f, 64.0f, 17.0f, "Refresh", 0, true);
	RKText(bodyX + 74.0f, RK_PY + RK_PH - 20.0f, "Hotkeys: F6 (Ranking) | F7 (VIP Shop) | /top, /buyvip", RKColor(135, 155, 170, 255), false);

	// ---- Confirm VIP Purchase Modal ----
	if (this->m_ConfirmVipIdx >= 0 && this->m_ConfirmVipIdx < 3)
	{
		const char* vTitles[3] = { "1 DAY VIP PASS", "7 DAYS VIP PASS", "30 DAYS VIP PASS" };
		const DWORD vCosts[3] = { 1000, 6000, 20000 };

		RKRect(RK_PX, RK_PY, RK_PW, RK_PH, 0.0f, 0.0f, 0.0f, 0.62f);

		const float mw = 264.0f, mh = 118.0f;
		const float mx = RK_PX + (RK_PW - mw) * 0.5f;
		const float my = RK_PY + (RK_PH - mh) * 0.5f;

		RKRect(mx, my, mw, mh, 0.92f, 0.74f, 0.25f, 0.98f);
		RKRect(mx + 1.0f, my + 1.0f, mw - 2.0f, mh - 2.0f, 0.055f, 0.080f, 0.112f, 0.98f);
		RKRect(mx + 1.0f, my + 1.0f, mw - 2.0f, 22.0f, 0.11f, 0.095f, 0.055f, 0.98f);
		RKTextCentered(mx + mw * 0.5f, my + 7.0f, "CONFIRM VIP PURCHASE", RKColor(255, 226, 120, 255), true);

		RKTextCentered(mx + mw * 0.5f, my + 34.0f, vTitles[this->m_ConfirmVipIdx], RKColor(255, 240, 175, 255), true);

		char cNum[24], cBuf[64];
		RKFormatComma(vCosts[this->m_ConfirmVipIdx], cNum, sizeof(cNum));
		sprintf_s(cBuf, sizeof(cBuf), "Price: %s WCoin", cNum);
		RKTextCentered(mx + mw * 0.5f, my + 52.0f, cBuf, RKColor(135, 232, 248, 255), true);

		RKDrawButton(mx + 22.0f, my + 82.0f, 100.0f, 24.0f, "Confirm Buy", 2, true);
		RKDrawButton(mx + mw - 122.0f, my + 82.0f, 100.0f, 24.0f, "Cancel", 0, true);
	}

	RKSetFont(oldFont ? oldFont : g_hFont);
	SetTextColor = oldTC;
	SetBackgroundTextColor = oldBG;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

void CRankingUI::UpdateMouse()
{
	if (SceneFlag != MAIN_SCENE || !Hero) return;

	// 1. Collapsible HUD Buttons (Slot 9: TOP, Slot 10: VIP)
	if (!HudIconBarCollapsed())
	{
		float btnW, btnH, topX, vipX, by;
		GetRankingHudLayout(&btnW, &btnH, &topX, &vipX, &by);
		if (RKHit(topX, by, btnW, btnH))
		{
			MouseOnWindow = true;
			if (MouseLButtonPush)
			{
				MouseLButtonPush = false;
				MouseLButton = false;
				MouseLButtonPop = false;
				PlayBuffer(25, 0, 0);
				this->Toggle();
			}
			return;
		}
		if (RKHit(vipX, by, btnW, btnH))
		{
			MouseOnWindow = true;
			if (MouseLButtonPush)
			{
				MouseLButtonPush = false;
				MouseLButton = false;
				MouseLButtonPop = false;
				PlayBuffer(25, 0, 0);
				this->ToggleVipShop();
			}
			return;
		}
	}

	if (!this->m_Open || !RKHit(RK_PX, RK_PY, RK_PW, RK_PH)) return;

	MouseOnWindow = true;
	if (!MouseLButtonPush) return;

	MouseLButtonPush = false;
	MouseLButton = false;
	MouseLButtonPop = false;
	PlayBuffer(25, 0, 0);

	// Confirm VIP Modal intercepts clicks first
	if (this->m_ConfirmVipIdx >= 0 && this->m_ConfirmVipIdx < 3)
	{
		const WORD vDays[3] = { 1, 7, 30 };
		const float mw = 264.0f, mh = 118.0f;
		const float mx = RK_PX + (RK_PW - mw) * 0.5f;
		const float my = RK_PY + (RK_PH - mh) * 0.5f;

		if (RKHit(mx + 22.0f, my + 82.0f, 100.0f, 24.0f))
		{
			int idx = this->m_ConfirmVipIdx;
			this->m_ConfirmVipIdx = -1;
			this->RequestBuyVip(vDays[idx]);
			return;
		}
		if (RKHit(mx + mw - 122.0f, my + 82.0f, 100.0f, 24.0f))
		{
			this->m_ConfirmVipIdx = -1;
			return;
		}
		return;
	}

	// Close [X]
	if (RKHit(RK_PX + RK_PW - 24.0f, RK_PY + 4.0f, 18.0f, 17.0f))
	{
		this->m_ConfirmVipIdx = -1;
		this->m_Open = false;
		return;
	}

	// 4 Tabs
	const float tabY = RK_PY + 31.0f;
	const float tabW = (RK_PW - 24.0f - 12.0f) / 4.0f;
	for (int i = 0; i < 4; i++)
	{
		float tx = RK_PX + 12.0f + i * (tabW + 4.0f);
		if (RKHit(tx, tabY, tabW, 19.0f))
		{
			this->Open(i);
			return;
		}
	}

	const float bodyX = RK_PX + 12.0f;
	const float bodyW = RK_PW - 24.0f;

	// Refresh button
	if (RKHit(bodyX, RK_PY + RK_PH - 24.0f, 64.0f, 17.0f))
	{
		this->Open(this->m_Tab);
		return;
	}

	// Class filter pills
	if (this->m_Tab == 1)
	{
		float tableTop = RK_PY + 56.0f;
		float cw = (bodyW - 12.0f) / 4.0f;
		for (int c = 0; c < 4; c++)
		{
			float cx = bodyX + c * (cw + 4.0f);
			if (RKHit(cx, tableTop, cw, 17.0f))
			{
				this->m_ClassFilter = c;
				this->RequestRanking((BYTE)(2 + c));
				return;
			}
		}
	}
	else if (this->m_Tab == 3)
	{
		// VIP Shop Buy buttons
		const DWORD costs[3] = { 1000, 6000, 20000 };
		float y = RK_PY + 76.0f;
		for (int i = 0; i < 3; i++)
		{
			if (RKHit(bodyX + bodyW - 92.0f, y + 16.0f, 82.0f, 24.0f))
			{
				if (this->m_MyWCoin >= costs[i])
				{
					this->m_ConfirmVipIdx = i;
				}
				return;
			}
			y += 64.0f;
		}
	}
}
