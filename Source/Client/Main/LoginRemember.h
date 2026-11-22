#pragma once

// "Remember me" checkbox + saved-accounts dropdown for the login window.
// Everything is frame-relative (original 250x216 art units); LoginScale.cpp feeds the mouse and calls Render().
// Passwords are stored encrypted with the Windows user's DPAPI key in LoginAccounts.dat (next to main.exe).

#define LOGIN_SAVED_MAX 5

struct LOGIN_SAVED_INFO
{
	char account[11];

	char password[11];
};

class CLoginRemember
{
public:

	CLoginRemember();

	void Init();

	// Returns true when the click was consumed or the mouse is over the open list (hide it from the original code).
	bool Update(float rx, float ry, bool click, int state);

	void Render(float frameX, float frameY, float rx, float ry, int state);

	void OnConnectResult(BYTE result);

	bool OverList(float rx, float ry);

private:

	void Load();

	void Save();

	void Upsert(const char* account, const char* password);

	void Fill(int index);

	bool InArrow(float rx, float ry);

	bool InCheck(float rx, float ry);

	bool InList(float rx, float ry);

	int RowAt(float ry);

	bool m_Remember;

	bool m_Open;

	bool m_Prefilled;

	char m_SnapAccount[11];

	char m_SnapPassword[11];

	int m_Count;

	LOGIN_SAVED_INFO m_List[LOGIN_SAVED_MAX];
};

extern CLoginRemember gLoginRemember;
