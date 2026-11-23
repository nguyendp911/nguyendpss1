#include "stdafx.h"
#include "CustomCombo.h"
#include "Map.h"
#include "Notice.h"
#include "Quest.h"
#include "SkillManager.h"
#include "Util.h"

CCustomCombo gCustomCombo;

CCustomCombo::CCustomCombo()
{
	this->Init();
}

CCustomCombo::~CCustomCombo()
{

}

void CCustomCombo::Init()
{
	this->m_Enable = 1;
	this->m_ActiveDuration = 10;
	this->m_CooldownDuration = 50;
	this->m_DamageRate = 200;
	this->m_DamageRatePvE = 200;
	this->m_DamageRatePvP = 50;
	this->m_ProcDelayMs = 0;
	this->m_PvPRequire3Skills = 1;

	// DW (Class 0): Blast -> Hell Fire -> Evil Spirit
	this->m_ComboSkills[0][0] = 13;
	this->m_ComboSkills[0][1] = 10;
	this->m_ComboSkills[0][2] = 9;

	// DK (Class 1): Weapon Skill -> Twisting Slash -> Death Stab
	this->m_ComboSkills[1][0] = 22;
	this->m_ComboSkills[1][1] = 41;
	this->m_ComboSkills[1][2] = 43;

	// ELF (Class 2): Penetration -> Ice Arrow -> Triple Shot
	this->m_ComboSkills[2][0] = 52;
	this->m_ComboSkills[2][1] = 51;
	this->m_ComboSkills[2][2] = 24;

	// MG (Class 3): Twisting Slash -> Fire Slash -> Power Slash
	this->m_ComboSkills[3][0] = 41;
	this->m_ComboSkills[3][1] = 55;
	this->m_ComboSkills[3][2] = 56;

	for (int n = 0; n < MAX_CLASS; n++)
	{
		this->m_SkillClass[n] = this->m_ComboSkills[n][2]; // Default PvE skill is skill 3
		this->m_EnableClass[n] = 1;
	}

	memset(this->m_UserData, 0, sizeof(this->m_UserData));
}

void CCustomCombo::Load(char* path)
{
	this->Init();

	this->m_Enable = GetPrivateProfileInt("CustomCombo", "Enable", 1, path);
	this->m_ActiveDuration = GetPrivateProfileInt("CustomCombo", "ActiveDuration", 10, path);
	this->m_CooldownDuration = GetPrivateProfileInt("CustomCombo", "CooldownDuration", 50, path);
	this->m_DamageRate = GetPrivateProfileInt("CustomCombo", "DamageRate", 200, path);
	this->m_DamageRatePvE = GetPrivateProfileInt("CustomCombo", "DamageRatePvE", this->m_DamageRate, path);
	this->m_DamageRatePvP = GetPrivateProfileInt("CustomCombo", "DamageRatePvP", 50, path);
	this->m_ProcDelayMs = GetPrivateProfileInt("CustomCombo", "ProcDelayMs", 0, path);
	this->m_PvPRequire3Skills = GetPrivateProfileInt("CustomCombo", "PvPRequire3Skills", 1, path);

	// Load 3-skill combo sequence for each class from CustomCombo.txt
	char key[64];
	for (int c = 0; c < MAX_CLASS; c++)
	{
		sprintf_s(key, "Skill1_Class%d", c);
		this->m_ComboSkills[c][0] = GetPrivateProfileInt("ComboSkill", key, this->m_ComboSkills[c][0], path);

		sprintf_s(key, "Skill2_Class%d", c);
		this->m_ComboSkills[c][1] = GetPrivateProfileInt("ComboSkill", key, this->m_ComboSkills[c][1], path);

		sprintf_s(key, "Skill3_Class%d", c);
		this->m_ComboSkills[c][2] = GetPrivateProfileInt("ComboSkill", key, this->m_ComboSkills[c][2], path);

		// Single skill fallback for PvE:
		// Check SkillPvE_Class%d, then SkillClass%d. Default for DK is 41 (Twisting Slash), others use Skill3.
		int defaultPvESkill = (c == CLASS_DK) ? 41 : this->m_ComboSkills[c][2];
		sprintf_s(key, "SkillPvE_Class%d", c);
		this->m_SkillClass[c] = GetPrivateProfileInt("ComboSkill", key, -999, path);
		if (this->m_SkillClass[c] == -999)
		{
			sprintf_s(key, "SkillClass%d", c);
			this->m_SkillClass[c] = GetPrivateProfileInt("ComboSkill", key, defaultPvESkill, path);
		}
	}

	// Load enable per class
	this->m_EnableClass[0] = GetPrivateProfileInt("ComboClass", "EnableClass0", 1, path);
	this->m_EnableClass[1] = GetPrivateProfileInt("ComboClass", "EnableClass1", 1, path);
	this->m_EnableClass[2] = GetPrivateProfileInt("ComboClass", "EnableClass2", 1, path);
	this->m_EnableClass[3] = GetPrivateProfileInt("ComboClass", "EnableClass3", 1, path);

	LogAdd(LOG_BLUE, "[CustomCombo] Loaded: Enable=%d, Active=%ds, Cooldown=%ds, PvE=%d%%, PvP=%d%%, Delay=%dms, PvP3Skills=%d",
		this->m_Enable, this->m_ActiveDuration, this->m_CooldownDuration, this->m_DamageRatePvE, this->m_DamageRatePvP, this->m_ProcDelayMs, this->m_PvPRequire3Skills);
	for (int c = 0; c < MAX_CLASS; c++)
	{
		LogAdd(LOG_BLUE, "[CustomCombo] Class %d Skills: [1]=%d -> [2]=%d -> [3]=%d (PvE=%d)",
			c, this->m_ComboSkills[c][0], this->m_ComboSkills[c][1], this->m_ComboSkills[c][2], this->m_SkillClass[c]);
	}
}

