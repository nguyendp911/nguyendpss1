#include "stdafx.h"
#include "OptionsMenu.h"
#include "Font.h"
#include "HealthBar.h"
#include "Language.h"
#include "Sound.h"
#include "Window.h"
#include "WeaponView.h"

COptionsMenu gOptionsMenu;

// Sidebar layout (virtual 640x480 units)
#define ROW_GAP 4
#define PANEL_W 330
#define PANEL_H 264
#define PANEL_Y 108
#define SIDE_W 100
#define HEADER_H 28
#define PANEL_PAD 8
#define TAB_H 22
#define TAB_GAP 4

static void FlatRect(float x, float y, float w, float h, float r, float g, float b);
static void DrawArrowButton(float x, float y, float w, float h, bool right, int state);

COptionsMenu::COptionsMenu()
{
	this->CurrentOption = OPTION_NONE;

	memset(this->Resolutions, 0, sizeof(this->Resolutions));

	this->BoxWidth = PANEL_W - SIDE_W - PANEL_PAD * 2;

	this->BoxHeight = 20;

	this->PanelX = ImgCenterScreenPosX((float)PANEL_W);

	this->PanelY = (float)PANEL_Y;

	this->MainPosX = this->PanelX + (float)(SIDE_W + PANEL_PAD);

	this->MainPosY = this->PanelY + (float)(HEADER_H + PANEL_PAD - (this->BoxHeight + ROW_GAP));

	this->PVPWithoutControl = false;
	this->DeleteShadows = (GetPrivateProfileInt("Antilag", "DeleteShadows", 0, ".\\Config.ini") != 0);
	this->DeleteObjects = (GetPrivateProfileInt("Antilag", "DeleteObjects", 0, ".\\Config.ini") != 0);
	this->DeleteFloor = (GetPrivateProfileInt("Antilag", "DeleteFloor", 0, ".\\Config.ini") != 0);
	this->DeleteSkills = (GetPrivateProfileInt("Antilag", "DeleteSkills", 0, ".\\Config.ini") != 0);
	this->DeleteStaticEffects = (GetPrivateProfileInt("Antilag", "DeleteStaticEffects", 0, ".\\Config.ini") != 0);
	this->DeleteDynamicEffects = (GetPrivateProfileInt("Antilag", "DeleteDynamicEffects", 0, ".\\Config.ini") != 0);
	this->DeleteInterface = (GetPrivateProfileInt("Antilag", "DeleteInterface", 0, ".\\Config.ini") != 0);
}

COptionsMenu::~COptionsMenu()
{
	char Text[10] = { 0 };

	wsprintf(Text, "%d", (this->DeleteShadows) ? 1 : 0);
	WritePrivateProfileString("Antilag", "DeleteShadows", Text, ".\\Config.ini");

	wsprintf(Text, "%d", (this->DeleteObjects) ? 1 : 0);
	WritePrivateProfileString("Antilag", "DeleteObjects", Text, ".\\Config.ini");

	wsprintf(Text, "%d", (this->DeleteFloor) ? 1 : 0);
	WritePrivateProfileString("Antilag", "DeleteFloor", Text, ".\\Config.ini");

	wsprintf(Text, "%d", (this->DeleteSkills) ? 1 : 0);
	WritePrivateProfileString("Antilag", "DeleteSkills", Text, ".\\Config.ini");

	wsprintf(Text, "%d", (this->DeleteStaticEffects) ? 1 : 0);
	WritePrivateProfileString("Antilag", "DeleteStaticEffects", Text, ".\\Config.ini");

	wsprintf(Text, "%d", (this->DeleteDynamicEffects) ? 1 : 0);
	WritePrivateProfileString("Antilag", "DeleteDynamicEffects", Text, ".\\Config.ini");

	wsprintf(Text, "%d", (this->DeleteInterface) ? 1 : 0);
	WritePrivateProfileString("Antilag", "DeleteInterface", Text, ".\\Config.ini");
}

void COptionsMenu::Init()
{
	this->ApplyAntilagDefaults();

	SetCompleteHook(0xE8, 0x0051AF65, &this->CheckErrorMessage);

	SetCompleteHook(0xE9, 0x0051B3CA, &this->OptionsWindowUI);

	SetCompleteHook(0xE9, 0x00515954, &this->OptionsWindowControl);

	MemorySet(0x004C0026, 0x90, 13);
}

void COptionsMenu::CheckErrorMessage()
{
	if (!ErrorMessage)
	{
		gOptionsMenu.CurrentOption = OPTION_NONE;
	}

	DisableAlphaBlend();
}

_declspec(naked) void COptionsMenu::OptionsWindowUI()
{
	static DWORD jmpBack = 0x0051C173;

	_asm
	{
		Pushad;
	}

	gOptionsMenu.RenderOptionsMenu();

	_asm
	{
		Popad;
		Jmp[jmpBack];
	}
}

__declspec(naked) void COptionsMenu::OptionsWindowControl()
{
	static DWORD jmpBack = 0x0051AE80;

	_asm
	{
		Pushad;
	}

	if (gOptionsMenu.CheckOptionsMenu())
	{
		goto EXIT;
	}

	MouseLButton = false;

	MouseLButtonPush = false;

	MouseUpdateTime = 0;

	MouseUpdateTimeMax = 6;

EXIT:

	_asm
	{
		Popad;
		Jmp[jmpBack];
	}
}

static const int g_TabIds[4] = { 1, 2, 3, 4 };

void COptionsMenu::RenderOptionsMenu()
{
	DWORD backupBgTextColor = SetBackgroundTextColor;

	DWORD backupTextColor = SetTextColor;

	eMenuOptions cur = (this->CurrentOption == OPTION_NONE) ? OPTION_GENERAL : this->CurrentOption;

	this->RenderFrame(cur);

	switch (cur)
	{
		case OPTION_ANTILAG:
		{
			this->RenderAntilag();

			break;
		}

		case OPTION_WINDOW:
		{
			this->RenderScreen();

			break;
		}

		case OPTION_FONT:
		{
			this->RenderFont();

			break;
		}

		default:
		{
			this->RenderGeneral();

			break;
		}
	}

	SetBackgroundTextColor = backupBgTextColor;

	SetTextColor = backupTextColor;
}

bool COptionsMenu::CheckOptionsMenu()
{
	eMenuOptions cur = (this->CurrentOption == OPTION_NONE) ? OPTION_GENERAL : this->CurrentOption;

	if (this->CheckFrame())
	{
		return true;
	}

	switch (cur)
	{
		case OPTION_ANTILAG:
		{
			return this->CheckAntilag();
		}

		case OPTION_WINDOW:
		{
			return this->CheckScreen();
		}

		case OPTION_FONT:
		{
			return this->CheckFont();
		}

		default:
		{
			return this->CheckGeneral();
		}
	}

	return false;
}

