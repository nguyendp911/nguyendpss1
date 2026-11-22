#include "stdafx.h"
#include "CommandWindow.h"
#include "DuelManager.h"
#include "Font.h"
#include "JewelBankUI.h"
#include "MarketWindow.h"
#include "Protocol.h"
#include "SmartAutoHelperUI.h"
#include "Sound.h"
#include <stdio.h>
#include <string.h>

CCommandWindow gCommandWindow;

static bool CMHit(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static DWORD CMColor(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static void CMSetFont(HFONT f)
{
	if (f) SelectObject(m_hFontDC, f);
}

static float CMMeasureW(const char* text)
{
	if (!text || !text[0]) return 0.0f;
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (float)sz.cx / (g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f);
}

static void CMRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

static void CMText(float x, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	CMSetFont(bold ? g_hFontBold : g_hFont);
	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	SetBackgroundTextColor = 0;
	SetTextColor = CMColor(0, 0, 0, 220);
	RenderText((int)x + 1, (int)y + 1, (char*)text, 0, RT3_SORT_LEFT, NULL);
	SetTextColor = color;
	RenderText((int)x, (int)y, (char*)text, 0, RT3_SORT_LEFT, NULL);
}

static void CMTextCentered(float cx, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	CMSetFont(bold ? g_hFontBold : g_hFont);
	float w = CMMeasureW(text);
	CMText(cx - w * 0.5f, y, text, color, bold);
}

CCommandWindow::CCommandWindow()
{
	this->m_IsOpen = false;
	this->m_Width = 368.0f;
	this->m_Height = 236.0f;
	this->m_PosX = (640.0f - this->m_Width) * 0.5f;
	this->m_PosY = (480.0f - this->m_Height) * 0.5f - 28.0f;
	this->m_ActiveMode = CMD_MODE_NONE;
	this->m_IsDragging = false;
	this->m_DragOffsetX = 0.0f;
	this->m_DragOffsetY = 0.0f;
}

CCommandWindow::~CCommandWindow()
{
}

void CCommandWindow::Toggle()
{
	if (CheckInputInterfaces()) return;

	this->m_IsOpen = !this->m_IsOpen;
	this->m_ActiveMode = CMD_MODE_NONE;
	this->m_IsDragging = false;
	PlayBuffer(this->m_IsOpen ? 25 : 29, 0, 0);
}

void CCommandWindow::Show(bool show)
{
	this->m_IsOpen = show;
	if (!show)
	{
		this->m_ActiveMode = CMD_MODE_NONE;
		this->m_IsDragging = false;
	}
}

static void DrawCommandCard(float x, float y, float w, float h, const char* badge, const char* title, const char* sub, DWORD accentCol, float ar, float ag, float ab, bool active)
{
	bool hov = CMHit(x, y, w, h);
	float br = active ? ar : (hov ? 0.42f : 0.24f);
	float bg = active ? ag : (hov ? 0.74f : 0.30f);
	float bb = active ? ab : (hov ? 0.86f : 0.36f);
	float fr = active ? 0.14f : (hov ? 0.10f : 0.065f);
	float fg = active ? 0.22f : (hov ? 0.16f : 0.090f);
	float fb = active ? 0.28f : (hov ? 0.22f : 0.120f);

	CMRect(x, y, w, h, br, bg, bb, 0.96f);
	CMRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fr, fg, fb, 0.95f);
	CMRect(x + 1.0f, y + 1.0f, 3.0f, h - 2.0f, ar, ag, ab, 0.95f);

	// Mini steel icon badge on left
	CMRect(x + 10.0f, y + 8.0f, 26.0f, 24.0f, br, bg, bb, 0.92f);
	CMRect(x + 11.0f, y + 9.0f, 24.0f, 22.0f, 0.045f, 0.065f, 0.090f, 0.96f);
	CMTextCentered(x + 23.0f, y + 15.0f, badge, accentCol, true);

	CMText(x + 44.0f, y + 9.0f, title, hov || active ? CMColor(255, 255, 255, 255) : CMColor(230, 240, 248, 255), true);
	CMText(x + 44.0f, y + 23.0f, sub, CMColor(150, 175, 195, 255), false);
}

void CCommandWindow::Render()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		this->m_IsOpen = false;
		this->m_ActiveMode = CMD_MODE_NONE;
		return;
	}

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTC = SetTextColor;
	const DWORD oldBG = SetBackgroundTextColor;

	if (this->m_IsOpen)
	{
		const float px = this->m_PosX;
		const float py = this->m_PosY;
		const float pw = this->m_Width;
		const float ph = this->m_Height;

		// Cold Steel Frame
		CMRect(px - 2.0f, py - 2.0f, pw + 4.0f, ph + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
		CMRect(px, py, pw, ph, 0.23f, 0.27f, 0.32f, 0.98f);
		CMRect(px + 1.0f, py + 1.0f, pw - 2.0f, ph - 2.0f, 0.045f, 0.062f, 0.082f, 0.96f);

		// Title Bar
		CMRect(px + 1.0f, py + 1.0f, pw - 2.0f, 24.0f, 0.075f, 0.105f, 0.140f, 0.98f);
		CMRect(px + 1.0f, py + 25.0f, pw - 2.0f, 1.0f, 0.435f, 0.714f, 0.769f, 0.85f);
		CMText(px + 10.0f, py + 7.0f, "COMMAND MENU (D)", CMColor(150, 232, 248, 255), true);

		// Close [X]
		float cx = px + pw - 24.0f, cy = py + 4.0f;
		bool hovX = CMHit(cx, cy, 18.0f, 17.0f);
		CMRect(cx, cy, 18.0f, 17.0f, hovX ? 0.85f : 0.28f, hovX ? 0.30f : 0.34f, hovX ? 0.30f : 0.40f, 0.95f);
		CMRect(cx + 1.0f, cy + 1.0f, 16.0f, 15.0f, hovX ? 0.28f : 0.08f, hovX ? 0.08f : 0.11f, hovX ? 0.08f : 0.15f, 0.95f);
		CMTextCentered(cx + 9.0f, cy + 4.0f, "X", CMColor(235, 242, 248, 255), true);

		// 2x3 Command Cards
		const float bw = 166.0f;
		const float bh = 40.0f;
		const float col1 = px + 14.0f;
		const float col2 = px + 188.0f;
		const float row1 = py + 36.0f;
		const float row2 = py + 84.0f;
		const float row3 = py + 132.0f;

		DrawCommandCard(col1, row1, bw, bh, "PT", "Party Invite", "Right-Click player", CMColor(125, 240, 155, 255), 0.30f, 0.85f, 0.45f, this->m_ActiveMode == CMD_MODE_PARTY);
		DrawCommandCard(col2, row1, bw, bh, "TR", "Trade Request", "Right-Click player", CMColor(120, 232, 255, 255), 0.30f, 0.78f, 0.95f, this->m_ActiveMode == CMD_MODE_TRADE);
		DrawCommandCard(col1, row2, bw, bh, "GD", "Join Guild", "Right-Click GM", CMColor(255, 218, 105, 255), 0.92f, 0.72f, 0.25f, this->m_ActiveMode == CMD_MODE_GUILD);
		DrawCommandCard(col2, row2, bw, bh, "DL", "Duel PvP", "Right-Click player", CMColor(255, 130, 120, 255), 0.92f, 0.34f, 0.30f, this->m_ActiveMode == CMD_MODE_DUEL);
		DrawCommandCard(col1, row3, bw, bh, "JB", "Jewel Bank", "Open Bank (J)", CMColor(145, 235, 245, 255), 0.35f, 0.82f, 0.88f, false);
		DrawCommandCard(col2, row3, bw, bh, "MK", "Marketplace", "Open Market (F5)", CMColor(255, 205, 115, 255), 0.90f, 0.65f, 0.28f, false);

		// Footer hint bar
		CMRect(px + 14.0f, py + 184.0f, pw - 28.0f, 38.0f, 0.18f, 0.23f, 0.28f, 0.90f);
		CMRect(px + 15.0f, py + 185.0f, pw - 30.0f, 36.0f, 0.055f, 0.075f, 0.100f, 0.94f);
		CMTextCentered(px + pw * 0.5f, py + 191.0f, "Select Party / Trade / Guild / Duel -> Right-Click player", CMColor(245, 224, 155, 255), false);
		CMTextCentered(px + pw * 0.5f, py + 205.0f, "Press [D] or [ESC] to close or cancel target mode", CMColor(150, 175, 195, 255), false);
	}

	if (this->m_ActiveMode != CMD_MODE_NONE)
	{
		this->RenderTargetCursor();
	}

	CMSetFont(oldFont ? oldFont : g_hFont);
	SetTextColor = oldTC;
	SetBackgroundTextColor = oldBG;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

void CCommandWindow::RenderTargetCursor()
{
	if (this->m_ActiveMode == CMD_MODE_NONE) return;
	if (this->m_IsOpen && (MouseOnWindow || CMHit(this->m_PosX, this->m_PosY, this->m_Width, this->m_Height))) return;

	// Small active-mode banner at top-center so player knows target mode is active
	const char* modeText = (this->m_ActiveMode == CMD_MODE_PARTY) ? "PARTY MODE: Right-Click a player (ESC to cancel)"
		: (this->m_ActiveMode == CMD_MODE_TRADE) ? "TRADE MODE: Right-Click a player (ESC to cancel)"
		: (this->m_ActiveMode == CMD_MODE_DUEL) ? "DUEL PVP MODE: Right-Click a player (ESC to cancel)"
		: "GUILD MODE: Right-Click a Guild Master (ESC to cancel)";
	CMSetFont(g_hFontBold);
	float mw = CMMeasureW(modeText) + 20.0f;
	float mx = 320.0f - mw * 0.5f;
	CMRect(mx, 36.0f, mw, 16.0f, 0.40f, 0.76f, 0.88f, 0.95f);
	CMRect(mx + 1.0f, 37.0f, mw - 2.0f, 14.0f, 0.06f, 0.09f, 0.13f, 0.92f);
	CMTextCentered(320.0f, 39.5f, modeText, CMColor(255, 232, 130, 255), true);

	int sel = (int)SelectedCharacter;
	if (sel < 0 || sel >= MAX_MAIN_VIEWPORT) return;

	DWORD targetAddr = *(DWORD*)(0x07ABF5D0) + (sel * 916);
	if (!targetAddr || *(BYTE*)targetAddr == 0 || targetAddr == Hero) return;
	if (*(BYTE*)(targetAddr + 0x84) != 1) return; // Players only

	char* targetName = (char*)(targetAddr + 0x1C1);
	if (!targetName || targetName[0] == '\0') return;

	const char* hint = (this->m_ActiveMode == CMD_MODE_PARTY) ? "Right-Click: Invite to Party"
		: (this->m_ActiveMode == CMD_MODE_TRADE) ? "Right-Click: Request Trade"
		: (this->m_ActiveMode == CMD_MODE_DUEL) ? "Right-Click: Challenge to Duel"
		: "Right-Click: Join Guild";
	DWORD hintCol = (this->m_ActiveMode == CMD_MODE_PARTY) ? CMColor(130, 250, 155, 255)
		: (this->m_ActiveMode == CMD_MODE_TRADE) ? CMColor(120, 238, 255, 255)
		: (this->m_ActiveMode == CMD_MODE_DUEL) ? CMColor(255, 130, 120, 255)
		: CMColor(255, 220, 115, 255);

	float boxW = 165.0f;
	float boxH = 38.0f;
	float boxX = (float)MouseX + 14.0f;
	float boxY = (float)MouseY + 10.0f;
	if (boxX + boxW > 635.0f) boxX = (float)MouseX - boxW - 6.0f;
	if (boxY + boxH > 475.0f) boxY = (float)MouseY - boxH - 6.0f;

	CMRect(boxX, boxY, boxW, boxH, 0.42f, 0.76f, 0.88f, 0.96f);
	CMRect(boxX + 1.0f, boxY + 1.0f, boxW - 2.0f, boxH - 2.0f, 0.05f, 0.075f, 0.11f, 0.94f);

	char nameBuf[48];
	sprintf_s(nameBuf, sizeof(nameBuf), "[ %s ]", targetName);
	CMTextCentered(boxX + boxW * 0.5f, boxY + 6.0f, nameBuf, CMColor(255, 232, 115, 255), true);
	CMTextCentered(boxX + boxW * 0.5f, boxY + 21.0f, hint, hintCol, false);
}

void CCommandWindow::UpdateMouse()
{
	if (SceneFlag != MAIN_SCENE || !Hero) return;

	if (this->m_IsOpen)
	{
		if (CMHit(this->m_PosX, this->m_PosY, this->m_Width, this->m_Height))
		{
			MouseOnWindow = true;

			if (MouseLButtonPush)
			{
				// Close [X]
				if (CMHit(this->m_PosX + this->m_Width - 24.0f, this->m_PosY + 4.0f, 18.0f, 17.0f))
				{
					MouseLButtonPush = false; MouseLButton = false; MouseLButtonPop = false;
					PlayBuffer(29, 0, 0);
					this->Show(false);
					return;
				}

				// Drag title bar
				if (CMHit(this->m_PosX, this->m_PosY, this->m_Width - 28.0f, 25.0f))
				{
					this->m_IsDragging = true;
					this->m_DragOffsetX = (float)MouseX - this->m_PosX;
					this->m_DragOffsetY = (float)MouseY - this->m_PosY;
					return;
				}

				const float bw = 166.0f;
				const float bh = 40.0f;
				const float col1 = this->m_PosX + 14.0f;
				const float col2 = this->m_PosX + 188.0f;
				const float row1 = this->m_PosY + 36.0f;
				const float row2 = this->m_PosY + 84.0f;
				const float row3 = this->m_PosY + 132.0f;

				MouseLButtonPush = false; MouseLButton = false; MouseLButtonPop = false;

				if (CMHit(col1, row1, bw, bh))
				{
					this->m_IsOpen = false;
					this->m_ActiveMode = CMD_MODE_PARTY;
					PlayBuffer(25, 0, 0);
					return;
				}
				if (CMHit(col2, row1, bw, bh))
				{
					this->m_IsOpen = false;
					this->m_ActiveMode = CMD_MODE_TRADE;
					PlayBuffer(25, 0, 0);
					return;
				}
				if (CMHit(col1, row2, bw, bh))
				{
					this->m_IsOpen = false;
					this->m_ActiveMode = CMD_MODE_GUILD;
					PlayBuffer(25, 0, 0);
					return;
				}
				if (CMHit(col2, row2, bw, bh))
				{
					this->m_IsOpen = false;
					this->m_ActiveMode = CMD_MODE_DUEL;
					PlayBuffer(25, 0, 0);
					return;
				}
				if (CMHit(col1, row3, bw, bh))
				{
					PlayBuffer(25, 0, 0);
					this->Show(false);
					gJewelBankUI.Toggle();
					return;
				}
				if (CMHit(col2, row3, bw, bh))
				{
					PlayBuffer(25, 0, 0);
					this->Show(false);
					gMarketWindow.Toggle();
					return;
				}
			}
		}

		if (this->m_IsDragging)
		{
			if (MouseLButton)
			{
				this->m_PosX = (float)MouseX - this->m_DragOffsetX;
				this->m_PosY = (float)MouseY - this->m_DragOffsetY;
				if (this->m_PosX < 4.0f) this->m_PosX = 4.0f;
				if (this->m_PosY < 4.0f) this->m_PosY = 4.0f;
				if (this->m_PosX + this->m_Width > 636.0f) this->m_PosX = 636.0f - this->m_Width;
				if (this->m_PosY + this->m_Height > 440.0f) this->m_PosY = 440.0f - this->m_Height;
			}
			else
			{
				this->m_IsDragging = false;
			}
		}
	}

	// Right-click target execution
	if (this->m_ActiveMode != CMD_MODE_NONE && MouseRButtonPush)
	{
		MouseRButtonPush = false;
		MouseRButton = false;

		int sel = (int)SelectedCharacter;
		if (sel >= 0 && sel < MAX_MAIN_VIEWPORT)
		{
			DWORD targetAddr = *(DWORD*)(0x07ABF5D0) + (sel * 916);
			if (targetAddr && *(BYTE*)targetAddr != 0 && targetAddr != Hero && *(BYTE*)(targetAddr + 0x84) == 1)
			{
				WORD targetKey = *(WORD*)(targetAddr + 0x1DC);
				char* targetName = (char*)(targetAddr + 0x1C1);

				if (this->m_ActiveMode == CMD_MODE_PARTY) this->ExecuteParty(targetKey, targetName);
				else if (this->m_ActiveMode == CMD_MODE_TRADE) this->ExecuteTrade(targetKey, targetName);
				else if (this->m_ActiveMode == CMD_MODE_GUILD) this->ExecuteGuild(targetKey, targetName);
				else if (this->m_ActiveMode == CMD_MODE_DUEL) gDuelManager.OpenSetupWindow(targetKey, targetName);
			}
		}

		this->m_ActiveMode = CMD_MODE_NONE;
	}
}

void CCommandWindow::ExecuteParty(WORD targetKey, const char* targetName)
{
	if (!Hero) return;

	char* heroName = (char*)(Hero + 0x1C1);
	if (PartyNumber > 0 && heroName && strcmp(Party[0].Name, heroName) != 0)
	{
		char msg[] = "[Party] Only the Party Leader can invite players.";
		UIChatLogWindow_AddText("", msg, 1);
		CreateNotice(msg, 1);
		return;
	}

	PMSG_PARTY_REQUEST_SEND pMsg;
	pMsg.header.setE(0x40, sizeof(pMsg));
	pMsg.index[0] = SET_NUMBERHB(targetKey);
	pMsg.index[1] = SET_NUMBERLB(targetKey);
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);

	char szNotice[128];
	sprintf_s(szNotice, sizeof(szNotice), "[Party] Sent party invitation to %s.", (targetName && targetName[0]) ? targetName : "player");
	UIChatLogWindow_AddText("", szNotice, 1);
	CreateNotice(szNotice, 1);
}

