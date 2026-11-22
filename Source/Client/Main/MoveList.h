#pragma once

#include "ProtocolDefines.h"
#include <set>
#include <string>
#include <vector>

//**********************************************//
//************ GameServer -> Client ************//
//**********************************************//

struct PMSG_MOVE_LIST_RECV
{
	PSWMSG_HEAD header; // C2:F3:E5
	BYTE PKLimitFree;
	BYTE count;
};

struct MOVE_LIST_INFO
{
	BYTE MapNumber;
	char MapName[32];
	bool CanMove;
	short MinLevel;
	short MaxLevel;
	short MinReset;
	short MaxReset;
	short AccountLevel;
	DWORD Money;
};

enum MOVELIST_TAB
{
	MOVELIST_TAB_ALL = 0,
	MOVELIST_TAB_FAVORITES = 1,
	MOVELIST_TAB_NORMAL = 2,
	MOVELIST_TAB_HIGH = 3,
};

class CMoveList
{
public:
	CMoveList();
	virtual ~CMoveList();

	bool GetMoveListState();
	void Toggle();
	void Render();
	void UpdateMouse();
	void GCMoveListRecv(PMSG_MOVE_LIST_RECV* lpMsg);

private:
	void LoadFavorites();
	void SaveFavorites();
	bool IsFavorite(const char* mapName) const;
	void ToggleFavorite(const char* mapName);
	void BuildFilteredList(std::vector<int>& outIndices) const;

	bool CheckMove(const MOVE_LIST_INFO& Move);
	bool CheckSpecialRequirements(const MOVE_LIST_INFO& Move);

private:
	bool MoveListSwitch;
	int m_CurrentTab;
	int m_ScrollOffset;
	bool m_FavoritesLoaded;

	std::vector<MOVE_LIST_INFO> m_MoveList;
	std::set<std::string> m_Favorites;

	BYTE PKLimitFree;
};

extern CMoveList gMoveList;
