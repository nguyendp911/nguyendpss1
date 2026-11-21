#include "stdafx.h"
#include "AutoAdvancedUI.h"
#include "AutoHelper.h"
#include "Offsets.h"
#include "Sound.h"
#include "Util.h"

CAutoAdvancedUI gAutoAdvancedUI;

static const float AA_PW = 348.0f;
static const float AA_PH = 398.0f;
static const float AA_PX = (640.0f - AA_PW) / 2.0f;
static const float AA_PY = 24.0f;
static const float AA_HEADER_H = 26.0f;
static const float AA_ROW_H = 20.0f;
static const int AA_ROWS = 11;

static inline DWORD AAColor4b(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static inline void AARect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
}

static inline bool AAHit(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static inline float AAMeasureW(const char* text)
{
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (WindowWidth > 0) ? ((float)sz.cx * 640.0f / (float)WindowWidth) : (float)strlen(text) * 5.0f;
}

static inline void AASetFont(HFONT f)
{
	SelectObject(m_hFontDC, f);
	((void(__thiscall*)(void*, HFONT))0x0040F650)(*(void**)0x055C9FF8, f);
}

static void AADrawButton(float x, float y, float w, float h, const char* text, bool primary, bool enabled)
{
	bool hover = enabled && AAHit(x, y, w, h);
	if (!enabled)
	{
		AARect(x, y, w, h, 0.18f, 0.21f, 0.25f, 0.85f);
		AARect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 0.08f, 0.10f, 0.13f, 0.90f);
	}
	else if (primary)
	{
		AARect(x, y, w, h, hover ? 0.55f : 0.435f, hover ? 0.85f : 0.714f, hover ? 0.92f : 0.769f, 0.95f);
		AARect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, hover ? 0.14f : 0.10f, hover ? 0.24f : 0.18f, hover ? 0.30f : 0.23f, 0.96f);
	}
	else
	{
		AARect(x, y, w, h, hover ? 0.435f : 0.25f, hover ? 0.714f : 0.30f, hover ? 0.769f : 0.36f, 0.92f);
		AARect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, hover ? 0.13f : 0.09f, hover ? 0.17f : 0.12f, hover ? 0.22f : 0.15f, 0.94f);
	}

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	EnableAlphaTest(true);
	AASetFont(g_hFont);
	SetBackgroundTextColor = 0x00000000;
	SetTextColor = enabled ? (primary ? AAColor4b(235, 248, 255, 255) : AAColor4b(210, 222, 232, 255)) : AAColor4b(120, 130, 140, 255);
	float tw = AAMeasureW(text);
	RenderText((int)(x + (w - tw) * 0.5f), (int)(y + (h - 10.0f) * 0.5f), text, 0, RT3_SORT_LEFT, NULL);
}

static const char* s_Rows[AA_ROWS] =
{
	"Prioritize World Boss & Elite monsters",
	"Skip stuck or unreachable monsters",
	"Smart potion (larger pot at critical HP)",
	"Stop when out of HP potions",
	"Stop when inventory is full",
	"Stop when no monsters for",
	"Show hunting statistics panel",
	"Auto follow party leader (same map)",
	"Auto warp to party leader (other map)",
	"Play alert sound when Auto stops",
	"Escape to Lorencia when HP < 15% & no pots"
};

static bool* RowValue(AutoHelperConfig& c, int row)
{
	switch (row)
	{
		case 0: return &c.PreferBoss;
		case 1: return &c.SkipStuck;
		case 2: return &c.SmartPotion;
		case 3: return &c.StopNoPotion;
		case 4: return &c.StopInvFull;
		case 5: return &c.StopIdle;
		case 6: return &c.ShowStats;
		case 7: return &c.PartyFollowLeader;
		case 8: return &c.FollowTeleport;
		case 9: return &c.AlertSound;
		case 10: return &c.TownEscape;
	}
	return 0;
}

static float RowY(int row)
{
	return AA_PY + AA_HEADER_H + 10.0f + row * AA_ROW_H;
}

CAutoAdvancedUI::CAutoAdvancedUI()
{
	this->m_Open = false;
}

void CAutoAdvancedUI::Open()
{
	this->m_Open = true;
}

void CAutoAdvancedUI::Close()
{
	this->m_Open = false;
}