void CCommandWindow::ExecuteTrade(WORD targetKey, const char* targetName)
{
	if (!Hero) return;

	PMSG_TRADE_REQUEST_SEND pMsg;
	pMsg.header.setE(0x36, sizeof(pMsg));
	pMsg.index[0] = SET_NUMBERHB(targetKey);
	pMsg.index[1] = SET_NUMBERLB(targetKey);
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);

	char szNotice[128];
	sprintf_s(szNotice, sizeof(szNotice), "[Trade] Sent trade request to %s.", (targetName && targetName[0]) ? targetName : "player");
	UIChatLogWindow_AddText("", szNotice, 1);
	CreateNotice(szNotice, 1);
}

void CCommandWindow::ExecuteGuild(WORD targetKey, const char* targetName)
{
	if (!Hero) return;

	if (*(short*)(Hero + 0x1DA) != -1 && *(short*)(Hero + 0x1DA) != 0)
	{
		char msg[] = "[Guild] You are already in a guild.";
		UIChatLogWindow_AddText("", msg, 1);
		CreateNotice(msg, 1);
		return;
	}

	PMSG_GUILD_REQUEST_SEND pMsg;
	pMsg.header.set(0x50, sizeof(pMsg));
	pMsg.index[0] = SET_NUMBERHB(targetKey);
	pMsg.index[1] = SET_NUMBERLB(targetKey);
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);

	char szNotice[128];
	sprintf_s(szNotice, sizeof(szNotice), "[Guild] Sent guild join request to %s.", (targetName && targetName[0]) ? targetName : "Guild Master");
	UIChatLogWindow_AddText("", szNotice, 1);
	CreateNotice(szNotice, 1);
}
