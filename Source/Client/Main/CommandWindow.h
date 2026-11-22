#pragma once

#include "ProtocolDefines.h"

struct PMSG_PARTY_REQUEST_SEND
{
	PBMSG_HEAD header; // C3:40
	BYTE index[2];
};

struct PMSG_TRADE_REQUEST_SEND
{
	PBMSG_HEAD header; // C3:36
	BYTE index[2];
};

struct PMSG_GUILD_REQUEST_SEND
{
	PBMSG_HEAD header; // C1:50
	BYTE index[2];
};

enum eCommandMode
{
	CMD_MODE_NONE = 0,
	CMD_MODE_PARTY = 1,
	CMD_MODE_TRADE = 2,
	CMD_MODE_GUILD = 3,
};

class CCommandWindow
{
public:
	CCommandWindow();
	~CCommandWindow();

	void Toggle();
	void Show(bool show);
	bool IsOpen() const { return this->m_IsOpen; }
	bool IsActiveMode() const { return (this->m_ActiveMode != CMD_MODE_NONE); }

	void Render();
	void UpdateMouse();

private:
	void RenderTargetCursor();
	void ExecuteParty(WORD targetKey, const char* targetName);
	void ExecuteTrade(WORD targetKey, const char* targetName);
	void ExecuteGuild(WORD targetKey, const char* targetName);

	bool m_IsOpen;
	float m_PosX;
	float m_PosY;
	float m_Width;
	float m_Height;
	int m_ActiveMode;

	bool m_IsDragging;
	float m_DragOffsetX;
	float m_DragOffsetY;
};

extern CCommandWindow gCommandWindow;
