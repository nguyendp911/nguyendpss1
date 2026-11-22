#include "stdafx.h"
#include "JewelBankUI.h"
#include "Interface.h"
#include "Font.h"
#include "ItemPost.h"
#include "Protocol.h"
#include "stb.h"
#include <math.h>
#include <stdio.h>

CJewelBankUI gJewelBankUI;

// Panel geometry in 640x480 virtual space (Cold Steel HD theme)
static const float JB_PW = 382.0f;
static const float JB_PH = 334.0f;
static const float JB_PX = (640.0f - JB_PW) * 0.5f;
static const float JB_PY = 56.0f;
static const float JB_ROW_Y = 50.0f;
static const float JB_ROW_H = 42.0f;
static const float JB_BTN_W = 44.0f;
static const float JB_BTN_H = 19.0f;
static const float JB_BTN_GAP = 4.0f;
static const float JB_ALL_Y = 268.0f;
static const float JB_RESULT_Y = 304.0f;

// Same order as GameServer: Chaos, Bless, Soul, Life, Creation
static const int s_JewelItem[JEWELBANK_TYPES] = {
	GET_ITEM(12, 15),
	GET_ITEM(14, 13),
	GET_ITEM(14, 14),
	GET_ITEM(14, 16),
	GET_ITEM(14, 22)
};

static const char* s_JewelName[JEWELBANK_TYPES] = {
	"Jewel of Chaos",
	"Jewel of Bless",
	"Jewel of Soul",
	"Jewel of Life",
	"Jewel of Creation"
};

static void GetJewelBankHudLayout(float* btnW, float* btnH, float* btnX, float* y)
{
	const float asp = (g_fScreenRate_x > 0.0f) ? (g_fScreenRate_y / g_fScreenRate_x) : 0.75f;
	*btnH = 22.0f;
	*btnW = (*btnH) * asp;
	*y = 8.0f;
	// Pinned slot 5: Camera(0,1), Combo(2), SmartAuto(3,4), JewelBank(5)
	*btnX = 8.0f + (*btnW + 3.0f) * 5.0f;
}

