#include "stdafx.h"
#include "CashShopUI.h"
#include "Interface.h"
#include "Font.h"
#include "ItemPost.h"
#include "Protocol.h"
#include "Sound.h"
#include "stb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CCashShopUI gCashShopUI;

static const float CS_PW = 444.0f;
static const float CS_PH = 334.0f;
static const float CS_PX = (640.0f - CS_PW) * 0.5f; // 98
static const float CS_PY = 54.0f;

static void GetCashShopHudLayout(float* btnW, float* btnH, float* btnX, float* y)
{
	const float asp = (g_fScreenRate_x > 0.0f) ? (g_fScreenRate_y / g_fScreenRate_x) : 0.75f;
	*btnH = 22.0f;
	*btnW = (*btnH) * asp;
	*y = 8.0f;
	// Collapsible slot 11: Pinned(0..5), PK(6), Market(7), Recorder(8), TOP(9), VIP(10), CashShop(11)
	*btnX = 8.0f + (*btnW + 3.0f) * 11.0f;
}

static bool CSHit(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static DWORD CSColor(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static void CSSetFont(HFONT f)
{
	if (f) SelectObject(m_hFontDC, f);
}

static float CSMeasureW(const char* text)
{
	if (!text || !text[0]) return 0.0f;
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (float)sz.cx / (g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f);
}

static void CSRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

static void CSText(float x, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	CSSetFont(bold ? g_hFontBold : g_hFont);
	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	SetBackgroundTextColor = 0;
	SetTextColor = CSColor(0, 0, 0, 220);
	RenderText((int)x + 1, (int)y + 1, (char*)text, 0, RT3_SORT_LEFT, NULL);
	SetTextColor = color;
	RenderText((int)x, (int)y, (char*)text, 0, RT3_SORT_LEFT, NULL);
}

static void CSTextCentered(float cx, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	CSSetFont(bold ? g_hFontBold : g_hFont);
	float w = CSMeasureW(text);
	CSText(cx - w * 0.5f, y, text, color, bold);
}

static void CSFormatComma(DWORD val, char* out, int size)
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

static void CSDrawButton(float x, float y, float w, float h, const char* text, int style, bool enabled = true)
{
	bool hov = enabled && CSHit(x, y, w, h);
	float br = 0.24f, bg = 0.29f, bb = 0.35f;
	float fr = 0.085f, fg = 0.115f, fb = 0.155f;
	DWORD tc = CSColor(220, 234, 244, 255);

	if (!enabled)
	{
		br = 0.16f; bg = 0.19f; bb = 0.22f;
		fr = 0.06f; fg = 0.075f; fb = 0.095f;
		tc = CSColor(105, 118, 130, 255);
	}
	else if (style == 1) // Active tab
	{
		br = 0.42f; bg = 0.80f; bb = 0.90f;
		fr = 0.12f; fg = 0.24f; fb = 0.32f;
		tc = CSColor(235, 250, 255, 255);
	}
	else if (style == 2) // Buy button (Gold/Green)
	{
		br = hov ? 0.95f : 0.78f; bg = hov ? 0.80f : 0.62f; bb = hov ? 0.32f : 0.20f;
		fr = hov ? 0.26f : 0.17f; fg = hov ? 0.20f : 0.13f; fb = hov ? 0.07f : 0.05f;
		tc = CSColor(255, 238, 160, 255);
	}
	else if (hov)
	{
		br = 0.38f; bg = 0.68f; bb = 0.78f;
		fr = 0.11f; fg = 0.18f; fb = 0.24f;
	}

	CSRect(x, y, w, h, br, bg, bb, 0.96f);
	CSRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fr, fg, fb, 0.95f);
	CSTextCentered(x + w * 0.5f, y + (h - 9.0f) * 0.5f, text, tc, (style == 1 || style == 2));
}

static void DrawShopIconQuad(GLuint texId, float x, float y, float w, float h, float u0, float u1, float r, float g, float b, float a)
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

static void RenderCashShopItem3D(float boxX, float boxY, float boxW, float boxH, WORD itemType, BYTE itemLevel)
{
	EndBitmap();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glViewport2(0, 0, WindowWidth, WindowHeight);
	gluPerspective2(1.0f, (float)WindowWidth / (float)WindowHeight, 20.0f, 2000.0f);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	GetOpenGLMatrix(CameraMatrix);
	EnableDepthTest();
	EnableDepthMask();
	glClear(GL_DEPTH_BUFFER_BIT);

	int sx = (int)(boxX * g_fScreenRate_x);
	int sy = (int)((float)WindowHeight - (boxY + boxH) * g_fScreenRate_y);
	int sw = (int)(boxW * g_fScreenRate_x);
	int sh = (int)(boxH * g_fScreenRate_y);
	if (sw < 1) sw = 1;
	if (sh < 1) sh = 1;
	glEnable(GL_SCISSOR_TEST);
	glScissor(sx, sy, sw, sh);

	RenderItem3D(boxX + 4.0f, boxY + 2.0f, boxW - 8.0f, boxH - 4.0f, itemType, itemLevel << 3, 0, false);

	glDisable(GL_SCISSOR_TEST);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	BeginBitmap();
}

CCashShopUI::CCashShopUI()
{
	this->m_Open = false;
	this->m_Enable = true;
	this->m_Category = 0; // All
	this->m_Page = 0;
	this->m_WCoin = 0;
	this->m_Count = 0;
	memset(this->m_Products, 0, sizeof(this->m_Products));
	this->m_WaitTick = 0;
	this->m_StatusTick = 0;
	this->m_StatusGood = true;
	this->m_StatusText[0] = 0;
	this->m_ConfirmProductIdx = -1;
	this->m_SelectedClass = 0;
	this->m_TexShopCS = 0;
}

CCashShopUI::~CCashShopUI()
{
}

void CCashShopUI::LoadImages()
{
	if (this->m_TexShopCS != 0) return;

	int w = 0, h = 0, ch = 0;
	unsigned char* data = stbi_load("Data\\Interface\\CashShopCS.png", &w, &h, &ch, 4);
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
		this->m_TexShopCS = (unsigned int)tex;
		stbi_image_free(data);
	}
}

