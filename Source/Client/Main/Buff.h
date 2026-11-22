#pragma once

#include <string>
#include <vector>

struct ActiveBuffInfo
{
	int skillId;
	std::string name;
	bool isDebuff;
	DWORD startTick;
	DWORD durationMs;
};

class CBuff
{
public:

	CBuff();

	virtual ~CBuff();

	void Init();

	void Render();

	void AddActiveBuff(int skillId, const char* name, int durationSec, bool isDebuff = false);

	void RemoveActiveBuff(int skillId);

	void ClearAll();

private:

	static void InsertBuffPhysicalEffect(eEffectState buff, DWORD o);

	static void ClearBuffPhysicalEffect(eEffectState buff, DWORD o);

private:

	std::vector<ActiveBuffInfo> m_ActiveBuffs;
};

extern CBuff gBuff;
