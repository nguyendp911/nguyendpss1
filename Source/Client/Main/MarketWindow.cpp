#include "stdafx.h"
#include "MarketWindow.h"
#include "Interface.h"
#include "Font.h"
#include "Item.h"
#include "ItemManager.h"
#include "ItemPost.h"
#include "Protocol.h"
#include "stb.h"
#include <stdio.h>
#include <stdlib.h>

CMarketWindow gMarketWindow;

// Cold Steel HD Window Geometry (640x480 virtual space)
static const float MK_PW = 500.0f;
static const float MK_PH = 372.0f;
static const float MK_PX = (640.0f - MK_PW) * 0.5f; // 70
static const float MK_PY = 52.0f;

static void GetMarketHudLayout(float* btnW, float* btnH, float* btnX, float* y)
{
	const float asp = (g_fScreenRate_x > 0.0f) ? (g_fScreenRate_y / g_fScreenRate_x) : 0.75f;
	*btnH = 22.0f;
	*btnW = (*btnH) * asp;
	*y = 8.0f;
	// Collapsible slot 7: Pinned(0..5), PK(6), Market(7), Recorder(8)
	*btnX = 8.0f + (*btnW + 3.0f) * 7.0f;
}

static bool MKHit(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static DWORD MKColor(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static void MKSetFont(HFONT f)
{
	if (f) SelectObject(m_hFontDC, f);
}

static float MKMeasureW(const char* text)
{
	if (!text || !text[0]) return 0.0f;
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (float)sz.cx / (g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f);
}

static void MKRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

static void MKText(float x, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	MKSetFont(bold ? g_hFontBold : g_hFont);
	EnableAlphaTest(true);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	SetBackgroundTextColor = 0;
	SetTextColor = MKColor(0, 0, 0, 220);
	RenderText((int)x + 1, (int)y + 1, (char*)text, 0, RT3_SORT_LEFT, NULL);
	SetTextColor = color;
	RenderText((int)x, (int)y, (char*)text, 0, RT3_SORT_LEFT, NULL);
}

static void MKTextCentered(float cx, float y, const char* text, DWORD color, bool bold = false)
{
	if (!text || !text[0]) return;
	MKSetFont(bold ? g_hFontBold : g_hFont);
	float w = MKMeasureW(text);
	MKText(cx - w * 0.5f, y, text, color, bold);
}

static void MKDrawButton(float x, float y, float w, float h, const char* text, int style, bool enabled = true)
{
	bool hov = enabled && MKHit(x, y, w, h);

	float br = 0.24f, bg = 0.29f, bb = 0.35f;
	float fr = 0.09f, fg = 0.125f, fb = 0.165f;
	DWORD tc = MKColor(218, 232, 242, 255);

	if (!enabled)
	{
		br = 0.15f; bg = 0.18f; bb = 0.21f;
		fr = 0.055f; fg = 0.07f; fb = 0.09f;
		tc = MKColor(100, 114, 126, 255);
	}
	else if (style == 1) // Active / Primary Cyan-Steel
	{
		br = hov ? 0.50f : 0.34f; bg = hov ? 0.86f : 0.68f; bb = hov ? 0.96f : 0.78f;
		fr = hov ? 0.14f : 0.095f; fg = hov ? 0.32f : 0.23f; fb = hov ? 0.40f : 0.30f;
		tc = MKColor(238, 252, 255, 255);
	}
	else if (style == 2) // Emerald (Buy / List / Claim)
	{
		br = hov ? 0.34f : 0.22f; bg = hov ? 0.84f : 0.66f; bb = hov ? 0.56f : 0.44f;
		fr = hov ? 0.09f : 0.06f; fg = hov ? 0.31f : 0.22f; fb = hov ? 0.19f : 0.14f;
		tc = MKColor(228, 255, 238, 255);
	}
	else if (style == 3) // Crimson (Cancel)
	{
		br = hov ? 0.88f : 0.70f; bg = hov ? 0.32f : 0.22f; bb = hov ? 0.34f : 0.24f;
		fr = hov ? 0.34f : 0.24f; fg = hov ? 0.09f : 0.06f; fb = hov ? 0.10f : 0.07f;
		tc = MKColor(255, 228, 228, 255);
	}
	else if (hov)
	{
		br = 0.42f; bg = 0.70f; bb = 0.78f;
		fr = 0.14f; fg = 0.20f; fb = 0.26f;
		tc = MKColor(245, 252, 255, 255);
	}

	MKRect(x, y, w, h, br, bg, bb, 0.96f);
	MKRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fr, fg, fb, 0.96f);
	MKTextCentered(x + w * 0.5f, y + (h - 9.0f) * 0.5f, text, tc, style != 0);
}

static void DrawMarketIconQuad(GLuint tex, float x, float y, float w, float h, float u0, float u1, float r, float g, float b, float a)
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

static void FormatComma(DWORD num, char* out, int outSize)
{
	char raw[32];
	sprintf_s(raw, sizeof(raw), "%u", num);
	int len = (int)strlen(raw);
	int idx = 0;
	for (int i = 0; i < len && idx < outSize - 1; i++)
	{
		if (i > 0 && (len - i) % 3 == 0 && idx < outSize - 1) out[idx++] = ',';
		out[idx++] = raw[i];
	}
	out[idx] = '\0';
}

static void GetCurrencyInfo(BYTE priceType, const char** outName, DWORD* outColor)
{
	switch (priceType)
	{
		case MARKET_PRICE_ZEN:   *outName = "Zen";   *outColor = MKColor(255, 215, 85, 255); break;
		case MARKET_PRICE_BLESS: *outName = "Bless"; *outColor = MKColor(255, 140, 225, 255); break;
		case MARKET_PRICE_SOUL:  *outName = "Soul";  *outColor = MKColor(95, 195, 255, 255); break;
		case MARKET_PRICE_CHAOS: *outName = "Chaos"; *outColor = MKColor(255, 165, 70, 255); break;
		case MARKET_PRICE_WCOIN: *outName = "WCoin"; *outColor = MKColor(80, 235, 255, 255); break;
		default:                 *outName = "Zen";   *outColor = MKColor(255, 215, 85, 255); break;
	}
}

static void BuildItemDisplayName(const ITEM* item, char* out, int outSize, DWORD* outColor)
{
	char baseName[64] = { 0 };
	int level = (item->Level >> 3) & 15;
	gItemManager.GetItemName(item->Type, level, baseName);
	bool hasPlus = (strchr(baseName, '+') != NULL);

	if (item->Option1 & 0x3F)
	{
		if (level > 0 && !hasPlus) sprintf_s(out, outSize, "Exc %s +%d", baseName, level);
		else sprintf_s(out, outSize, "Exc %s", baseName);
		*outColor = MKColor(115, 245, 145, 255);
	}
	else if (level >= 7)
	{
		if (!hasPlus) sprintf_s(out, outSize, "%s +%d", baseName, level);
		else strcpy_s(out, outSize, baseName);
		*outColor = MKColor(255, 215, 95, 255);
	}
	else if (level > 0)
	{
		if (!hasPlus) sprintf_s(out, outSize, "%s +%d", baseName, level);
		else strcpy_s(out, outSize, baseName);
		*outColor = MKColor(135, 195, 255, 255);
	}
	else
	{
		strcpy_s(out, outSize, baseName);
		*outColor = MKColor(235, 244, 252, 255);
	}
}

// Renders any MU 3D item uniformly scaled & centered inside (boxX, boxY, boxW, boxH) with scissor clipping
static void RenderMarketItemFit3D(float boxX, float boxY, float boxW, float boxH, ITEM* pItem)
{
	if (!pItem || pItem->Type < 0 || pItem->Type >= MAX_ITEM) return;

	ITEM_ATTRIBUTE* attr = (ITEM_ATTRIBUTE*)(ItemAttribute + pItem->Type * sizeof(ITEM_ATTRIBUTE));
	float cw = (attr && attr->Width > 0) ? (float)attr->Width : 1.0f;
	float ch = (attr && attr->Height > 0) ? (float)attr->Height : 1.0f;

	// Native MU inventory cell is 20x20 virtual units; compute uniform fit scale inside the box
	float natW = cw * 20.0f;
	float natH = ch * 20.0f;
	float sxScale = (boxW * 0.90f) / natW;
	float syScale = (boxH * 0.90f) / natH;
	float fitScale = (sxScale < syScale) ? sxScale : syScale;
	if (fitScale > 1.0f) fitScale = 1.0f;
	if (fitScale < 0.25f) fitScale = 0.25f;

	// Box center in OpenGL NDC [-1..1]
	float cx = boxX + boxW * 0.5f;
	float cy = boxY + boxH * 0.5f;
	float ndcX = (cx / 640.0f) * 2.0f - 1.0f;
	float ndcY = 1.0f - (cy / 480.0f) * 2.0f;

	// Scissor rect in physical window pixels (strictly inside the box)
	int scX = (int)(boxX * g_fScreenRate_x);
	int scW = (int)(boxW * g_fScreenRate_x);
	int scH = (int)(boxH * g_fScreenRate_y);
	int scY = WindowHeight - (int)((boxY + boxH) * g_fScreenRate_y);

	EndBitmap();

	glEnable(GL_SCISSOR_TEST);
	glScissor(scX, scY, scW > 0 ? scW : 1, scH > 0 ? scH : 1);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glViewport2(0, 0, WindowWidth, WindowHeight);
	glTranslatef(ndcX, ndcY, 0.0f);
	glScalef(fitScale, fitScale, 1.0f);
	glTranslatef(-ndcX, -ndcY, 0.0f);
	gluPerspective2(1.0f, (float)WindowWidth / (float)WindowHeight, 20.0f, 2000.0f);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	GetOpenGLMatrix(CameraMatrix);
	EnableDepthTest();
	EnableDepthMask();
	glClear(GL_DEPTH_BUFFER_BIT);

	ObjectSelect_Angle[0] = 0.0f;
	ObjectSelect_Angle[1] = 0.0f;
	ObjectSelect_Angle[2] = 0.0f;
	ObjectSelect_HeadAngle[0] = 0.0f;
	ObjectSelect_HeadAngle[1] = 0.0f;
	ObjectSelect_HeadAngle[2] = 0.0f;

	// Pass native item rect centered at (cx, cy) so RenderItem3D places the model centered at (ndcX, ndcY)
	RenderItem3D(cx - natW * 0.5f, cy - natH * 0.5f - 2.0f, natW, natH, pItem->Type, pItem->Level, pItem->Option1, false);

	MyUpdateMousePosition();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glDisable(GL_SCISSOR_TEST);

	BeginBitmap();
}

static ITEM* GetBagItem(int slot)
{
	if (slot < 0 || slot >= INVENTORY_ITEM_SIZE) return NULL;
	ITEM* pItem = GetInventoryItem(slot);
	if (!pItem || pItem->Type < 0 || pItem->Type >= MAX_ITEM) return NULL;
	return pItem;
}

CMarketWindow::CMarketWindow()
{
	this->m_bIsOpen = false;
	this->m_TexBtnCS = 0;
	this->m_dwMyWCoin = 0;

	this->m_CurrentTab = MARKET_TAB_BROWSE;
	this->m_SelectedCategory = MARKET_CAT_ALL;
	this->m_CurrentPage = 1;
	this->m_TotalPages = 1;
	this->m_TotalItems = 0;
	this->m_MyItemsPage = 1;

	this->m_bHoveringItem = false;
	memset(&this->m_HoverItem, 0, sizeof(this->m_HoverItem));

	this->m_SelectedInventorySlot = -1;
	strcpy_s(this->m_szPriceInput, sizeof(this->m_szPriceInput), "100000");
	this->m_bEditingPrice = false;
	this->m_SelectedPriceType = MARKET_PRICE_ZEN;

	this->m_bShowConfirmBuy = false;
	this->m_ConfirmBuyMarketId = 0;
	memset(this->m_ConfirmBuyItemName, 0, sizeof(this->m_ConfirmBuyItemName));
	this->m_ConfirmBuyPrice = 0;
	this->m_ConfirmBuyPriceType = MARKET_PRICE_ZEN;
}

CMarketWindow::~CMarketWindow()
{
}

void CMarketWindow::LoadImages()
{
	if (this->m_TexBtnCS != 0) return;

	int w = 0, h = 0, ch = 0;
	unsigned char* data = stbi_load("Data\\Interface\\MarketCS.png", &w, &h, &ch, 4);
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

void CMarketWindow::Open()
{
	if (SceneFlag != MAIN_SCENE || !Hero) return;
	this->m_bIsOpen = true;
	this->m_bShowConfirmBuy = false;
	this->m_bEditingPrice = false;
	if (this->m_CurrentTab == MARKET_TAB_MY_ITEMS)
	{
		this->RequestMyItems();
	}
	else
	{
		this->RequestPage(this->m_SelectedCategory, this->m_CurrentPage);
	}
}

void CMarketWindow::Close()
{
	this->m_bIsOpen = false;
	this->m_bShowConfirmBuy = false;
	this->m_bEditingPrice = false;
}

void CMarketWindow::Toggle()
{
	if (SceneFlag != MAIN_SCENE || !Hero) return;
	PlayBuffer(25, 0, 0);
	if (this->m_bIsOpen) this->Close();
	else this->Open();
}

void CMarketWindow::OnChar(wchar_t ch)
{
	if (!this->m_bIsOpen || !this->m_bEditingPrice) return;

	size_t len = strlen(this->m_szPriceInput);
	if (ch == VK_BACK)
	{
		if (len > 0) this->m_szPriceInput[len - 1] = '\0';
	}
	else if (ch >= L'0' && ch <= L'9')
	{
		if (len < 10)
		{
			this->m_szPriceInput[len] = (char)ch;
			this->m_szPriceInput[len + 1] = '\0';
		}
	}
}

void CMarketWindow::ConvertBytesToItem(BYTE* data, ITEM* pOutItem)
{
	memset(pOutItem, 0, sizeof(ITEM));
	int wType = data[0] | ((data[9] & 0xF0) * 32) | ((data[7] & 0x80) * 2);
	if (wType < 0 || wType >= MAX_ITEM) return;

	pOutItem->Type = (short)wType;
	pOutItem->Durability = data[2];
	gItem.ItemConvert(pOutItem, data[1], data[7]);
}

void CMarketWindow::RequestPage(int category, int page)
{
	CG_MARKET_PAGE_REQ pMsg;
	pMsg.header.set(0x98, 0x01, sizeof(pMsg));
	pMsg.category = (BYTE)category;
	pMsg.page = (BYTE)page;
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CMarketWindow::RequestBuy(DWORD marketId)
{
	CG_MARKET_BUY_REQ pMsg;
	pMsg.header.set(0x98, 0x02, sizeof(pMsg));
	pMsg.marketId = marketId;
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CMarketWindow::RequestSell(int slot, DWORD price, int priceType)
{
	CG_MARKET_SELL_REQ pMsg;
	pMsg.header.set(0x98, 0x03, sizeof(pMsg));
	pMsg.slot = (BYTE)(INVENTORY_WEAR_SIZE + slot);
	pMsg.price = price;
	pMsg.priceType = (BYTE)priceType;
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CMarketWindow::RequestMyItems()
{
	CG_MARKET_MY_ITEMS_REQ pMsg;
	pMsg.header.set(0x98, 0x04, sizeof(pMsg));
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CMarketWindow::RequestCancel(DWORD marketId)
{
	CG_MARKET_CANCEL_SELL_REQ pMsg;
	pMsg.header.set(0x98, 0x05, sizeof(pMsg));
	pMsg.marketId = marketId;
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CMarketWindow::RequestClaim(DWORD marketId)
{
	CG_MARKET_CLAIM_PAYMENT_REQ pMsg;
	pMsg.header.set(0x98, 0x06, sizeof(pMsg));
	pMsg.marketId = marketId;
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CMarketWindow::OnReceivePage(BYTE* lpMsg)
{
	GC_MARKET_PAGE_ANS* pMsg = (GC_MARKET_PAGE_ANS*)lpMsg;
	this->m_SelectedCategory = pMsg->category;
	this->m_CurrentPage = pMsg->currentPage;
	this->m_TotalPages = (pMsg->totalPages > 0) ? pMsg->totalPages : 1;
	this->m_TotalItems = pMsg->totalItems;
	this->m_dwMyWCoin = pMsg->myWCoin;

	this->m_MarketItems.clear();
	MARKET_ITEM_ENTRY* items = (MARKET_ITEM_ENTRY*)(lpMsg + sizeof(GC_MARKET_PAGE_ANS));
	for (int i = 0; i < pMsg->itemCount; i++)
	{
		CLIENT_MARKET_ITEM entry;
		entry.marketId = items[i].marketId;
		memcpy(entry.sellerName, items[i].sellerName, sizeof(entry.sellerName));
		entry.sellerName[10] = '\0';
		memcpy(entry.itemBytes, items[i].itemBytes, sizeof(entry.itemBytes));
		entry.price = items[i].price;
		entry.priceType = items[i].priceType;
		this->ConvertBytesToItem(entry.itemBytes, &entry.item);
		this->m_MarketItems.push_back(entry);
	}
}

void CMarketWindow::OnReceiveBuyResult(BYTE result, DWORD marketId)
{
	this->m_bShowConfirmBuy = false;
	this->RequestPage(this->m_SelectedCategory, this->m_CurrentPage);
	switch (result)
	{
		case 2: CreateNotice("[Market] Not enough currency!", 0); break;
		case 3: CreateNotice("[Market] Inventory is full!", 0); break;
		case 4: CreateNotice("[Market] Item is no longer available!", 0); break;
		case 5: CreateNotice("[Market] You cannot buy your own item!", 0); break;
		default: break;
	}
}

void CMarketWindow::OnReceiveSellResult(BYTE result)
{
	if (result == 1)
	{
		this->m_SelectedInventorySlot = -1;
		this->m_bEditingPrice = false;
		PlayBuffer(25, 0, 0);
		this->RequestMyItems();
	}
	else
	{
		CreateNotice("[Market] Failed to list item!", 0);
	}
}

void CMarketWindow::OnReceiveMyItems(BYTE* lpMsg)
{
	GC_MARKET_MY_ITEMS_ANS* pMsg = (GC_MARKET_MY_ITEMS_ANS*)lpMsg;
	this->m_dwMyWCoin = pMsg->myWCoin;
	this->m_MyItems.clear();

	MY_MARKET_ITEM_ENTRY* items = (MY_MARKET_ITEM_ENTRY*)(lpMsg + sizeof(GC_MARKET_MY_ITEMS_ANS));
	for (int i = 0; i < pMsg->count; i++)
	{
		CLIENT_MY_MARKET_ITEM entry;
		entry.marketId = items[i].marketId;
		memcpy(entry.itemBytes, items[i].itemBytes, sizeof(entry.itemBytes));
		entry.price = items[i].price;
		entry.priceType = items[i].priceType;
		entry.status = items[i].status;
		memcpy(entry.buyerName, items[i].buyerName, sizeof(entry.buyerName));
		entry.buyerName[10] = '\0';
		this->ConvertBytesToItem(entry.itemBytes, &entry.item);
		this->m_MyItems.push_back(entry);
	}
}

void CMarketWindow::OnReceiveCancelResult(BYTE result, DWORD marketId)
{
	if (result == 2) CreateNotice("[Market] Inventory is full!", 0);
	this->RequestMyItems();
}

void CMarketWindow::OnReceiveClaimResult(BYTE result, DWORD marketId, DWORD price, BYTE priceType)
{
	this->RequestMyItems();
}

void CMarketWindow::Render()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		this->m_bIsOpen = false;
		return;
	}

	if (this->m_TexBtnCS == 0)
	{
		this->LoadImages();
	}

	// ---- Top-Left Cold Steel HUD Button (9th icon) ----
	float btnW, btnH, bx, by;
	GetMarketHudLayout(&btnW, &btnH, &bx, &by);
	const bool showHudBtn = !HudIconBarCollapsed();
	bool hovBtn = showHudBtn && MKHit(bx, by, btnW, btnH);

	if (showHudBtn)
	{
		if (this->m_TexBtnCS != 0)
		{
			float u0 = this->m_bIsOpen ? 0.50f : 0.00f;
			float u1 = this->m_bIsOpen ? 1.00f : 0.50f;
			float c = (this->m_bIsOpen || hovBtn) ? 1.0f : 0.90f;
			DrawMarketIconQuad((GLuint)this->m_TexBtnCS, bx, by, btnW, btnH, u0, u1, c, c, c, 0.98f);
		}
		else
		{
			MKRect(bx, by, btnW, btnH, 0.23f, 0.27f, 0.32f, 0.96f);
			MKRect(bx + 1.0f, by + 1.0f, btnW - 2.0f, btnH - 2.0f, 0.06f, 0.09f, 0.12f, 0.95f);
			MKTextCentered(bx + btnW * 0.5f, by + 6.0f, "MK", MKColor(255, 215, 95, 255), true);
		}
	}

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTC = SetTextColor;
	const DWORD oldBG = SetBackgroundTextColor;

	if (hovBtn)
	{
		const char* tip = "Marketplace (F5)";
		MKSetFont(g_hFont);
		float tw = MKMeasureW(tip);
		float ty = by + btnH + 4.0f;
		MKRect(bx, ty, tw + 10.0f, 15.0f, 0.23f, 0.27f, 0.32f, 0.95f);
		MKRect(bx + 1.0f, ty + 1.0f, tw + 8.0f, 13.0f, 0.055f, 0.075f, 0.098f, 0.92f);
		MKText(bx + 5.0f, ty + 2.5f, tip, MKColor(230, 242, 248, 255), false);
	}

	if (this->m_bIsOpen)
	{
		this->m_bHoveringItem = false;

		// Cold Steel Frame
		MKRect(MK_PX - 2.0f, MK_PY - 2.0f, MK_PW + 4.0f, MK_PH + 4.0f, 0.0f, 0.0f, 0.0f, 0.55f);
		MKRect(MK_PX, MK_PY, MK_PW, MK_PH, 0.23f, 0.27f, 0.32f, 0.98f);
		MKRect(MK_PX + 1.0f, MK_PY + 1.0f, MK_PW - 2.0f, MK_PH - 2.0f, 0.055f, 0.075f, 0.098f, 0.96f);
		MKRect(MK_PX, MK_PY, MK_PW, 2.0f, 0.435f, 0.714f, 0.769f, 0.98f);

		// Header Bar
		MKRect(MK_PX + 1.0f, MK_PY + 2.0f, MK_PW - 2.0f, 24.0f, 0.095f, 0.135f, 0.185f, 0.95f);
		MKRect(MK_PX + 1.0f, MK_PY + 26.0f, MK_PW - 2.0f, 1.0f, 0.23f, 0.27f, 0.32f, 0.92f);

		MKText(MK_PX + 12.0f, MK_PY + 8.0f, "MARKETPLACE", MKColor(145, 228, 246, 255), true);

		char wcBuf[48], wcNum[24];
		FormatComma(this->m_dwMyWCoin, wcNum, sizeof(wcNum));
		sprintf_s(wcBuf, sizeof(wcBuf), "WCoin: %s", wcNum);
		MKText(MK_PX + 125.0f, MK_PY + 9.0f, wcBuf, MKColor(90, 235, 255, 255), true);

		MKText(MK_PX + 382.0f, MK_PY + 9.0f, "F5: Open-Close", MKColor(135, 152, 168, 255), false);
		MKDrawButton(MK_PX + MK_PW - 22.0f, MK_PY + 5.0f, 16.0f, 16.0f, "X", 0, true);

		// 3 Main Tabs
		const float tabY = MK_PY + 32.0f;
		const float tabW = 152.0f;
		const float tabH = 21.0f;
		MKDrawButton(MK_PX + 12.0f, tabY, tabW, tabH, "Browse Market", this->m_CurrentTab == MARKET_TAB_BROWSE ? 1 : 0);
		MKDrawButton(MK_PX + 12.0f + tabW + 10.0f, tabY, tabW, tabH, "Sell Item", this->m_CurrentTab == MARKET_TAB_SELL ? 1 : 0);
		MKDrawButton(MK_PX + 12.0f + (tabW + 10.0f) * 2.0f, tabY, tabW, tabH, "My Listings", this->m_CurrentTab == MARKET_TAB_MY_ITEMS ? 1 : 0);

		if (this->m_CurrentTab == MARKET_TAB_BROWSE) this->RenderBrowseTab();
		else if (this->m_CurrentTab == MARKET_TAB_SELL) this->RenderSellTab();
		else this->RenderMyItemsTab();

		if (this->m_bShowConfirmBuy)
		{
			this->RenderConfirmBuyModal();
		}

		// Render native MU item tooltip when hovering an item
		if (this->m_bHoveringItem && !this->m_bShowConfirmBuy)
		{
			RenderItemInfo(MouseX + 18, MouseY + 12, &this->m_HoverItem, false);
		}
	}

	MKSetFont(oldFont ? oldFont : g_hFont);
	SetTextColor = oldTC;
	SetBackgroundTextColor = oldBG;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	DisableAlphaBlend();
}

void CMarketWindow::RenderBrowseTab()
{
	// Category Filter Bar
	static const char* s_Cats[5] = { "All", "Weapons", "Armor", "Wings/Acc", "Jewels/Misc" };
	const float catY = MK_PY + 58.0f;
	float cx = MK_PX + 12.0f;
	const float catWidths[5] = { 46.0f, 72.0f, 62.0f, 78.0f, 84.0f };
	for (int c = 0; c < 5; c++)
	{
		MKDrawButton(cx, catY, catWidths[c], 18.0f, s_Cats[c], (this->m_SelectedCategory == c) ? 1 : 0);
		cx += catWidths[c] + 5.0f;
	}
	MKDrawButton(MK_PX + MK_PW - 72.0f, catY, 60.0f, 18.0f, "Refresh", 0);

	// 6 Item Rows
	const float rowX = MK_PX + 12.0f;
	const float rowW = MK_PW - 24.0f;
	const float startY = MK_PY + 82.0f;
	const float rowH = 42.0f;

	if (this->m_MarketItems.empty())
	{
		MKRect(rowX, startY, rowW, rowH * 6.0f - 4.0f, 0.04f, 0.055f, 0.075f, 0.92f);
		MKTextCentered(MK_PX + MK_PW * 0.5f, startY + 110.0f, "No items listed in this category.", MKColor(145, 162, 178, 255), true);
	}
	else
	{
		for (size_t i = 0; i < this->m_MarketItems.size() && i < 6; i++)
		{
			const CLIENT_MARKET_ITEM& entry = this->m_MarketItems[i];
			float y = startY + rowH * (float)i;

			MKRect(rowX, y, rowW, rowH - 4.0f, 0.18f, 0.22f, 0.28f, 0.90f);
			MKRect(rowX + 1.0f, y + 1.0f, rowW - 2.0f, rowH - 6.0f, 0.07f, 0.095f, 0.125f, 0.94f);

			// 3D Item Box
			float iconX = rowX + 5.0f, iconY = y + 4.0f, iconS = 30.0f;
			MKRect(iconX, iconY, iconS, iconS, 0.28f, 0.44f, 0.52f, 0.92f);
			MKRect(iconX + 1.0f, iconY + 1.0f, iconS - 2.0f, iconS - 2.0f, 0.035f, 0.05f, 0.07f, 0.96f);

			if (entry.item.Type >= 0 && entry.item.Type < MAX_ITEM)
			{
				RenderMarketItemFit3D(iconX + 1.0f, iconY + 1.0f, iconS - 2.0f, iconS - 2.0f, (ITEM*)&entry.item);
				if (MKHit(iconX, iconY, iconS, iconS))
				{
					this->m_bHoveringItem = true;
					this->m_HoverItem = entry.item;
				}
			}

			// Name & Seller
			char itemName[96];
			DWORD nameColor = MKColor(235, 245, 255, 255);
			BuildItemDisplayName(&entry.item, itemName, sizeof(itemName), &nameColor);
			MKText(rowX + 42.0f, y + 6.0f, itemName, nameColor, true);

			char sellerBuf[48];
			sprintf_s(sellerBuf, sizeof(sellerBuf), "Seller: %s", entry.sellerName);
			MKText(rowX + 42.0f, y + 20.0f, sellerBuf, MKColor(150, 168, 184, 255), false);

			// Price
			const char* curName = "Zen";
			DWORD curCol = MKColor(255, 215, 85, 255);
			GetCurrencyInfo(entry.priceType, &curName, &curCol);
			char pNum[24], pBuf[48];
			FormatComma(entry.price, pNum, sizeof(pNum));
			sprintf_s(pBuf, sizeof(pBuf), "%s %s", pNum, curName);
			MKText(rowX + 275.0f, y + 13.0f, pBuf, curCol, true);

			// Buy Button
			MKDrawButton(rowX + rowW - 64.0f, y + 9.0f, 58.0f, 20.0f, "Buy", 2, true);
		}
	}

	// Pagination Footer
	const float py = MK_PY + MK_PH - 30.0f;
	const float midX = MK_PX + MK_PW * 0.5f;
	MKDrawButton(midX - 110.0f, py, 56.0f, 19.0f, "< Prev", 0, this->m_CurrentPage > 1);
	char pgBuf[48];
	sprintf_s(pgBuf, sizeof(pgBuf), "Page %d / %d (%d items)", this->m_CurrentPage, this->m_TotalPages, this->m_TotalItems);
	MKTextCentered(midX, py + 5.0f, pgBuf, MKColor(185, 205, 220, 255), false);
	MKDrawButton(midX + 54.0f, py, 56.0f, 19.0f, "Next >", 0, this->m_CurrentPage < this->m_TotalPages);
}

void CMarketWindow::RenderSellTab()
{
	const float leftX = MK_PX + 14.0f;
	const float topY = MK_PY + 62.0f;
	const float cell = 26.0f;
	const float gridW = cell * 8.0f;
	const float gridH = cell * 8.0f;

	MKText(leftX, topY, "Select an item from your inventory:", MKColor(175, 198, 214, 255), true);

	const float gx = leftX;
	const float gy = topY + 16.0f;
	MKRect(gx - 1.0f, gy - 1.0f, gridW + 2.0f, gridH + 2.0f, 0.25f, 0.32f, 0.39f, 0.95f);
	MKRect(gx, gy, gridW, gridH, 0.035f, 0.048f, 0.068f, 0.96f);

	// Grid lines
	for (int r = 0; r < 8; r++)
	{
		for (int c = 0; c < 8; c++)
		{
			MKRect(gx + c * cell, gy + r * cell, cell - 1.0f, cell - 1.0f, 0.055f, 0.075f, 0.10f, 0.95f);
		}
	}

	// Render inventory items inside grid (skip secondary cells of multi-cell items)
	bool occupied[INVENTORY_ITEM_SIZE] = { false };
	for (int slot = 0; slot < INVENTORY_ITEM_SIZE; slot++)
	{
		if (occupied[slot]) continue;

		ITEM* pItem = GetBagItem(slot);
		if (!pItem) continue;

		int col = slot % 8;
		int row = slot / 8;
		ITEM_ATTRIBUTE* attr = (ITEM_ATTRIBUTE*)(ItemAttribute + pItem->Type * sizeof(ITEM_ATTRIBUTE));
		int iw = (attr && attr->Width > 0) ? attr->Width : 1;
		int ih = (attr && attr->Height > 0) ? attr->Height : 1;

		for (int dy = 0; dy < ih && (row + dy) < 8; dy++)
		{
			for (int dx = 0; dx < iw && (col + dx) < 8; dx++)
			{
				occupied[(row + dy) * 8 + (col + dx)] = true;
			}
		}

		float ix = gx + col * cell;
		float iy = gy + row * cell;
		float bw = iw * cell - 1.0f;
		float bh = ih * cell - 1.0f;

		bool sel = (this->m_SelectedInventorySlot == slot);
		bool hov = MKHit(ix, iy, bw, bh);

		if (sel)
		{
			MKRect(ix, iy, bw, bh, 0.30f, 0.72f, 0.85f, 0.85f);
			MKRect(ix + 1.0f, iy + 1.0f, bw - 2.0f, bh - 2.0f, 0.09f, 0.24f, 0.32f, 0.92f);
		}
		else if (hov)
		{
			MKRect(ix, iy, bw, bh, 0.14f, 0.24f, 0.32f, 0.88f);
		}
		else
		{
			MKRect(ix, iy, bw, bh, 0.08f, 0.115f, 0.155f, 0.88f);
		}

		RenderMarketItemFit3D(ix + 1.0f, iy + 1.0f, bw - 2.0f, bh - 2.0f, pItem);

		if (hov)
		{
			this->m_bHoveringItem = true;
			this->m_HoverItem = *pItem;
		}
	}

	// Right Panel: Selected Item + Price + Currency + List Button
	const float rx = gx + gridW + 16.0f;
	const float rw = MK_PX + MK_PW - 14.0f - rx;
	const float ry = gy;

	MKRect(rx, ry, rw, gridH, 0.20f, 0.25f, 0.31f, 0.94f);
	MKRect(rx + 1.0f, ry + 1.0f, rw - 2.0f, gridH - 2.0f, 0.06f, 0.085f, 0.112f, 0.96f);

	ITEM* selItem = GetBagItem(this->m_SelectedInventorySlot);
	if (selItem)
	{
		float px = rx + (rw - 56.0f) * 0.5f;
		float py = ry + 10.0f;
		MKRect(px, py, 56.0f, 56.0f, 0.28f, 0.48f, 0.58f, 0.94f);
		MKRect(px + 1.0f, py + 1.0f, 54.0f, 54.0f, 0.035f, 0.05f, 0.07f, 0.96f);
		RenderMarketItemFit3D(px + 2.0f, py + 2.0f, 52.0f, 52.0f, selItem);

		char iName[96];
		DWORD iCol = MKColor(235, 245, 255, 255);
		BuildItemDisplayName(selItem, iName, sizeof(iName), &iCol);
		MKTextCentered(rx + rw * 0.5f, py + 62.0f, iName, iCol, true);
	}
	else
	{
		MKTextCentered(rx + rw * 0.5f, ry + 34.0f, "Click an item on the left", MKColor(135, 152, 168, 255), false);
	}

	// Price Input Box
	MKText(rx + 12.0f, ry + 84.0f, "Price:", MKColor(195, 212, 224, 255), true);
	float inX = rx + 12.0f, inY = ry + 97.0f, inW = rw - 24.0f, inH = 19.0f;
	MKRect(inX, inY, inW, inH, this->m_bEditingPrice ? 0.45f : 0.24f, this->m_bEditingPrice ? 0.82f : 0.30f, this->m_bEditingPrice ? 0.94f : 0.36f, 0.96f);
	MKRect(inX + 1.0f, inY + 1.0f, inW - 2.0f, inH - 2.0f, 0.03f, 0.045f, 0.065f, 0.98f);
	char dispPrice[32];
	sprintf_s(dispPrice, sizeof(dispPrice), "%s%s", this->m_szPriceInput, (this->m_bEditingPrice && ((GetTickCount() / 450) & 1)) ? "_" : "");
	MKText(inX + 6.0f, inY + 4.5f, dispPrice, MKColor(255, 225, 110, 255), true);

	// Currency Buttons (5)
	MKText(rx + 12.0f, ry + 122.0f, "Currency:", MKColor(195, 212, 224, 255), true);
	static const char* s_Cur[5] = { "Zen", "Bless", "Soul", "Chaos", "WCoin" };
	float cw = (rw - 24.0f - 8.0f) / 3.0f;
	for (int c = 0; c < 5; c++)
	{
		int row = c / 3;
		int col = c % 3;
		float bx = rx + 12.0f + col * (cw + 4.0f);
		float by = ry + 135.0f + row * 21.0f;
		MKDrawButton(bx, by, cw, 17.0f, s_Cur[c], (this->m_SelectedPriceType == c) ? 1 : 0);
	}

	// List Button
	DWORD priceVal = (DWORD)strtoul(this->m_szPriceInput, NULL, 10);
	MKDrawButton(rx + 12.0f, ry + gridH - 26.0f, rw - 24.0f, 21.0f, "List Item for Sale (5% Tax)", 2, (selItem != NULL && priceVal > 0));
}

void CMarketWindow::RenderMyItemsTab()
{
	const float rowX = MK_PX + 12.0f;
	const float rowW = MK_PW - 24.0f;
	const float startY = MK_PY + 62.0f;
	const float rowH = 44.0f;

	MKDrawButton(MK_PX + MK_PW - 72.0f, MK_PY + 32.0f + 26.0f, 60.0f, 18.0f, "Refresh", 0);
	MKText(rowX, MK_PY + 61.0f, "Your active & recent market listings:", MKColor(175, 198, 214, 255), true);

	const float listY = startY + 18.0f;

	if (this->m_MyItems.empty())
	{
		MKRect(rowX, listY, rowW, rowH * 5.0f, 0.04f, 0.055f, 0.075f, 0.92f);
		MKTextCentered(MK_PX + MK_PW * 0.5f, listY + 95.0f, "You have no active listings.", MKColor(145, 162, 178, 255), true);
		return;
	}

	int totalPages = ((int)this->m_MyItems.size() + 5) / 6;
	if (totalPages < 1) totalPages = 1;
	if (this->m_MyItemsPage > totalPages) this->m_MyItemsPage = totalPages;
	if (this->m_MyItemsPage < 1) this->m_MyItemsPage = 1;

	int startIdx = (this->m_MyItemsPage - 1) * 6;
	for (int k = 0; k < 6; k++)
	{
		int idx = startIdx + k;
		if (idx >= (int)this->m_MyItems.size()) break;

		const CLIENT_MY_MARKET_ITEM& entry = this->m_MyItems[idx];
		float y = listY + rowH * (float)k;

		MKRect(rowX, y, rowW, rowH - 4.0f, 0.18f, 0.22f, 0.28f, 0.90f);
		MKRect(rowX + 1.0f, y + 1.0f, rowW - 2.0f, rowH - 6.0f, 0.07f, 0.095f, 0.125f, 0.94f);

		float iconX = rowX + 5.0f, iconY = y + 5.0f, iconS = 30.0f;
		MKRect(iconX, iconY, iconS, iconS, 0.28f, 0.44f, 0.52f, 0.92f);
		MKRect(iconX + 1.0f, iconY + 1.0f, iconS - 2.0f, iconS - 2.0f, 0.035f, 0.05f, 0.07f, 0.96f);

		if (entry.item.Type >= 0 && entry.item.Type < MAX_ITEM)
		{
			gItemPost.RenderItemPreview3D(iconX + 1.0f, iconY - 3.5f, iconS - 2.0f, iconS - 2.0f, (ITEM*)&entry.item);
			if (MKHit(iconX, iconY, iconS, iconS))
			{
				this->m_bHoveringItem = true;
				this->m_HoverItem = entry.item;
			}
		}

		char itemName[96];
		DWORD nameColor = MKColor(235, 245, 255, 255);
		BuildItemDisplayName(&entry.item, itemName, sizeof(itemName), &nameColor);
		MKText(rowX + 42.0f, y + 6.0f, itemName, nameColor, true);

		const char* curName = "Zen";
		DWORD curCol = MKColor(255, 215, 85, 255);
		GetCurrencyInfo(entry.priceType, &curName, &curCol);
		char pNum[24], pBuf[48];
		FormatComma(entry.price, pNum, sizeof(pNum));
		sprintf_s(pBuf, sizeof(pBuf), "%s %s", pNum, curName);
		MKText(rowX + 42.0f, y + 21.0f, pBuf, curCol, false);

		if (entry.status == 0)
		{
			MKText(rowX + 275.0f, y + 14.0f, "Selling", MKColor(135, 225, 255, 255), true);
			MKDrawButton(rowX + rowW - 70.0f, y + 10.0f, 64.0f, 20.0f, "Cancel", 3, true);
		}
		else if (entry.status == 1)
		{
			MKText(rowX + 275.0f, y + 14.0f, "Sold (Pending)", MKColor(255, 215, 90, 255), true);
			MKDrawButton(rowX + rowW - 70.0f, y + 10.0f, 64.0f, 20.0f, "Claim", 2, true);
		}
		else
		{
			char soldBuf[48];
			sprintf_s(soldBuf, sizeof(soldBuf), "Sold to %s", entry.buyerName[0] ? entry.buyerName : "Player");
			MKText(rowX + 275.0f, y + 14.0f, soldBuf, MKColor(120, 235, 155, 255), false);
		}
	}

	if (totalPages > 1)
	{
		const float py = MK_PY + MK_PH - 28.0f;
		const float midX = MK_PX + MK_PW * 0.5f;
		MKDrawButton(midX - 90.0f, py, 52.0f, 18.0f, "< Prev", 0, this->m_MyItemsPage > 1);
		char pgBuf[32];
		sprintf_s(pgBuf, sizeof(pgBuf), "Page %d / %d", this->m_MyItemsPage, totalPages);
		MKTextCentered(midX, py + 4.0f, pgBuf, MKColor(185, 205, 220, 255), false);
		MKDrawButton(midX + 38.0f, py, 52.0f, 18.0f, "Next >", 0, this->m_MyItemsPage < totalPages);
	}
}

void CMarketWindow::RenderConfirmBuyModal()
{
	MKRect(MK_PX, MK_PY, MK_PW, MK_PH, 0.0f, 0.0f, 0.0f, 0.60f);

	const float mw = 280.0f, mh = 128.0f;
	const float mx = MK_PX + (MK_PW - mw) * 0.5f;
	const float my = MK_PY + (MK_PH - mh) * 0.5f;

	MKRect(mx, my, mw, mh, 0.35f, 0.68f, 0.78f, 0.98f);
	MKRect(mx + 1.0f, my + 1.0f, mw - 2.0f, mh - 2.0f, 0.06f, 0.085f, 0.115f, 0.98f);

	MKTextCentered(mx + mw * 0.5f, my + 14.0f, "CONFIRM PURCHASE", MKColor(145, 228, 246, 255), true);
	MKTextCentered(mx + mw * 0.5f, my + 38.0f, this->m_ConfirmBuyItemName, MKColor(255, 240, 180, 255), true);

	const char* curName = "Zen";
	DWORD curCol = MKColor(255, 215, 85, 255);
	GetCurrencyInfo(this->m_ConfirmBuyPriceType, &curName, &curCol);
	char pNum[24], pBuf[64];
	FormatComma(this->m_ConfirmBuyPrice, pNum, sizeof(pNum));
	sprintf_s(pBuf, sizeof(pBuf), "Price: %s %s", pNum, curName);
	MKTextCentered(mx + mw * 0.5f, my + 58.0f, pBuf, curCol, true);

	MKDrawButton(mx + 26.0f, my + 90.0f, 104.0f, 22.0f, "Confirm Buy", 2, true);
	MKDrawButton(mx + mw - 130.0f, my + 90.0f, 104.0f, 22.0f, "Cancel", 3, true);
}

void CMarketWindow::UpdateMouse()
{
	if (SceneFlag != MAIN_SCENE || !Hero) return;

	// 1. HUD Button
	if (!HudIconBarCollapsed())
	{
		float btnW, btnH, bx, by;
		GetMarketHudLayout(&btnW, &btnH, &bx, &by);
		if (MKHit(bx, by, btnW, btnH))
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
	}

	if (!this->m_bIsOpen || !MKHit(MK_PX, MK_PY, MK_PW, MK_PH)) return;

	MouseOnWindow = true;
	if (!MouseLButtonPush) return;

	MouseLButtonPush = false;
	MouseLButton = false;
	MouseLButtonPop = false;

	if (this->m_bShowConfirmBuy)
	{
		this->UpdateConfirmBuyMouse();
		return;
	}

	// Close button (X)
	if (MKHit(MK_PX + MK_PW - 22.0f, MK_PY + 5.0f, 16.0f, 16.0f))
	{
		PlayBuffer(25, 0, 0);
		this->Close();
		return;
	}

	// 3 Tabs
	const float tabY = MK_PY + 32.0f;
	const float tabW = 152.0f;
	const float tabH = 21.0f;
	for (int t = 0; t < 3; t++)
	{
		if (MKHit(MK_PX + 12.0f + (tabW + 10.0f) * t, tabY, tabW, tabH))
		{
			PlayBuffer(25, 0, 0);
			this->m_CurrentTab = t;
			this->m_bEditingPrice = false;
			if (t == MARKET_TAB_BROWSE) this->RequestPage(this->m_SelectedCategory, this->m_CurrentPage);
			else if (t == MARKET_TAB_MY_ITEMS) this->RequestMyItems();
			return;
		}
	}

	if (this->m_CurrentTab == MARKET_TAB_BROWSE) this->UpdateBrowseMouse();
	else if (this->m_CurrentTab == MARKET_TAB_SELL) this->UpdateSellMouse();
	else this->UpdateMyItemsMouse();
}

void CMarketWindow::UpdateBrowseMouse()
{
	const float catY = MK_PY + 58.0f;
	float cx = MK_PX + 12.0f;
	const float catWidths[5] = { 46.0f, 72.0f, 62.0f, 78.0f, 84.0f };
	for (int c = 0; c < 5; c++)
	{
		if (MKHit(cx, catY, catWidths[c], 18.0f))
		{
			PlayBuffer(25, 0, 0);
			this->m_SelectedCategory = c;
			this->m_CurrentPage = 1;
			this->RequestPage(this->m_SelectedCategory, 1);
			return;
		}
		cx += catWidths[c] + 5.0f;
	}

	if (MKHit(MK_PX + MK_PW - 72.0f, catY, 60.0f, 18.0f))
	{
		PlayBuffer(25, 0, 0);
		this->RequestPage(this->m_SelectedCategory, this->m_CurrentPage);
		return;
	}

	const float rowX = MK_PX + 12.0f;
	const float rowW = MK_PW - 24.0f;
	const float startY = MK_PY + 82.0f;
	const float rowH = 42.0f;

	for (size_t i = 0; i < this->m_MarketItems.size() && i < 6; i++)
	{
		float y = startY + rowH * (float)i;
		if (MKHit(rowX + rowW - 64.0f, y + 9.0f, 58.0f, 20.0f))
		{
			PlayBuffer(25, 0, 0);
			const CLIENT_MARKET_ITEM& entry = this->m_MarketItems[i];
			this->m_bShowConfirmBuy = true;
			this->m_ConfirmBuyMarketId = entry.marketId;
			DWORD dummy = 0;
			BuildItemDisplayName(&entry.item, this->m_ConfirmBuyItemName, sizeof(this->m_ConfirmBuyItemName), &dummy);
			this->m_ConfirmBuyPrice = entry.price;
			this->m_ConfirmBuyPriceType = entry.priceType;
			return;
		}
	}

	const float py = MK_PY + MK_PH - 30.0f;
	const float midX = MK_PX + MK_PW * 0.5f;
	if (this->m_CurrentPage > 1 && MKHit(midX - 110.0f, py, 56.0f, 19.0f))
	{
		PlayBuffer(25, 0, 0);
		this->RequestPage(this->m_SelectedCategory, this->m_CurrentPage - 1);
		return;
	}
	if (this->m_CurrentPage < this->m_TotalPages && MKHit(midX + 54.0f, py, 56.0f, 19.0f))
	{
		PlayBuffer(25, 0, 0);
		this->RequestPage(this->m_SelectedCategory, this->m_CurrentPage + 1);
		return;
	}
}

void CMarketWindow::UpdateSellMouse()
{
	const float leftX = MK_PX + 14.0f;
	const float topY = MK_PY + 62.0f;
	const float cell = 26.0f;
	const float gridW = cell * 8.0f;
	const float gridH = cell * 8.0f;
	const float gx = leftX;
	const float gy = topY + 16.0f;

	// Inventory grid click (skip secondary cells of multi-cell items)
	bool occupied[INVENTORY_ITEM_SIZE] = { false };
	for (int slot = 0; slot < INVENTORY_ITEM_SIZE; slot++)
	{
		if (occupied[slot]) continue;

		ITEM* pItem = GetBagItem(slot);
		if (!pItem) continue;

		int col = slot % 8;
		int row = slot / 8;
		ITEM_ATTRIBUTE* attr = (ITEM_ATTRIBUTE*)(ItemAttribute + pItem->Type * sizeof(ITEM_ATTRIBUTE));
		int iw = (attr && attr->Width > 0) ? attr->Width : 1;
		int ih = (attr && attr->Height > 0) ? attr->Height : 1;

		for (int dy = 0; dy < ih && (row + dy) < 8; dy++)
		{
			for (int dx = 0; dx < iw && (col + dx) < 8; dx++)
			{
				occupied[(row + dy) * 8 + (col + dx)] = true;
			}
		}

		if (MKHit(gx + col * cell, gy + row * cell, iw * cell - 1.0f, ih * cell - 1.0f))
		{
			PlayBuffer(25, 0, 0);
			this->m_SelectedInventorySlot = slot;
			this->m_bEditingPrice = false;
			return;
		}
	}

	const float rx = gx + gridW + 16.0f;
	const float rw = MK_PX + MK_PW - 14.0f - rx;
	const float ry = gy;

	// Price input click
	if (MKHit(rx + 12.0f, ry + 97.0f, rw - 24.0f, 19.0f))
	{
		this->m_bEditingPrice = true;
		return;
	}
	this->m_bEditingPrice = false;

	// Currency selector
	float cw = (rw - 24.0f - 8.0f) / 3.0f;
	for (int c = 0; c < 5; c++)
	{
		int row = c / 3;
		int col = c % 3;
		if (MKHit(rx + 12.0f + col * (cw + 4.0f), ry + 135.0f + row * 21.0f, cw, 17.0f))
		{
			PlayBuffer(25, 0, 0);
			this->m_SelectedPriceType = c;
			return;
		}
	}

	// List Item button
	ITEM* selItem = GetBagItem(this->m_SelectedInventorySlot);
	DWORD priceVal = (DWORD)strtoul(this->m_szPriceInput, NULL, 10);
	if (selItem != NULL && priceVal > 0 && MKHit(rx + 12.0f, ry + gridH - 26.0f, rw - 24.0f, 21.0f))
	{
		PlayBuffer(25, 0, 0);
		this->RequestSell(this->m_SelectedInventorySlot, priceVal, this->m_SelectedPriceType);
		return;
	}
}

void CMarketWindow::UpdateMyItemsMouse()
{
	if (MKHit(MK_PX + MK_PW - 72.0f, MK_PY + 58.0f, 60.0f, 18.0f))
	{
		PlayBuffer(25, 0, 0);
		this->RequestMyItems();
		return;
	}

	const float rowX = MK_PX + 12.0f;
	const float rowW = MK_PW - 24.0f;
	const float listY = MK_PY + 80.0f;
	const float rowH = 44.0f;

	int totalPages = ((int)this->m_MyItems.size() + 5) / 6;
	if (totalPages < 1) totalPages = 1;
	int startIdx = (this->m_MyItemsPage - 1) * 6;

	for (int k = 0; k < 6; k++)
	{
		int idx = startIdx + k;
		if (idx >= (int)this->m_MyItems.size()) break;

		const CLIENT_MY_MARKET_ITEM& entry = this->m_MyItems[idx];
		float y = listY + rowH * (float)k;

		if (entry.status == 0 && MKHit(rowX + rowW - 70.0f, y + 10.0f, 64.0f, 20.0f))
		{
			PlayBuffer(25, 0, 0);
			this->RequestCancel(entry.marketId);
			return;
		}
		if (entry.status == 1 && MKHit(rowX + rowW - 70.0f, y + 10.0f, 64.0f, 20.0f))
		{
			PlayBuffer(25, 0, 0);
			this->RequestClaim(entry.marketId);
			return;
		}
	}

	if (totalPages > 1)
	{
		const float py = MK_PY + MK_PH - 28.0f;
		const float midX = MK_PX + MK_PW * 0.5f;
		if (this->m_MyItemsPage > 1 && MKHit(midX - 90.0f, py, 52.0f, 18.0f))
		{
			PlayBuffer(25, 0, 0);
			this->m_MyItemsPage--;
		}
		else if (this->m_MyItemsPage < totalPages && MKHit(midX + 38.0f, py, 52.0f, 18.0f))
		{
			PlayBuffer(25, 0, 0);
			this->m_MyItemsPage++;
		}
	}
}

void CMarketWindow::UpdateConfirmBuyMouse()
{
	const float mw = 280.0f, mh = 128.0f;
	const float mx = MK_PX + (MK_PW - mw) * 0.5f;
	const float my = MK_PY + (MK_PH - mh) * 0.5f;

	if (MKHit(mx + 26.0f, my + 90.0f, 104.0f, 22.0f))
	{
		PlayBuffer(25, 0, 0);
		this->RequestBuy(this->m_ConfirmBuyMarketId);
		this->m_bShowConfirmBuy = false;
		return;
	}
	if (MKHit(mx + mw - 130.0f, my + 90.0f, 104.0f, 22.0f))
	{
		PlayBuffer(25, 0, 0);
		this->m_bShowConfirmBuy = false;
		return;
	}
}