static void FlatRect(float x, float y, float w, float h, float r, float g, float b)
{
	EnableAlphaTest(true);

	glColor3f(r, g, b);

	RenderColor(x, y, w, h);
}

// Flat arrow button (state: 0xFE normal, 0xFF hover, 0x100 pressed)
static void DrawArrowButton(float x, float y, float w, float h, bool right, int state)
{
	FlatRect(x, y, w, h, 0.23f, 0.27f, 0.32f);

	if (state == 0x100)
	{
		FlatRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 0.30f, 0.56f, 0.62f);
	}
	else if (state == 0xFF)
	{
		FlatRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 0.17f, 0.21f, 0.25f);
	}
	else
	{
		FlatRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 0.09f, 0.11f, 0.13f);
	}

	const int steps = 4;

	const float step = h / 13.0f;

	const float cx = x + w / 2.0f;

	const float cy = y + h / 2.0f;

	if (state == 0xFF || state == 0x100)
	{
		glColor3f(1.0f, 1.0f, 1.0f);
	}
	else
	{
		glColor3f(0.435f, 0.714f, 0.769f);
	}

	for (int k = 0; k < steps; k++)
	{
		float hh = (float)(steps - k) * 2.0f * step;

		float xx = right ? (cx - (float)steps * step / 2.0f + (float)k * step) : (cx + (float)steps * step / 2.0f - (float)(k + 1) * step);

		RenderColor(xx, cy - hh / 2.0f, step, hh);
	}

	DisableAlphaBlend();

	glColor3f(1.0f, 1.0f, 1.0f);
}

static void TabLayout(float PanelX, float PanelY, int index, float& x, float& y, float& w)
{
	x = PanelX + (float)PANEL_PAD;

	w = (float)(SIDE_W - PANEL_PAD);

	if (index < 4)
	{
		y = PanelY + (float)(HEADER_H + PANEL_PAD) + (float)(index * (TAB_H + TAB_GAP));
	}
	else
	{
		y = PanelY + (float)(PANEL_H - PANEL_PAD - TAB_H);
	}
}

void COptionsMenu::RenderFrame(eMenuOptions cur)
{
	const float X = this->PanelX;

	const float Y = this->PanelY;

	/* border + body */
	FlatRect(X - 1.0f, Y - 1.0f, (float)PANEL_W + 2.0f, (float)PANEL_H + 2.0f, 0.23f, 0.27f, 0.32f);
	FlatRect(X, Y, (float)PANEL_W, (float)PANEL_H, 0.09f, 0.11f, 0.13f);

	/* sidebar */
	FlatRect(X, Y + (float)HEADER_H, (float)SIDE_W, (float)(PANEL_H - HEADER_H), 0.07f, 0.085f, 0.10f);
	FlatRect(X + (float)SIDE_W, Y + (float)HEADER_H, 1.0f, (float)(PANEL_H - HEADER_H), 0.23f, 0.27f, 0.32f);

	/* header */
	FlatRect(X, Y + (float)(HEADER_H - 1), (float)PANEL_W, 1.0f, 0.23f, 0.27f, 0.32f);
	FlatRect(X, Y, 3.0f, (float)(HEADER_H - 1), 0.435f, 0.714f, 0.769f);

	SetBackgroundTextColor = Color4b(255, 255, 255, 0);

	SetTextColor = Color4b(255, 255, 255, 255);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s", GlobalText[385]);

	EnableAlphaTest(true);

	RenderText((int)X + 12, CenterTextPosY(Text, (int)(Y + (float)(HEADER_H / 2))), Text, REAL_WIDTH(PANEL_W - 24), RT3_SORT_LEFT, NULL);

	/* tabs */
	const int textIds[5] = { 919, 926, 920, 921, 388 };

	for (int i = 0; i < 5; i++)
	{
		float x, y, w;

		TabLayout(X, Y, i, x, y, w);

		bool selected = (i < 4 && g_TabIds[i] == (int)cur);

		bool hover = IsWorkZone((int)x, (int)y, (int)w, TAB_H);

		if (selected)
		{
			FlatRect(x, y, w, (float)TAB_H, 0.17f, 0.21f, 0.25f);
			FlatRect(x, y, 3.0f, (float)TAB_H, 0.435f, 0.714f, 0.769f);
		}
		else if (hover)
		{
			FlatRect(x, y, w, (float)TAB_H, 0.12f, 0.15f, 0.18f);
			FlatRect(x, y, 3.0f, (float)TAB_H, 0.23f, 0.27f, 0.32f);
		}

		SetBackgroundTextColor = Color4b(255, 255, 255, 0);

		SetTextColor = (selected || hover) ? Color4b(255, 255, 255, 255) : Color4b(205, 214, 222, 255);

		sprintf_s(Text, "%s", GlobalText[textIds[i]]);

		EnableAlphaTest(true);

		RenderText((int)x + 12, CenterTextPosY(Text, (int)(y + (float)(TAB_H / 2))), Text, REAL_WIDTH((int)w - 14), RT3_SORT_LEFT, NULL);
	}

	DisableAlphaBlend();

	glColor3f(1.0f, 1.0f, 1.0f);

	SetBackgroundTextColor = Color4b(255, 255, 255, 0);

	SetTextColor = Color4b(255, 255, 255, 255);
}

bool COptionsMenu::CheckFrame()
{
	for (int i = 0; i < 5; i++)
	{
		float x, y, w;

		TabLayout(this->PanelX, this->PanelY, i, x, y, w);

		if (IsWorkZone((int)x, (int)y, (int)w, TAB_H))
		{
			if (MouseLButton && MouseLButtonPush)
			{
				MouseLButtonPush = false;

				MouseUpdateTime = 0;

				MouseUpdateTimeMax = 6;

				PlayBuffer(25, 0, 0);

				if (i < 4)
				{
					this->CurrentOption = (eMenuOptions)g_TabIds[i];
				}
				else
				{
					this->CurrentOption = OPTION_NONE;

					ErrorMessage = *(BYTE*)0x083A7C28;

					*(BYTE*)0x083A7C28 = 0;
				}
			}

			return true;
		}
	}

	/* the rest of the panel swallows clicks so they do not reach the game behind it */
	return false;
}