bool CCustomCombo::MatchComboSkill(int classId, int step, int skillId)
{
	if (CHECK_RANGE(classId, MAX_CLASS) == 0 || step < 0 || step >= 3)
	{
		return false;
	}

	int reqSkill = this->m_ComboSkills[classId][step];
	if (reqSkill == -1)
	{
		return true;
	}

	if (skillId == reqSkill)
	{
		return true;
	}

	// DK Weapon Skill compatibility for step 0 (Skill 1):
	// If configured skill is in weapon skill range 18..23, any weapon skill (18..23) is accepted!
	if (classId == CLASS_DK && step == 0)
	{
		if (reqSkill >= 18 && reqSkill <= 23)
		{
			if (skillId >= 18 && skillId <= 23)
			{
				return true;
			}
		}
	}

	// MG Skill compatibility for both Physical (41->55->56) and Magic (13->10->9)
	if (classId == CLASS_MG)
	{
		if (step == 0)
		{
			if ((skillId >= 18 && skillId <= 23) || skillId == 41 || skillId == 13)
			{
				return true;
			}
		}
		else if (step == 1)
		{
			if (skillId == 55 || skillId == 10 || skillId == 14)
			{
				return true;
			}
		}
		else if (step == 2)
		{
			if (skillId == 56 || skillId == 9)
			{
				return true;
			}
		}
	}

	return false;
}

void CCustomCombo::ClearUser(int aIndex)
{
	if (aIndex >= 0 && aIndex < MAX_OBJECT)
	{
		memset(&this->m_UserData[aIndex], 0, sizeof(this->m_UserData[aIndex]));
	}
}

void CCustomCombo::CommandCombo(LPOBJ lpObj)
{
	this->TriggerCombo(lpObj);
}

void CCustomCombo::CGComboTriggerRecv(int aIndex)
{
	if (!gObjIsConnectedGP(aIndex))
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	this->TriggerCombo(lpObj);
}

void CCustomCombo::CGComboPKModeRecv(int aIndex, PMSG_CUSTOM_COMBO_PKMODE_SEND* lpMsg)
{
	if (!gObjIsConnectedGP(aIndex) || lpMsg == NULL)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];
	if (lpObj->Type != OBJECT_USER)
	{
		return;
	}

	this->m_UserData[aIndex].pkMode = (lpMsg->pkMode != 0);
	LogAdd(LOG_BLUE, "[CustomCombo] User %s PK Mode: %d", lpObj->Name, this->m_UserData[aIndex].pkMode);
}

bool CCustomCombo::TriggerCombo(LPOBJ lpObj)
{
	if (!this->m_Enable)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, "[Combo] Tinh nang Combo All Class hien dang tat!");
		return false;
	}

	if (lpObj->Type != OBJECT_USER || lpObj->Index < 0 || lpObj->Index >= MAX_OBJECT)
	{
		return false;
	}

	if (CHECK_RANGE(lpObj->Class, MAX_CLASS) == 0 || this->m_EnableClass[lpObj->Class] == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, "[Combo] Class cua ban khong duoc phep su dung Combo!");
		return false;
	}

	DWORD dwCurrentTick = GetTickCount();
	CUSTOM_COMBO_USER_DATA* lpData = &this->m_UserData[lpObj->Index];

	if (dwCurrentTick < lpData->activeTick)
	{
		DWORD remainSec = (lpData->activeTick - dwCurrentTick) / 1000 + 1;
		gNotice.GCNoticeSend(lpObj->Index, 1, "[Combo] Combo dang duoc kich hoat! Con %d giay.", remainSec);
		return false;
	}

	if (dwCurrentTick < lpData->cooldownTick)
	{
		DWORD remainSec = (lpData->cooldownTick - dwCurrentTick) / 1000 + 1;
		gNotice.GCNoticeSend(lpObj->Index, 1, "[Combo] Dang trong thoi gian hoi chieu! Con %d giay.", remainSec);
		return false;
	}

	lpData->activeTick = dwCurrentTick + (this->m_ActiveDuration * 1000);
	lpData->cooldownTick = lpData->activeTick + (this->m_CooldownDuration * 1000);
	lpData->lastProcTick = 0;


	this->SendComboState(lpObj);

	gNotice.GCNoticeSend(lpObj->Index, 1, "[Combo] Kich hoat Combo All Class thanh cong! Duy tri %d giay.", this->m_ActiveDuration);

	return true;
}

