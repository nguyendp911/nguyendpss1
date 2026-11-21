#include "stdafx.h"
#include <algorithm>
#include <cmath>
#include "SmartAutoHelperUI.h"
#include "AutoHelper.h"
#include "AutoAdvancedUI.h"
#include "Defines.h"
#include "Offsets.h"
#include "Sound.h"
#include "stb.h"
#include "Util.h"

CSmartAutoHelperUI gSmartAutoHelperUI;

// =========================================================================
// Cold Steel UI Primitives (matching Login form & HUD aesthetic)
// =========================================================================
static inline DWORD CSColor(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static inline void CSRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
}

static inline bool CSHit(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static inline void CSSetFont(HFONT f)
{
	SelectObject(m_hFontDC, f);
	((void(__thiscall*)(void*, HFONT))0x0040F650)(*(void**)0x055C9FF8, f);
}

static inline float CSMeasureW(const char* text)
{
	if (!text || !text[0]) return 0.0f;
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (WindowWidth > 0) ? ((float)sz.cx * 640.0f / (float)WindowWidth) : (float)strlen(text) * 5.0f;
}

static inline void CSText(float x, float y, const char* text, DWORD color, bool bold = false)
{
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	EnableAlphaTest(true);
	CSSetFont(bold ? g_hFontBold : g_hFont);
	SetBackgroundTextColor = 0x00000000;
	SetTextColor = color;
	RenderText((int)x, (int)y, text, 0, RT3_SORT_LEFT, NULL);
}

static inline void CSTextCentered(float cx, float y, const char* text, DWORD color, bool bold = false)
{
	CSSetFont(bold ? g_hFontBold : g_hFont);
	float tw = CSMeasureW(text);
	CSText(cx - tw * 0.5f, y, text, color, bold);
}

static void CSDrawCard(float x, float y, float w, float h, const char* title)
{
	// Outer steel border + inner dark slate
	CSRect(x, y, w, h, 0.21f, 0.26f, 0.32f, 0.92f);
	CSRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 0.045f, 0.062f, 0.082f, 0.94f);
	// Subtle header strip
	CSRect(x + 1.0f, y + 1.0f, w - 2.0f, 14.0f, 0.09f, 0.13f, 0.18f, 0.90f);
	CSRect(x + 1.0f, y + 1.0f, 2.0f, 14.0f, 0.435f, 0.714f, 0.769f, 0.95f);
	CSText(x + 6.0f, y + 3.0f, title, CSColor(145, 215, 232, 255), true);
}

static void CSDrawCheckbox(float x, float y, const char* label, bool checked)
{
	bool hov = CSHit(x, y, 10.0f + CSMeasureW(label) + 6.0f, 12.0f);
	CSRect(x, y, 10.0f, 10.0f,
		checked ? 0.35f : (hov ? 0.40f : 0.25f),
		checked ? 0.78f : (hov ? 0.55f : 0.31f),
		checked ? 0.88f : (hov ? 0.65f : 0.38f), 0.96f);
	CSRect(x + 1.0f, y + 1.0f, 8.0f, 8.0f, 0.05f, 0.07f, 0.10f, 0.96f);
	if (checked)
	{
		CSRect(x + 2.5f, y + 2.5f, 5.0f, 5.0f, 0.22f, 0.88f, 0.96f, 1.0f);
	}
	if (label && label[0])
	{
		CSText(x + 14.0f, y + 0.5f, label, checked ? CSColor(232, 242, 248, 255) : CSColor(162, 176, 190, 255), false);
	}
}

static void CSDrawButton(float x, float y, float w, float h, const char* text, int style)
{
	// style: 0 = secondary steel, 1 = primary cyan, 2 = active emerald, 3 = danger red
	bool hov = CSHit(x, y, w, h);
	if (style == 1)
	{
		CSRect(x, y, w, h, hov ? 0.50f : 0.38f, hov ? 0.86f : 0.72f, hov ? 0.95f : 0.82f, 0.98f);
		CSRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, hov ? 0.13f : 0.09f, hov ? 0.26f : 0.19f, hov ? 0.33f : 0.25f, 0.96f);
	}
	else if (style == 2)
	{
		CSRect(x, y, w, h, hov ? 0.30f : 0.20f, hov ? 0.92f : 0.82f, hov ? 0.62f : 0.52f, 0.98f);
		CSRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, hov ? 0.09f : 0.06f, hov ? 0.24f : 0.18f, hov ? 0.16f : 0.12f, 0.96f);
	}
	else if (style == 3)
	{
		CSRect(x, y, w, h, hov ? 0.92f : 0.78f, hov ? 0.35f : 0.25f, hov ? 0.35f : 0.25f, 0.98f);
		CSRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, hov ? 0.28f : 0.20f, hov ? 0.09f : 0.07f, hov ? 0.09f : 0.07f, 0.96f);
	}
	else
	{
		CSRect(x, y, w, h, hov ? 0.435f : 0.24f, hov ? 0.714f : 0.29f, hov ? 0.769f : 0.35f, 0.95f);
		CSRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, hov ? 0.13f : 0.08f, hov ? 0.17f : 0.11f, hov ? 0.22f : 0.14f, 0.95f);
	}
	DWORD tc = (style == 1 || style == 2 || style == 3) ? CSColor(240, 250, 255, 255) : (hov ? CSColor(235, 245, 252, 255) : CSColor(195, 210, 222, 255));
	CSTextCentered(x + w * 0.5f, y + (h - 9.0f) * 0.5f, text, tc, style != 0);
}

static void CSDrawInputBox(float x, float y, float w, float h, const char* text, bool focused)
{
	CSRect(x, y, w, h, focused ? 0.35f : 0.23f, focused ? 0.80f : 0.28f, focused ? 0.92f : 0.34f, 0.96f);
	CSRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 0.03f, 0.045f, 0.065f, 0.98f);

	char buf[96];
	bool blink = focused && ((GetTickCount() / 450) % 2 == 0);
	sprintf_s(buf, sizeof(buf), "%s%s", text ? text : "", blink ? "|" : "");
	CSText(x + 4.0f, y + (h - 9.0f) * 0.5f, buf, focused ? CSColor(245, 252, 255, 255) : CSColor(210, 222, 232, 255), false);
}

static void CSDrawSlider(float x, float y, float w, float h, int percent, float r, float g, float b)
{
	if (percent < 0) percent = 0;
	if (percent > 100) percent = 100;

	CSRect(x, y, w, h, 0.22f, 0.27f, 0.33f, 0.92f);
	CSRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 0.04f, 0.055f, 0.075f, 0.95f);

	float fillW = (w - 2.0f) * (percent / 100.0f);
	if (fillW > 0.5f)
	{
		CSRect(x + 1.0f, y + 1.0f, fillW, h - 2.0f, r, g, b, 0.88f);
		CSRect(x + 1.0f, y + 1.0f, fillW, 2.0f, r + 0.15f, g + 0.15f, b + 0.15f, 0.95f);
	}

	char pct[16];
	sprintf_s(pct, sizeof(pct), "%d%%", percent);
	CSText(x + w + 5.0f, y - 0.5f, pct, CSColor(225, 238, 246, 255), false);
}

