#include "stdafx.h"
#include "RegisterUI.h"
#include "Font.h"
#include "Protocol.h"
#include "Sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CRegisterUI gRegisterUI;

static const float REG_BTN_W = 136.0f;
static const float REG_BTN_H = 19.0f;
static const float REG_BTN_X = (640.0f - REG_BTN_W) * 0.5f;
static const float REG_BTN_Y = 434.0f;

static const float REG_MW = 326.0f;
static const float REG_MH = 276.0f;
static const float REG_MX = (640.0f - REG_MW) * 0.5f;
static const float REG_MY = 82.0f;

static bool RHit(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static DWORD RColor(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static void RSetFont(HFONT f)
{
	if (f) SelectObject(m_hFontDC, f);
}

static float RMeasureW(const char* text)
{
	if (!text || !text[0]) return 0.0f;
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (float)sz.cx / (g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f);
}

static void RRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

static void RText(float x, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	RSetFont(bold ? g_hFontBold : g_hFont);
	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	SetBackgroundTextColor = 0;
	SetTextColor = RColor(0, 0, 0, 225);
	RenderText((int)x + 1, (int)y + 1, (char*)text, 0, RT3_SORT_LEFT, NULL);
	SetTextColor = color;
	RenderText((int)x, (int)y, (char*)text, 0, RT3_SORT_LEFT, NULL);
}

static void RTextCentered(float cx, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	RSetFont(bold ? g_hFontBold : g_hFont);
	float w = RMeasureW(text);
	RText(cx - w * 0.5f, y, text, color, bold);
}

static void RDrawButton(float x, float y, float w, float h, const char* text, int style, bool enabled = true)
{
	bool hov = enabled && RHit(x, y, w, h);
	float br = 0.24f, bg = 0.30f, bb = 0.37f;
	float fr = 0.08f, fg = 0.11f, fb = 0.15f;
	DWORD tc = RColor(220, 236, 246, 255);

	if (!enabled)
	{
		br = 0.16f; bg = 0.19f; bb = 0.22f;
		fr = 0.06f; fg = 0.075f; fb = 0.095f;
		tc = RColor(110, 122, 134, 255);
	}
	else if (style == 1) // Cyan
	{
		br = 0.42f; bg = 0.80f; bb = 0.90f;
		fr = 0.11f; fg = 0.22f; fb = 0.30f;
		tc = RColor(235, 250, 255, 255);
	}
	else if (style == 2) // Green
	{
		br = 0.24f; bg = 0.82f; bb = 0.48f;
		fr = 0.08f; fg = 0.25f; fb = 0.15f;
		tc = RColor(195, 255, 215, 255);
	}
	else if (style == 3) // Red
	{
		br = 0.84f; bg = 0.28f; bb = 0.28f;
		fr = 0.26f; fg = 0.08f; fb = 0.09f;
		tc = RColor(255, 210, 210, 255);
	}

	if (hov) { br *= 1.15f; bg *= 1.15f; bb *= 1.15f; fr *= 1.25f; fg *= 1.25f; fb *= 1.25f; }
	RRect(x, y, w, h, br, bg, bb, 0.96f);
	RRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fr, fg, fb, 0.96f);
	RTextCentered(x + w * 0.5f, y + (h - 10.0f) * 0.5f, text, tc, style != 0);
}

static void RDrawInput(float x, float y, float w, float h, const char* text, const char* placeholder, bool focused, bool masked)
{
	RRect(x, y, w, h, focused ? 0.44f : 0.22f, focused ? 0.82f : 0.28f, focused ? 0.92f : 0.35f, 0.96f);
	RRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 0.035f, 0.048f, 0.065f, 0.98f);

	char disp[32] = { 0 };
	if (text && text[0])
	{
		if (masked)
		{
			size_t len = strlen(text);
			if (len > 16) len = 16;
			memset(disp, '*', len);
			disp[len] = 0;
		}
		else
		{
			strcpy_s(disp, sizeof(disp), text);
		}
		if (focused && ((GetTickCount() / 450) & 1)) strcat_s(disp, sizeof(disp), "_");
		RText(x + 6.0f, y + 4.5f, disp, RColor(235, 246, 255, 255), false);
	}
	else if (focused)
	{
		RText(x + 6.0f, y + 4.5f, ((GetTickCount() / 450) & 1) ? "_" : "", RColor(235, 246, 255, 255), false);
	}
	else if (placeholder && placeholder[0])
	{
		RText(x + 6.0f, y + 4.5f, placeholder, RColor(105, 122, 138, 255), false);
	}
}

