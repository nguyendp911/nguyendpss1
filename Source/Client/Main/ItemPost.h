#pragma once

#include "Defines.h"
#include "Structs.h"
#include <string>
#include <vector>

struct POST_ITEM_HOVER_LINE
{
	int x;
	int y;
	int width;
	int height;
	ITEM item;
	DWORD dwTick;
};

struct POST_CHAT_LINE
{
	std::string strID;
	std::string rawText;
	std::string cleanText;
	std::string linkName;
	std::string strHex;
	ITEM item;
	DWORD dwTick;
};

class CItemPost
{
public:
	CItemPost();
	virtual ~CItemPost();

	void Init();

	static void ItemToHex(ITEM* pItem, char* pszHex, int nMaxHexLen);
	static bool HexToItem(const char* pszHex, ITEM* pOutItem);
	static bool ExtractItemPostHex(const std::string& strText, std::string& strHexOut);
	static bool ExtractItemLinkBounds(const std::string& rawText, size_t& outLinkStart, size_t& outLinkLen);
	static std::string CleanItemPostText(const std::string& strText);

	bool OnChatTextAdded(const char* strID, const char* strText, int MsgType, std::string& outCleanText);
	void AddPostLineDirect(const POST_CHAT_LINE& line);
	bool FindPostByText(const char* pszText, POST_CHAT_LINE& outLine);

	void RegisterRenderedPostItem(int x, int y, int width, int height, const std::string& strHex);
	void UpdateMouse();
	void Render();

	bool OnItemClickWithModifier(ITEM* pItem);
	bool IsHoveringItem() const { return this->m_bShowItemToolTip; }
	void RenderItemPreview3D(float sx, float sy, float Width, float Height, ITEM* pItem);
	void RenderColdSteelTooltip(int count);

private:
	bool m_bShowItemToolTip;
	int m_TipX, m_TipY, m_TipW, m_TipH;
	ITEM m_HoverItem;
	std::vector<POST_ITEM_HOVER_LINE> m_HoverLines;
	std::vector<POST_CHAT_LINE> m_PostChatLines;
};

extern CItemPost gItemPost;