static bool JBHit(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static DWORD JBColor(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static void JBSetFont(HFONT f)
{
	if (f) SelectObject(m_hFontDC, f);
}

static float JBMeasureW(const char* text)
{
	if (!text || !text[0]) return 0.0f;
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (float)sz.cx / (g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f);
}

static void JBRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

static void JBText(float x, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	JBSetFont(bold ? g_hFontBold : g_hFont);
	EnableAlphaTest(true);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	SetBackgroundTextColor = 0;
	SetTextColor = JBColor(0, 0, 0, 220);
	RenderText((int)x + 1, (int)y + 1, (char*)text, 0, RT3_SORT_LEFT, NULL);
	SetTextColor = color;
	RenderText((int)x, (int)y, (char*)text, 0, RT3_SORT_LEFT, NULL);
}

static void JBTextCentered(float cx, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	JBSetFont(bold ? g_hFontBold : g_hFont);
	float w = JBMeasureW(text);
	JBText(cx - w * 0.5f, y, text, color, bold);
}

static void JBDrawButton(float x, float y, float w, float h, const char* text, int style, bool enabled)
{
	bool hov = enabled && JBHit(x, y, w, h);

	float br = 0.24f, bg = 0.29f, bb = 0.35f;
	float fr = 0.09f, fg = 0.125f, fb = 0.165f;
	DWORD tc = JBColor(220, 234, 244, 255);

	if (!enabled)
	{
		br = 0.16f; bg = 0.19f; bb = 0.22f;
		fr = 0.06f; fg = 0.075f; fb = 0.095f;
		tc = JBColor(105, 118, 130, 255);
	}
	else if (style == 1) // Primary cyan-steel (Deposit)
	{
		br = hov ? 0.48f : 0.32f; bg = hov ? 0.84f : 0.66f; bb = hov ? 0.94f : 0.76f;
		fr = hov ? 0.13f : 0.09f; fg = hov ? 0.30f : 0.22f; fb = hov ? 0.38f : 0.29f;
		tc = JBColor(235, 252, 255, 255);
	}
	else if (style == 2) // Emerald accent (Deposit All)
	{
		br = hov ? 0.34f : 0.22f; bg = hov ? 0.82f : 0.66f; bb = hov ? 0.56f : 0.44f;
		fr = hov ? 0.09f : 0.06f; fg = hov ? 0.30f : 0.22f; fb = hov ? 0.19f : 0.14f;
		tc = JBColor(225, 255, 238, 255);
	}
	else if (hov)
	{
		br = 0.42f; bg = 0.70f; bb = 0.78f;
		fr = 0.14f; fg = 0.20f; fb = 0.26f;
		tc = JBColor(245, 252, 255, 255);
	}

	JBRect(x, y, w, h, br, bg, bb, 0.96f);
	JBRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fr, fg, fb, 0.96f);
	JBTextCentered(x + w * 0.5f, y + (h - 9.0f) * 0.5f, text, tc, style != 0);
}

static void DrawJewelIconQuad(GLuint tex, float x, float y, float w, float h, float u0, float u1, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex);
	glColor4f(r, g, b, a);

	const float sx0 = x * g_fScreenRate_x;
	const float sx1 = (x + w) * g_fScreenRate_x;
	const float sy0 = (float)WindowHeight - y * g_fScreenRate_y;
	const float sy1 = (float)WindowHeight - (y + h) * g_fScreenRate_y;

	glBegin(GL_QUADS);
	glTexCoord2f(u0, 0.0f); glVertex2f(sx0, sy0);
	glTexCoord2f(u1, 0.0f); glVertex2f(sx1, sy0);
	glTexCoord2f(u1, 1.0f); glVertex2f(sx1, sy1);
	glTexCoord2f(u0, 1.0f); glVertex2f(sx0, sy1);
	glEnd();

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

static void FormatNumber(unsigned long long v, char* out, int size)
{
	char raw[32];
	sprintf_s(raw, sizeof(raw), "%llu", v);
	int len = (int)strlen(raw);
	int o = 0;

	for (int i = 0; i < len && o < size - 2; i++)
	{
		out[o++] = raw[i];
		int left = len - i - 1;
		if (left > 0 && (left % 3) == 0 && o < size - 2)
		{
			out[o++] = ',';
		}
	}
	out[o] = 0;
}

static float ButtonX(int n)
{
	float rowW = JB_PW - 20.0f;
	float first = JB_PX + 10.0f + rowW - 6.0f - (4.0f * JB_BTN_W + 3.0f * JB_BTN_GAP);
	return first + n * (JB_BTN_W + JB_BTN_GAP);
}

CJewelBankUI::CJewelBankUI()
{
	this->m_Open = false;
	this->m_Enable = true;
	this->m_TexBtnCS = 0;
	memset(this->m_Bank, 0, sizeof(this->m_Bank));
	memset(this->m_Inventory, 0, sizeof(this->m_Inventory));
	memset(this->m_Items, 0, sizeof(this->m_Items));
	for (int n = 0; n < JEWELBANK_TYPES; n++)
	{
		this->m_Items[n].Type = (short)s_JewelItem[n];
		this->m_Items[n].Durability = 1;
	}
	this->m_WaitTick = 0;
	this->m_RefreshTick = 0;
	this->m_ResultTick = 0;
	this->m_ResultGood = false;
	this->m_ResultText[0] = 0;
}

void CJewelBankUI::LoadImages()
{
	if (this->m_TexBtnCS != 0) return;

	int w = 0, h = 0, ch = 0;
	unsigned char* data = stbi_load("Data\\Interface\\JewelBankCS.png", &w, &h, &ch, 4);
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
		this->m_TexBtnCS = (unsigned int)tex;
	}
	if (data) stbi_image_free(data);
}

void CJewelBankUI::Toggle()
{
	if (SceneFlag != MAIN_SCENE || !Hero) return;

	this->m_Open = !this->m_Open;
	PlayBuffer(25, 0, 0);

	if (this->m_Open)
	{
		this->m_ResultText[0] = 0;
		this->SendInfoRequest();
	}
}

void CJewelBankUI::Close()
{
	this->m_Open = false;
}