void COptionsMenu::RenderAllOptions()
{
	float Width = (float)this->BoxWidth;

	float Height = (float)this->BoxHeight;

	float PosX = this->MainPosX;

	float PosY = this->MainPosY;

	SetBackgroundTextColor = Color4b(255, 255, 255, 0);
	SetTextColor = Color4b(255, 255, 255, 255);

	char Text[32] = { 0 };

	/* OPTIONS */
	DisableAlphaBlend();
	glColor3f(0.8f, 0.6f, 0.4f);
	RenderBitmap(240, PosX, PosY, Width, Height, (0.0f / 256.0f), (0.0f / 64.0f), (213.0f / 256.0f), (64.0f / 64.0f), true, true);
	sprintf_s(Text, "%s", GlobalText[385]);
	EnableAlphaTest(true);
	RenderText((int)PosX, CenterTextPosY(Text, (int)(PosY + (Height / 2.0f))), Text, REAL_WIDTH((int)Width), RT3_SORT_CENTER, NULL);
	PosY += (Height + ROW_GAP);

	/* GENERAL */
	this->RenderBox(PosX, PosY, Width, Height);
	sprintf_s(Text, "%s", GlobalText[919]);
	EnableAlphaTest(true);
	RenderText((int)PosX, CenterTextPosY(Text, (int)(PosY + (Height / 2.0f))), Text, REAL_WIDTH((int)Width), RT3_SORT_CENTER, NULL);
	PosY += (Height + ROW_GAP);

	/* ANTILAG */
	this->RenderBox(PosX, PosY, Width, Height);
	sprintf_s(Text, "%s", GlobalText[926]);
	EnableAlphaTest(true);
	RenderText((int)PosX, CenterTextPosY(Text, (int)(PosY + (Height / 2.0f))), Text, REAL_WIDTH((int)Width), RT3_SORT_CENTER, NULL);
	PosY += (Height + ROW_GAP);

	/* SCREEN */
	this->RenderBox(PosX, PosY, Width, Height);
	sprintf_s(Text, "%s", GlobalText[920]);
	EnableAlphaTest(true);
	RenderText((int)PosX, CenterTextPosY(Text, (int)(PosY + (Height / 2.0f))), Text, REAL_WIDTH((int)Width), RT3_SORT_CENTER, NULL);
	PosY += (Height + ROW_GAP);

	/* FONT */
	this->RenderBox(PosX, PosY, Width, Height);
	sprintf_s(Text, "%s", GlobalText[921]);
	EnableAlphaTest(true);
	RenderText((int)PosX, CenterTextPosY(Text, (int)(PosY + (Height / 2.0f))), Text, REAL_WIDTH((int)Width), RT3_SORT_CENTER, NULL);
	PosY += (Height + ROW_GAP);

	/* CLOSE */
	this->RenderBox(PosX, PosY, Width, Height);
	sprintf_s(Text, "%s", GlobalText[388]);
	EnableAlphaTest(true);
	RenderText((int)PosX, CenterTextPosY(Text, (int)(PosY + (Height / 2.0f))), Text, REAL_WIDTH((int)Width), RT3_SORT_CENTER, NULL);
	PosY += (Height + ROW_GAP);
}

bool COptionsMenu::CheckAllOptions()
{
	int Width = this->BoxWidth;

	int Height = this->BoxHeight;

	int PosX = (int)this->MainPosX;

	int PosY = (int)this->MainPosY + (Height + ROW_GAP);

	for (int i = OPTION_GENERAL; i < MAX_OPTIONS; i++)
	{
		if (IsWorkZone(PosX, PosY, Width, Height))
		{
			if (MouseLButton && MouseLButtonPush)
			{
				MouseLButtonPush = false;

				MouseUpdateTime = 0;

				MouseUpdateTimeMax = 6;

				PlayBuffer(25, 0, 0);

				this->CurrentOption = (eMenuOptions)i;

				return true;
			}
		}

		PosY += (Height + ROW_GAP);
	}

	if (this->CheckClose(PosX, PosY))
	{
		return true;
	}

	return false;
}

