#pragma once

#include <string>
#include <vector>
#include <map>

#define MAX_AUTO_SKILLS 3
#define MAX_AUTO_BUFFS 3

struct AutoHelperConfig
{
	int HuntingRange;
	int AttackSkills[MAX_AUTO_SKILLS];
	int AttackMode; // 0 = Sequential, 1 = Random
	bool AttackMobs;
	bool SelfDefense;
	bool AttackPK;

	bool AutoPotionHP;
	int PotionHPThreshold;
	bool AutoPotionSD;
	int PotionSDThreshold;
	bool AutoPotionMP;
	int PotionMPThreshold;
	bool AutoAntidote;

	int BuffSkills[MAX_AUTO_BUFFS];
	bool BuffExpired;
	int BuffInterval;

	bool PickZen;
	bool PickJewel;
	bool PickExc;
	bool PickAncient;
	bool PickExtra;
	std::vector<std::string> ExtraItems;

	bool ReturnPos;
	int ReturnPosDelay;
	bool ReturnHunt;
	int ReturnHuntMin;
	int ReturnHuntMax;
	bool RepairEquip;

	bool PartySupport;
	bool PartyHeal;
	int PartyHealThreshold;
	int PartyBuffInterval;
	bool PartyFollowLeader;

	// Smart behaviours (window "Nâng cao")
	bool PreferBoss;      // the world boss and the elite monsters first
	bool SkipStuck;       // leave alone the monsters that cannot be hit or reached
	bool SmartPotion;     // size of the potion by urgency
	bool StopNoPotion;    // stop when the healing potions are over
	bool StopInvFull;     // stop when the inventory is full
	bool StopIdle;        // stop when there is no monster for IdleMinutes
	int IdleMinutes;
	bool ShowStats;       // statistics of the hunt on the screen
	bool AutoCombo;       // the combo is started by the Auto each time it is ready
	bool FollowTeleport;  // follow the leader to another map (teleport, costs zen)
	bool AlertSound;      // a sound and a notice when the Auto stops for a problem
	bool TownEscape;      // life under 15% with no potion left: go back to Lorencia and stop
};

struct AutoHuntStats
{
	DWORD startTick;
	DWORD kills;
	unsigned long long exp;
	unsigned long long zen;
	DWORD jewels;
	DWORD items;
};

class CAutoHelper
{
public:
	CAutoHelper();
	virtual ~CAutoHelper();

	void Init();
	void Toggle();
	void Start();
	void Stop();

	// Profiles of the configuration of the character (1 ~ 3): the settings of a hunt that can be put back with one click
	void SaveProfile(int n);
	void LoadProfile(int n);
	void Alert(const char* text);
	bool IsActive() const { return this->m_Active; }

	void Work();

	// Manual play: with a skill selected, the right button held on a monster that is out of the range of the skill makes
	// the hero run toward it (the skill is cast by the engine as soon as the monster is in range)
	void ManualSkillChase();
	void Reset(bool bClearConfig = true);
	void CheckCharacterChange();
	void InitDefaultConfig();
	void ValidateConfigForClass();
	void LoadConfig(const char* characterName);
	void SaveConfig(const char* characterName);
	void GetCharName(char* outName, int maxLen);

	void OnEntityDie(WORD index);
	bool IsDeadEntity(WORD index);

	AutoHelperConfig& GetConfig() { return this->m_Config; }

	// Statistics of the hunt and events of the game
	void RenderStats();
	void OnExperience(WORD mobKey, DWORD exp);
	void OnZenPicked(int zen);
	void OnItemPicked(short type);
	void SetWorldBoss(WORD key);

	void AddExtraItem(const std::string& name);
	void RemoveExtraItem(int index);

	// Class skill helpers
	BYTE GetPlayerClass();
	std::vector<int> GetAvailableSkillsForClass(BYTE classCode);
	std::vector<int> GetAvailableBuffSkillsForClass(BYTE classCode);
	std::vector<int> GetAvailableAttackSkillsForClass(BYTE classCode);
	std::vector<int> GetPlayerLearnedSkills();
	std::vector<int> GetLearnedBuffSkills();
	std::vector<int> GetLearnedAttackSkills();
	const char* GetSkillNameById(int skillId);

private:
	void AutoPotion();
	void AutoCombat();
	void AutoBuff();
	void AutoComboTrigger();
	bool AutoLoot();
	bool AutoReturnPos();
	void AutoRepair();
	void AutoParty();
	bool AutoFollow();
	bool IsFollowingLeader();
	void CheckStopConditions();
	void CastBuffOnSelf(int skill, short heroKey);

	int FindPotionSlot(const int* order, int count);
	int CountFreeInventoryCells();
	bool IsBlacklisted(WORD key);

	int FindHealingPotionSlot();
	int FindManaPotionSlot();
	int FindAntidoteSlot();
	bool ShouldPickItem(short type, BYTE option1, BYTE specialNum, const char* name);
	bool IsJewel(short type);
	bool IsMoney(short type);

	void CastSkill(int skillId, int targetIndex, int targetX, int targetY, const std::vector<int>& targets);
	void UseItem(BYTE slot);
	void PickItem(int itemIndex);
	void MoveTo(int x, int y);

private:
	bool m_Active;
	AutoHelperConfig m_Config;

	int m_OriginX;
	int m_OriginY;
	int m_OriginMap;
	bool m_OriginSafeZone;

	DWORD m_LastPotionTick;
	DWORD m_LastAttackTick;
	DWORD m_LastBuffTick;
	DWORD m_LastComboTick;
	DWORD m_LastPartyBuffTick;
	DWORD m_LastLootTick;
	DWORD m_LastRepairTick;
	DWORD m_LastReturnPosTick;
	DWORD m_LastFollowTick;
	DWORD m_LootBusyTimer;

	int m_AttackSkillIndex;
	int m_BuffSkillIndex;
	int m_CurrentTarget;

	std::string m_LoadedCharacter;
	std::map<WORD, DWORD> m_DeadEntities;

	// Smart behaviours
	std::map<WORD, DWORD> m_Blacklist;      // monster key -> tick until it is ignored
	DWORD m_TargetProgressTick;             // last time the target lost life or the hero moved towards it
	int m_TargetLastHp;
	float m_TrackX;
	float m_TrackY;
	DWORD m_LastTargetSeenTick;
	DWORD m_NoPotionTick;
	DWORD m_LastAntidoteTick;
	DWORD m_LastInvCheckTick;
	DWORD m_LastFollowMoveTick;
	std::map<int, DWORD> m_BuffCastTick;    // buff skill -> last cast

	// Items that could not be picked (reserved for another player): after a few tries they are left alone for a while
	struct LootTry
	{
		int count;
		DWORD firstTick;
		DWORD blockedUntil;
		int posX;   // where the item was when it was tried: the slot is reused by other items later
		int posY;
		DWORD moveTick; // when the hero started to walk to it
	};
	std::map<int, LootTry> m_LootTries;

	// The numbers of the statistics panel are refreshed every few seconds, not every frame
	DWORD m_StatSnapTick;
	double m_StatPerHour;
	int m_StatEtaMinutes;
	WORD m_WorldBossKey;
	DWORD m_WorldBossTick;
	AutoHuntStats m_Stats;
};

extern CAutoHelper gAutoHelper;