void CJewelBankUI::SendInfoRequest()
{
	PMSG_JEWELBANK_INFO_REQ pMsg;
	pMsg.header.set(0x98, 0x20, sizeof(pMsg));
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CJewelBankUI::SendDeposit(BYTE jewel)
{
	DWORD now = GetTickCount();
	if (this->m_WaitTick != 0 && (now - this->m_WaitTick) < 350) return;
	this->m_WaitTick = now;

	PMSG_JEWELBANK_DEPOSIT_REQ pMsg;
	pMsg.header.set(0x98, 0x21, sizeof(pMsg));
	pMsg.jewel = jewel;
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CJewelBankUI::SendWithdraw(BYTE jewel, BYTE count)
{
	DWORD now = GetTickCount();
	if (this->m_WaitTick != 0 && (now - this->m_WaitTick) < 350) return;
	this->m_WaitTick = now;

	PMSG_JEWELBANK_WITHDRAW_REQ pMsg;
	pMsg.header.set(0x98, 0x22, sizeof(pMsg));
	pMsg.jewel = jewel;
	pMsg.count = count;
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CJewelBankUI::SetResult(const char* text, bool good)
{
	strcpy_s(this->m_ResultText, sizeof(this->m_ResultText), text);
	this->m_ResultGood = good;
	this->m_ResultTick = GetTickCount();
}

void CJewelBankUI::OnStateRecv(BYTE* lpMsg)
{
	PMSG_JEWELBANK_STATE_ANS* p = (PMSG_JEWELBANK_STATE_ANS*)lpMsg;

	this->m_Enable = (p->enable != 0);

	for (int n = 0; n < JEWELBANK_TYPES; n++)
	{
		this->m_Bank[n] = p->bank[n];
		this->m_Inventory[n] = p->inventory[n];
		memset(&this->m_Items[n], 0, sizeof(ITEM));
		this->m_Items[n].Type = (short)s_JewelItem[n];
		this->m_Items[n].Durability = 1;
	}

	if (p->result == 1)
	{
		this->m_RefreshTick = GetTickCount() + 700;
	}

	switch (p->result)
	{
		case 1: this->SetResult("Transaction completed!", true); break;
		case 2: this->SetResult("Inventory is full.", false); break;
		case 3: this->SetResult("Not enough jewels in bank.", false); break;
		case 4: this->SetResult("No jewels in inventory to deposit.", false); break;
		case 5: this->SetResult("Busy - please wait a moment.", false); break;
		case 6: this->SetResult("Jewel Bank is currently closed.", false); break;
		case 7: this->SetResult("Operation failed.", false); break;
		default: break;
	}
}

void CJewelBankUI::Render()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		this->m_Open = false;
		return;
	}

	if (this->m_TexBtnCS == 0)
	{
		this->LoadImages();
	}

	DWORD now = GetTickCount();

	if (this->m_RefreshTick != 0 && now >= this->m_RefreshTick)
	{
		this->m_RefreshTick = 0;
		if (this->m_Open)
		{
			this->SendInfoRequest();
		}
	}

	// ---- Top-Left Cold Steel HUD Button (Pinned slot 5) ----
	float btnW, btnH, bx, by;
	GetJewelBankHudLayout(&btnW, &btnH, &bx, &by);
	bool hovBtn = JBHit(bx, by, btnW, btnH);

	if (this->m_TexBtnCS != 0)
	{
		float u0 = this->m_Open ? 0.50f : 0.00f;
		float u1 = this->m_Open ? 1.00f : 0.50f;
		float c = (this->m_Open || hovBtn) ? 1.0f : 0.90f;
		DrawJewelIconQuad((GLuint)this->m_TexBtnCS, bx, by, btnW, btnH, u0, u1, c, c, c, 0.98f);
	}
	else
	{
		JBRect(bx, by, btnW, btnH, 0.23f, 0.27f, 0.32f, 0.96f);
		JBRect(bx + 1.0f, by + 1.0f, btnW - 2.0f, btnH - 2.0f, 0.06f, 0.09f, 0.12f, 0.95f);
		JBTextCentered(bx + btnW * 0.5f, by + 6.0f, "JB", JBColor(135, 228, 248, 255), true);
	}

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTC = SetTextColor;
	const DWORD oldBG = SetBackgroundTextColor;

	if (hovBtn)
	{
		const char* tip = "Jewel Bank (J)";
		JBSetFont(g_hFont);
		float tw = JBMeasureW(tip);
		float ty = by + btnH + 4.0f;
		JBRect(bx, ty, tw + 10.0f, 15.0f, 0.23f, 0.27f, 0.32f, 0.95f);
		JBRect(bx + 1.0f, ty + 1.0f, tw + 8.0f, 13.0f, 0.055f, 0.075f, 0.098f, 0.92f);
		JBText(bx + 5.0f, ty + 2.5f, tip, JBColor(230, 242, 248, 255), false);
	}

	// ---- Main Cold Steel Jewel Bank Window ----
	if (this->m_Open)
	{
		// Outer shadow + Cold Steel metallic frame
		JBRect(JB_PX - 2.0f, JB_PY - 2.0f, JB_PW + 4.0f, JB_PH + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
		JBRect(JB_PX, JB_PY, JB_PW, JB_PH, 0.23f, 0.27f, 0.32f, 0.98f);
		JBRect(JB_PX + 1.0f, JB_PY + 1.0f, JB_PW - 2.0f, JB_PH - 2.0f, 0.055f, 0.075f, 0.098f, 0.96f);
		JBRect(JB_PX, JB_PY, JB_PW, 2.0f, 0.435f, 0.714f, 0.769f, 0.98f);

		// Header bar
		JBRect(JB_PX + 1.0f, JB_PY + 2.0f, JB_PW - 2.0f, 24.0f, 0.095f, 0.135f, 0.185f, 0.95f);
		JBRect(JB_PX + 1.0f, JB_PY + 26.0f, JB_PW - 2.0f, 1.0f, 0.23f, 0.27f, 0.32f, 0.92f);

		const float midX = JB_PX + JB_PW * 0.5f;
		JBText(JB_PX + 12.0f, JB_PY + 8.0f, "JEWEL BANK", JBColor(145, 228, 246, 255), true);
		JBText(JB_PX + 96.0f, JB_PY + 9.0f, "Account-wide shared vault  |  J: Open-Close", JBColor(135, 152, 168, 255), false);
		JBDrawButton(JB_PX + JB_PW - 22.0f, JB_PY + 5.0f, 16.0f, 16.0f, "X", 0, true);

		// Subtitle info
		JBTextCentered(midX, JB_PY + 33.0f, "Store and withdraw jewels safely across all characters", JBColor(175, 195, 210, 255), false);

		const float rowX = JB_PX + 10.0f;
		const float rowW = JB_PW - 20.0f;
		char line[96], num[32];

		for (int i = 0; i < JEWELBANK_TYPES; i++)
		{
			float y = JB_PY + JB_ROW_Y + JB_ROW_H * i;

			// Row card
			JBRect(rowX, y, rowW, JB_ROW_H - 4.0f, 0.18f, 0.22f, 0.28f, 0.90f);
			JBRect(rowX + 1.0f, y + 1.0f, rowW - 2.0f, JB_ROW_H - 6.0f, 0.07f, 0.095f, 0.125f, 0.94f);

			// 3D Jewel Icon Box
			float iconX = rowX + 5.0f;
			float iconY = y + 4.0f;
			float iconS = 30.0f;
			JBRect(iconX, iconY, iconS, iconS, 0.28f, 0.44f, 0.52f, 0.92f);
			JBRect(iconX + 1.0f, iconY + 1.0f, iconS - 2.0f, iconS - 2.0f, 0.035f, 0.05f, 0.07f, 0.96f);

			if (this->m_Items[i].Type > 0)
			{
				// MU 0.97k RenderItem3D anchors 1x1 jewels slightly below box center; lift Y to center cleanly
				gItemPost.RenderItemPreview3D(iconX + 1.0f, iconY - 4.5f, iconS - 2.0f, iconS - 2.0f, &this->m_Items[i]);
			}

			// Jewel name & balances
			JBText(rowX + 42.0f, y + 6.0f, s_JewelName[i], JBColor(235, 246, 255, 255), true);

			FormatNumber(this->m_Bank[i], num, sizeof(num));
			sprintf_s(line, sizeof(line), "Bank: %s   |   Bag: %d", num, (int)this->m_Inventory[i]);
			JBText(rowX + 42.0f, y + 20.0f, line, JBColor(155, 210, 228, 255), false);

			// Action buttons
			float by2 = y + 9.0f;
			JBDrawButton(ButtonX(0), by2, JB_BTN_W, JB_BTN_H, "Put", 1, this->m_Inventory[i] > 0);
			JBDrawButton(ButtonX(1), by2, JB_BTN_W, JB_BTN_H, "-1", 0, this->m_Bank[i] > 0);
			JBDrawButton(ButtonX(2), by2, JB_BTN_W, JB_BTN_H, "-10", 0, this->m_Bank[i] > 0);
			JBDrawButton(ButtonX(3), by2, JB_BTN_W, JB_BTN_H, "All", 0, this->m_Bank[i] > 0);
		}

		// Footer buttons
		JBDrawButton(midX - 118.0f, JB_PY + JB_ALL_Y, 150.0f, 22.0f, "Deposit All Jewels", 2, true);
		JBDrawButton(midX + 42.0f,  JB_PY + JB_ALL_Y, 76.0f,  22.0f, "Refresh", 0, true);

		// Status message bar
		if (this->m_ResultText[0] != 0 && (now - this->m_ResultTick) < 6000)
		{
			JBTextCentered(midX, JB_PY + JB_RESULT_Y, this->m_ResultText,
				this->m_ResultGood ? JBColor(120, 245, 165, 255) : JBColor(255, 125, 115, 255), true);
		}
	}

	JBSetFont(oldFont ? oldFont : g_hFont);
	SetTextColor = oldTC;
	SetBackgroundTextColor = oldBG;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	DisableAlphaBlend();
}

void CJewelBankUI::UpdateMouse()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		return;
	}

	// 1. Top-Left HUD Button (always visible)
	float btnW, btnH, bx, by;
	GetJewelBankHudLayout(&btnW, &btnH, &bx, &by);

	if (JBHit(bx, by, btnW, btnH))
	{
		MouseOnWindow = true;

		if (MouseLButtonPush)
		{
			MouseLButtonPush = false;
			MouseLButton = false;
			MouseLButtonPop = false;
			this->Toggle();
		}
		return;
	}

	if (!this->m_Open || !JBHit(JB_PX, JB_PY, JB_PW, JB_PH))
	{
		return;
	}

	MouseOnWindow = true;

	if (!MouseLButtonPush)
	{
		return;
	}

	MouseLButtonPush = false;
	MouseLButton = false;
	MouseLButtonPop = false;

	// Close button (X)
	if (JBHit(JB_PX + JB_PW - 22.0f, JB_PY + 5.0f, 16.0f, 16.0f))
	{
		PlayBuffer(25, 0, 0);
		this->m_Open = false;
		return;
	}

	// Row buttons
	for (int i = 0; i < JEWELBANK_TYPES; i++)
	{
		float by2 = JB_PY + JB_ROW_Y + JB_ROW_H * i + 9.0f;

		if (this->m_Inventory[i] > 0 && JBHit(ButtonX(0), by2, JB_BTN_W, JB_BTN_H))
		{
			PlayBuffer(25, 0, 0);
			this->SendDeposit((BYTE)i);
			return;
		}

		if (this->m_Bank[i] > 0)
		{
			if (JBHit(ButtonX(1), by2, JB_BTN_W, JB_BTN_H))
			{
				PlayBuffer(25, 0, 0);
				this->SendWithdraw((BYTE)i, 1);
				return;
			}
			if (JBHit(ButtonX(2), by2, JB_BTN_W, JB_BTN_H))
			{
				PlayBuffer(25, 0, 0);
				this->SendWithdraw((BYTE)i, 10);
				return;
			}
			if (JBHit(ButtonX(3), by2, JB_BTN_W, JB_BTN_H))
			{
				PlayBuffer(25, 0, 0);
				this->SendWithdraw((BYTE)i, 0);
				return;
			}
		}
	}

	const float midX = JB_PX + JB_PW * 0.5f;

	// Deposit All Jewels
	if (JBHit(midX - 118.0f, JB_PY + JB_ALL_Y, 150.0f, 22.0f))
	{
		PlayBuffer(25, 0, 0);
		this->SendDeposit(0xFF);
		return;
	}

	// Refresh
	if (JBHit(midX + 42.0f, JB_PY + JB_ALL_Y, 76.0f, 22.0f))
	{
		PlayBuffer(25, 0, 0);
		this->SendInfoRequest();
		return;
	}
}