CRegisterUI::CRegisterUI()
{
	this->m_Open = false;
	this->m_Focus = 1;
	this->m_Pending = false;
	this->m_Account[0] = 0;
	this->m_Password[0] = 0;
	this->m_Confirm[0] = 0;
	this->m_SecCode[0] = 0;
	this->m_CaptchaInput[0] = 0;
	this->m_CaptchaCode[0] = 0;
	this->m_StatusTick = 0;
	this->m_StatusGood = true;
	this->m_StatusText[0] = 0;
	this->GenerateCaptcha();
}

CRegisterUI::~CRegisterUI()
{
}

void CRegisterUI::GenerateCaptcha()
{
	static DWORD salt = 0x13579BDF;
	salt ^= GetTickCount() * 2654435761u;
	salt ^= (salt << 13);
	salt ^= (salt >> 17);
	salt ^= (salt << 5);
	int code = 1000 + (int)(salt % 9000);
	sprintf_s(this->m_CaptchaCode, sizeof(this->m_CaptchaCode), "%04d", code);
	this->m_CaptchaInput[0] = 0;
}

void CRegisterUI::SetStatus(const char* text, bool good)
{
	strcpy_s(this->m_StatusText, sizeof(this->m_StatusText), text);
	this->m_StatusGood = good;
	this->m_StatusTick = GetTickCount();
}

void CRegisterUI::Open()
{
	this->m_Open = true;
	this->m_Focus = 1;
	this->m_Pending = false;
	this->m_Account[0] = 0;
	this->m_Password[0] = 0;
	this->m_Confirm[0] = 0;
	this->m_SecCode[0] = 0;
	this->m_StatusText[0] = 0;
	this->GenerateCaptcha();
	PlayBuffer(25, 0, 0);
}

void CRegisterUI::Close()
{
	this->m_Open = false;
	this->m_Pending = false;
	PlayBuffer(29, 0, 0);
}

void CRegisterUI::OnChar(wchar_t ch)
{
	if (!this->m_Open) return;

	if (ch == 27) // ESC
	{
		this->Close();
		return;
	}
	if (ch == '\t') // Tab -> next field
	{
		this->m_Focus = (this->m_Focus % 5) + 1;
		return;
	}
	if (ch == '\r') // Enter -> submit
	{
		this->Submit();
		return;
	}

	char* buf = NULL;
	size_t maxLen = 0;
	bool digitsOnly = false;

	switch (this->m_Focus)
	{
	case 1: buf = this->m_Account; maxLen = 10; break;
	case 2: buf = this->m_Password; maxLen = 10; break;
	case 3: buf = this->m_Confirm; maxLen = 10; break;
	case 4: buf = this->m_SecCode; maxLen = 7; digitsOnly = true; break;
	case 5: buf = this->m_CaptchaInput; maxLen = 4; digitsOnly = true; break;
	default: return;
	}

	size_t len = strlen(buf);
	if (ch == '\b')
	{
		if (len > 0) buf[len - 1] = 0;
		return;
	}

	if (len >= maxLen) return;
	if (digitsOnly)
	{
		if (ch >= L'0' && ch <= L'9') { buf[len] = (char)ch; buf[len + 1] = 0; }
	}
	else
	{
		if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9'))
		{
			buf[len] = (char)ch;
			buf[len + 1] = 0;
		}
	}
}

void CRegisterUI::Submit()
{
	if (this->m_Pending) return;

	if (strlen(this->m_Account) < 4)
	{
		this->SetStatus("Account ID must be 4-10 alphanumeric characters.", false);
		this->m_Focus = 1;
		return;
	}
	if (strlen(this->m_Password) < 4)
	{
		this->SetStatus("Password must be 4-10 alphanumeric characters.", false);
		this->m_Focus = 2;
		return;
	}
	if (strcmp(this->m_Password, this->m_Confirm) != 0)
	{
		this->SetStatus("Confirm Password does not match!", false);
		this->m_Focus = 3;
		return;
	}
	if (strlen(this->m_SecCode) != 7)
	{
		this->SetStatus("Security Code (Personal ID) must be 7 digits!", false);
		this->m_Focus = 4;
		return;
	}
	if (strcmp(this->m_CaptchaInput, this->m_CaptchaCode) != 0)
	{
		this->SetStatus("Verification Code is incorrect! Try again.", false);
		this->GenerateCaptcha();
		this->m_Focus = 5;
		return;
	}

	CG_REGISTER_ACCOUNT_REQ req;
	memset(&req, 0, sizeof(req));
	req.header.set(0x99, 0x10, sizeof(req));
	strcpy_s(req.account, sizeof(req.account), this->m_Account);
	strcpy_s(req.password, sizeof(req.password), this->m_Password);
	strcpy_s(req.securityCode, sizeof(req.securityCode), this->m_SecCode);

	this->m_Pending = true;
	this->SetStatus("Creating account...", true);
	gProtocol.DataSend((BYTE*)&req, sizeof(req));
	PlayBuffer(25, 0, 0);
}