void CAutoAdvancedUI::Render()
{
	if (!this->m_Open || SceneFlag != 5 || !Hero)
	{
		return;
	}

	AutoHelperConfig& cfg = gAutoHelper.GetConfig();

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTextColor = SetTextColor;
	const DWORD oldBgColor = SetBackgroundTextColor;

	// Cold Steel panel backdrop
	AARect(AA_PX - 2.0f, AA_PY - 2.0f, AA_PW + 4.0f, AA_PH + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
	AARect(AA_PX, AA_PY, AA_PW, AA_PH, 0.23f, 0.27f, 0.32f, 0.96f);
	AARect(AA_PX + 1.0f, AA_PY + 1.0f, AA_PW - 2.0f, AA_PH - 2.0f, 0.055f, 0.075f, 0.098f, 0.95f);
	AARect(AA_PX, AA_PY, AA_PW, 2.0f, 0.435f, 0.714f, 0.769f, 0.98f);

	// Header strip
	AARect(AA_PX + 1.0f, AA_PY + 2.0f, AA_PW - 2.0f, AA_HEADER_H - 2.0f, 0.10f, 0.14f, 0.19f, 0.92f);
	AARect(AA_PX + 1.0f, AA_PY + AA_HEADER_H, AA_PW - 2.0f, 1.0f, 0.23f, 0.27f, 0.32f, 0.90f);

	// Close button [X]
	AADrawButton(AA_PX + AA_PW - 22.0f, AA_PY + 5.0f, 16.0f, 16.0f, "X", false, true);

	// Title
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	EnableAlphaTest(true);
	AASetFont(g_hFontBold);
	SetBackgroundTextColor = 0x00000000;
	SetTextColor = AAColor4b(140, 225, 245, 255);
	const char* title = "SMART AUTO - ADVANCED";
	float titleW = AAMeasureW(title);
	RenderText((int)(AA_PX + (AA_PW - titleW) * 0.5f), (int)(AA_PY + 8.0f), title, 0, RT3_SORT_LEFT, NULL);

	// Checkbox rows
	AASetFont(g_hFont);
	for (int row = 0; row < AA_ROWS; row++)
	{
		float y = RowY(row);
		bool checked = *RowValue(cfg, row);

		// Cold Steel checkbox
		float cx = AA_PX + 16.0f;
		float cy = y + 1.0f;
		AARect(cx, cy, 11.0f, 11.0f, checked ? 0.435f : 0.28f, checked ? 0.714f : 0.33f, checked ? 0.769f : 0.39f, 0.95f);
		AARect(cx + 1.0f, cy + 1.0f, 9.0f, 9.0f, 0.06f, 0.08f, 0.11f, 0.96f);
		if (checked)
		{
			AARect(cx + 3.0f, cy + 3.0f, 5.0f, 5.0f, 0.25f, 0.88f, 0.96f, 1.0f);
		}

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		EnableAlphaTest(true);
		SetTextColor = checked ? AAColor4b(232, 242, 248, 255) : AAColor4b(165, 178, 190, 255);
		RenderText((int)(cx + 16.0f), (int)(y + 1.0f), s_Rows[row], 0, RT3_SORT_LEFT, NULL);

		if (row == 5)
		{
			float sx = AA_PX + AA_PW - 106.0f;
			char num[16];
			sprintf_s(num, sizeof(num), "%d", cfg.IdleMinutes);

			AADrawButton(sx, y - 1.0f, 16.0f, 15.0f, "-", false, cfg.IdleMinutes > 1);
			AADrawButton(sx + 44.0f, y - 1.0f, 16.0f, 15.0f, "+", false, cfg.IdleMinutes < 60);

			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			EnableAlphaTest(true);
			SetTextColor = AAColor4b(255, 215, 90, 255);
			float nw = AAMeasureW(num);
			RenderText((int)(sx + 30.0f - nw * 0.5f), (int)(y + 1.0f), num, 0, RT3_SORT_LEFT, NULL);
			SetTextColor = AAColor4b(175, 188, 200, 255);
			RenderText((int)(sx + 64.0f), (int)(y + 1.0f), "min", 0, RT3_SORT_LEFT, NULL);
		}
	}

	float by = RowY(AA_ROWS) + 8.0f;
	AARect(AA_PX + 14.0f, by - 4.0f, AA_PW - 28.0f, 1.0f, 0.23f, 0.27f, 0.32f, 0.85f);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	EnableAlphaTest(true);
	SetTextColor = AAColor4b(255, 215, 90, 255);
	RenderText((int)(AA_PX + 16.0f), (int)(by + 2.0f), "Quick Profiles (Profile 1 - 3):", 0, RT3_SORT_LEFT, NULL);

	for (int n = 0; n < 3; n++)
	{
		char text[16];
		float bx = AA_PX + 16.0f + n * 108.0f;
		sprintf_s(text, sizeof(text), "Save %d", n + 1);
		AADrawButton(bx, by + 18.0f, 100.0f, 18.0f, text, false, true);
		sprintf_s(text, sizeof(text), "Load %d", n + 1);
		AADrawButton(bx, by + 40.0f, 100.0f, 18.0f, text, true, true);
	}

	by += 66.0f;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	EnableAlphaTest(true);
	SetTextColor = AAColor4b(150, 165, 178, 255);
	const char* hint = "Click Save in the main window to store for this character";
	float hw = AAMeasureW(hint);
	RenderText((int)(AA_PX + (AA_PW - hw) * 0.5f), (int)by, hint, 0, RT3_SORT_LEFT, NULL);

	AADrawButton(AA_PX + AA_PW * 0.5f - 48.0f, by + 16.0f, 96.0f, 20.0f, "Done", true, true);

	AASetFont(g_hFont);
	SetTextColor = oldTextColor;
	SetBackgroundTextColor = oldBgColor;
	SelectObject(m_hFontDC, oldFont);
}

bool CAutoAdvancedUI::UpdateMouse()
{
	if (!this->m_Open)
	{
		return false;
	}

	if (SceneFlag != 5 || !Hero)
	{
		this->m_Open = false;
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

	AutoHelperConfig& cfg = gAutoHelper.GetConfig();

	if (AAHit(AA_PX + AA_PW - 22.0f, AA_PY + 5.0f, 16.0f, 16.0f))
	{
		PlayBuffer(25, 0, 0);
		this->m_Open = false;
		return true;
	}

	for (int row = 0; row < AA_ROWS; row++)
	{
		float y = RowY(row);
		if (AAHit(AA_PX + 16.0f, y, (row == 5) ? 210.0f : (AA_PW - 32.0f), AA_ROW_H - 2.0f))
		{
			PlayBuffer(25, 0, 0);
			bool* value = RowValue(cfg, row);
			*value = !*value;
			return true;
		}

		if (row == 5)
		{
			float sx = AA_PX + AA_PW - 106.0f;
			if (cfg.IdleMinutes > 1 && AAHit(sx, y - 1.0f, 16.0f, 15.0f))
			{
				PlayBuffer(25, 0, 0);
				cfg.IdleMinutes--;
				return true;
			}
			if (cfg.IdleMinutes < 60 && AAHit(sx + 44.0f, y - 1.0f, 16.0f, 15.0f))
			{
				PlayBuffer(25, 0, 0);
				cfg.IdleMinutes++;
				return true;
			}
		}
	}

	float by = RowY(AA_ROWS) + 8.0f;
	for (int n = 0; n < 3; n++)
	{
		float bx = AA_PX + 16.0f + n * 108.0f;
		if (AAHit(bx, by + 18.0f, 100.0f, 18.0f))
		{
			PlayBuffer(25, 0, 0);
			gAutoHelper.SaveProfile(n + 1);
			CreateNotice("[SmartAuto] Da luu ho so cau hinh.", 1);
			return true;
		}
		if (AAHit(bx, by + 40.0f, 100.0f, 18.0f))
		{
			PlayBuffer(25, 0, 0);
			gAutoHelper.LoadProfile(n + 1);
			CreateNotice("[SmartAuto] Da nap ho so cau hinh.", 1);
			return true;
		}
	}

	by += 66.0f;
	if (AAHit(AA_PX + AA_PW * 0.5f - 48.0f, by + 16.0f, 96.0f, 20.0f))
	{
		PlayBuffer(25, 0, 0);
		this->m_Open = false;
	}

	return true;
}
