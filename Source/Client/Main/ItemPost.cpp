#include "stdafx.h"
#include "ItemPost.h"
#include "Item.h"
#include "ItemManager.h"
#include "Offsets.h"
#include "Protocol.h"
#include "Util.h"

CItemPost gItemPost;

static inline DWORD PostColor4b(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static inline void PostDrawRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
}

static inline void PostSetFont(HFONT hFont)
{
	SelectObject(m_hFontDC, hFont);
	((void(__thiscall*)(void*, HFONT))0x0040F650)(*(void**)0x055C9FF8, hFont);
}

static inline float PostMeasureTextW(const char* text)
{
	if (!text || !text[0])
	{
		return 0.0f;
	}
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (WindowWidth > 0) ? ((float)sz.cx * 640.0f / (float)WindowWidth) : (float)strlen(text) * 5.0f;
}

struct PMSG_CHAT_SEND_POST
{
	PBMSG_HEAD header; // C1:00
	char name[10];
	char message[140];
};

static void __cdecl MySendChat(const char* Text)
{
	if (!Text || !Text[0])
	{
		return;
	}

	PMSG_CHAT_SEND_POST pMsg;
	pMsg.header.type = 0xC1;
	pMsg.header.head = 0x00;
	memset(pMsg.name, 0, sizeof(pMsg.name));
	memset(pMsg.message, 0, sizeof(pMsg.message));

	if (Hero)
	{
		strncpy_s(pMsg.name, sizeof(pMsg.name), (char*)(Hero + 0x1C1), _TRUNCATE);
	}

	strncpy_s(pMsg.message, sizeof(pMsg.message), Text, _TRUNCATE);

	int len = (int)strlen(pMsg.message);
	pMsg.header.size = (BYTE)(sizeof(PBMSG_HEAD) + 10 + len + 1);

	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

// Hook RenderTipTextList (0x004C2420) so posted-item tooltips draw in Cold Steel style
static BYTE* g_pRenderTipTextListTrampoline = NULL;
static bool g_bColdSteelTip = false;

static void __cdecl MyRenderTipTextList(int sx, int sy, int textNum, int tab, int sort, int point)
{
	if (g_bColdSteelTip && textNum > 0)
	{
		gItemPost.RenderColdSteelTooltip(textNum);
		return;
	}

	if (g_pRenderTipTextListTrampoline)
	{
		((void(__cdecl*)(int, int, int, int, int, int))g_pRenderTipTextListTrampoline)(sx, sy, textNum, tab, sort, point);
	}
}

CItemPost::CItemPost()
{
	this->m_bShowItemToolTip = false;
	this->m_TipX = this->m_TipY = this->m_TipW = this->m_TipH = 0;
	memset(&this->m_HoverItem, 0, sizeof(this->m_HoverItem));
	this->m_HoverItem.Type = -1;
	this->m_HoverLines.clear();
	this->m_PostChatLines.clear();
}

CItemPost::~CItemPost()
{
}

void CItemPost::Init()
{
	this->m_bShowItemToolTip = false;
	this->m_TipX = this->m_TipY = this->m_TipW = this->m_TipH = 0;
	this->m_HoverLines.clear();
	this->m_PostChatLines.clear();

	// Replace SendChat (0x004C1B90) to support 140-byte messages for /post [Item] {HEX}
	SetCompleteHook(0xE9, 0x004C1B90, &MySendChat);

	// Hook 0x004C2420 (RenderTipTextList): first 5 bytes are whole instructions
	g_pRenderTipTextListTrampoline = (BYTE*)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (g_pRenderTipTextListTrampoline)
	{
		memcpy(g_pRenderTipTextListTrampoline, (void*)0x004C2420, 5);
		g_pRenderTipTextListTrampoline[5] = 0xE9;
		*(DWORD*)(g_pRenderTipTextListTrampoline + 6) = (0x004C2420 + 5) - ((DWORD)g_pRenderTipTextListTrampoline + 10);

		SetCompleteHook(0xE9, 0x004C2420, &MyRenderTipTextList);
	}
}

void CItemPost::ItemToHex(ITEM* pItem, char* pszHex, int nMaxHexLen)
{
	if (!pItem || !pszHex || nMaxHexLen < 13)
	{
		return;
	}

	BYTE data[6] = { 0 };
	data[0] = (BYTE)(pItem->Type & 0xFF);
	data[1] = (BYTE)(pItem->Level & 0xFF);
	data[2] = pItem->Durability;
	data[3] = (BYTE)(((pItem->Type & 256) >> 1) | (pItem->Option1 & 0x7F));
	data[4] = pItem->SpecialNum;
	data[5] = (BYTE)((pItem->Type & 0x1E00) >> 5);

	for (int i = 0; i < 6; i++)
	{
		sprintf_s(&pszHex[i * 2], 3, "%02X", data[i]);
	}
	pszHex[12] = '\0';
}

bool CItemPost::HexToItem(const char* pszHex, ITEM* pOutItem)
{
	if (!pszHex || !pOutItem)
	{
		return false;
	}

	size_t len = strlen(pszHex);
	if (len < 12)
	{
		return false;
	}

	BYTE data[12] = { 0 };
	int byteCount = (len >= 24) ? 12 : 6;
	for (int i = 0; i < byteCount; i++)
	{
		char szByte[3] = { pszHex[i * 2], pszHex[i * 2 + 1], '\0' };
		char* pEnd = NULL;
		data[i] = (BYTE)strtoul(szByte, &pEnd, 16);
	}

	WORD wType = (WORD)data[0] + ((WORD)(data[3] & 128) * 2) + ((WORD)(data[5] & 240) * 32);
	if (wType >= MAX_ITEM)
	{
		return false;
	}

	BYTE byLevel = data[1];
	BYTE byDurability = data[2];
	BYTE byOption1 = data[3] & 0x7F;

	memset(pOutItem, 0, sizeof(ITEM));
	pOutItem->Type = (short)wType;
	pOutItem->Durability = byDurability;
	pOutItem->Option1 = byOption1;
	pOutItem->SpecialNum = data[4];

	if (byteCount >= 12)
	{
		pOutItem->Special[0] = data[6];
		pOutItem->SpecialValue[0] = data[7];
		pOutItem->Special[1] = data[8];
		pOutItem->SpecialValue[1] = data[9];
		pOutItem->Special[2] = data[10];
		pOutItem->SpecialValue[2] = data[11];
	}

	gItem.ItemConvert(pOutItem, byLevel, byOption1);
	return true;
}

bool CItemPost::ExtractItemPostHex(const std::string& strText, std::string& strHexOut)
{
	size_t nStart = strText.find('{');
	if (nStart == std::string::npos)
	{
		return false;
	}

	size_t hexStart = nStart + 1;
	size_t count = 0;
	while (hexStart + count < strText.length() && isxdigit((unsigned char)strText[hexStart + count]) && count < 24)
	{
		count++;
	}

	if (count < 12)
	{
		return false;
	}

	if (count % 2 != 0)
	{
		count--;
	}

	if (count < 12)
	{
		return false;
	}

	strHexOut = strText.substr(hexStart, count);
	return true;
}

bool CItemPost::ExtractItemLinkBounds(const std::string& rawText, size_t& outLinkStart, size_t& outLinkLen)
{
	size_t hexPos = rawText.find('{');
	size_t searchPos = (hexPos != std::string::npos) ? hexPos : rawText.length();

	size_t bracketClose = rawText.rfind(']', searchPos);
	if (bracketClose == std::string::npos)
	{
		return false;
	}

	size_t bracketOpen = rawText.rfind('[', bracketClose);
	if (bracketOpen == std::string::npos || bracketOpen >= bracketClose)
	{
		return false;
	}

	outLinkStart = bracketOpen;
	outLinkLen = (bracketClose - bracketOpen + 1);
	return true;
}

std::string CItemPost::CleanItemPostText(const std::string& strText)
{
	size_t nStart = strText.find('{');
	if (nStart != std::string::npos)
	{
		size_t cutStart = nStart;
		if (cutStart > 0 && strText[cutStart - 1] == ' ')
		{
			cutStart--;
		}

		size_t nEnd = strText.find('}', nStart);
		if (nEnd != std::string::npos)
		{
			return strText.substr(0, cutStart) + strText.substr(nEnd + 1);
		}
		else
		{
			return strText.substr(0, cutStart);
		}
	}
	return strText;
}

bool CItemPost::OnChatTextAdded(const char* strID, const char* strText, int MsgType, std::string& outCleanText)
{
	if (!strText || !strText[0])
	{
		return false;
	}

	std::string rawText(strText);
	std::string strHex;
	if (!ExtractItemPostHex(rawText, strHex))
	{
		return false;
	}

	std::string cleanText = CleanItemPostText(rawText);

	size_t linkStart = 0, linkLen = 0;
	std::string linkName = "";
	if (ExtractItemLinkBounds(rawText, linkStart, linkLen))
	{
		linkName = rawText.substr(linkStart, linkLen);
	}

	POST_CHAT_LINE line;
	line.strID = (strID ? strID : "");
	line.rawText = rawText;
	line.cleanText = cleanText;
	line.linkName = linkName;
	line.strHex = strHex;
	line.dwTick = GetTickCount();

	if (HexToItem(strHex.c_str(), &line.item))
	{
		this->AddPostLineDirect(line);
	}

	outCleanText = cleanText;
	return true;
}

void CItemPost::AddPostLineDirect(const POST_CHAT_LINE& line)
{
	this->m_PostChatLines.push_back(line);
	if (this->m_PostChatLines.size() > 50)
	{
		this->m_PostChatLines.erase(this->m_PostChatLines.begin());
	}
}

bool CItemPost::FindPostByText(const char* pszText, POST_CHAT_LINE& outLine)
{
	if (!pszText || !pszText[0])
	{
		return false;
	}

	for (auto it = this->m_PostChatLines.rbegin(); it != this->m_PostChatLines.rend(); ++it)
	{
		if (!it->cleanText.empty() && strstr(pszText, it->cleanText.c_str()) != NULL)
		{
			outLine = *it;
			return true;
		}
	}

	for (auto it = this->m_PostChatLines.rbegin(); it != this->m_PostChatLines.rend(); ++it)
	{
		if (!it->linkName.empty() && strstr(pszText, it->linkName.c_str()) != NULL)
		{
			outLine = *it;
			return true;
		}
	}

	return false;
}

void CItemPost::RegisterRenderedPostItem(int x, int y, int width, int height, const std::string& strHex)
{
	if (strHex.empty() || width <= 0 || height <= 0)
	{
		return;
	}

	DWORD dwNow = GetTickCount();

	for (size_t i = 0; i < this->m_HoverLines.size(); i++)
	{
		if (abs(this->m_HoverLines[i].x - x) <= 12 && abs(this->m_HoverLines[i].y - y) <= 8)
		{
			this->m_HoverLines[i].x = x;
			this->m_HoverLines[i].y = y;
			this->m_HoverLines[i].width = width;
			this->m_HoverLines[i].height = height;
			this->m_HoverLines[i].dwTick = dwNow;
			HexToItem(strHex.c_str(), &this->m_HoverLines[i].item);
			return;
		}
	}

	POST_ITEM_HOVER_LINE line;
	line.x = x;
	line.y = y;
	line.width = width;
	line.height = height;
	line.dwTick = dwNow;
	if (HexToItem(strHex.c_str(), &line.item))
	{
		this->m_HoverLines.push_back(line);
		if (this->m_HoverLines.size() > 50)
		{
			this->m_HoverLines.erase(this->m_HoverLines.begin());
		}
	}
}

void CItemPost::UpdateMouse()
{
	DWORD dwNow = GetTickCount();

	for (auto it = this->m_HoverLines.begin(); it != this->m_HoverLines.end(); )
	{
		if (dwNow - it->dwTick > 1500)
		{
			it = this->m_HoverLines.erase(it);
		}
		else
		{
			++it;
		}
	}

	this->m_bShowItemToolTip = false;

	for (size_t i = 0; i < this->m_HoverLines.size(); i++)
	{
		const auto& line = this->m_HoverLines[i];
		if (MouseX >= line.x && MouseX <= (line.x + line.width) &&
			MouseY >= line.y && MouseY <= (line.y + line.height))
		{
			this->m_bShowItemToolTip = true;
			this->m_HoverItem = line.item;
			break;
		}
	}
}

void CItemPost::Render()
{
	if (!this->m_bShowItemToolTip || this->m_HoverItem.Type < 0 || this->m_HoverItem.Type >= MAX_ITEM)
	{
		return;
	}

	ITEM_ATTRIBUTE* pItemAttr = (ITEM_ATTRIBUTE*)(ItemAttribute + this->m_HoverItem.Type * sizeof(ITEM_ATTRIBUTE));
	if (!pItemAttr)
	{
		return;
	}

	float fWidth = (float)(pItemAttr->Width * 25.0f);
	float fHeight = (float)(pItemAttr->Height * 25.0f);
	if (fWidth < 48.0f) fWidth = 48.0f;
	if (fHeight < 48.0f) fHeight = 48.0f;

	const int tooltipHalfW = 75;

	int sx = MouseX + tooltipHalfW + 10;
	if (sx + tooltipHalfW + (int)fWidth + 16 > 635)
	{
		sx = MouseX - tooltipHalfW - (int)fWidth - 10;
	}
	if (sx - tooltipHalfW < 10)
	{
		sx = 10 + tooltipHalfW;
	}

	int knownLines = *(int*)0x07EAA154;
	if (knownLines < 4) knownLines = 4;
	if (knownLines > 28) knownLines = 28;
	int knownHeight = knownLines * 13 + 12;

	int sy = MouseY - 8;
	if (sy < knownHeight + 8)
	{
		sy = knownHeight + 8;
	}
	if (sy > 470)
	{
		sy = 470;
	}

	// 1. Render Cold Steel tooltip via RenderItemInfo -> MyRenderTipTextList hook
	this->m_TipW = 0;
	g_bColdSteelTip = (g_pRenderTipTextListTrampoline != NULL);
	RenderItemInfo(sx, sy, &this->m_HoverItem, false);
	g_bColdSteelTip = false;

	if (this->m_TipW > 0)
	{
		// Place Cold Steel 3D preview box beside the tooltip, aligned to its top
		float bx = (float)(this->m_TipX + this->m_TipW + 6);
		if (bx + fWidth + 12.0f > 636.0f)
		{
			bx = (float)(this->m_TipX - 6) - (fWidth + 12.0f);
		}
		float by = (float)this->m_TipY;
		if (by + fHeight + 12.0f > 474.0f)
		{
			by = 474.0f - (fHeight + 12.0f);
		}

		float boxW = fWidth + 12.0f;
		float boxH = fHeight + 12.0f;

		// Cold Steel frame: steel border + dark slate body + cyan top accent
		PostDrawRect(bx, by, boxW, boxH, 0.23f, 0.27f, 0.32f, 0.96f);
		PostDrawRect(bx + 1.0f, by + 1.0f, boxW - 2.0f, boxH - 2.0f, 0.055f, 0.075f, 0.098f, 0.94f);
		PostDrawRect(bx, by, boxW, 2.0f, 0.435f, 0.714f, 0.769f, 0.95f);

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		DisableAlphaBlend();

		this->RenderItemPreview3D(bx + 6.0f, by + 6.0f, fWidth, fHeight, &this->m_HoverItem);
	}
}

void CItemPost::RenderColdSteelTooltip(int count)
{
	static const DWORD s_Colors[12] =
	{
		PostColor4b(235, 238, 242, 255), // 0: White
		PostColor4b(125, 185, 255, 255), // 1: Blue
		PostColor4b(255, 95, 95, 255),   // 2: Red
		PostColor4b(255, 215, 90, 255),  // 3: Gold
		PostColor4b(115, 245, 150, 255), // 4: Green
		PostColor4b(225, 65, 75, 255),   // 5: Dark Red
		PostColor4b(200, 150, 255, 255), // 6: Purple
		PostColor4b(110, 145, 240, 255), // 7: Indigo
		PostColor4b(215, 175, 55, 255),  // 8: Amber
		PostColor4b(95, 230, 220, 255),  // 9: Cyan
		PostColor4b(165, 172, 180, 255), // 10: Gray
		PostColor4b(240, 115, 190, 255)  // 11: Pink
	};

	if (count > 60)
	{
		count = 60;
	}

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTextColor = SetTextColor;
	const DWORD oldBgColor = SetBackgroundTextColor;

	float lineH = (WindowHeight > 0) ? (15.0f * 480.0f / (float)WindowHeight) : 12.0f;
	if (lineH < 9.0f) lineH = 9.0f;
	if (lineH > 13.0f) lineH = 13.0f;

	const float padX = 12.0f;
	const float padTop = 8.0f;
	const float padBottom = 8.0f;

	float maxW = 105.0f;
	float totalH = 0.0f;

	for (int i = 0; i < count; i++)
	{
		const char* text = TextList[i];
		if (!text || text[0] == '\0')
		{
			totalH += lineH * 0.45f;
			continue;
		}

		PostSetFont(i == 0 ? g_hFontBold : g_hFont);
		float tw = PostMeasureTextW(text);
		if (tw > maxW)
		{
			maxW = tw;
		}
		totalH += lineH;
	}

	float w = maxW + padX * 2.0f;
	float h = totalH + padTop + padBottom;

	float x = (float)MouseX + 14.0f;
	if (x + w > 636.0f)
	{
		x = (float)MouseX - 14.0f - w;
	}
	if (x < 4.0f)
	{
		x = 4.0f;
	}

	float top = (float)MouseY - 10.0f - h;
	if (top < 4.0f)
	{
		top = 4.0f;
	}
	if (top + h > 474.0f)
	{
		top = 474.0f - h;
	}

	// Cold Steel panel: steel outer border + dark slate body + cyan top accent bar
	PostDrawRect(x, top, w, h, 0.23f, 0.27f, 0.32f, 0.96f);
	PostDrawRect(x + 1.0f, top + 1.0f, w - 2.0f, h - 2.0f, 0.055f, 0.075f, 0.098f, 0.94f);
	PostDrawRect(x, top, w, 2.0f, 0.435f, 0.714f, 0.769f, 0.95f);

	// Title header strip behind line 0
	if (count > 0 && TextList[0][0] != '\0')
	{
		PostDrawRect(x + 2.0f, top + 3.0f, w - 4.0f, lineH + 4.0f, 0.11f, 0.16f, 0.21f, 0.85f);
	}

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();

	SetBackgroundTextColor = 0x00000000;

	float cx = x + w * 0.5f;
	float y = top + padTop;

	for (int i = 0; i < count; i++)
	{
		const char* text = TextList[i];
		if (!text || text[0] == '\0')
		{
			y += lineH * 0.45f;
			continue;
		}

		int colorIndex = TextListColor[i];
		DWORD color = (colorIndex >= 0 && colorIndex < 12) ? s_Colors[colorIndex] : s_Colors[0];

		PostSetFont(i == 0 ? g_hFontBold : g_hFont);
		float tw = PostMeasureTextW(text);
		SetTextColor = color;
		RenderText((int)(cx - tw * 0.5f), (int)y, text, 0, RT3_SORT_LEFT, NULL);

		y += lineH;
	}

	PostSetFont(g_hFont);
	SetTextColor = oldTextColor;
	SetBackgroundTextColor = oldBgColor;
	SelectObject(m_hFontDC, oldFont);

	this->m_TipX = (int)x;
	this->m_TipY = (int)top;
	this->m_TipW = (int)w;
	this->m_TipH = (int)h;
}

void CItemPost::RenderItemPreview3D(float sx, float sy, float Width, float Height, ITEM* pItem)
{
	if (!pItem || pItem->Type < 0 || pItem->Type >= MAX_ITEM)
	{
		return;
	}

	ITEM_ATTRIBUTE* attr = (ITEM_ATTRIBUTE*)(ItemAttribute + pItem->Type * sizeof(ITEM_ATTRIBUTE));
	if (attr)
	{
		float cw = (attr->Width > 0) ? (float)attr->Width : 1.0f;
		float ch = (attr->Height > 0) ? (float)attr->Height : 1.0f;

		float scale = Width / cw;
		if (Height / ch < scale)
		{
			scale = Height / ch;
		}

		float fitW = cw * scale;
		float fitH = ch * scale;

		sx += (Width - fitW) * 0.5f;
		sy += (Height - fitH) * 0.5f;
		Width = fitW;
		Height = fitH;
	}

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

	ObjectSelect_Angle[0] = 0.0f;
	ObjectSelect_Angle[1] = 0.0f;
	ObjectSelect_Angle[2] = 0.0f;
	ObjectSelect_HeadAngle[0] = 0.0f;
	ObjectSelect_HeadAngle[1] = 0.0f;
	ObjectSelect_HeadAngle[2] = 0.0f;

	RenderItem3D(sx, sy, Width, Height, pItem->Type, pItem->Level, pItem->Option1, false);

	MyUpdateMousePosition();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	BeginBitmap();
}

bool CItemPost::OnItemClickWithModifier(ITEM* pItem)
{
	if (!pItem || pItem->Type < 0 || pItem->Type >= MAX_ITEM)
	{
		return false;
	}

	DWORD dwNow = GetTickCount();
	static DWORD s_dwLastPostTick = 0;
	if (dwNow - s_dwLastPostTick < 800)
	{
		return false;
	}
	s_dwLastPostTick = dwNow;

	char szHex[32] = { 0 };
	ItemToHex(pItem, szHex, sizeof(szHex));

	int itemLevel = GET_ITEM_OPT_LEVEL(pItem->Level);
	bool isExc = (GET_ITEM_OPT_EXC(pItem->Option1) != 0);

	if (pItem->Part == EQUIPMENT_WING || pItem->Type == GET_ITEM(13, 3) ||
		pItem->Type == GET_ITEM(0, 19) || pItem->Type == GET_ITEM(2, 13) ||
		pItem->Type == GET_ITEM(4, 18) || pItem->Type == GET_ITEM(5, 10))
	{
		isExc = false;
	}

	char szBaseName[64] = { 0 };
	gItemManager.GetItemName(pItem->Type, itemLevel, szBaseName);

	char szItemName[64] = { 0 };
	if (isExc)
	{
		sprintf_s(szItemName, sizeof(szItemName), "Excellent %s", szBaseName);
	}
	else
	{
		strcpy_s(szItemName, sizeof(szItemName), szBaseName);
	}

	char szPostMessage[140] = { 0 };
	sprintf_s(szPostMessage, sizeof(szPostMessage), "/post [%s] {%s}", szItemName, szHex);

	MySendChat(szPostMessage);

	PlayBuffer(25, NULL, FALSE);
	return true;
}