static void DrawIconQuad(GLuint texId, float x, float y, float w, float h, float u0, float u1, float r, float g, float b, float alpha)
{
	if (texId == 0) return;

	float sx = x * g_fScreenRate_x;
	float sy = (float)WindowHeight - y * g_fScreenRate_y;
	float sw = w * g_fScreenRate_x;
	float sh = h * g_fScreenRate_y;

	EnableAlphaTest(true);
	EnableAlphaBlend();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texId);
	glColor4f(r, g, b, alpha);

	glBegin(GL_QUADS);
	glTexCoord2f(u0, 0.0f); glVertex2f(sx, sy);
	glTexCoord2f(u1, 0.0f); glVertex2f(sx + sw, sy);
	glTexCoord2f(u1, 1.0f); glVertex2f(sx + sw, sy - sh);
	glTexCoord2f(u0, 1.0f); glVertex2f(sx, sy - sh);
	glEnd();

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

static void GetAutoHudButtonLayout(float* btnW, float* btnH, float* playX, float* cfgX, float* y)
{
	const float asp = (g_fScreenRate_x > 0.0f) ? (g_fScreenRate_y / g_fScreenRate_x) : 0.75f;
	*btnH = 22.0f;
	*btnW = (*btnH) * asp;
	*y = 8.0f;
	// Aligned pixel-perfectly after Camera (0,1) and Combo (2,3)
	*playX = 8.0f + (*btnW + 3.0f) * 4.0f;
	*cfgX  = 8.0f + (*btnW + 3.0f) * 5.0f;
}

// =========================================================================
// CSmartAutoHelperUI
// =========================================================================
CSmartAutoHelperUI::CSmartAutoHelperUI()
{
	this->m_IsOpen = false;
	this->m_Width = 480.0f;
	this->m_Height = 374.0f;
	this->m_PosX = (640.0f - this->m_Width) * 0.5f;
	this->m_PosY = 44.0f;

	this->m_ShowSkillPicker = false;
	this->m_PickingSlotType = 0;
	this->m_PickingSlotIndex = 0;

	memset(this->m_ItemInputBuffer, 0, sizeof(this->m_ItemInputBuffer));
	this->m_ItemInputFocused = false;
	this->m_SelectedExtraItemIndex = -1;
	this->m_ExtraItemScrollOffset = 0;

	this->m_FocusedNumberField = 0;
	memset(this->m_NumberInputBuffer, 0, sizeof(this->m_NumberInputBuffer));

	this->m_TexAutoCS = 0;
	this->m_SkillTextureID = 0;

	this->m_IsDragging = false;
	this->m_DragOffsetX = 0.0f;
	this->m_DragOffsetY = 0.0f;
}

CSmartAutoHelperUI::~CSmartAutoHelperUI()
{
}

void CSmartAutoHelperUI::Init()
{
}

void CSmartAutoHelperUI::LoadImages()
{
	if (this->m_TexAutoCS == 0)
	{
		int w = 0, h = 0, ch = 0;
		unsigned char* data = stbi_load("Data\\Interface\\AutoCS.png", &w, &h, &ch, 4);
		if (data && w > 0 && h > 0)
		{
			glGenTextures(1, &this->m_TexAutoCS);
			glBindTexture(GL_TEXTURE_2D, this->m_TexAutoCS);

			int mw = w, mh = h;
			unsigned char* cur = data;
			unsigned char* nextBuf = NULL;

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
					int sy0 = y * 2;
					int sy1 = (sy0 + 1 < mh) ? (sy0 + 1) : sy0;
					for (int x = 0; x < nw; x++)
					{
						int sx0 = x * 2;
						int sx1 = (sx0 + 1 < mw) ? (sx0 + 1) : sx0;

						for (int c = 0; c < 4; c++)
						{
							int sum = cur[(sy0 * mw + sx0) * 4 + c]
							        + cur[(sy0 * mw + sx1) * 4 + c]
							        + cur[(sy1 * mw + sx0) * 4 + c]
							        + cur[(sy1 * mw + sx1) * 4 + c];
							dst[(y * nw + x) * 4 + c] = (unsigned char)(sum / 4);
						}
					}
				}

				if (nextBuf) free(nextBuf);
				nextBuf = dst;
				cur = dst;
				mw = nw;
				mh = nh;
			}

			if (nextBuf) free(nextBuf);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
			glBindTexture(GL_TEXTURE_2D, 0);
			stbi_image_free(data);
		}
	}

	if (this->m_SkillTextureID == 0)
	{
		int sW = 0, sH = 0, sCh = 0;
		unsigned char* sData = stbi_load("Data\\Interface\\SkillHD.png", &sW, &sH, &sCh, 4);
		if (!sData) sData = stbi_load("Data\\Interface\\Skill.png", &sW, &sH, &sCh, 4);
		if (sData)
		{
			glGenTextures(1, &this->m_SkillTextureID);
			glBindTexture(GL_TEXTURE_2D, this->m_SkillTextureID);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sW, sH, 0, GL_RGBA, GL_UNSIGNED_BYTE, sData);
			glBindTexture(GL_TEXTURE_2D, 0);
			stbi_image_free(sData);
		}
	}
}

void CSmartAutoHelperUI::Toggle()
{
	this->Show(!this->m_IsOpen);
}

void CSmartAutoHelperUI::Show(bool show)
{
	this->m_IsOpen = show;
	this->m_ShowSkillPicker = false;
	this->m_ItemInputFocused = false;
	this->m_FocusedNumberField = 0;
	this->m_IsDragging = false;

	if (this->m_IsOpen)
	{
		PlayBuffer(25, 0, 0);
	}
	else
	{
		gAutoAdvancedUI.Close();
	}
}

void CSmartAutoHelperUI::CommitNumberInput()
{
	if (this->m_FocusedNumberField == 0) return;

	AutoHelperConfig& cfg = gAutoHelper.GetConfig();
	int val = atoi(this->m_NumberInputBuffer);

	switch (this->m_FocusedNumberField)
	{
	case 1:
		if (val < 1) val = 1;
		if (val > 999) val = 999;
		cfg.ReturnPosDelay = val;
		break;
	case 2:
		if (val < 1) val = 1;
		if (val > 999) val = 999;
		cfg.BuffInterval = val;
		break;
	case 3:
		if (val < 1) val = 1;
		if (val > 999) val = 999;
		cfg.PartyBuffInterval = val;
		break;
	case 4:
		if (val < 0) val = 0;
		if (val > 999) val = 999;
		cfg.ReturnHuntMin = val;
		break;
	case 5:
		if (val < 0) val = 0;
		if (val > 999) val = 999;
		cfg.ReturnHuntMax = val;
		break;
	}
	this->m_FocusedNumberField = 0;
}