void CCashShopUI::Toggle()
{
	if (this->m_Open) this->Close();
	else this->Open();
}

void CCashShopUI::Open()
{
	this->m_Open = true;
	this->RequestInfo();
}

void CCashShopUI::RequestInfo()
{
	PMSG_CASHSHOP_INFO_REQ pMsg;
	pMsg.header.set(0x98, 0x11, sizeof(pMsg));
	gProtocol.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CCashShopUI::RequestBuy(BYTE productIdx, BYTE param)
{
	DWORD now = GetTickCount();
	if (this->m_WaitTick != 0 && (now - this->m_WaitTick) < 400) return;
	this->m_WaitTick = now;

	PMSG_CASHSHOP_BUY_REQ pMsg;
	pMsg.header.set(0x98, 0x10, sizeof(pMsg));
	pMsg.product = productIdx;
	pMsg.param = param;
	gProtocol.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CCashShopUI::OnListRecv(BYTE* lpMsg)
{
	PMSG_CASHSHOP_LIST_ANS* p = (PMSG_CASHSHOP_LIST_ANS*)lpMsg;
	this->m_Enable = (p->enable != 0);
	this->m_WCoin = p->wcoin;
	this->m_Count = (p->count > CASHSHOP_MAX_PRODUCT) ? CASHSHOP_MAX_PRODUCT : p->count;

	PMSG_CASHSHOP_ENTRY* list = (PMSG_CASHSHOP_ENTRY*)(lpMsg + sizeof(PMSG_CASHSHOP_LIST_ANS));
	for (int i = 0; i < this->m_Count; i++)
	{
		this->m_Products[i] = list[i];
	}
}

void CCashShopUI::OnBuyRecv(BYTE* lpMsg)
{
	PMSG_CASHSHOP_BUY_ANS* p = (PMSG_CASHSHOP_BUY_ANS*)lpMsg;
	this->m_WCoin = p->wcoin;
	this->m_StatusTick = GetTickCount();

	if (p->result == 1)
	{
		this->m_StatusGood = true;
		const char* pname = (p->product >= 1 && p->product <= this->m_Count) ? this->m_Products[p->product - 1].name : "Item";
		sprintf_s(this->m_StatusText, sizeof(this->m_StatusText), "Purchased %s successfully!", pname);
	}
	else if (p->result == 2)
	{
		this->m_StatusGood = false;
		strcpy_s(this->m_StatusText, sizeof(this->m_StatusText), "Not enough WCoin!");
	}
	else if (p->result == 3)
	{
		this->m_StatusGood = false;
		strcpy_s(this->m_StatusText, sizeof(this->m_StatusText), "Inventory is full!");
	}
	else if (p->result == 5)
	{
		this->m_StatusGood = false;
		strcpy_s(this->m_StatusText, sizeof(this->m_StatusText), "Please choose a different class!");
	}
	else if (p->result == 6)
	{
		this->m_StatusGood = false;
		strcpy_s(this->m_StatusText, sizeof(this->m_StatusText), "Unequip all items before changing class!");
	}
	else
	{
		this->m_StatusGood = false;
		strcpy_s(this->m_StatusText, sizeof(this->m_StatusText), "Purchase failed.");
	}
}

void CCashShopUI::Render()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		this->m_Open = false;
		return;
	}

	if (this->m_TexShopCS == 0) this->LoadImages();

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTC = SetTextColor;
	const DWORD oldBG = SetBackgroundTextColor;

	// ---- Collapsible Top-Left HUD Button (Slot 11) ----
	if (!HudIconBarCollapsed())
	{
		float btnW, btnH, bx, by;
		GetCashShopHudLayout(&btnW, &btnH, &bx, &by);
		bool hov = CSHit(bx, by, btnW, btnH);

		if (this->m_TexShopCS != 0)
		{
			float u0 = this->m_Open ? 0.50f : 0.00f;
			float u1 = this->m_Open ? 1.00f : 0.50f;
			float c = (this->m_Open || hov) ? 1.0f : 0.90f;
			DrawShopIconQuad((GLuint)this->m_TexShopCS, bx, by, btnW, btnH, u0, u1, c, c, c, 0.98f);
		}
		else
		{
			CSRect(bx, by, btnW, btnH, 0.25f, 0.34f, 0.42f, 0.96f);
			CSRect(bx + 1.0f, by + 1.0f, btnW - 2.0f, btnH - 2.0f, 0.06f, 0.085f, 0.115f, 0.94f);
			CSTextCentered(bx + btnW * 0.5f, by + 6.0f, "CS", CSColor(255, 224, 115, 255), true);
		}

		if (hov)
		{
			const char* tip = "Cash Shop (X)";
			CSSetFont(g_hFont);
			float tw = CSMeasureW(tip);
			float ty = by + btnH + 4.0f;
			CSRect(bx, ty, tw + 10.0f, 15.0f, 0.23f, 0.27f, 0.32f, 0.95f);
			CSRect(bx + 1.0f, ty + 1.0f, tw + 8.0f, 13.0f, 0.055f, 0.075f, 0.098f, 0.92f);
			CSText(bx + 5.0f, ty + 2.5f, tip, CSColor(230, 242, 248, 255), false);
		}
	}

	if (!this->m_Open)
	{
		CSSetFont(oldFont ? oldFont : g_hFont);
		SetTextColor = oldTC;
		SetBackgroundTextColor = oldBG;
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		DisableAlphaBlend();
		return;
	}

	// ---- Main Centered Cold Steel Window ----
	CSRect(CS_PX - 2.0f, CS_PY - 2.0f, CS_PW + 4.0f, CS_PH + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
	CSRect(CS_PX, CS_PY, CS_PW, CS_PH, 0.23f, 0.27f, 0.32f, 0.98f);
	CSRect(CS_PX + 1.0f, CS_PY + 1.0f, CS_PW - 2.0f, CS_PH - 2.0f, 0.045f, 0.062f, 0.082f, 0.96f);

	// Title Bar
	CSRect(CS_PX + 1.0f, CS_PY + 1.0f, CS_PW - 2.0f, 24.0f, 0.075f, 0.105f, 0.140f, 0.98f);
	CSRect(CS_PX + 1.0f, CS_PY + 25.0f, CS_PW - 2.0f, 1.0f, 0.435f, 0.714f, 0.769f, 0.85f);
	CSText(CS_PX + 10.0f, CS_PY + 7.0f, "CASH SHOP (X)", CSColor(150, 232, 248, 255), true);

	char wcNum[24], wcBuf[48];
	CSFormatComma(this->m_WCoin, wcNum, sizeof(wcNum));
	sprintf_s(wcBuf, sizeof(wcBuf), "WCoin: %s", wcNum);
	CSText(CS_PX + 210.0f, CS_PY + 7.5f, wcBuf, CSColor(255, 224, 105, 255), true);

	CSDrawButton(CS_PX + CS_PW - 24.0f, CS_PY + 4.0f, 18.0f, 17.0f, "X", 0, true);

	// 3 Clean Category Tabs: All | Jewels | VIP & Services
	const char* catNames[3] = { "All Items", "Jewels", "VIP & Services" };
	const int catIds[3] = { 0, 2, 4 };
	const float tabY = CS_PY + 31.0f;
	const float tabW = (CS_PW - 24.0f - 8.0f) / 3.0f;
	for (int i = 0; i < 3; i++)
	{
		float tx = CS_PX + 12.0f + i * (tabW + 4.0f);
		CSDrawButton(tx, tabY, tabW, 19.0f, catNames[i], (this->m_Category == catIds[i]) ? 1 : 0, true);
	}

	// Filter products by category (0 = All)
	int filtered[CASHSHOP_MAX_PRODUCT];
	int fCount = 0;
	for (int i = 0; i < this->m_Count; i++)
	{
		if (this->m_Category == 0 || this->m_Products[i].category == this->m_Category)
		{
			filtered[fCount++] = i;
		}
	}

	int perPage = 4;
	int totalPages = (fCount + perPage - 1) / perPage;
	if (totalPages < 1) totalPages = 1;
	if (this->m_Page >= totalPages) this->m_Page = totalPages - 1;
	if (this->m_Page < 0) this->m_Page = 0;

	const float bodyX = CS_PX + 12.0f;
	const float bodyY = CS_PY + 58.0f;
	const float cardW = (CS_PW - 24.0f - 8.0f) * 0.5f; // 206
	const float cardH = 104.0f;

	if (fCount == 0)
	{
		CSTextCentered(CS_PX + CS_PW * 0.5f, bodyY + 80.0f, "No items available in this category.", CSColor(150, 170, 185, 255), false);
	}
	else
	{
		int start = this->m_Page * perPage;
		for (int slot = 0; slot < perPage && (start + slot) < fCount; slot++)
		{
			int pIdx = filtered[start + slot];
			const PMSG_CASHSHOP_ENTRY& prod = this->m_Products[pIdx];

			int col = slot % 2;
			int row = slot / 2;
			float cx = bodyX + col * (cardW + 8.0f);
			float cy = bodyY + row * (cardH + 8.0f);

			// Card frame
			CSRect(cx, cy, cardW, cardH, 0.24f, 0.30f, 0.36f, 0.94f);
			CSRect(cx + 1.0f, cy + 1.0f, cardW - 2.0f, cardH - 2.0f, 0.062f, 0.086f, 0.115f, 0.96f);
			CSRect(cx + 1.0f, cy + 1.0f, cardW - 2.0f, 2.0f, (prod.type == 1) ? 0.92f : 0.40f, (prod.type == 1) ? 0.75f : 0.72f, (prod.type == 1) ? 0.25f : 0.82f, 0.85f);

			// Item preview box
			float ibx = cx + 8.0f, iby = cy + 10.0f, ibw = 42.0f, ibh = 42.0f;
			CSRect(ibx, iby, ibw, ibh, 0.26f, 0.34f, 0.42f, 0.95f);
			CSRect(ibx + 1.0f, iby + 1.0f, ibw - 2.0f, ibh - 2.0f, 0.035f, 0.050f, 0.070f, 0.96f);

			if (prod.type == 0)
			{
				RenderCashShopItem3D(ibx + 1.0f, iby + 1.0f, ibw - 2.0f, ibh - 2.0f, prod.item, prod.level);
			}
			else if (prod.type == 1)
			{
				CSTextCentered(ibx + ibw * 0.5f, iby + 15.0f, "VIP", CSColor(255, 224, 105, 255), true);
			}
			else
			{
				CSTextCentered(ibx + ibw * 0.5f, iby + 15.0f, "CLS", CSColor(135, 232, 255, 255), true);
			}

			// Name & short description (wrapped up to 2 lines)
			CSText(cx + 56.0f, cy + 12.0f, prod.name, (prod.type == 1) ? CSColor(255, 226, 120, 255) : CSColor(235, 245, 252, 255), true);

			char line1[32] = { 0 }, line2[32] = { 0 };
			int dlen = (int)strlen(prod.description);
			if (dlen <= 26)
			{
				strcpy_s(line1, sizeof(line1), prod.description);
			}
			else
			{
				int split = 26;
				while (split > 10 && prod.description[split] != ' ') split--;
				if (split <= 10) split = 26;
				strncpy_s(line1, sizeof(line1), prod.description, split);
				const char* rest = prod.description + split;
				while (*rest == ' ') rest++;
				strncpy_s(line2, sizeof(line2), rest, 28);
			}
			CSText(cx + 56.0f, cy + 28.0f, line1, CSColor(160, 185, 202, 255), false);
			if (line2[0]) CSText(cx + 56.0f, cy + 40.0f, line2, CSColor(160, 185, 202, 255), false);

			// Divider + Price + Buy button
			CSRect(cx + 8.0f, cy + 62.0f, cardW - 16.0f, 1.0f, 0.20f, 0.26f, 0.32f, 0.85f);

			char pNum[24], pBuf[40];
			CSFormatComma(prod.price, pNum, sizeof(pNum));
			sprintf_s(pBuf, sizeof(pBuf), "%s WCoin", pNum);
			CSText(cx + 10.0f, cy + 75.0f, pBuf, CSColor(255, 220, 95, 255), true);

			bool canBuy = (this->m_Enable && this->m_WCoin >= prod.price);
			CSDrawButton(cx + cardW - 72.0f, cy + 70.0f, 64.0f, 24.0f, "Buy", 2, canBuy);
		}
	}

	// Status banner
	if (this->m_StatusText[0] != 0 && (GetTickCount() - this->m_StatusTick) < 5000)
	{
		DWORD sc = this->m_StatusGood ? CSColor(120, 240, 155, 255) : CSColor(255, 115, 115, 255);
		CSTextCentered(CS_PX + CS_PW * 0.5f, CS_PY + CS_PH - 42.0f, this->m_StatusText, sc, true);
	}

	// Footer Pagination + Refresh
	const float footY = CS_PY + CS_PH - 25.0f;
	CSDrawButton(bodyX, footY, 60.0f, 18.0f, "Refresh", 0, true);
	if (totalPages > 1)
	{
		const float midX = CS_PX + CS_PW * 0.5f;
		CSDrawButton(midX - 86.0f, footY, 48.0f, 18.0f, "< Prev", 0, this->m_Page > 0);
		char pgBuf[24];
		sprintf_s(pgBuf, sizeof(pgBuf), "Page %d / %d", this->m_Page + 1, totalPages);
		CSTextCentered(midX, footY + 4.0f, pgBuf, CSColor(185, 205, 220, 255), false);
		CSDrawButton(midX + 38.0f, footY, 48.0f, 18.0f, "Next >", 0, (this->m_Page + 1) < totalPages);
	}

	// ---- Confirm Purchase Modal (supports Class Picker for Type==2) ----
	if (this->m_ConfirmProductIdx >= 0 && this->m_ConfirmProductIdx < this->m_Count)
	{
		const PMSG_CASHSHOP_ENTRY& cp = this->m_Products[this->m_ConfirmProductIdx];
		const bool isClassChange = (cp.type == 2);
		CSRect(CS_PX, CS_PY, CS_PW, CS_PH, 0.0f, 0.0f, 0.0f, 0.62f);

		const float mw = 300.0f;
		const float mh = isClassChange ? 168.0f : 122.0f;
		const float mx = CS_PX + (CS_PW - mw) * 0.5f;
		const float my = CS_PY + (CS_PH - mh) * 0.5f;

		CSRect(mx, my, mw, mh, 0.42f, 0.76f, 0.88f, 0.98f);
		CSRect(mx + 1.0f, my + 1.0f, mw - 2.0f, mh - 2.0f, 0.055f, 0.080f, 0.112f, 0.98f);
		CSRect(mx + 1.0f, my + 1.0f, mw - 2.0f, 22.0f, 0.085f, 0.125f, 0.168f, 0.98f);
		CSTextCentered(mx + mw * 0.5f, my + 7.0f, isClassChange ? "CHANGE CLASS" : "CONFIRM PURCHASE", CSColor(150, 232, 248, 255), true);

		CSTextCentered(mx + mw * 0.5f, my + 32.0f, cp.name, CSColor(255, 238, 165, 255), true);

		char cNum[24], cBuf[64];
		CSFormatComma(cp.price, cNum, sizeof(cNum));
		sprintf_s(cBuf, sizeof(cBuf), "Price: %s WCoin", cNum);
		CSTextCentered(mx + mw * 0.5f, my + 49.0f, cBuf, CSColor(135, 232, 248, 255), true);

		if (isClassChange)
		{
			CSTextCentered(mx + mw * 0.5f, my + 67.0f, "Select target class (unequip all gear first):", CSColor(200, 218, 232, 255), false);
			const char* clsNames[4] = { "DW", "DK", "ELF", "MG" };
			float cw = (mw - 32.0f - 12.0f) / 4.0f;
			for (int c = 0; c < 4; c++)
			{
				CSDrawButton(mx + 16.0f + c * (cw + 4.0f), my + 84.0f, cw, 22.0f, clsNames[c], (this->m_SelectedClass == c) ? 1 : 0, true);
			}
		}

		float btnY = my + mh - 36.0f;
		CSDrawButton(mx + 34.0f, btnY, 108.0f, 24.0f, "Confirm Buy", 2, true);
		CSDrawButton(mx + mw - 142.0f, btnY, 108.0f, 24.0f, "Cancel", 0, true);
	}

	CSSetFont(oldFont ? oldFont : g_hFont);
	SetTextColor = oldTC;
	SetBackgroundTextColor = oldBG;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

void CCashShopUI::UpdateMouse()
{
	if (SceneFlag != MAIN_SCENE || !Hero) return;

	// 1. Collapsible HUD Button (Slot 11)
	if (!HudIconBarCollapsed())
	{
		float btnW, btnH, bx, by;
		GetCashShopHudLayout(&btnW, &btnH, &bx, &by);
		if (CSHit(bx, by, btnW, btnH))
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
	}

	if (!this->m_Open || !CSHit(CS_PX, CS_PY, CS_PW, CS_PH)) return;

	MouseOnWindow = true;
	if (!MouseLButtonPush) return;

	MouseLButtonPush = false;
	MouseLButton = false;
	MouseLButtonPop = false;
	PlayBuffer(25, 0, 0);

	// Confirm Purchase Modal intercepts clicks first
	if (this->m_ConfirmProductIdx >= 0 && this->m_ConfirmProductIdx < this->m_Count)
	{
		const bool isClassChange = (this->m_Products[this->m_ConfirmProductIdx].type == 2);
		const float mw = 300.0f;
		const float mh = isClassChange ? 168.0f : 122.0f;
		const float mx = CS_PX + (CS_PW - mw) * 0.5f;
		const float my = CS_PY + (CS_PH - mh) * 0.5f;

		if (isClassChange)
		{
			float cw = (mw - 32.0f - 12.0f) / 4.0f;
			for (int c = 0; c < 4; c++)
			{
				if (CSHit(mx + 16.0f + c * (cw + 4.0f), my + 84.0f, cw, 22.0f))
				{
					this->m_SelectedClass = c;
					return;
				}
			}
		}

		float btnY = my + mh - 36.0f;
		if (CSHit(mx + 34.0f, btnY, 108.0f, 24.0f))
		{
			int idx = this->m_ConfirmProductIdx;
			this->m_ConfirmProductIdx = -1;
			this->RequestBuy((BYTE)(idx + 1), isClassChange ? (BYTE)this->m_SelectedClass : 0);
			return;
		}
		if (CSHit(mx + mw - 142.0f, btnY, 108.0f, 24.0f))
		{
			this->m_ConfirmProductIdx = -1;
			return;
		}
		return;
	}

	// Close [X]
	if (CSHit(CS_PX + CS_PW - 24.0f, CS_PY + 4.0f, 18.0f, 17.0f))
	{
		this->m_ConfirmProductIdx = -1;
		this->Close();
		return;
	}

	// Category tabs (All, Jewels, VIP & Services)
	const int catIds[3] = { 0, 2, 4 };
	const float tabY = CS_PY + 31.0f;
	const float tabW = (CS_PW - 24.0f - 8.0f) / 3.0f;
	for (int i = 0; i < 3; i++)
	{
		float tx = CS_PX + 12.0f + i * (tabW + 4.0f);
		if (CSHit(tx, tabY, tabW, 19.0f))
		{
			this->m_Category = catIds[i];
			this->m_Page = 0;
			return;
		}
	}

	int filtered[CASHSHOP_MAX_PRODUCT];
	int fCount = 0;
	for (int i = 0; i < this->m_Count; i++)
	{
		if (this->m_Category == 0 || this->m_Products[i].category == this->m_Category)
		{
			filtered[fCount++] = i;
		}
	}

	int perPage = 4;
	int totalPages = (fCount + perPage - 1) / perPage;
	if (totalPages < 1) totalPages = 1;

	const float bodyX = CS_PX + 12.0f;
	const float bodyY = CS_PY + 58.0f;
	const float cardW = (CS_PW - 24.0f - 8.0f) * 0.5f;
	const float cardH = 104.0f;

	// Buy buttons
	int start = this->m_Page * perPage;
	for (int slot = 0; slot < perPage && (start + slot) < fCount; slot++)
	{
		int pIdx = filtered[start + slot];
		int col = slot % 2;
		int row = slot / 2;
		float cx = bodyX + col * (cardW + 8.0f);
		float cy = bodyY + row * (cardH + 8.0f);

		if (CSHit(cx + cardW - 72.0f, cy + 70.0f, 64.0f, 24.0f))
		{
			if (this->m_Enable && this->m_WCoin >= this->m_Products[pIdx].price)
			{
				this->m_ConfirmProductIdx = pIdx;
			}
			return;
		}
	}

	// Footer Refresh + Pagination
	const float footY = CS_PY + CS_PH - 25.0f;
	if (CSHit(bodyX, footY, 60.0f, 18.0f))
	{
		this->RequestInfo();
		return;
	}
	if (totalPages > 1)
	{
		const float midX = CS_PX + CS_PW * 0.5f;
		if (this->m_Page > 0 && CSHit(midX - 86.0f, footY, 48.0f, 18.0f))
		{
			this->m_Page--;
			return;
		}
		if ((this->m_Page + 1) < totalPages && CSHit(midX + 38.0f, footY, 48.0f, 18.0f))
		{
			this->m_Page++;
			return;
		}
	}
}