void CCustomCombo::SendComboState(LPOBJ lpObj)
{
	if (lpObj == NULL || lpObj->Type != OBJECT_USER || lpObj->Index < 0 || lpObj->Index >= MAX_OBJECT)
	{
		return;
	}

	PMSG_CUSTOM_COMBO_STATE_SEND pMsg;
	pMsg.header.set(0xF3, 0xEA, sizeof(pMsg));

	DWORD dwCurrentTick = GetTickCount();
	CUSTOM_COMBO_USER_DATA* lpData = &this->m_UserData[lpObj->Index];

	if (dwCurrentTick < lpData->activeTick)
	{
		pMsg.active = 1;
		pMsg.activeRemainMs = lpData->activeTick - dwCurrentTick;
		pMsg.cooldownRemainMs = (lpData->cooldownTick > dwCurrentTick) ? (lpData->cooldownTick - dwCurrentTick) : 0;
	}
	else
	{
		pMsg.active = 0;
		pMsg.activeRemainMs = 0;
		pMsg.cooldownRemainMs = (dwCurrentTick < lpData->cooldownTick) ? (lpData->cooldownTick - dwCurrentTick) : 0;
	}

	DataSend(lpObj->Index, (BYTE*)&pMsg, pMsg.header.size);
}

bool CCustomCombo::CheckCombo(LPOBJ lpObj, LPOBJ lpTarget, CSkill* lpSkill, int baseDamage, int* outDamage, WORD* outEffect)
{
	if (!this->m_Enable)
	{
		return false;
	}

	if (lpObj == NULL || lpTarget == NULL || lpObj->Type != OBJECT_USER || lpObj->Index < 0 || lpObj->Index >= MAX_OBJECT)
	{
		return false;
	}

	if (CHECK_RANGE(lpObj->Class, MAX_CLASS) == 0 || this->m_EnableClass[lpObj->Class] == 0)
	{
		return false;
	}

	DWORD dwCurrentTick = GetTickCount();
	CUSTOM_COMBO_USER_DATA* lpData = &this->m_UserData[lpObj->Index];

	if (dwCurrentTick >= lpData->activeTick)
	{
		return false;
	}

	// Strictly require 3-skill combo sequence whenever:
	// 1. Target is another Player (PvP / Free PK)
	// 2. Both players are in an active Duel with each other
	// 3. Attacker has toggled PK Mode ON on client (lpData->pkMode != 0)
	bool isTargetPlayer = (lpTarget->Type == OBJECT_USER);
	bool isDuel = (lpObj->DuelUser != -1 && lpObj->DuelUser == lpTarget->Index && lpTarget->DuelUser == lpObj->Index);
	bool isCastle = (CC_MAP_RANGE(lpObj->Map) != 0);
	bool isPKModeOn = (lpData->pkMode != 0);

	bool isPvP = isTargetPlayer || isDuel || isCastle || isPKModeOn;

	if (isDuel || (isPvP && this->m_PvPRequire3Skills))
	{
		// In PvP / Duel / PK Mode: MUST execute configured 3 skills in exact sequence!
		if (lpSkill == NULL)
		{
			lpData->pvpSkillStep = 0;
			return false;
		}

		int curSkill = (lpSkill->m_index != 0) ? lpSkill->m_index : lpSkill->m_skill;

		// If time between skills exceeds 3500ms, reset sequence
		if (dwCurrentTick - lpData->pvpLastSkillTick > 3500)
		{
			lpData->pvpSkillStep = 0;
		}

		if (lpData->pvpSkillStep == 0)
		{
			// Check if current skill matches Skill 1
			if (this->MatchComboSkill(lpObj->Class, 0, curSkill))
			{
				lpData->pvpSkillHistory[0] = curSkill;
				lpData->pvpSkillStep = 1;
				lpData->pvpLastSkillTick = dwCurrentTick;
			}
			return false;
		}
		else if (lpData->pvpSkillStep == 1)
		{
			// Check if current skill matches Skill 2
			if (this->MatchComboSkill(lpObj->Class, 1, curSkill))
			{
				lpData->pvpSkillHistory[1] = curSkill;
				lpData->pvpSkillStep = 2;
				lpData->pvpLastSkillTick = dwCurrentTick;
			}
			else if (this->MatchComboSkill(lpObj->Class, 0, curSkill))
			{
				// Repeated Skill 1: refresh tick and stay at step 1
				lpData->pvpLastSkillTick = dwCurrentTick;
			}
			else
			{
				// Wrong skill: reset sequence
				lpData->pvpSkillStep = 0;
			}
			return false;
		}
		else if (lpData->pvpSkillStep == 2)
		{
			// Check if current skill matches Skill 3 (Finisher)
			if (this->MatchComboSkill(lpObj->Class, 2, curSkill))
			{
				// SUCCESS: All 3 configured skills executed in exact sequence!
				lpData->pvpSkillStep = 0; // Reset for next combo sequence
				lpData->pvpLastSkillTick = dwCurrentTick;
			}
			else if (this->MatchComboSkill(lpObj->Class, 1, curSkill))
			{
				// Repeated Skill 2: refresh tick and stay at step 2
				lpData->pvpLastSkillTick = dwCurrentTick;
				return false;
			}
			else if (this->MatchComboSkill(lpObj->Class, 0, curSkill))
			{
				// Restarted with Skill 1
				lpData->pvpSkillStep = 1;
				lpData->pvpLastSkillTick = dwCurrentTick;
				return false;
			}
			else
			{
				lpData->pvpSkillStep = 0;
				return false;
			}
		}
	}
	else
	{
		// PvE (Monsters / Bosses): Single-skill combo (when PK Mode is OFF and not in Duel)
		if (lpSkill == NULL)
		{
			return false;
		}

		int curSkill = (lpSkill->m_index != 0) ? lpSkill->m_index : lpSkill->m_skill;
		int reqSkill = this->m_SkillClass[lpObj->Class];

		// If reqSkill == -1: Any skill can trigger combo in PvE!
		// Otherwise: ONLY the skill configured in CustomCombo.txt (SkillPvE_Class) can trigger combo!
		if (reqSkill != -1)
		{
			// If configured with weapon skill (18..23) for DK, accept any weapon skill (18..23)
			if (lpObj->Class == CLASS_DK && reqSkill >= 18 && reqSkill <= 23 && curSkill >= 18 && curSkill <= 23)
			{
				// Allowed weapon skill
			}
			else if (curSkill != reqSkill)
			{
				return false;
			}
		}
	}

	if (outDamage != NULL)
	{
		int damageRate = isPvP ? this->m_DamageRatePvP : this->m_DamageRatePvE;
		int rateBonus = (baseDamage * damageRate) / 100;

		int statBonus = (lpObj->Strength * 2) + (lpObj->Dexterity * 2) + (lpObj->Energy * 3);
		if (isPvP)
		{
			statBonus = statBonus / 2; // Balanced for PvP
		}
		else if (statBonus < 3000)
		{
			statBonus = 3000;
		}

		*outDamage = baseDamage + rateBonus + statBonus;
	}

	if (outEffect != NULL)
	{
		// Authentic MU Online Combo damage flag (0x80)
		*outEffect = 0x80;
	}

	return true;
}

void CCustomCombo::SendComboEffect(LPOBJ lpObj, LPOBJ lpTarget)
{
	if (lpObj == NULL || lpTarget == NULL || lpObj->Index < 0 || lpObj->Index >= MAX_OBJECT)
	{
		return;
	}

	DWORD dwCurrentTick = GetTickCount();
	CUSTOM_COMBO_USER_DATA* lpData = &this->m_UserData[lpObj->Index];

	// Throttle visual effect broadcast to avoid packet flooding on AoE skills
	if (dwCurrentTick < lpData->lastProcTick + 250)
	{
		return;
	}

	lpData->lastProcTick = dwCurrentTick;

	// Broadcast custom combo effect packet (C1:F3:EB) so all clients in viewport display the authentic DK combo shockwave
	PMSG_CUSTOM_COMBO_EFFECT_SEND pMsg;
	pMsg.header.set(0xF3, 0xEB, sizeof(pMsg));
	pMsg.userIndex = lpObj->Index;
	pMsg.targetIndex = lpTarget->Index;

	DataSend(lpObj->Index, (BYTE*)&pMsg, pMsg.header.size);
	MsgSendV2(lpObj, (BYTE*)&pMsg, pMsg.header.size);
}