// =========================================================================
// Top-Left Cold Steel HUD Buttons (aligned with Camera + Combo)
// =========================================================================
void CSmartAutoHelperUI::RenderHudButtons()
{
	if (this->m_TexAutoCS == 0)
	{
		this->LoadImages();
	}

	float btnW, btnH, playX, cfgX, y;
	GetAutoHudButtonLayout(&btnW, &btnH, &playX, &cfgX, &y);

	const bool active = gAutoHelper.IsActive();
	const bool cfgOpen = this->m_IsOpen;
	const bool hovPlay = CSHit(playX, y, btnW, btnH);
	const bool hovCfg  = CSHit(cfgX,  y, btnW, btnH);

	if (this->m_TexAutoCS != 0)
	{
		DWORD curTick = GetTickCount();
		// Tile 0 (0.00..0.25): Play (OFF) | Tile 1 (0.25..0.50): Pause (ACTIVE)
		float u0 = active ? 0.25f : 0.00f;
		float u1 = active ? 0.50f : 0.25f;
		float r = 1.0f, g = 1.0f, b = 1.0f;
		if (active)
		{
			float pulse = 0.88f + 0.12f * sinf((float)curTick * 0.008f);
			r = pulse; g = 1.0f; b = pulse;
		}
		else
		{
			r = g = b = hovPlay ? 1.0f : 0.90f;
		}
		DrawIconQuad(this->m_TexAutoCS, playX, y, btnW, btnH, u0, u1, r, g, b, 0.98f);

		// Tile 2 (0.50..0.75): Settings Normal | Tile 3 (0.75..1.00): Settings Active
		float su0 = cfgOpen ? 0.75f : 0.50f;
		float su1 = cfgOpen ? 1.00f : 0.75f;
		float c1 = (cfgOpen || hovCfg) ? 1.0f : 0.90f;
		DrawIconQuad(this->m_TexAutoCS, cfgX, y, btnW, btnH, su0, su1, c1, c1, c1, 0.98f);
	}

	if (hovPlay || hovCfg)
	{
		const char* tip = hovPlay
			? (active ? "SmartAuto: RUNNING (Home / Z)" : "SmartAuto: OFF (Home / Z)")
			: "SmartAuto Settings (End)";

		HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
		const DWORD oldTC = SetTextColor;
		const DWORD oldBG = SetBackgroundTextColor;

		CSSetFont(g_hFont);
		float tw = CSMeasureW(tip);
		float tx = hovPlay ? playX : cfgX;
		float ty = y + btnH + 4.0f;
		CSRect(tx, ty, tw + 10.0f, 15.0f, 0.23f, 0.27f, 0.32f, 0.95f);
		CSRect(tx + 1.0f, ty + 1.0f, tw + 8.0f, 13.0f, 0.055f, 0.075f, 0.098f, 0.92f);
		CSText(tx + 5.0f, ty + 2.5f, tip, CSColor(230, 242, 248, 255), false);

		CSSetFont(oldFont ? oldFont : g_hFont);
		SetTextColor = oldTC;
		SetBackgroundTextColor = oldBG;
	}
}

bool CSmartAutoHelperUI::UpdateHudButtonsMouse()
{
	float btnW, btnH, playX, cfgX, y;
	GetAutoHudButtonLayout(&btnW, &btnH, &playX, &cfgX, &y);

	const bool hovPlay = CSHit(playX, y, btnW, btnH);
	const bool hovCfg  = CSHit(cfgX,  y, btnW, btnH);

	if (!hovPlay && !hovCfg) return false;

	MouseOnWindow = true;

	if (MouseLButtonPush)
	{
		MouseLButtonPush = false;
		MouseLButton = false;
		MouseLButtonPop = false;
		PlayBuffer(25, 0, 0);

		if (hovPlay) gAutoHelper.Toggle();
		else if (hovCfg) this->Toggle();
		return true;
	}

	return false;
}

