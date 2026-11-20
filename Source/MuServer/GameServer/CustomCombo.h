#pragma once

#include "ProtocolDefines.h"
#include "User.h"

//**********************************************//
//************ Custom Combo Packets ************//
//**********************************************//

struct PMSG_CUSTOM_COMBO_TRIGGER_RECV
{
	PSBMSG_HEAD header; // C1:F3:EA
};

struct PMSG_CUSTOM_COMBO_STATE_SEND
{
	PSBMSG_HEAD header; // C1:F3:EA
	BYTE active;
	DWORD activeRemainMs;
	DWORD cooldownRemainMs;
};

struct PMSG_CUSTOM_COMBO_EFFECT_SEND
{
	PSBMSG_HEAD header; // C1:F3:EB
	WORD userIndex;
	WORD targetIndex;
};

struct PMSG_CUSTOM_COMBO_PKMODE_SEND
{
	PSBMSG_HEAD header; // C1:F3:EC
	BYTE pkMode;
};

struct CUSTOM_COMBO_USER_DATA
{
	DWORD activeTick;
	DWORD cooldownTick;
	DWORD lastProcTick;
	int pvpSkillHistory[3];
	DWORD pvpLastSkillTick;
	int pvpSkillStep;
	bool pkMode;
};

//**********************************************//
//************ Custom Combo Class **************//
//**********************************************//

class CCustomCombo
{
public:
	CCustomCombo();
	virtual ~CCustomCombo();

	void Init();
	void Load(char* path);

	void ClearUser(int aIndex);

	void CommandCombo(LPOBJ lpObj);
	void CGComboTriggerRecv(int aIndex);
	void CGComboPKModeRecv(int aIndex, PMSG_CUSTOM_COMBO_PKMODE_SEND* lpMsg);

	bool TriggerCombo(LPOBJ lpObj);
	void SendComboState(LPOBJ lpObj);

	bool CheckCombo(LPOBJ lpObj, LPOBJ lpTarget, CSkill* lpSkill, int baseDamage, int* outDamage, WORD* outEffect = 0);
	void SendComboEffect(LPOBJ lpObj, LPOBJ lpTarget);

	bool MatchComboSkill(int classId, int step, int skillId);
	bool IsPKMode(int aIndex) const { return (aIndex >= 0 && aIndex < MAX_OBJECT) ? this->m_UserData[aIndex].pkMode : false; }

public:
	int m_Enable;
	int m_ActiveDuration;   // in seconds (e.g. 10)
	int m_CooldownDuration; // in seconds (e.g. 50)
	int m_DamageRate;       // % bonus damage fallback
	int m_DamageRatePvE;    // % bonus damage for PvE
	int m_DamageRatePvP;    // % bonus damage for PvP
	int m_ProcDelayMs;      // min ms between combo procs per player (e.g. 350)
	int m_PvPRequire3Skills;// 1 = require 3 skills for PvP combo

	int m_ComboSkills[MAX_CLASS][3]; // Configured 3 skills for each class
	int m_SkillClass[MAX_CLASS];     // PvE designated finisher skill
	int m_EnableClass[MAX_CLASS];    // 1 = enabled for class, 0 = disabled

	CUSTOM_COMBO_USER_DATA m_UserData[MAX_OBJECT];
};

extern CCustomCombo gCustomCombo;
