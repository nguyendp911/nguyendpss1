#include "stdafx.h"
#include "MoveList.h"
#include "Controller.h"
#include "EventTimer.h"
#include "Font.h"
#include "MiniMap.h"
#include "PrintPlayer.h"
#include "Protect.h"
#include <stdio.h>

CMoveList gMoveList;

// Centered Cold Steel Window Geometry (640x480 virtual space)
static const float ML_PW = 368.0f;
static const float ML_PH = 338.0f;
static const float ML_PX = (640.0f - ML_PW) * 0.5f; // 136
static const float ML_PY = 56.0f;
static const int   ML_VISIBLE_ROWS = 13;
static const float ML_ROW_H = 17.0f;
static const float ML_LIST_Y = ML_PY + 76.0f;

static bool MLHit(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static DWORD MLColor(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static void MLSetFont(HFONT f)
{
	if (f) SelectObject(m_hFontDC, f);
}

static float MLMeasureW(const char* text)
{
	if (!text || !text[0]) return 0.0f;
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (float)sz.cx / (g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f);
}

static void MLRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

static void MLText(float x, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	MLSetFont(bold ? g_hFontBold : g_hFont);
	EnableAlphaTest(true);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	SetBackgroundTextColor = 0;
	SetTextColor = MLColor(0, 0, 0, 220);
	RenderText((int)x + 1, (int)y + 1, (char*)text, 0, RT3_SORT_LEFT, NULL);
	SetTextColor = color;
	RenderText((int)x, (int)y, (char*)text, 0, RT3_SORT_LEFT, NULL);
}

static void MLTextCentered(float cx, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	MLSetFont(bold ? g_hFontBold : g_hFont);
	float w = MLMeasureW(text);
	MLText(cx - w * 0.5f, y, text, color, bold);
}

static void MLDrawButton(float x, float y, float w, float h, const char* text, bool active)
{
	bool hov = MLHit(x, y, w, h);
	float br = active ? 0.36f : (hov ? 0.42f : 0.24f);
	float bg = active ? 0.72f : (hov ? 0.70f : 0.29f);
	float bb = active ? 0.82f : (hov ? 0.78f : 0.35f);
	float fr = active ? 0.10f : (hov ? 0.14f : 0.09f);
	float fg = active ? 0.24f : (hov ? 0.20f : 0.125f);
	float fb = active ? 0.31f : (hov ? 0.26f : 0.165f);
	DWORD tc = (active || hov) ? MLColor(240, 252, 255, 255) : MLColor(200, 216, 228, 255);

	MLRect(x, y, w, h, br, bg, bb, 0.96f);
	MLRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fr, fg, fb, 0.96f);
	MLTextCentered(x + w * 0.5f, y + (h - 9.0f) * 0.5f, text, tc, active);
}

static void MLDrawCheckbox(float x, float y, bool checked, bool hover)
{
	const float sz = 10.0f;
	float br = checked ? 0.45f : (hover ? 0.42f : 0.28f);
	float bg = checked ? 0.82f : (hover ? 0.68f : 0.34f);
	float bb = checked ? 0.92f : (hover ? 0.78f : 0.42f);

	MLRect(x, y, sz, sz, br, bg, bb, 0.98f);
	MLRect(x + 1.0f, y + 1.0f, sz - 2.0f, sz - 2.0f, 0.04f, 0.06f, 0.085f, 0.98f);

	if (checked)
	{
		// Crisp filled cyan-gold inner check square
		MLRect(x + 2.5f, y + 2.5f, sz - 5.0f, sz - 5.0f, 0.42f, 0.85f, 0.95f, 1.0f);
	}
}

CMoveList::CMoveList()
{
	this->MoveListSwitch = false;
	this->m_CurrentTab = MOVELIST_TAB_ALL;
	this->m_ScrollOffset = 0;
	this->m_FavoritesLoaded = false;
	this->PKLimitFree = 0;
	this->m_MoveList.clear();
	this->m_Favorites.clear();
}

CMoveList::~CMoveList()
{
}

void CMoveList::LoadFavorites()
{
	if (this->m_FavoritesLoaded) return;
	this->m_FavoritesLoaded = true;
	this->m_Favorites.clear();

	char buf[2048] = { 0 };
	GetPrivateProfileStringA("FavoriteMaps", "List", "", buf, sizeof(buf), ".\\Config.ini");

	char* ctx = NULL;
	char* tok = strtok_s(buf, ",", &ctx);
	while (tok != NULL)
	{
		while (*tok == ' ') tok++;
		if (tok[0] != '\0')
		{
			this->m_Favorites.insert(std::string(tok));
		}
		tok = strtok_s(NULL, ",", &ctx);
	}
}

void CMoveList::SaveFavorites()
{
	std::string joined;
	for (std::set<std::string>::const_iterator it = this->m_Favorites.begin(); it != this->m_Favorites.end(); ++it)
	{
		if (!joined.empty()) joined += ",";
		joined += *it;
	}
	WritePrivateProfileStringA("FavoriteMaps", "List", joined.c_str(), ".\\Config.ini");
}

bool CMoveList::IsFavorite(const char* mapName) const
{
	if (!mapName || !mapName[0]) return false;
	return (this->m_Favorites.find(std::string(mapName)) != this->m_Favorites.end());
}

void CMoveList::ToggleFavorite(const char* mapName)
{
	if (!mapName || !mapName[0]) return;
	std::string key(mapName);
	if (this->m_Favorites.find(key) != this->m_Favorites.end())
	{
		this->m_Favorites.erase(key);
	}
	else
	{
		this->m_Favorites.insert(key);
	}
	this->SaveFavorites();
}

void CMoveList::BuildFilteredList(std::vector<int>& outIndices) const
{
	outIndices.clear();
	for (int i = 0; i < (int)this->m_MoveList.size(); i++)
	{
		const MOVE_LIST_INFO& it = this->m_MoveList[i];
		if (this->m_CurrentTab == MOVELIST_TAB_ALL)
		{
			outIndices.push_back(i);
		}
		else if (this->m_CurrentTab == MOVELIST_TAB_FAVORITES)
		{
			if (this->IsFavorite(it.MapName))
			{
				outIndices.push_back(i);
			}
		}
		else if (this->m_CurrentTab == MOVELIST_TAB_NORMAL)
		{
			if (it.MinLevel <= 50 && (it.AccountLevel <= 0))
			{
				outIndices.push_back(i);
			}
		}
		else if (this->m_CurrentTab == MOVELIST_TAB_HIGH)
		{
			if (it.MinLevel > 50 || it.AccountLevel > 0)
			{
				outIndices.push_back(i);
			}
		}
	}
}

bool CMoveList::GetMoveListState()
{
	return this->MoveListSwitch;
}

void CMoveList::Toggle()
{
	if (gProtect.m_MainInfo.EnableMoveList == 0) return;
	if (CheckInputInterfaces()) return;

	if (CheckRightInterfaces() || ErrorMessage)
	{
		this->MoveListSwitch = false;
		return;
	}

	if (gEventTimer.GetEventTimerState()) gEventTimer.Toggle();
	if (gMiniMap.GetMiniMapState()) gMiniMap.Toggle();

	this->MoveListSwitch = !this->MoveListSwitch;

	if (this->MoveListSwitch)
	{
		this->LoadFavorites();
		this->m_ScrollOffset = 0;
	}

	PlayBuffer(25, 0, 0);
}

void CMoveList::Render()
{
	if (!this->MoveListSwitch) return;

	this->LoadFavorites();

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTC = SetTextColor;
	const DWORD oldBG = SetBackgroundTextColor;

	// 1. Cold Steel Outer Frame (centered on screen)
	MLRect(ML_PX - 2.0f, ML_PY - 2.0f, ML_PW + 4.0f, ML_PH + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
	MLRect(ML_PX, ML_PY, ML_PW, ML_PH, 0.23f, 0.27f, 0.32f, 0.98f);
	MLRect(ML_PX + 1.0f, ML_PY + 1.0f, ML_PW - 2.0f, ML_PH - 2.0f, 0.055f, 0.075f, 0.098f, 0.96f);
	MLRect(ML_PX, ML_PY, ML_PW, 2.0f, 0.435f, 0.714f, 0.769f, 0.98f);

	// Header Bar
	MLRect(ML_PX + 1.0f, ML_PY + 2.0f, ML_PW - 2.0f, 24.0f, 0.095f, 0.135f, 0.185f, 0.95f);
	MLRect(ML_PX + 1.0f, ML_PY + 26.0f, ML_PW - 2.0f, 1.0f, 0.23f, 0.27f, 0.32f, 0.92f);

	MLText(ML_PX + 12.0f, ML_PY + 8.0f, "TELEPORT WINDOW", MLColor(145, 228, 246, 255), true);
	MLText(ML_PX + 258.0f, ML_PY + 9.0f, "M: Open-Close", MLColor(135, 152, 168, 255), false);
	MLDrawButton(ML_PX + ML_PW - 22.0f, ML_PY + 5.0f, 16.0f, 16.0f, "X", false);

	// 2. Tabs Bar (All | Favorites | Normal | High / VIP)
	const float tabY = ML_PY + 32.0f;
	const float tabH = 20.0f;
	const float tabGap = 5.0f;
	const float tabW = (ML_PW - 20.0f - tabGap * 3.0f) / 4.0f;
	char favTabLabel[32];
	if (this->m_Favorites.empty()) strcpy_s(favTabLabel, sizeof(favTabLabel), "Favorites");
	else sprintf_s(favTabLabel, sizeof(favTabLabel), "Fav (%d)", (int)this->m_Favorites.size());

	const char* tabLabels[4] = { "All Maps", favTabLabel, "Normal", "High / VIP" };
	for (int t = 0; t < 4; t++)
	{
		float tx = ML_PX + 10.0f + t * (tabW + tabGap);
		MLDrawButton(tx, tabY, tabW, tabH, tabLabels[t], this->m_CurrentTab == t);
	}

	// 3. Column Headers
	const float colY = ML_PY + 58.0f;
	MLRect(ML_PX + 10.0f, colY - 2.0f, ML_PW - 20.0f, 15.0f, 0.08f, 0.11f, 0.15f, 0.92f);
	MLRect(ML_PX + 10.0f, colY + 13.0f, ML_PW - 20.0f, 1.0f, 0.21f, 0.26f, 0.32f, 0.92f);

	MLText(ML_PX + 14.0f,  colY + 1.0f, "Fav",   MLColor(150, 170, 188, 255), true);
	MLText(ML_PX + 40.0f,  colY + 1.0f, "Map",   MLColor(150, 170, 188, 255), true);
	MLText(ML_PX + 185.0f, colY + 1.0f, "Level", MLColor(150, 170, 188, 255), true);
	MLText(ML_PX + 245.0f, colY + 1.0f, "Cost",  MLColor(150, 170, 188, 255), true);
	MLText(ML_PX + 322.0f, colY + 1.0f, "VIP",   MLColor(150, 170, 188, 255), true);

	// 4. Filtered Map Rows
	std::vector<int> filtered;
	this->BuildFilteredList(filtered);
	const int total = (int)filtered.size();
	int maxScroll = (total > ML_VISIBLE_ROWS) ? (total - ML_VISIBLE_ROWS) : 0;
	if (this->m_ScrollOffset > maxScroll) this->m_ScrollOffset = maxScroll;
	if (this->m_ScrollOffset < 0) this->m_ScrollOffset = 0;

	const float listX = ML_PX + 10.0f;
	const float listW = ML_PW - 28.0f;
	const float listH = ML_VISIBLE_ROWS * ML_ROW_H;

	MLRect(listX, ML_LIST_Y, listW, listH, 0.038f, 0.052f, 0.072f, 0.94f);

	if (total == 0)
	{
		const char* emptyMsg = (this->m_CurrentTab == MOVELIST_TAB_FAVORITES)
			? "No favorite maps yet. Check a box to add!"
			: "No maps available in this tab.";
		MLTextCentered(ML_PX + ML_PW * 0.5f, ML_LIST_Y + listH * 0.45f, emptyMsg, MLColor(135, 152, 168, 255), false);
	}
	else
	{
		char text[32];
		for (int row = 0; row < ML_VISIBLE_ROWS; row++)
		{
			int fIdx = this->m_ScrollOffset + row;
			if (fIdx >= total) break;

			MOVE_LIST_INFO& it = this->m_MoveList[filtered[fIdx]];
			it.CanMove = this->CheckMove(it);

			float ry = ML_LIST_Y + row * ML_ROW_H;
			bool fav = this->IsFavorite(it.MapName);
			bool hovCheck = MLHit(listX + 5.0f, ry + 3.0f, 12.0f, 12.0f);
			bool hovRow = MLHit(listX, ry, listW, ML_ROW_H - 1.0f);

			if (hovRow && !hovCheck && it.CanMove)
			{
				MLRect(listX, ry, listW, ML_ROW_H - 1.0f, 0.14f, 0.25f, 0.32f, 0.92f);
				MLRect(listX, ry, 2.0f, ML_ROW_H - 1.0f, 0.435f, 0.714f, 0.769f, 1.0f);
			}
			else if ((row & 1) == 1)
			{
				MLRect(listX, ry, listW, ML_ROW_H - 1.0f, 0.052f, 0.070f, 0.095f, 0.80f);
			}

			// Favorite checkbox
			MLDrawCheckbox(listX + 6.0f, ry + 3.5f, fav, hovCheck);

			DWORD rowColor = it.CanMove
				? (fav ? MLColor(145, 238, 255, 255) : MLColor(232, 242, 250, 255))
				: MLColor(150, 95, 95, 255);

			MLText(ML_PX + 40.0f, ry + 4.0f, it.MapName, rowColor, fav);

			if (it.MinLevel == -1) strcpy_s(text, sizeof(text), "~");
			else sprintf_s(text, sizeof(text), "%d", it.MinLevel);
			MLText(ML_PX + 190.0f, ry + 4.0f, text, rowColor, false);

			ConvertGold(it.Money, text);
			MLText(ML_PX + 245.0f, ry + 4.0f, text, it.CanMove ? MLColor(255, 215, 95, 255) : rowColor, false);

			if (it.AccountLevel > 0)
			{
				MLText(ML_PX + 320.0f, ry + 4.0f, "[VIP]", MLColor(255, 110, 110, 255), true);
			}
		}
	}

	// 5. Scrollbar
	const float sbX = listX + listW + 3.0f;
	MLRect(sbX, ML_LIST_Y, 4.0f, listH, 0.08f, 0.11f, 0.14f, 0.90f);
	if (total > ML_VISIBLE_ROWS)
	{
		float thumbH = listH * ((float)ML_VISIBLE_ROWS / (float)total);
		if (thumbH < 18.0f) thumbH = 18.0f;
		float thumbY = ML_LIST_Y + (listH - thumbH) * ((float)this->m_ScrollOffset / (float)maxScroll);
		MLRect(sbX, thumbY, 4.0f, thumbH, 0.435f, 0.714f, 0.769f, 0.95f);
	}

	// 6. Footer
	const float fy = ML_PY + ML_PH - 28.0f;
	MLText(ML_PX + 12.0f, fy + 6.0f, "Check box to save favorite maps", MLColor(135, 152, 168, 255), false);
	MLDrawButton(ML_PX + ML_PW - 82.0f, fy, 72.0f, 20.0f, "Close", false);

	MLSetFont(oldFont ? oldFont : g_hFont);
	SetTextColor = oldTC;
	SetBackgroundTextColor = oldBG;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	DisableAlphaBlend();
}

void CMoveList::UpdateMouse()
{
	if (gProtect.m_MainInfo.EnableMoveList == 0 || !this->MoveListSwitch) return;

	if (CheckRightInterfaces() || ErrorMessage)
	{
		this->MoveListSwitch = false;
		return;
	}

	if (!MLHit(ML_PX, ML_PY, ML_PW, ML_PH)) return;

	MouseOnWindow = true;

	std::vector<int> filtered;
	this->BuildFilteredList(filtered);
	const int total = (int)filtered.size();
	int maxScroll = (total > ML_VISIBLE_ROWS) ? (total - ML_VISIBLE_ROWS) : 0;

	// Mouse wheel scrolling
	if (gController.MouseWheel != 0)
	{
		if (gController.MouseWheel > 0) this->m_ScrollOffset--;
		else this->m_ScrollOffset++;
		if (this->m_ScrollOffset < 0) this->m_ScrollOffset = 0;
		if (this->m_ScrollOffset > maxScroll) this->m_ScrollOffset = maxScroll;
		gController.MouseWheel = 0;
		return;
	}

	if (!MouseLButton || !MouseLButtonPush) return;

	MouseLButtonPush = false;
	MouseLButton = false;
	MouseLButtonPop = false;
	MouseUpdateTime = 0;
	MouseUpdateTimeMax = 6;

	// Close (X) & Close button
	if (MLHit(ML_PX + ML_PW - 22.0f, ML_PY + 5.0f, 16.0f, 16.0f) ||
		MLHit(ML_PX + ML_PW - 82.0f, ML_PY + ML_PH - 28.0f, 72.0f, 20.0f))
	{
		this->Toggle();
		return;
	}

	// Tabs
	const float tabY = ML_PY + 32.0f;
	const float tabH = 20.0f;
	const float tabGap = 5.0f;
	const float tabW = (ML_PW - 20.0f - tabGap * 3.0f) / 4.0f;
	for (int t = 0; t < 4; t++)
	{
		float tx = ML_PX + 10.0f + t * (tabW + tabGap);
		if (MLHit(tx, tabY, tabW, tabH))
		{
			PlayBuffer(25, 0, 0);
			this->m_CurrentTab = t;
			this->m_ScrollOffset = 0;
			return;
		}
	}

	// Rows (checkbox vs teleport)
	const float listX = ML_PX + 10.0f;
	const float listW = ML_PW - 28.0f;

	for (int row = 0; row < ML_VISIBLE_ROWS; row++)
	{
		int fIdx = this->m_ScrollOffset + row;
		if (fIdx >= total) break;

		float ry = ML_LIST_Y + row * ML_ROW_H;
		MOVE_LIST_INFO& it = this->m_MoveList[filtered[fIdx]];

		// Checkbox hit area
		if (MLHit(listX + 3.0f, ry + 1.0f, 18.0f, ML_ROW_H - 2.0f))
		{
			PlayBuffer(25, 0, 0);
			this->ToggleFavorite(it.MapName);
			return;
		}

		// Row hit area -> teleport
		if (MLHit(listX + 22.0f, ry, listW - 22.0f, ML_ROW_H - 1.0f))
		{
			if (it.CanMove)
			{
				char Text[100];
				wsprintf(Text, "/move %s", it.MapName);
				SendChat(Text);
				this->Toggle();
			}
			return;
		}
	}
}

bool CMoveList::CheckMove(const MOVE_LIST_INFO& Move)
{
	bool bResult = true;

	STRUCT_DECRYPT;

	if (Move.MinLevel != -1 && *(WORD*)(CharacterAttribute + 0x0E) < Move.MinLevel)
	{
		bResult = false;
		goto EXIT;
	}

	if (Move.MaxLevel != -1 && *(WORD*)(CharacterAttribute + 0x0E) > Move.MaxLevel)
	{
		bResult = false;
		goto EXIT;
	}

	if (Move.MinReset != -1 && gPrintPlayer.ViewReset < (DWORD)Move.MinReset)
	{
		bResult = false;
		goto EXIT;
	}

	if (Move.MaxReset != -1 && gPrintPlayer.ViewReset > (DWORD)Move.MaxReset)
	{
		bResult = false;
		goto EXIT;
	}

	if (*(DWORD*)(CharacterMachine + 0x548) < Move.Money)
	{
		bResult = false;
		goto EXIT;
	}

	if (this->PKLimitFree == 0 && *(BYTE*)(Hero + 0x2EA) >= PKLVL_OUTLAW)
	{
		bResult = false;
		goto EXIT;
	}

	bResult = this->CheckSpecialRequirements(Move);

EXIT:

	STRUCT_ENCRYPT;

	return bResult;
}

bool CMoveList::CheckSpecialRequirements(const MOVE_LIST_INFO& Move)
{
	switch (Move.MapNumber)
	{
		case MAP_ATLANS:
		{
			if (*(short*)(Hero + 0x2B8) == GET_ITEM_MODEL(13, 2) || *(short*)(Hero + 0x2B8) == GET_ITEM_MODEL(13, 3))
			{
				return false;
			}
			break;
		}

		case MAP_ICARUS:
		{
			if (*(short*)(Hero + 0x2A0) == -1 && *(short*)(Hero + 0x2B8) != GET_ITEM_MODEL(13, 3))
			{
				return false;
			}
			if (*(short*)(Hero + 0x2B8) == GET_ITEM_MODEL(13, 2))
			{
				return false;
			}
			break;
		}
	}

	return true;
}

void CMoveList::GCMoveListRecv(PMSG_MOVE_LIST_RECV* lpMsg)
{
	if (gProtect.m_MainInfo.EnableMoveList == 0) return;

	this->PKLimitFree = lpMsg->PKLimitFree;
	this->m_MoveList.clear();

	for (int i = 0; i < lpMsg->count; i++)
	{
		MOVE_LIST_INFO* lpInfo = (MOVE_LIST_INFO*)(((BYTE*)lpMsg) + sizeof(PMSG_MOVE_LIST_RECV) + (sizeof(MOVE_LIST_INFO) * i));

		MOVE_LIST_INFO info;
		info.MapNumber = lpInfo->MapNumber;
		memcpy(info.MapName, lpInfo->MapName, sizeof(info.MapName));
		info.MapName[31] = '\0';
		info.MinLevel = lpInfo->MinLevel;
		info.MaxLevel = lpInfo->MaxLevel;
		info.MinReset = lpInfo->MinReset;
		info.MaxReset = lpInfo->MaxReset;
		info.AccountLevel = lpInfo->AccountLevel;
		info.Money = lpInfo->Money;
		info.CanMove = lpInfo->CanMove;

		this->m_MoveList.push_back(info);
	}

	this->m_ScrollOffset = 0;
}