void COptionsMenu::RenderGeneral()
{
	float PosX = this->MainPosX;

	float PosY = this->MainPosY;

	SetBackgroundTextColor = Color4b(255, 255, 255, 0);
	SetTextColor = Color4b(255, 255, 255, 255);

	this->RenderGeneralTitle(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	if (SceneFlag == CHARACTER_SCENE || SceneFlag == MAIN_SCENE)
	{
		this->RenderLanguage(PosX, PosY);
		PosY += ((float)this->BoxHeight + ROW_GAP);
	}

	this->RenderPVPWithoutControl(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderAutomaticAttack(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderSoundOnWhisper(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderSoundVolume(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderMusicVolume(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	if (m_MusicOnOff)
	{
		this->RenderMusicControls(PosX, PosY);
		PosY += ((float)this->BoxHeight + ROW_GAP);
	}

}

bool COptionsMenu::CheckGeneral()
{
	int PosX = (int)this->MainPosX;

	int PosY = (int)this->MainPosY + (this->BoxHeight + ROW_GAP);

	if (SceneFlag == CHARACTER_SCENE || SceneFlag == MAIN_SCENE)
	{
		if (this->CheckLanguage(PosX, PosY))
		{
			return true;
		}

		PosY += (this->BoxHeight + ROW_GAP);
	}

	if (this->CheckPVPWithoutControl(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckAutomaticAttack(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckSoundOnWhisper(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckSoundVolume(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckMusicVolume(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (m_MusicOnOff)
	{
		if (this->CheckMusicControls(PosX, PosY))
		{
			return true;
		}

		PosY += (this->BoxHeight + ROW_GAP);
	}


	return false;
}

void COptionsMenu::RenderGeneralTitle(float PosX, float PosY)
{
	return; // the sidebar tab already names the section

	DisableAlphaBlend();

	glColor3f(0.8f, 0.6f, 0.4f);

	RenderBitmap(240, PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight, (0.0f / 256.0f), (0.0f / 64.0f), (213.0f / 256.0f), (64.0f / 64.0f), true, true);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s", GlobalText[919]);

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

void COptionsMenu::RenderLanguage(float PosX, float PosY)
{
	DisableAlphaBlend();

	glColor3f(1.0f, 1.0f, 1.0f);

	RenderBitmap(240, PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight, (0.0f / 256.0f), (0.0f / 64.0f), (213.0f / 256.0f), (64.0f / 64.0f), true, true);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s", gLanguage.iLanguageValues[gLanguage.LangNum].first.c_str());

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);

	if (gLanguage.LangNum > LANGUAGE_ENGLISH)
	{
		if (IsWorkZone((int)PosX, (int)PosY, this->BoxHeight, this->BoxHeight))
		{
			if (MouseLButtonPush)
			{
				DrawArrowButton(PosX, PosY, (float)this->BoxHeight, (float)this->BoxHeight, false, 0x100);
			}
			else
			{
				DrawArrowButton(PosX, PosY, (float)this->BoxHeight, (float)this->BoxHeight, false, 0xFF);
			}
		}
		else
		{
			DrawArrowButton(PosX, PosY, (float)this->BoxHeight, (float)this->BoxHeight, false, 0xFE);
		}
	}

	if (gLanguage.LangNum < LANGUAGE_PORTUGUESE)
	{
		if (IsWorkZone((int)PosX + (this->BoxWidth - this->BoxHeight), (int)PosY, this->BoxHeight, this->BoxHeight))
		{
			if (MouseLButtonPush)
			{
				DrawArrowButton(PosX + (float)(this->BoxWidth - this->BoxHeight), PosY, (float)this->BoxHeight, (float)this->BoxHeight, true, 0x100);
			}
			else
			{
				DrawArrowButton(PosX + (float)(this->BoxWidth - this->BoxHeight), PosY, (float)this->BoxHeight, (float)this->BoxHeight, true, 0xFF);
			}
		}
		else
		{
			DrawArrowButton(PosX + (float)(this->BoxWidth - this->BoxHeight), PosY, (float)this->BoxHeight, (float)this->BoxHeight, true, 0xFE);
		}
	}
}

bool COptionsMenu::CheckLanguage(int PosX, int PosY)
{
	if (gLanguage.LangNum > LANGUAGE_ENGLISH)
	{
		if (IsWorkZone(PosX, PosY, this->BoxHeight, this->BoxHeight))
		{
			if (MouseLButton && MouseLButtonPush)
			{
				MouseLButtonPush = false;

				MouseUpdateTime = 0;

				MouseUpdateTimeMax = 6;

				PlayBuffer(25, 0, 0);

				sprintf_s(gLanguage.m_Language, "%s", gLanguage.iLanguageValues[gLanguage.LangNum - 1].second.c_str());

				gLanguage.ReloadLanguage();
			}

			return true;
		}
	}

	if (gLanguage.LangNum < LANGUAGE_PORTUGUESE)
	{
		if (IsWorkZone(PosX + (this->BoxWidth - this->BoxHeight), PosY, this->BoxHeight, this->BoxHeight))
		{
			if (MouseLButton && MouseLButtonPush)
			{
				MouseLButtonPush = false;

				MouseUpdateTime = 0;

				MouseUpdateTimeMax = 6;

				PlayBuffer(25, 0, 0);

				sprintf_s(gLanguage.m_Language, "%s", gLanguage.iLanguageValues[gLanguage.LangNum + 1].second.c_str());

				gLanguage.ReloadLanguage();
			}

			return true;
		}
	}

	return false;
}

void COptionsMenu::RenderPVPWithoutControl(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s: %s", GlobalText[936], this->PVPWithoutControl ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckPVPWithoutControl(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			this->PVPWithoutControl ^= 1u;

			if (this->PVPWithoutControl)
			{
				SetCompleteHook(0xE9, 0x0048322F, 0x00483240);

				SetCompleteHook(0xE9, 0x0044B532, 0x0044B550);
			}
			else
			{
				BYTE replace[5] = { 0x6A, 0x11, 0xFF, 0x15, 0x98 };

				MemoryCpy(0x0048322F, replace, sizeof(replace));

				MemoryCpy(0x0044B532, replace, sizeof(replace));
			}
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderAutomaticAttack(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s: %s", GlobalText[386], m_bAutoAttack ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckAutomaticAttack(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			m_bAutoAttack ^= 1u;
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderSoundOnWhisper(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s: %s", GlobalText[387], m_bWhisperSound ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckSoundOnWhisper(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			m_bWhisperSound ^= 1u;
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderSoundVolume(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	if (!m_SoundOnOff)
	{
		char Text[64] = { 0 };

		sprintf_s(Text, "%s: Off", GlobalText[922]);

		EnableAlphaTest(true);

		RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
	}
	else
	{
		float Width = (float)this->BoxWidth / 3.0f;

		char Text[64] = { 0 };

		sprintf_s(Text, "%s:", GlobalText[922]);

		EnableAlphaTest(true);

		RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH((int)Width), RT3_SORT_CENTER, NULL);

		PosX += Width;

		Width *= 2.0f;

		Width -= 8.0f;

		float Height = 8.0f;

		PosY += (((float)this->BoxHeight - Height) / 2.0f);

		EnableAlphaTest(true);

		glColor3f(0.2f, 0.2f, 0.2f);

		RenderColor(PosX, PosY, Width + 2.0f, Height);

		glColor3f(0.0f, 0.0f, 0.0f);

		RenderColor(PosX + 1, PosY + 1.0f, Width, Height - 2.0f);

		glColor3f(1.0f, 1.0f, 1.0f);

		for (int i = 0; i < MAX_SOUND_LEVEL; i++)
		{
			if (i >= gSound.SoundVolumeLevel)
			{
				glColor3f(0.0f, 0.0f, 0.0f);
			}
			else
			{
				glColor3f(1.0f, 1.0f, 1.0f);
			}

			RenderColor(PosX + 1, PosY + 1.0f, (Width / MAX_SOUND_LEVEL), Height - 2.0f);

			PosX += (Width / MAX_SOUND_LEVEL);
		}

		DisableAlphaBlend();

		glColor3f(1.0f, 1.0f, 1.0f);
	}
}

bool COptionsMenu::CheckSoundVolume(int PosX, int PosY)
{
	if (!m_SoundOnOff)
	{
		return false;
	}

	int MaxWidth = (this->BoxWidth / 3 * 2) - 8;

	int Height = 8;

	PosX += (this->BoxWidth / 3);

	PosY += ((this->BoxHeight - Height) / 2);

	int Width = ((MaxWidth - 2) / MAX_SOUND_LEVEL);

	MaxWidth -= Width;

	int X = PosX + 1;

	int volumeLevel = 0;

	while (MouseX < X || MouseX > X + Width || MouseY < PosY + 2 || MouseY > PosY + (Height - 1) || !MouseLButtonPush)
	{
		X += Width;

		volumeLevel++;

		if (X >= (PosX + MaxWidth))
		{
			return false;
		}
	}

	gSound.UpdateSoundVolumeLevel(volumeLevel);

	return true;
}

void COptionsMenu::RenderMusicVolume(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	if (!m_MusicOnOff)
	{
		char Text[64] = { 0 };

		sprintf_s(Text, "%s: Off", GlobalText[923]);

		EnableAlphaTest(true);

		RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
	}
	else
	{
		float Width = (float)this->BoxWidth / 3.0f;

		char Text[64] = { 0 };

		sprintf_s(Text, "%s:", GlobalText[923]);

		EnableAlphaTest(true);

		RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH((int)Width), RT3_SORT_CENTER, NULL);

		PosX += Width;

		Width *= 2.0f;

		Width -= 8.0f;

		float Height = 8.0f;

		PosY += (((float)this->BoxHeight - Height) / 2.0f);

		EnableAlphaTest(true);

		glColor3f(0.2f, 0.2f, 0.2f);

		RenderColor(PosX, PosY, Width + 2.0f, Height);

		glColor3f(0.0f, 0.0f, 0.0f);

		RenderColor(PosX + 1, PosY + 1.0f, Width, Height - 2.0f);

		glColor3f(1.0f, 1.0f, 1.0f);

		for (int i = 0; i < MAX_MUSIC_LEVEL; i++)
		{
			if (i >= gSound.MusicVolumeLevel)
			{
				glColor3f(0.0f, 0.0f, 0.0f);
			}
			else
			{
				glColor3f(1.0f, 1.0f, 1.0f);
			}

			RenderColor(PosX + 1, PosY + 1.0f, (Width / MAX_MUSIC_LEVEL), Height - 2.0f);

			PosX += (Width / MAX_MUSIC_LEVEL);
		}

		DisableAlphaBlend();

		glColor3f(1.0f, 1.0f, 1.0f);
	}
}

bool COptionsMenu::CheckMusicVolume(int PosX, int PosY)
{
	if (!m_MusicOnOff)
	{
		return false;
	}

	int MaxWidth = (this->BoxWidth / 3 * 2) - 8;

	int Height = 8;

	PosX += (this->BoxWidth / 3);

	PosY += ((this->BoxHeight - Height) / 2);

	int Width = ((MaxWidth - 2) / MAX_MUSIC_LEVEL);

	MaxWidth -= Width;

	int X = PosX + 1;

	int volumeLevel = 0;

	while (MouseX < X || MouseX > X + Width || MouseY < PosY + 2 || MouseY > PosY + (Height - 1) || !MouseLButtonPush)
	{
		X += Width;

		volumeLevel++;

		if (X >= (PosX + MaxWidth))
		{
			return false;
		}
	}

	gSound.UpdateMusicVolumeLevel(volumeLevel);

	return true;
}

void COptionsMenu::RenderMusicControls(float PosX, float PosY)
{
	float MaxWidth = (float)this->BoxWidth / 2.0f;

	float StartPosX = PosX + (MaxWidth / 2.0f);

	this->RenderBox(StartPosX, PosY, MaxWidth, (float)this->BoxHeight);

	float Width = ((float)this->BoxHeight / 2.0f);

	StartPosX += ((MaxWidth - Width) / 2.0f);

	PosY += (((float)this->BoxHeight - Width) / 2.0f);

	EnableAlphaTest(true);

	if (gSound.MusicPlaying)
	{
		glColor3f(1.0f, 0.0f, 0.0f);

		RenderColor(StartPosX, PosY, Width, Width);
	}
	else
	{
		glColor3f(0.0f, 1.0f, 0.0f);

		RenderTriangleColor(StartPosX, PosY, Width, Width);
	}

	DisableAlphaBlend();

	glColor3f(1.0f, 1.0f, 1.0f);
}

bool COptionsMenu::CheckMusicControls(int PosX, int PosY)
{
	int Width = this->BoxWidth / 2;

	int StartPosX = (int)PosX + (Width / 2);

	if (IsWorkZone(StartPosX, PosY, Width, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			if (gSound.MusicPlaying)
			{
				gSound.ButtonStopMusic();
			}
			else
			{
				gSound.ButtonPlayMusic();
			}
		}

		return true;
	}

	return false;
}

void COptionsMenu::ApplyAntilagDefaults()
{
	if (this->DeleteShadows)
	{
		SetCompleteHook(0xE9, 0x00441F0D, 0x00442086);

		SetByte(0x00441F0D + 5, 0x90);
	}
	else
	{
		BYTE replace[6] = { 0x0F, 0x84, 0x73, 0x01, 0x00, 0x00 };

		MemoryCpy(0x00441F0D, replace, sizeof(replace));
	}

	if (this->DeleteObjects)
	{
		SetByte(0x004FD800, 0xC3);
	}
	else
	{
		SetByte(0x004FD800, 0x83);
	}

	if (this->DeleteFloor)
	{
		SetByte(0x004F9AC0, 0xC3);
	}
	else
	{
		SetByte(0x004F9AC0, 0x51);
	}

	if (this->DeleteSkills)
	{
		SetByte(0x00473710, 0xC3); // RenderJoints

		SetByte(0x0046BBA0, 0xC3); // RenderEffects

		SetByte(0x004F76C0, 0xC3); // AddTerrainLight
	}
	else
	{
		SetByte(0x00473710, 0x83); // RenderJoints

		SetByte(0x0046BBA0, 0x83); // RenderEffects

		SetByte(0x004F76C0, 0x83); // AddTerrainLight
	}

	if (this->DeleteStaticEffects)
	{
		SetByte(0x00479670, 0xC3); // RenderSprite
	}
	else
	{
		SetByte(0x00479670, 0x8B); // RenderSprite
	}

	if (this->DeleteDynamicEffects)
	{
		SetByte(0x00478C00, 0xC3); // RenderParticles
	}
	else
	{
		SetByte(0x00478C00, 0x83); // RenderParticles
	}

	if (this->DeleteInterface)
	{
		SetByte(0x00525483, 0x84); // Skip Move Interface

		SetByte(0x00525CB5, 0x74); // Skip Render Interface

		SetDword(0x00525B69, 0x1E0); // Height for BeginOpengl

		SetByte(0x0047FCE0, 0xC3); // Skip Render Notices
	}
	else
	{
		SetByte(0x00525483, 0x85); // Skip Move Interface

		SetByte(0x00525CB5, 0x75); // Skip Render Interface

		SetDword(0x00525B69, 0x1B0); // Height for BeginOpengl

		SetByte(0x0047FCE0, 0x83); // Skip Render Notices
	}
}

void COptionsMenu::RenderAntilag()
{
	float PosX = this->MainPosX;

	float PosY = this->MainPosY;

	SetBackgroundTextColor = Color4b(255, 255, 255, 0);
	SetTextColor = Color4b(255, 255, 255, 255);

	this->RenderAntilagTitle(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderDeleteShadows(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderDeleteObjects(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderDeleteFloor(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderDeleteSkills(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderDeleteStaticEffects(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderDeleteDynamicEffects(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderDeleteWings(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderDeleteHPBar(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderDeleteInterface(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

}

bool COptionsMenu::CheckAntilag()
{
	int PosX = (int)this->MainPosX;

	int PosY = (int)this->MainPosY + (this->BoxHeight + ROW_GAP);

	if (this->CheckDeleteShadows(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckDeleteObjects(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckDeleteFloor(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckDeleteSkills(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckDeleteStaticEffects(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckDeleteDynamicEffects(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckDeleteWings(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckDeleteHPBar(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckDeleteInterface(PosX, PosY))
	{
		return true;
	}


	return false;
}

void COptionsMenu::RenderAntilagTitle(float PosX, float PosY)
{
	return; // the sidebar tab already names the section

	DisableAlphaBlend();

	glColor3f(0.8f, 0.6f, 0.4f);

	RenderBitmap(240, PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight, (0.0f / 256.0f), (0.0f / 64.0f), (213.0f / 256.0f), (64.0f / 64.0f), true, true);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s", GlobalText[926]);

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

void COptionsMenu::RenderDeleteShadows(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s: %s", GlobalText[927], this->DeleteShadows ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckDeleteShadows(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			this->DeleteShadows ^= 1;

			if (this->DeleteShadows)
			{
				SetCompleteHook(0xE9, 0x00441F0D, 0x00442086);

				SetByte(0x00441F0D + 5, 0x90);
			}
			else
			{
				BYTE replace[6] = { 0x0F, 0x84, 0x73, 0x01, 0x00, 0x00 };

				MemoryCpy(0x00441F0D, replace, sizeof(replace));
			}
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderDeleteObjects(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s: %s", GlobalText[928], this->DeleteObjects ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckDeleteObjects(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			this->DeleteObjects ^= 1;

			if (this->DeleteObjects)
			{
				SetByte(0x004FD800, 0xC3);
			}
			else
			{
				SetByte(0x004FD800, 0x83);
			}
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderDeleteFloor(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s: %s", GlobalText[929], this->DeleteFloor ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckDeleteFloor(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			this->DeleteFloor ^= 1;

			if (this->DeleteFloor)
			{
				SetByte(0x004F9AC0, 0xC3);
			}
			else
			{
				SetByte(0x004F9AC0, 0x51);
			}
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderDeleteSkills(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s: %s", GlobalText[930], this->DeleteSkills ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckDeleteSkills(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			this->DeleteSkills ^= 1;

			if (this->DeleteSkills)
			{
				SetByte(0x00473710, 0xC3); // RenderJoints

				SetByte(0x0046BBA0, 0xC3); // RenderEffects

				SetByte(0x004F76C0, 0xC3); // AddTerrainLight
			}
			else
			{
				SetByte(0x00473710, 0x83); // RenderJoints

				SetByte(0x0046BBA0, 0x83); // RenderEffects

				SetByte(0x004F76C0, 0x83); // AddTerrainLight
			}
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderDeleteStaticEffects(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s: %s", GlobalText[931], this->DeleteStaticEffects ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckDeleteStaticEffects(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			this->DeleteStaticEffects ^= 1;

			if (this->DeleteStaticEffects)
			{
				SetByte(0x00479670, 0xC3); // RenderSprite
			}
			else
			{
				SetByte(0x00479670, 0x8B); // RenderSprite
			}
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderDeleteDynamicEffects(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s: %s", GlobalText[932], this->DeleteDynamicEffects ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckDeleteDynamicEffects(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			this->DeleteDynamicEffects ^= 1;

			if (this->DeleteDynamicEffects)
			{
				SetByte(0x00478C00, 0xC3); // RenderParticles
			}
			else
			{
				SetByte(0x00478C00, 0x83); // RenderParticles
			}
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderDeleteWings(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s: %s", GlobalText[933], gWeaponView.DeleteWings ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckDeleteWings(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			gWeaponView.DeleteWings ^= 1;
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderDeleteHPBar(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s: %s", GlobalText[934], gHealthBar.DeleteHealthBar ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckDeleteHPBar(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			gHealthBar.DeleteHealthBar ^= 1;
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderDeleteInterface(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s: %s", GlobalText[935], this->DeleteInterface ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckDeleteInterface(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			this->DeleteInterface ^= 1;

			if (this->DeleteInterface)
			{
				SetByte(0x00525483, 0x84); // Skip Move Interface

				SetByte(0x00525CB5, 0x74); // Skip Render Interface

				SetDword(0x00525B69, 0x1E0); // Height for BeginOpengl

				SetByte(0x0047FCE0, 0xC3); // Skip Render Notices
			}
			else
			{
				SetByte(0x00525483, 0x85); // Skip Move Interface

				SetByte(0x00525CB5, 0x75); // Skip Render Interface

				SetDword(0x00525B69, 0x1B0); // Height for BeginOpengl

				SetByte(0x0047FCE0, 0x83); // Skip Render Notices
			}
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderScreen()
{
	float PosX = this->MainPosX;

	float PosY = this->MainPosY;

	SetBackgroundTextColor = Color4b(255, 255, 255, 0);
	SetTextColor = Color4b(255, 255, 255, 255);

	this->RenderScreenTitle(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderWindowMode(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	if (gWindow.m_WindowMode)
	{
		this->RenderBorderless(PosX, PosY);
		PosY += ((float)this->BoxHeight + ROW_GAP);
	}

	this->RenderResolutions(PosX, PosY);

}

bool COptionsMenu::CheckScreen()
{
	int PosX = (int)this->MainPosX;

	int PosY = (int)this->MainPosY + (this->BoxHeight + ROW_GAP);

	if (this->CheckWindowMode(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (gWindow.m_WindowMode)
	{
		if (this->CheckBorderless(PosX, PosY))
		{
			return true;
		}

		PosY += (this->BoxHeight + ROW_GAP);
	}

	if (this->CheckResolutions(PosX, PosY))
	{
		return true;
	}


	return false;
}

void COptionsMenu::RenderScreenTitle(float PosX, float PosY)
{
	return; // the sidebar tab already names the section

	DisableAlphaBlend();

	glColor3f(0.8f, 0.6f, 0.4f);

	RenderBitmap(240, PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight, (0.0f / 256.0f), (0.0f / 64.0f), (213.0f / 256.0f), (64.0f / 64.0f), true, true);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s", GlobalText[920]);

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

void COptionsMenu::RenderWindowMode(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s", gWindow.m_WindowMode ? "Windowed" : "FullScreen");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckWindowMode(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			gWindow.ChangeWindowState(gWindow.m_WindowMode ^ 1, gWindow.m_Borderless, m_Resolution);
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderBorderless(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s: %s", "Borderless", gWindow.m_Borderless ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckBorderless(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			gWindow.ChangeWindowState(gWindow.m_WindowMode, gWindow.m_Borderless ^ 1, m_Resolution);
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderResolutions(float PosX, float& PosY)
{
	this->Resolutions[m_Resolution] = 1;

	char Text[64] = { 0 };

	float cw = (float)(this->BoxWidth - 4) / 2.0f;

	for (int i = 0; i < MAX_RESOLUTION_VALUE; i++)
	{
		float x = PosX + (float)(i % 2) * (cw + 4.0f);

		DisableAlphaBlend();

		glColor3f(1.0f, 1.0f, 1.0f);

		RenderBitmap(240, x, PosY, cw, (float)this->BoxHeight, (0.0f / 256.0f), (0.0f / 64.0f), (213.0f / 256.0f), (64.0f / 64.0f), true, true);

		if (this->Resolutions[i])
		{
			glColor3f(1.0f, 1.0f, 0.0f);

			RenderBitmap(240, x, PosY, cw, (float)this->BoxHeight, (0.0f / 256.0f), (0.0f / 64.0f), (213.0f / 256.0f), (64.0f / 64.0f), true, true);

			glColor3f(1.0f, 1.0f, 1.0f);

			DisableAlphaBlend();
		}
		else if (IsWorkZone((int)x, (int)PosY, (int)cw, this->BoxHeight))
		{
			EnableAlphaBlend();

			glColor3f(0.8f, 0.6f, 0.4f);

			RenderBitmap(240, x, PosY, cw, (float)this->BoxHeight, (0.0f / 256.0f), (0.0f / 64.0f), (213.0f / 256.0f), (64.0f / 64.0f), true, true);

			glColor3f(1.0f, 1.0f, 1.0f);

			DisableAlphaBlend();
		}

		sprintf_s(Text, "%d x %d", gWindow.iResolutionValues[i].first, gWindow.iResolutionValues[i].second);

		EnableAlphaTest(true);

		RenderText((int)x, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH((int)cw), RT3_SORT_CENTER, NULL);

		if (i % 2 == 1 || i == MAX_RESOLUTION_VALUE - 1)
		{
			PosY += ((float)this->BoxHeight + ROW_GAP);
		}
	}
}

bool COptionsMenu::CheckResolutions(int PosX, int& PosY)
{
	int cw = (this->BoxWidth - 4) / 2;

	for (int i = 0; i < MAX_RESOLUTION_VALUE; i++)
	{
		int x = PosX + (i % 2) * (cw + 4);

		if (IsWorkZone(x, PosY, cw, this->BoxHeight))
		{
			if (MouseLButton && MouseLButtonPush)
			{
				if (this->Resolutions[i] == 1)
				{
					return true;
				}

				MouseLButtonPush = false;

				MouseUpdateTime = 0;

				MouseUpdateTimeMax = 6;

				PlayBuffer(25, 0, 0);

				this->Resolutions[m_Resolution] = 0;

				gWindow.ChangeWindowState(gWindow.m_WindowMode, gWindow.m_Borderless, i);
			}

			return true;
		}

		if (i % 2 == 1 || i == MAX_RESOLUTION_VALUE - 1)
		{
			PosY += (this->BoxHeight + ROW_GAP);
		}
	}

	return false;
}

void COptionsMenu::RenderFont()
{
	float PosX = this->MainPosX;

	float PosY = this->MainPosY;

	SetBackgroundTextColor = Color4b(255, 255, 255, 0);
	SetTextColor = Color4b(255, 255, 255, 255);

	this->RenderFontTitle(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderFontStyle(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderFontBold(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderFontItalic(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderFontButtons(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

	this->RenderResetFonts(PosX, PosY);
	PosY += ((float)this->BoxHeight + ROW_GAP);

}

bool COptionsMenu::CheckFont()
{
	int PosX = (int)this->MainPosX;

	int PosY = (int)this->MainPosY + (this->BoxHeight + ROW_GAP);

	if (this->CheckFontStyle(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckFontBold(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckFontItalic(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckFontButtons(PosX, PosY))
	{
		return true;
	}

	PosY += (this->BoxHeight + ROW_GAP);

	if (this->CheckResetFonts(PosX, PosY))
	{
		return true;
	}


	return false;
}

void COptionsMenu::RenderFontTitle(float PosX, float PosY)
{
	return; // the sidebar tab already names the section

	DisableAlphaBlend();

	glColor3f(0.8f, 0.6f, 0.4f);

	RenderBitmap(240, PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight, (0.0f / 256.0f), (0.0f / 64.0f), (213.0f / 256.0f), (64.0f / 64.0f), true, true);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s", GlobalText[921]);

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

void COptionsMenu::RenderFontStyle(float PosX, float& PosY)
{
	DisableAlphaBlend();

	glColor3f(1.0f, 1.0f, 1.0f);

	RenderBitmap(240, PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight, (0.0f / 256.0f), (0.0f / 64.0f), (213.0f / 256.0f), (64.0f / 64.0f), true, true);

	char Text[64] = { 0 };

	try
	{
		sprintf_s(Text, "%s", gFont.AllFonts.at(gFont.CurrentFontNumber).c_str());
	}
	catch (...)
	{
		sprintf_s(Text, "%s", gFont.MyFontFaceName);
	}

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);

	if (gFont.CurrentFontNumber > 0)
	{
		if (IsWorkZone((int)PosX, (int)PosY, this->BoxHeight, this->BoxHeight))
		{
			if (MouseLButtonPush)
			{
				DrawArrowButton(PosX, PosY, (float)this->BoxHeight, (float)this->BoxHeight, false, 0x100);
			}
			else
			{
				DrawArrowButton(PosX, PosY, (float)this->BoxHeight, (float)this->BoxHeight, false, 0xFF);
			}
		}
		else
		{
			DrawArrowButton(PosX, PosY, (float)this->BoxHeight, (float)this->BoxHeight, false, 0xFE);
		}
	}

	if (gFont.CurrentFontNumber < (int)gFont.AllFonts.size() - 1)
	{
		if (IsWorkZone((int)PosX + (this->BoxWidth - this->BoxHeight), (int)PosY, this->BoxHeight, this->BoxHeight))
		{
			if (MouseLButtonPush)
			{
				DrawArrowButton(PosX + (float)(this->BoxWidth - this->BoxHeight), PosY, (float)this->BoxHeight, (float)this->BoxHeight, true, 0x100);
			}
			else
			{
				DrawArrowButton(PosX + (float)(this->BoxWidth - this->BoxHeight), PosY, (float)this->BoxHeight, (float)this->BoxHeight, true, 0xFF);
			}
		}
		else
		{
			DrawArrowButton(PosX + (float)(this->BoxWidth - this->BoxHeight), PosY, (float)this->BoxHeight, (float)this->BoxHeight, true, 0xFE);
		}
	}
}

bool COptionsMenu::CheckFontStyle(int PosX, int PosY)
{
	if (gFont.CurrentFontNumber > 0)
	{
		if (IsWorkZone(PosX, PosY, this->BoxHeight, this->BoxHeight))
		{
			if (MouseLButton && MouseLButtonPush)
			{
				MouseLButtonPush = false;

				MouseUpdateTime = 0;

				MouseUpdateTimeMax = 6;

				PlayBuffer(25, 0, 0);

				gFont.CurrentFontNumber--;

				try
				{
					sprintf_s(gFont.MyFontFaceName, "%s", gFont.AllFonts.at(gFont.CurrentFontNumber).c_str());

					gFont.ReloadFont();
				}
				catch (...)
				{
					gFont.CurrentFontNumber++;
				}
			}

			return true;
		}
	}

	if (gFont.CurrentFontNumber < (int)gFont.AllFonts.size() - 1)
	{
		if (IsWorkZone(PosX + (this->BoxWidth - this->BoxHeight), PosY, this->BoxHeight, this->BoxHeight))
		{
			if (MouseLButton && MouseLButtonPush)
			{
				MouseLButtonPush = false;

				MouseUpdateTime = 0;

				MouseUpdateTimeMax = 6;

				PlayBuffer(25, 0, 0);

				gFont.CurrentFontNumber++;

				try
				{
					sprintf_s(gFont.MyFontFaceName, "%s", gFont.AllFonts.at(gFont.CurrentFontNumber).c_str());

					gFont.ReloadFont();
				}
				catch (...)
				{
					gFont.CurrentFontNumber--;
				}
			}

			return true;
		}
	}

	return false;
}

void COptionsMenu::RenderFontBold(float PosX, float& PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "Bold: %s", (gFont.Bold) ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckFontBold(int PosX, int& PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			gFont.Bold ^= 1;

			gFont.ReloadFont();
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderFontItalic(float PosX, float& PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "Italic: %s", (gFont.Italic) ? "On" : "Off");

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckFontItalic(int PosX, int& PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			gFont.Italic ^= 1;

			gFont.ReloadFont();
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderFontButtons(float PosX, float& PosY)
{
	DisableAlphaBlend();

	glColor3f(1.0f, 1.0f, 1.0f);

	RenderBitmap(240, PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight, (0.0f / 256.0f), (0.0f / 64.0f), (213.0f / 256.0f), (64.0f / 64.0f), true, true);

	char Text[3] = { 0 };

	sprintf_s(Text, "%d", FontHeight);

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);

	if (FontHeight > 1)
	{
		if (IsWorkZone((int)PosX, (int)PosY, this->BoxHeight, this->BoxHeight))
		{
			if (MouseLButtonPush)
			{
				DrawArrowButton(PosX, PosY, (float)this->BoxHeight, (float)this->BoxHeight, false, 0x100);
			}
			else
			{
				DrawArrowButton(PosX, PosY, (float)this->BoxHeight, (float)this->BoxHeight, false, 0xFF);
			}
		}
		else
		{
			DrawArrowButton(PosX, PosY, (float)this->BoxHeight, (float)this->BoxHeight, false, 0xFE);
		}
	}

	if (FontHeight < 25)
	{
		if (IsWorkZone((int)PosX + (this->BoxWidth - this->BoxHeight), (int)PosY, this->BoxHeight, this->BoxHeight))
		{
			if (MouseLButtonPush)
			{
				DrawArrowButton(PosX + (float)(this->BoxWidth - this->BoxHeight), PosY, (float)this->BoxHeight, (float)this->BoxHeight, true, 0x100);
			}
			else
			{
				DrawArrowButton(PosX + (float)(this->BoxWidth - this->BoxHeight), PosY, (float)this->BoxHeight, (float)this->BoxHeight, true, 0xFF);
			}
		}
		else
		{
			DrawArrowButton(PosX + (float)(this->BoxWidth - this->BoxHeight), PosY, (float)this->BoxHeight, (float)this->BoxHeight, true, 0xFE);
		}
	}
}

bool COptionsMenu::CheckFontButtons(int PosX, int& PosY)
{
	if (FontHeight > 1)
	{
		if (IsWorkZone(PosX, PosY, this->BoxHeight, this->BoxHeight))
		{
			if (MouseLButton && MouseLButtonPush)
			{
				MouseLButtonPush = false;

				MouseUpdateTime = 0;

				MouseUpdateTimeMax = 6;

				PlayBuffer(25, 0, 0);

				FontHeight -= 1;

				gFont.ReloadFont();
			}

			return true;
		}
	}

	if (FontHeight < 25)
	{
		if (IsWorkZone(PosX + (this->BoxWidth - this->BoxHeight), PosY, this->BoxHeight, this->BoxHeight))
		{
			if (MouseLButton && MouseLButtonPush)
			{
				MouseLButtonPush = false;

				MouseUpdateTime = 0;

				MouseUpdateTimeMax = 6;

				PlayBuffer(25, 0, 0);

				FontHeight += 1;

				gFont.ReloadFont();
			}

			return true;
		}
	}

	return false;
}

void COptionsMenu::RenderResetFonts(float PosX, float& PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s", GlobalText[924]);

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (this->BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckResetFonts(int PosX, int& PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			gFont.Bold = 0;

			gFont.Italic = 0;

			FontHeight = 13;

			gFont.CurrentFontNumber = 0;

			sprintf_s(gFont.MyFontFaceName, "%s", gFont.AllFonts[gFont.CurrentFontNumber].c_str());

			gFont.ReloadFont();
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderClose(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s", GlobalText[388]);

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckClose(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			this->CurrentOption = OPTION_NONE;

			ErrorMessage = *(BYTE*)0x083A7C28;

			*(BYTE*)0x083A7C28 = 0;
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderBack(float PosX, float PosY)
{
	this->RenderBox(PosX, PosY, (float)this->BoxWidth, (float)this->BoxHeight);

	char Text[64] = { 0 };

	sprintf_s(Text, "%s", GlobalText[925]);

	EnableAlphaTest(true);

	RenderText((int)PosX, CenterTextPosY(Text, ((int)PosY + (BoxHeight / 2))), Text, REAL_WIDTH(this->BoxWidth), RT3_SORT_CENTER, NULL);
}

bool COptionsMenu::CheckBack(int PosX, int PosY)
{
	if (IsWorkZone(PosX, PosY, this->BoxWidth, this->BoxHeight))
	{
		if (MouseLButton && MouseLButtonPush)
		{
			MouseLButtonPush = false;

			MouseUpdateTime = 0;

			MouseUpdateTimeMax = 6;

			PlayBuffer(25, 0, 0);

			this->CurrentOption = OPTION_NONE;
		}

		return true;
	}

	return false;
}

void COptionsMenu::RenderBox(float PosX, float PosY, float Width, float Height)
{
	DisableAlphaBlend();

	glColor3f(1.0f, 1.0f, 1.0f);

	RenderBitmap(240, PosX, PosY, Width, Height, (0.0f / 256.0f), (0.0f / 64.0f), (213.0f / 256.0f), (64.0f / 64.0f), true, true);

	if (IsWorkZone((int)PosX, (int)PosY, (int)Width, (int)Height))
	{
		glColor3f(0.8f, 0.6f, 0.4f);

		EnableAlphaBlend();

		RenderBitmap(240, PosX, PosY, Width, Height, (0.0f / 256.0f), (0.0f / 64.0f), (213.0f / 256.0f), (64.0f / 64.0f), true, true);

		glColor3f(1.0f, 1.0f, 1.0f);

		DisableAlphaBlend();
	}
}