// =========================================================================
// Main Cold Steel SmartAuto Window Render
// =========================================================================
void CSmartAutoHelperUI::Render()
{
	if (SceneFlag != 5 || !Hero) return;

	this->RenderHudButtons();

	if (!this->m_IsOpen) return;

	AutoHelperConfig& cfg = gAutoHelper.GetConfig();
	const float px = this->m_PosX;
	const float py = this->m_PosY;
	const float pw = this->m_Width;
	const float ph = this->m_Height;

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTC = SetTextColor;
	const DWORD oldBG = SetBackgroundTextColor;

	// 1. Main Cold Steel Window Frame (matching Login form)
	CSRect(px - 2.0f, py - 2.0f, pw + 4.0f, ph + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
	CSRect(px, py, pw, ph, 0.23f, 0.27f, 0.32f, 0.98f);
	CSRect(px + 1.0f, py + 1.0f, pw - 2.0f, ph - 2.0f, 0.055f, 0.075f, 0.098f, 0.96f);
	CSRect(px, py, pw, 2.0f, 0.435f, 0.714f, 0.769f, 0.98f);

	// Header Bar (h = 26)
	CSRect(px + 1.0f, py + 2.0f, pw - 2.0f, 24.0f, 0.095f, 0.135f, 0.185f, 0.95f);
	CSRect(px + 1.0f, py + 26.0f, pw - 2.0f, 1.0f, 0.23f, 0.27f, 0.32f, 0.92f);

	CSText(px + 12.0f, py + 8.0f, "SMART AUTO", CSColor(145, 228, 246, 255), true);
	CSText(px + 92.0f, py + 9.0f, "Home/Z: Start-Stop  |  End: Open-Close", CSColor(135, 152, 168, 255), false);
	CSDrawButton(px + pw - 22.0f, py + 5.0f, 16.0f, 16.0f, "X", 0);

	const float colL = px + 10.0f;
	const float colR = px + 244.0f;
	const float colW = 226.0f;
	char buf[64];

	// ---- LEFT COLUMN ----
	// Card 1: Range & Return (y = py + 34, h = 66)
	CSDrawCard(colL, py + 34.0f, colW, 66.0f, "RANGE & RETURN");
	CSText(colL + 8.0f, py + 53.0f, "Hunt Range:", CSColor(195, 210, 222, 255));
	sprintf_s(buf, sizeof(buf), "%d", cfg.HuntingRange);
	CSDrawButton(colL + 78.0f, py + 51.0f, 16.0f, 13.0f, "-", 0);
	CSTextCentered(colL + 106.0f, py + 53.0f, buf, CSColor(255, 215, 90, 255), true);
	CSDrawButton(colL + 118.0f, py + 51.0f, 16.0f, 13.0f, "+", 0);
	CSText(colL + 138.0f, py + 53.0f, "tiles", CSColor(150, 165, 178, 255));

	CSDrawCheckbox(colL + 8.0f, py + 68.0f, "Return after:", cfg.ReturnPos);
	sprintf_s(buf, sizeof(buf), "%d", cfg.ReturnPosDelay);
	CSDrawInputBox(colL + 84.0f, py + 66.0f, 28.0f, 13.0f, this->m_FocusedNumberField == 1 ? this->m_NumberInputBuffer : buf, this->m_FocusedNumberField == 1);
	CSText(colL + 116.0f, py + 68.0f, "sec", CSColor(150, 165, 178, 255));

	CSDrawCheckbox(colL + 8.0f, py + 83.0f, "Return:", cfg.ReturnHunt);
	sprintf_s(buf, sizeof(buf), "%d", cfg.ReturnHuntMin);
	CSDrawInputBox(colL + 66.0f, py + 81.0f, 28.0f, 13.0f, this->m_FocusedNumberField == 4 ? this->m_NumberInputBuffer : buf, this->m_FocusedNumberField == 4);
	CSText(colL + 97.0f, py + 83.0f, "~", CSColor(170, 185, 198, 255));
	sprintf_s(buf, sizeof(buf), "%d", cfg.ReturnHuntMax);
	CSDrawInputBox(colL + 106.0f, py + 81.0f, 28.0f, 13.0f, this->m_FocusedNumberField == 5 ? this->m_NumberInputBuffer : buf, this->m_FocusedNumberField == 5);

	// Card 2: Auto Recovery (y = py + 106, h = 68)
	CSDrawCard(colL, py + 106.0f, colW, 68.0f, "AUTO RECOVERY");
	CSDrawCheckbox(colL + 8.0f, py + 124.0f, "Auto HP Potion", cfg.AutoPotionHP);
	CSDrawSlider(colL + 22.0f, py + 137.0f, 152.0f, 9.0f, cfg.PotionHPThreshold, 0.85f, 0.24f, 0.24f);
	CSDrawCheckbox(colL + 8.0f, py + 149.0f, "Auto MP Potion", cfg.AutoPotionMP);
	CSDrawSlider(colL + 22.0f, py + 162.0f, 152.0f, 9.0f, cfg.PotionMPThreshold, 0.20f, 0.52f, 0.90f);

	// Card 3: Combat & Skills (y = py + 180, h = 88)
	CSDrawCard(colL, py + 180.0f, colW, 88.0f, "COMBAT & SKILLS");
	CSDrawCheckbox(colL + 8.0f,   py + 198.0f, "Attack Mobs", cfg.AttackMobs);
	CSDrawCheckbox(colL + 80.0f,  py + 198.0f, "Defense",     cfg.SelfDefense);
	CSDrawCheckbox(colL + 136.0f, py + 198.0f, "Attack PK",   cfg.AttackPK);

	CSDrawCheckbox(colL + 8.0f,   py + 213.0f, "Sequential",   cfg.AttackMode == 0);
	CSDrawCheckbox(colL + 104.0f, py + 213.0f, "Random Skill", cfg.AttackMode == 1);

	for (int i = 0; i < 3; i++)
	{
		float sx = colL + 12.0f + i * 42.0f;
		float sy = py + 229.0f;
		bool hov = CSHit(sx, sy, 32.0f, 32.0f);
		bool has = (cfg.AttackSkills[i] >= 0);
		CSRect(sx, sy, 32.0f, 32.0f, hov ? 0.45f : (has ? 0.35f : 0.22f), hov ? 0.82f : (has ? 0.72f : 0.27f), hov ? 0.92f : (has ? 0.82f : 0.33f), 0.96f);
		CSRect(sx + 1.0f, sy + 1.0f, 30.0f, 30.0f, 0.04f, 0.055f, 0.075f, 0.96f);
		if (has)
		{
			this->RenderSkillIcon(cfg.AttackSkills[i], sx + 3.0f, sy + 3.0f, 26.0f, 26.0f);
		}
		else
		{
			CSTextCentered(sx + 16.0f, sy + 11.0f, "+", CSColor(110, 128, 145, 255), true);
		}
	}
	CSText(colL + 144.0f, py + 235.0f, "Click: Select", CSColor(130, 145, 160, 255));
	CSText(colL + 144.0f, py + 247.0f, "R-Click: Clear", CSColor(130, 145, 160, 255));

	// Card 4: Auto Buff (y = py + 274, h = 62)
	CSDrawCard(colL, py + 274.0f, colW, 62.0f, "AUTO BUFF");
	CSDrawCheckbox(colL + 8.0f, py + 291.0f, "On Expire", cfg.BuffExpired);
	CSDrawCheckbox(colL + 92.0f, py + 291.0f, "Interval:", !cfg.BuffExpired);
	sprintf_s(buf, sizeof(buf), "%d", cfg.BuffInterval);
	CSDrawInputBox(colL + 150.0f, py + 289.0f, 28.0f, 13.0f, this->m_FocusedNumberField == 2 ? this->m_NumberInputBuffer : buf, this->m_FocusedNumberField == 2);
	CSText(colL + 181.0f, py + 291.0f, "s", CSColor(150, 165, 178, 255));

	for (int i = 0; i < 3; i++)
	{
		float sx = colL + 12.0f + i * 42.0f;
		float sy = py + 305.0f;
		bool hov = CSHit(sx, sy, 28.0f, 28.0f);
		bool has = (cfg.BuffSkills[i] > 0);
		CSRect(sx, sy, 28.0f, 28.0f, hov ? 0.45f : (has ? 0.35f : 0.22f), hov ? 0.82f : (has ? 0.72f : 0.27f), hov ? 0.92f : (has ? 0.82f : 0.33f), 0.96f);
		CSRect(sx + 1.0f, sy + 1.0f, 26.0f, 26.0f, 0.04f, 0.055f, 0.075f, 0.96f);
		if (has)
		{
			this->RenderSkillIcon(cfg.BuffSkills[i], sx + 2.0f, sy + 2.0f, 24.0f, 24.0f);
		}
		else
		{
			CSTextCentered(sx + 14.0f, sy + 9.0f, "+", CSColor(110, 128, 145, 255), true);
		}
	}

	// ---- RIGHT COLUMN ----
	// Card 5: Loot & Repair (y = py + 34, h = 210)
	CSDrawCard(colR, py + 34.0f, colW, 210.0f, "LOOT & REPAIR");
	CSDrawCheckbox(colR + 8.0f,   py + 52.0f, "Auto Repair",      cfg.RepairEquip);
	CSDrawCheckbox(colR + 106.0f, py + 52.0f, "Pick Zen",         cfg.PickZen);
	CSDrawCheckbox(colR + 8.0f,   py + 66.0f, "Pick Jewels",      cfg.PickJewel);
	CSDrawCheckbox(colR + 106.0f, py + 66.0f, "Excellent Items",  cfg.PickExc);
	CSDrawCheckbox(colR + 8.0f,   py + 80.0f, "Pick extra items by name:", cfg.PickExtra);

	CSDrawInputBox(colR + 8.0f, py + 95.0f, 162.0f, 16.0f, this->m_ItemInputBuffer, this->m_ItemInputFocused);
	CSDrawButton(colR + 174.0f, py + 95.0f, 44.0f, 16.0f, "Add", 1);

	// Extra items scrollable list box
	const float listX = colR + 8.0f;
	const float listY = py + 115.0f;
	const float listW = 210.0f;
	const float listH = 104.0f;
	CSRect(listX, listY, listW, listH, 0.21f, 0.26f, 0.32f, 0.95f);
	CSRect(listX + 1.0f, listY + 1.0f, listW - 2.0f, listH - 2.0f, 0.03f, 0.045f, 0.065f, 0.96f);

	const int maxVisible = 7;
	int itemCount = (int)cfg.ExtraItems.size();
	if (this->m_ExtraItemScrollOffset > itemCount - maxVisible) this->m_ExtraItemScrollOffset = itemCount - maxVisible;
	if (this->m_ExtraItemScrollOffset < 0) this->m_ExtraItemScrollOffset = 0;

	for (int row = 0; row < maxVisible; row++)
	{
		int idx = this->m_ExtraItemScrollOffset + row;
		if (idx >= itemCount) break;
		float ry = listY + 3.0f + row * 14.0f;
		if (idx == this->m_SelectedExtraItemIndex)
		{
			CSRect(listX + 2.0f, ry - 1.0f, listW - 4.0f, 13.0f, 0.14f, 0.30f, 0.38f, 0.90f);
		}
		CSText(listX + 6.0f, ry + 1.0f, cfg.ExtraItems[idx].c_str(),
			(idx == this->m_SelectedExtraItemIndex) ? CSColor(240, 252, 255, 255) : CSColor(200, 214, 226, 255), false);
	}
	if (itemCount == 0)
	{
		CSText(listX + 8.0f, listY + 44.0f, "(No items added)", CSColor(115, 130, 145, 255));
	}

	// Delete & scroll controls under list
	CSDrawButton(colR + 8.0f,   py + 223.0f, 64.0f, 15.0f, "Remove", 0);
	CSDrawButton(colR + 174.0f, py + 223.0f, 20.0f, 15.0f, "^", 0);
	CSDrawButton(colR + 198.0f, py + 223.0f, 20.0f, 15.0f, "v", 0);

	// Card 6: Party Support (y = py + 250, h = 86)
	CSDrawCard(colR, py + 250.0f, colW, 86.0f, "PARTY SUPPORT");
	CSDrawCheckbox(colR + 8.0f, py + 268.0f, "Enable Party Support", cfg.PartySupport);
	CSText(colR + 8.0f, py + 284.0f, "Party Buff Interval:", CSColor(185, 200, 212, 255));
	sprintf_s(buf, sizeof(buf), "%d", cfg.PartyBuffInterval);
	CSDrawInputBox(colR + 104.0f, py + 282.0f, 30.0f, 13.0f, this->m_FocusedNumberField == 3 ? this->m_NumberInputBuffer : buf, this->m_FocusedNumberField == 3);
	CSText(colR + 138.0f, py + 284.0f, "sec", CSColor(150, 165, 178, 255));

	CSDrawCheckbox(colR + 8.0f, py + 301.0f, "Auto Party Heal (Elf Heal)", cfg.PartyHeal);
	CSDrawSlider(colR + 22.0f, py + 316.0f, 152.0f, 9.0f, cfg.PartyHealThreshold, 0.22f, 0.80f, 0.50f);

	// ---- FOOTER ACTION BUTTONS (y = py + 344, h = 21) ----
	const float fy = py + 344.0f;
	const float fh = 21.0f;
	const float fw = 84.0f;
	const float fgap = 10.0f;
	const float fx0 = px + (pw - (fw * 5.0f + fgap * 4.0f)) * 0.5f;

	CSDrawButton(fx0 + (fw + fgap) * 0.0f, fy, fw, fh, "Save", 0);
	CSDrawButton(fx0 + (fw + fgap) * 1.0f, fy, fw, fh, "Load", 0);
	CSDrawButton(fx0 + (fw + fgap) * 2.0f, fy, fw, fh, gAutoHelper.IsActive() ? "STOP AUTO" : "START", gAutoHelper.IsActive() ? 3 : 2);
	CSDrawButton(fx0 + (fw + fgap) * 3.0f, fy, fw, fh, "Advanced", 1);
	CSDrawButton(fx0 + (fw + fgap) * 4.0f, fy, fw, fh, "Close", 0);

	// Modals on top
	if (this->m_ShowSkillPicker)
	{
		this->RenderSkillPicker();
	}

	gAutoAdvancedUI.Render();

	CSSetFont(oldFont ? oldFont : g_hFont);
	SetTextColor = oldTC;
	SetBackgroundTextColor = oldBG;
	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

// =========================================================================
// Skill Icon Renderer (from SkillHD.png)
// =========================================================================
void CSmartAutoHelperUI::RenderSkillIcon(int skill, float x, float y, float w, float h)
{
	if (skill < 0) return;

	if (skill == 0)
	{
		CSTextCentered(x + w * 0.5f, y + (h - 9.0f) * 0.5f, "DANH", CSColor(215, 232, 242, 255), true);
		return;
	}

	if (this->m_SkillTextureID == 0) this->LoadImages();
	if (this->m_SkillTextureID == 0) return;

	int sIndex = skill - 1;
	int col = sIndex % 8;
	int row = sIndex / 8;
	float u0 = (col * 32.0f + 6.0f) / 256.0f;
	float v0 = (row * 36.0f + 4.0f) / 256.0f;
	float u1 = u0 + (20.0f / 256.0f);
	float v1 = v0 + (28.0f / 256.0f);

	float scale = (std::min)(w / 20.0f, h / 28.0f);
	float iw = 20.0f * scale;
	float ih = 28.0f * scale;
	float ix = x + (w - iw) * 0.5f;
	float iy = y + (h - ih) * 0.5f;

	float sx = ix * g_fScreenRate_x;
	float sy = (480.0f - iy) * g_fScreenRate_y;
	float sw = iw * g_fScreenRate_x;
	float sh = ih * g_fScreenRate_y;

	EnableAlphaTest(true);
	EnableAlphaBlend();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, this->m_SkillTextureID);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glBegin(GL_QUADS);
	glTexCoord2f(u0, v0); glVertex2f(sx, sy);
	glTexCoord2f(u1, v0); glVertex2f(sx + sw, sy);
	glTexCoord2f(u1, v1); glVertex2f(sx + sw, sy - sh);
	glTexCoord2f(u0, v1); glVertex2f(sx, sy - sh);
	glEnd();

	DisableAlphaBlend();
}

std::vector<int> CSmartAutoHelperUI::BuildPickerSkills()
{
	std::vector<int> list;
	list.push_back(-1); // Clear slot
	for (size_t i = 0; i < this->m_AvailableSkills.size(); i++)
	{
		list.push_back(this->m_AvailableSkills[i]);
	}
	return list;
}

void CSmartAutoHelperUI::RenderSkillPicker()
{
	std::vector<int> pickerSkills = this->BuildPickerSkills();
	int count = (int)pickerSkills.size();
	if (count == 0) return;

	int cols = (count > 8) ? 8 : (count < 6 ? 6 : count);
	int rows = (count + cols - 1) / cols;

	const float boxW = 34.0f, boxH = 34.0f, gap = 5.0f;
	float gridW = cols * boxW + (cols - 1) * gap;
	float trayW = (gridW + 24.0f > 320.0f) ? (gridW + 24.0f) : 320.0f;
	float headerH = 24.0f, padY = 8.0f;
	float padX = (trayW - gridW) * 0.5f;
	float trayH = headerH + padY * 2.0f + rows * boxH + (rows - 1) * gap;

	float trayX = (640.0f - trayW) * 0.5f;
	float trayY = (480.0f - trayH) * 0.5f;

	// Cold Steel Modal
	CSRect(trayX - 2.0f, trayY - 2.0f, trayW + 4.0f, trayH + 4.0f, 0.0f, 0.0f, 0.0f, 0.65f);
	CSRect(trayX, trayY, trayW, trayH, 0.23f, 0.27f, 0.32f, 0.98f);
	CSRect(trayX + 1.0f, trayY + 1.0f, trayW - 2.0f, trayH - 2.0f, 0.055f, 0.075f, 0.098f, 0.97f);
	CSRect(trayX, trayY, trayW, 2.0f, 0.435f, 0.714f, 0.769f, 0.98f);
	CSRect(trayX + 1.0f, trayY + 2.0f, trayW - 2.0f, headerH - 2.0f, 0.095f, 0.135f, 0.185f, 0.95f);

	const char* title = (this->m_PickingSlotType == 0) ? "SELECT ATTACK SKILL" : "SELECT BUFF SKILL";
	CSText(trayX + 10.0f, trayY + 7.0f, title, CSColor(145, 228, 246, 255), true);
	CSDrawButton(trayX + trayW - 20.0f, trayY + 4.0f, 15.0f, 15.0f, "X", 0);

	AutoHelperConfig& cfg = gAutoHelper.GetConfig();
	int activeSkill = (this->m_PickingSlotType == 0) ? cfg.AttackSkills[this->m_PickingSlotIndex] : cfg.BuffSkills[this->m_PickingSlotIndex];

	int hoveredSkill = -999;
	float hovX = 0.0f, hovY = 0.0f;

	for (int i = 0; i < count; i++)
	{
		int r = i / cols, c = i % cols;
		float sx = trayX + padX + c * (boxW + gap);
		float sy = trayY + headerH + padY + r * (boxH + gap);
		int skillId = pickerSkills[i];
		bool isHover = CSHit(sx, sy, boxW, boxH);
		bool isSelected = (skillId >= 0 && skillId == activeSkill) || (skillId < 0 && activeSkill <= 0);
		if (isHover) { hoveredSkill = skillId; hovX = sx; hovY = sy; }

		CSRect(sx, sy, boxW, boxH,
			isSelected ? 0.25f : (isHover ? 0.48f : 0.23f),
			isSelected ? 0.88f : (isHover ? 0.78f : 0.28f),
			isSelected ? 0.96f : (isHover ? 0.86f : 0.34f), 0.98f);
		CSRect(sx + 1.0f, sy + 1.0f, boxW - 2.0f, boxH - 2.0f, 0.04f, 0.06f, 0.08f, 0.96f);

		if (skillId < 0)
		{
			CSTextCentered(sx + boxW * 0.5f, sy + 12.0f, "NONE", CSColor(235, 110, 110, 255), true);
		}
		else
		{
			this->RenderSkillIcon(skillId, sx + 4.0f, sy + 4.0f, boxW - 8.0f, boxH - 8.0f);
		}
	}

	if (hoveredSkill > 0)
	{
		const char* sname = gAutoHelper.GetSkillNameById(hoveredSkill);
		if (sname && sname[0])
		{
			CSSetFont(g_hFont);
			float tw = CSMeasureW(sname);
			float tx = hovX + (boxW - tw) * 0.5f - 5.0f;
			float ty = hovY - 18.0f;
			CSRect(tx, ty, tw + 10.0f, 14.0f, 0.35f, 0.70f, 0.80f, 0.95f);
			CSRect(tx + 1.0f, ty + 1.0f, tw + 8.0f, 12.0f, 0.05f, 0.07f, 0.10f, 0.95f);
			CSText(tx + 5.0f, ty + 2.0f, sname, CSColor(235, 248, 255, 255), false);
		}
	}
}

bool CSmartAutoHelperUI::HandleSkillPickerClick()
{
	std::vector<int> pickerSkills = this->BuildPickerSkills();
	int count = (int)pickerSkills.size();
	if (count == 0) { this->m_ShowSkillPicker = false; return true; }

	int cols = (count > 8) ? 8 : (count < 6 ? 6 : count);
	int rows = (count + cols - 1) / cols;
	const float boxW = 34.0f, boxH = 34.0f, gap = 5.0f;
	float gridW = cols * boxW + (cols - 1) * gap;
	float trayW = (gridW + 24.0f > 320.0f) ? (gridW + 24.0f) : 320.0f;
	float headerH = 24.0f, padY = 8.0f;
	float padX = (trayW - gridW) * 0.5f;
	float trayH = headerH + padY * 2.0f + rows * boxH + (rows - 1) * gap;
	float trayX = (640.0f - trayW) * 0.5f;
	float trayY = (480.0f - trayH) * 0.5f;

	if (CSHit(trayX + trayW - 20.0f, trayY + 4.0f, 15.0f, 15.0f) || !CSHit(trayX, trayY, trayW, trayH))
	{
		PlayBuffer(25, 0, 0);
		this->m_ShowSkillPicker = false;
		return true;
	}

	AutoHelperConfig& cfg = gAutoHelper.GetConfig();
	for (int i = 0; i < count; i++)
	{
		int r = i / cols, c = i % cols;
		float sx = trayX + padX + c * (boxW + gap);
		float sy = trayY + headerH + padY + r * (boxH + gap);
		if (CSHit(sx, sy, boxW, boxH))
		{
			int picked = pickerSkills[i];
			if (picked < 0) picked = -1;
			if (this->m_PickingSlotType == 0) cfg.AttackSkills[this->m_PickingSlotIndex] = picked;
			else cfg.BuffSkills[this->m_PickingSlotIndex] = picked;
			PlayBuffer(25, 0, 0);
			this->m_ShowSkillPicker = false;
			return true;
		}
	}

	return true;
}

// =========================================================================
// Mouse Interaction
// =========================================================================
void CSmartAutoHelperUI::UpdateMouse()
{
	if (SceneFlag != 5 || !Hero) return;

	if (this->UpdateHudButtonsMouse()) return;

	if (!this->m_IsOpen) return;

	if (gAutoAdvancedUI.UpdateMouse()) return;

	const float px = this->m_PosX;
	const float py = this->m_PosY;
	const float pw = this->m_Width;
	const float ph = this->m_Height;

	if (CSHit(px, py, pw, ph) || this->m_ShowSkillPicker || this->m_IsDragging)
	{
		MouseOnWindow = true;
	}

	// Window dragging
	if (this->m_IsDragging)
	{
		if (MouseLButton)
		{
			this->m_PosX = (float)MouseX - this->m_DragOffsetX;
			this->m_PosY = (float)MouseY - this->m_DragOffsetY;
			if (this->m_PosX < 4.0f) this->m_PosX = 4.0f;
			if (this->m_PosY < 4.0f) this->m_PosY = 4.0f;
			if (this->m_PosX + pw > 636.0f) this->m_PosX = 636.0f - pw;
			if (this->m_PosY + ph > 440.0f) this->m_PosY = 440.0f - ph;
		}
		else
		{
			this->m_IsDragging = false;
		}
		return;
	}

	AutoHelperConfig& cfg = gAutoHelper.GetConfig();
	const float colL = px + 10.0f;
	const float colR = px + 244.0f;

	// Live slider dragging while mouse is held
	if (MouseLButton && !this->m_ShowSkillPicker)
	{
		if (CSHit(colL + 20.0f, py + 135.0f, 156.0f, 13.0f))
		{
			int pct = (int)(((float)MouseX - (colL + 22.0f)) * 100.0f / 152.0f + 0.5f);
			if (pct < 5) pct = 5;
			if (pct > 95) pct = 95;
			cfg.PotionHPThreshold = pct;
		}
		else if (CSHit(colL + 20.0f, py + 160.0f, 156.0f, 13.0f))
		{
			int pct = (int)(((float)MouseX - (colL + 22.0f)) * 100.0f / 152.0f + 0.5f);
			if (pct < 5) pct = 5;
			if (pct > 95) pct = 95;
			cfg.PotionMPThreshold = pct;
		}
		else if (CSHit(colR + 20.0f, py + 314.0f, 156.0f, 13.0f))
		{
			int pct = (int)(((float)MouseX - (colR + 22.0f)) * 100.0f / 152.0f + 0.5f);
			if (pct < 5) pct = 5;
			if (pct > 95) pct = 95;
			cfg.PartyHealThreshold = pct;
		}
	}

	// Right-click on skill slots to clear
	if (MouseRButtonPush && !this->m_ShowSkillPicker)
	{
		for (int i = 0; i < 3; i++)
		{
			if (CSHit(colL + 12.0f + i * 42.0f, py + 229.0f, 32.0f, 32.0f))
			{
				cfg.AttackSkills[i] = -1;
				MouseRButtonPush = false;
				PlayBuffer(25, 0, 0);
				return;
			}
			if (CSHit(colL + 12.0f + i * 42.0f, py + 305.0f, 28.0f, 28.0f))
			{
				cfg.BuffSkills[i] = -1;
				MouseRButtonPush = false;
				PlayBuffer(25, 0, 0);
				return;
			}
		}
	}

	if (!MouseLButtonPush) return;

	if (this->m_ShowSkillPicker)
	{
		MouseLButtonPush = false;
		MouseLButton = false;
		this->HandleSkillPickerClick();
		return;
	}

	if (!CSHit(px, py, pw, ph)) return;

	MouseLButtonPush = false;
	MouseLButton = false;
	MouseLButtonPop = false;

	this->CommitNumberInput();
	this->m_ItemInputFocused = false;

	// Close [X]
	if (CSHit(px + pw - 22.0f, py + 5.0f, 16.0f, 16.0f))
	{
		PlayBuffer(25, 0, 0);
		this->Show(false);
		return;
	}

	// Drag header
	if (CSHit(px, py, pw - 26.0f, 26.0f))
	{
		this->m_IsDragging = true;
		this->m_DragOffsetX = (float)MouseX - px;
		this->m_DragOffsetY = (float)MouseY - py;
		return;
	}

	// Card 1: Range & Return
	if (CSHit(colL + 78.0f, py + 51.0f, 16.0f, 13.0f)) { if (cfg.HuntingRange > 1) cfg.HuntingRange--; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colL + 118.0f, py + 51.0f, 16.0f, 13.0f)) { if (cfg.HuntingRange < 8) cfg.HuntingRange++; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colL + 8.0f, py + 66.0f, 74.0f, 13.0f)) { cfg.ReturnPos = !cfg.ReturnPos; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colL + 84.0f, py + 66.0f, 28.0f, 13.0f)) { this->m_FocusedNumberField = 1; sprintf_s(this->m_NumberInputBuffer, sizeof(this->m_NumberInputBuffer), "%d", cfg.ReturnPosDelay); return; }
	if (CSHit(colL + 8.0f, py + 81.0f, 56.0f, 13.0f)) { cfg.ReturnHunt = !cfg.ReturnHunt; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colL + 66.0f, py + 81.0f, 28.0f, 13.0f)) { this->m_FocusedNumberField = 4; sprintf_s(this->m_NumberInputBuffer, sizeof(this->m_NumberInputBuffer), "%d", cfg.ReturnHuntMin); return; }
	if (CSHit(colL + 106.0f, py + 81.0f, 28.0f, 13.0f)) { this->m_FocusedNumberField = 5; sprintf_s(this->m_NumberInputBuffer, sizeof(this->m_NumberInputBuffer), "%d", cfg.ReturnHuntMax); return; }

	// Card 2: HP / MP
	if (CSHit(colL + 8.0f, py + 123.0f, 140.0f, 12.0f)) { cfg.AutoPotionHP = !cfg.AutoPotionHP; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colL + 8.0f, py + 148.0f, 140.0f, 12.0f)) { cfg.AutoPotionMP = !cfg.AutoPotionMP; PlayBuffer(25, 0, 0); return; }

	// Card 3: Combat & Attack Skills
	if (CSHit(colL + 8.0f,   py + 197.0f, 68.0f, 12.0f)) { cfg.AttackMobs = !cfg.AttackMobs; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colL + 80.0f,  py + 197.0f, 52.0f, 12.0f)) { cfg.SelfDefense = !cfg.SelfDefense; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colL + 136.0f, py + 197.0f, 60.0f, 12.0f)) { cfg.AttackPK = !cfg.AttackPK; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colL + 8.0f,   py + 212.0f, 92.0f, 12.0f)) { cfg.AttackMode = 0; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colL + 104.0f, py + 212.0f, 105.0f, 12.0f)) { cfg.AttackMode = 1; PlayBuffer(25, 0, 0); return; }

	for (int i = 0; i < 3; i++)
	{
		if (CSHit(colL + 12.0f + i * 42.0f, py + 229.0f, 32.0f, 32.0f))
		{
			this->m_PickingSlotType = 0;
			this->m_PickingSlotIndex = i;
			this->m_AvailableSkills = gAutoHelper.GetLearnedAttackSkills();
			this->m_ShowSkillPicker = true;
			PlayBuffer(25, 0, 0);
			return;
		}
	}

	// Card 4: Buffs
	if (CSHit(colL + 8.0f, py + 290.0f, 80.0f, 12.0f)) { cfg.BuffExpired = true; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colL + 92.0f, py + 290.0f, 56.0f, 12.0f)) { cfg.BuffExpired = false; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colL + 150.0f, py + 289.0f, 28.0f, 13.0f)) { this->m_FocusedNumberField = 2; sprintf_s(this->m_NumberInputBuffer, sizeof(this->m_NumberInputBuffer), "%d", cfg.BuffInterval); return; }

	for (int i = 0; i < 3; i++)
	{
		if (CSHit(colL + 12.0f + i * 42.0f, py + 305.0f, 28.0f, 28.0f))
		{
			this->m_PickingSlotType = 1;
			this->m_PickingSlotIndex = i;
			this->m_AvailableSkills = gAutoHelper.GetLearnedBuffSkills();
			this->m_ShowSkillPicker = true;
			PlayBuffer(25, 0, 0);
			return;
		}
	}

	// Card 5: Loot & Repair
	if (CSHit(colR + 8.0f,   py + 51.0f, 92.0f, 12.0f))  { cfg.RepairEquip = !cfg.RepairEquip; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colR + 106.0f, py + 51.0f, 95.0f, 12.0f))  { cfg.PickZen = !cfg.PickZen; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colR + 8.0f,   py + 65.0f, 92.0f, 12.0f))  { cfg.PickJewel = !cfg.PickJewel; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colR + 106.0f, py + 65.0f, 112.0f, 12.0f)) { cfg.PickExc = !cfg.PickExc; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colR + 8.0f,   py + 79.0f, 195.0f, 12.0f)) { cfg.PickExtra = !cfg.PickExtra; PlayBuffer(25, 0, 0); return; }

	if (CSHit(colR + 8.0f, py + 95.0f, 162.0f, 16.0f))
	{
		this->m_ItemInputFocused = true;
		return;
	}
	if (CSHit(colR + 174.0f, py + 95.0f, 44.0f, 16.0f))
	{
		if (this->m_ItemInputBuffer[0] != '\0')
		{
			gAutoHelper.AddExtraItem(this->m_ItemInputBuffer);
			memset(this->m_ItemInputBuffer, 0, sizeof(this->m_ItemInputBuffer));
			PlayBuffer(25, 0, 0);
		}
		return;
	}

	// Extra items list selection
	if (CSHit(colR + 8.0f, py + 115.0f, 210.0f, 104.0f))
	{
		int row = (int)(((float)MouseY - (py + 118.0f)) / 14.0f);
		int idx = this->m_ExtraItemScrollOffset + row;
		if (row >= 0 && row < 7 && idx >= 0 && idx < (int)cfg.ExtraItems.size())
		{
			this->m_SelectedExtraItemIndex = idx;
			PlayBuffer(25, 0, 0);
		}
		return;
	}
	if (CSHit(colR + 8.0f, py + 223.0f, 64.0f, 15.0f))
	{
		if (this->m_SelectedExtraItemIndex >= 0 && this->m_SelectedExtraItemIndex < (int)cfg.ExtraItems.size())
		{
			gAutoHelper.RemoveExtraItem(this->m_SelectedExtraItemIndex);
			this->m_SelectedExtraItemIndex = -1;
			PlayBuffer(25, 0, 0);
		}
		return;
	}
	if (CSHit(colR + 174.0f, py + 223.0f, 20.0f, 15.0f))
	{
		if (this->m_ExtraItemScrollOffset > 0) this->m_ExtraItemScrollOffset--;
		PlayBuffer(25, 0, 0);
		return;
	}
	if (CSHit(colR + 198.0f, py + 223.0f, 20.0f, 15.0f))
	{
		if (this->m_ExtraItemScrollOffset + 7 < (int)cfg.ExtraItems.size()) this->m_ExtraItemScrollOffset++;
		PlayBuffer(25, 0, 0);
		return;
	}

	// Card 6: Party
	if (CSHit(colR + 8.0f, py + 267.0f, 140.0f, 12.0f)) { cfg.PartySupport = !cfg.PartySupport; PlayBuffer(25, 0, 0); return; }
	if (CSHit(colR + 104.0f, py + 282.0f, 30.0f, 13.0f)) { this->m_FocusedNumberField = 3; sprintf_s(this->m_NumberInputBuffer, sizeof(this->m_NumberInputBuffer), "%d", cfg.PartyBuffInterval); return; }
	if (CSHit(colR + 8.0f, py + 300.0f, 180.0f, 12.0f)) { cfg.PartyHeal = !cfg.PartyHeal; PlayBuffer(25, 0, 0); return; }

	// Footer buttons
	const float fy = py + 344.0f, fh = 21.0f, fw = 84.0f, fgap = 10.0f;
	const float fx0 = px + (pw - (fw * 5.0f + fgap * 4.0f)) * 0.5f;

	if (CSHit(fx0 + (fw + fgap) * 0.0f, fy, fw, fh)) // Luu
	{
		char name[32] = { 0 };
		gAutoHelper.GetCharName(name, sizeof(name));
		gAutoHelper.SaveConfig(name);
		CreateNotice("[SmartAuto] Da luu cau hinh!", 1);
		PlayBuffer(25, 0, 0);
		return;
	}
	if (CSHit(fx0 + (fw + fgap) * 1.0f, fy, fw, fh)) // Tai
	{
		char name[32] = { 0 };
		gAutoHelper.GetCharName(name, sizeof(name));
		gAutoHelper.LoadConfig(name);
		CreateNotice("[SmartAuto] Da tai cau hinh!", 1);
		PlayBuffer(25, 0, 0);
		return;
	}
	if (CSHit(fx0 + (fw + fgap) * 2.0f, fy, fw, fh)) // Bat dau / Dung
	{
		gAutoHelper.Toggle();
		PlayBuffer(25, 0, 0);
		return;
	}
	if (CSHit(fx0 + (fw + fgap) * 3.0f, fy, fw, fh)) // Nang cao
	{
		gAutoAdvancedUI.Open();
		PlayBuffer(25, 0, 0);
		return;
	}
	if (CSHit(fx0 + (fw + fgap) * 4.0f, fy, fw, fh)) // Dong
	{
		this->Show(false);
		PlayBuffer(25, 0, 0);
		return;
	}
}