void CRegisterUI::OnRegisterAnsRecv(BYTE* lpMsg)
{
	GC_REGISTER_ACCOUNT_ANS* p = (GC_REGISTER_ACCOUNT_ANS*)lpMsg;
	this->m_Pending = false;
	this->GenerateCaptcha();

	switch (p->result)
	{
	case 1:
		this->SetStatus("Account created successfully! Close & Log in now.", true);
		this->m_Password[0] = 0;
		this->m_Confirm[0] = 0;
		break;
	case 2:
		this->SetStatus("Account ID already exists! Choose another name.", false);
		this->m_Focus = 1;
		break;
	case 3:
		this->SetStatus("Invalid input format (4-10 chars, 7-digit code).", false);
		break;
	case 5:
		this->SetStatus("Please wait a few seconds before trying again.", false);
		break;
	default:
		this->SetStatus("Server error creating account. Try again.", false);
		break;
	}
}

void CRegisterUI::Render(int loginState)
{
	if (loginState != 2 && loginState != 3)
	{
		this->m_Open = false;
		return;
	}

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTC = SetTextColor;
	const DWORD oldBG = SetBackgroundTextColor;

	// Sleek Cold Steel "Register Account" button docked cleanly below Login box
	RDrawButton(REG_BTN_X, REG_BTN_Y, REG_BTN_W, REG_BTN_H, "REGISTER ACCOUNT", 1, true);

	if (this->m_Open)
	{
		// Dim backdrop
		RRect(0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 0.0f, 0.0f, 0.50f);

		// Cold Steel Modal
		RRect(REG_MX - 2.0f, REG_MY - 2.0f, REG_MW + 4.0f, REG_MH + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
		RRect(REG_MX, REG_MY, REG_MW, REG_MH, 0.435f, 0.714f, 0.769f, 0.98f);
		RRect(REG_MX + 1.0f, REG_MY + 1.0f, REG_MW - 2.0f, REG_MH - 2.0f, 0.045f, 0.064f, 0.088f, 0.97f);

		// Title bar
		RRect(REG_MX + 1.0f, REG_MY + 1.0f, REG_MW - 2.0f, 24.0f, 0.075f, 0.105f, 0.145f, 0.98f);
		RRect(REG_MX + 1.0f, REG_MY + 25.0f, REG_MW - 2.0f, 1.0f, 0.435f, 0.714f, 0.769f, 0.85f);
		RText(REG_MX + 10.0f, REG_MY + 7.0f, "REGISTER NEW ACCOUNT", RColor(150, 234, 248, 255), true);
		RDrawButton(REG_MX + REG_MW - 24.0f, REG_MY + 4.0f, 18.0f, 17.0f, "X", 0, true);

		const float lx = REG_MX + 14.0f;
		const float ix = REG_MX + 124.0f;
		const float iw = REG_MW - 138.0f;
		float y = REG_MY + 38.0f;

		RText(lx, y + 4.5f, "Account ID:", RColor(200, 218, 232, 255), false);
		RDrawInput(ix, y, iw, 20.0f, this->m_Account, "4-10 letters/numbers", this->m_Focus == 1, false);
		y += 28.0f;

		RText(lx, y + 4.5f, "Password:", RColor(200, 218, 232, 255), false);
		RDrawInput(ix, y, iw, 20.0f, this->m_Password, "4-10 letters/numbers", this->m_Focus == 2, true);
		y += 28.0f;

		RText(lx, y + 4.5f, "Confirm Pass:", RColor(200, 218, 232, 255), false);
		RDrawInput(ix, y, iw, 20.0f, this->m_Confirm, "Re-enter password", this->m_Focus == 3, true);
		y += 28.0f;

		RText(lx, y + 4.5f, "Security Code:", RColor(255, 224, 120, 255), true);
		RDrawInput(ix, y, iw, 20.0f, this->m_SecCode, "7 digits (Personal ID)", this->m_Focus == 4, false);
		y += 30.0f;

		// Visual Verification Code box + input + refresh button
		RText(lx, y + 5.0f, "Verify Code:", RColor(150, 232, 248, 255), true);

		// Security captcha display plate
		RRect(ix, y, 66.0f, 22.0f, 0.42f, 0.76f, 0.88f, 0.95f);
		RRect(ix + 1.0f, y + 1.0f, 64.0f, 20.0f, 0.08f, 0.13f, 0.19f, 0.98f);
		// Subtle anti-bot line
		RRect(ix + 4.0f, y + 10.0f, 58.0f, 1.0f, 0.45f, 0.72f, 0.82f, 0.45f);
		char spaced[16];
		sprintf_s(spaced, sizeof(spaced), "%c %c %c %c", this->m_CaptchaCode[0], this->m_CaptchaCode[1], this->m_CaptchaCode[2], this->m_CaptchaCode[3]);
		RTextCentered(ix + 33.0f, y + 5.5f, spaced, RColor(255, 234, 120, 255), true);

		RDrawInput(ix + 70.0f, y + 1.0f, 72.0f, 20.0f, this->m_CaptchaInput, "4 digits", this->m_Focus == 5, false);
		RDrawButton(ix + 146.0f, y + 1.0f, 42.0f, 20.0f, "New", 0, true);
		y += 32.0f;

		// Status message
		if (this->m_StatusText[0])
		{
			RTextCentered(REG_MX + REG_MW * 0.5f, y, this->m_StatusText,
				this->m_StatusGood ? RColor(120, 250, 175, 255) : RColor(255, 130, 130, 255), true);
		}
		else
		{
			RTextCentered(REG_MX + REG_MW * 0.5f, y, "Tip: Security Code is used to delete characters / leave Guild.",
				RColor(140, 162, 180, 255), false);
		}

		// Bottom buttons
		const float btnY = REG_MY + REG_MH - 34.0f;
		RDrawButton(REG_MX + 34.0f, btnY, 122.0f, 24.0f, this->m_Pending ? "Creating..." : "Create Account", 2, !this->m_Pending);
		RDrawButton(REG_MX + REG_MW - 156.0f, btnY, 122.0f, 24.0f, "Close", 3, true);
	}

	RSetFont(oldFont ? oldFont : g_hFont);
	SetTextColor = oldTC;
	SetBackgroundTextColor = oldBG;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

bool CRegisterUI::UpdateMouse(int loginState, bool click)
{
	if (loginState != 2 && loginState != 3)
	{
		this->m_Open = false;
		return false;
	}

	if (!this->m_Open)
	{
		if (RHit(REG_BTN_X, REG_BTN_Y, REG_BTN_W, REG_BTN_H))
		{
			if (click)
			{
				this->Open();
			}
			return true;
		}
		return false;
	}

	// Modal is open: consume ALL mouse events so clicks never leak to Login box
	if (!click) return true;

	PlayBuffer(25, 0, 0);

	if (RHit(REG_MX + REG_MW - 24.0f, REG_MY + 4.0f, 18.0f, 17.0f)) { this->Close(); return true; }

	const float btnY = REG_MY + REG_MH - 34.0f;
	if (RHit(REG_MX + 34.0f, btnY, 122.0f, 24.0f)) { this->Submit(); return true; }
	if (RHit(REG_MX + REG_MW - 156.0f, btnY, 122.0f, 24.0f)) { this->Close(); return true; }

	const float ix = REG_MX + 124.0f;
	const float iw = REG_MW - 138.0f;
	float y = REG_MY + 38.0f;

	if (RHit(ix, y, iw, 20.0f)) { this->m_Focus = 1; return true; } y += 28.0f;
	if (RHit(ix, y, iw, 20.0f)) { this->m_Focus = 2; return true; } y += 28.0f;
	if (RHit(ix, y, iw, 20.0f)) { this->m_Focus = 3; return true; } y += 28.0f;
	if (RHit(ix, y, iw, 20.0f)) { this->m_Focus = 4; return true; } y += 30.0f;
	if (RHit(ix + 70.0f, y + 1.0f, 72.0f, 20.0f)) { this->m_Focus = 5; return true; }
	if (RHit(ix + 146.0f, y + 1.0f, 42.0f, 20.0f)) { this->GenerateCaptcha(); return true; }

	return true;
}