// =========================================================================
// Keyboard Input (Extra item name & number fields)
// =========================================================================
void CSmartAutoHelperUI::UpdateKeyboard()
{
	if (!this->m_IsOpen || SceneFlag != 5 || !Hero) return;

	static BYTE prevKey[256] = { 0 };
	auto keyPressed = [&](int vk) -> bool {
		bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
		bool edge = down && !prevKey[vk];
		prevKey[vk] = down ? 1 : 0;
		return edge;
	};

	if (keyPressed(VK_ESCAPE))
	{
		if (gAutoAdvancedUI.IsOpen()) gAutoAdvancedUI.Close();
		else if (this->m_ShowSkillPicker) this->m_ShowSkillPicker = false;
		else if (this->m_FocusedNumberField != 0) this->CommitNumberInput();
		else if (this->m_ItemInputFocused) this->m_ItemInputFocused = false;
		else this->Show(false);
		return;
	}

	if (this->m_FocusedNumberField != 0)
	{
		if (keyPressed(VK_RETURN)) { this->CommitNumberInput(); return; }
		if (keyPressed(VK_BACK))
		{
			size_t len = strlen(this->m_NumberInputBuffer);
			if (len > 0) this->m_NumberInputBuffer[len - 1] = '\0';
			return;
		}
		for (int vk = '0'; vk <= '9'; vk++)
		{
			if (keyPressed(vk))
			{
				size_t len = strlen(this->m_NumberInputBuffer);
				if (len < 3) { this->m_NumberInputBuffer[len] = (char)vk; this->m_NumberInputBuffer[len + 1] = '\0'; }
			}
		}
		for (int vk = VK_NUMPAD0; vk <= VK_NUMPAD9; vk++)
		{
			if (keyPressed(vk))
			{
				size_t len = strlen(this->m_NumberInputBuffer);
				if (len < 3) { this->m_NumberInputBuffer[len] = (char)('0' + (vk - VK_NUMPAD0)); this->m_NumberInputBuffer[len + 1] = '\0'; }
			}
		}
		return;
	}

	if (this->m_ItemInputFocused)
	{
		if (keyPressed(VK_RETURN))
		{
			if (this->m_ItemInputBuffer[0] != '\0')
			{
				gAutoHelper.AddExtraItem(this->m_ItemInputBuffer);
				memset(this->m_ItemInputBuffer, 0, sizeof(this->m_ItemInputBuffer));
				PlayBuffer(25, 0, 0);
			}
			return;
		}
		if (keyPressed(VK_BACK))
		{
			size_t len = strlen(this->m_ItemInputBuffer);
			if (len > 0) this->m_ItemInputBuffer[len - 1] = '\0';
			return;
		}

		bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
		for (int vk = 'A'; vk <= 'Z'; vk++)
		{
			if (keyPressed(vk))
			{
				size_t len = strlen(this->m_ItemInputBuffer);
				if (len + 1 < sizeof(this->m_ItemInputBuffer))
				{
					this->m_ItemInputBuffer[len] = shift ? (char)vk : (char)(vk + 32);
					this->m_ItemInputBuffer[len + 1] = '\0';
				}
			}
		}
		for (int vk = '0'; vk <= '9'; vk++)
		{
			if (keyPressed(vk))
			{
				size_t len = strlen(this->m_ItemInputBuffer);
				if (len + 1 < sizeof(this->m_ItemInputBuffer))
				{
					this->m_ItemInputBuffer[len] = (char)vk;
					this->m_ItemInputBuffer[len + 1] = '\0';
				}
			}
		}
		if (keyPressed(VK_SPACE))
		{
			size_t len = strlen(this->m_ItemInputBuffer);
			if (len + 1 < sizeof(this->m_ItemInputBuffer))
			{
				this->m_ItemInputBuffer[len] = ' ';
				this->m_ItemInputBuffer[len + 1] = '\0';
			}
		}
		if (keyPressed(VK_OEM_PLUS) || keyPressed(VK_ADD))
		{
			size_t len = strlen(this->m_ItemInputBuffer);
			if (len + 1 < sizeof(this->m_ItemInputBuffer))
			{
				this->m_ItemInputBuffer[len] = '+';
				this->m_ItemInputBuffer[len + 1] = '\0';
			}
		}
	}
}
