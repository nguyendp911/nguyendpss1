#include "stdafx.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <map>
#include "AutoHelper.h"
#include "PartyMatching.h"
#include "Buff.h"
#include "Defines.h"
#include "HealthBar.h"
#include "Offsets.h"
#include "Patchs.h"
#include "PrintPlayer.h"
#include "Protocol.h"
#include "Sound.h"
#include "CustomComboUI.h"
#include "CustomMonster.h"
#include "Item.h"
#include "Util.h"

CAutoHelper gAutoHelper;

CAutoHelper::CAutoHelper()
{
	this->m_Active = false;
	this->m_OriginX = 0;
	this->m_OriginY = 0;
	this->m_OriginMap = 0;

	this->m_LastPotionTick = 0;
	this->m_LastAttackTick = 0;
	this->m_LastBuffTick = 0;
	this->m_LastComboTick = 0;
	this->m_LastPartyBuffTick = 0;
	this->m_LastLootTick = 0;
	this->m_LastRepairTick = 0;
	this->m_LastReturnPosTick = 0;
	this->m_LastFollowTick = 0;

	this->m_AttackSkillIndex = 0;
	this->m_BuffSkillIndex = 0;
	this->m_CurrentTarget = -1;

	this->m_TargetProgressTick = 0;
	this->m_TargetLastHp = -2;
	this->m_TrackX = 0.0f;
	this->m_TrackY = 0.0f;
	this->m_LastTargetSeenTick = 0;
	this->m_NoPotionTick = 0;
	this->m_LastAntidoteTick = 0;
	this->m_LastInvCheckTick = 0;
	this->m_LastFollowMoveTick = 0;
	this->m_WorldBossKey = 0;
	this->m_WorldBossTick = 0;
	this->m_LootBusyTimer = 0;
	this->m_StatSnapTick = 0;
	this->m_StatPerHour = 0.0;
	this->m_StatEtaMinutes = -1;
	memset(&this->m_Stats, 0, sizeof(this->m_Stats));

	this->InitDefaultConfig();
}

CAutoHelper::~CAutoHelper()
{
}

void CAutoHelper::Init()
{
}

void CAutoHelper::InitDefaultConfig()
{
	this->m_Config.HuntingRange = 4;
	this->m_Config.AttackSkills[0] = -1;
	this->m_Config.AttackSkills[1] = -1;
	this->m_Config.AttackSkills[2] = -1;
	this->m_Config.AttackMode = 0; // 0 = Sequential, 1 = Random
	this->m_Config.AttackMobs = true;
	this->m_Config.SelfDefense = true;
	this->m_Config.AttackPK = false;

	this->m_Config.AutoPotionHP = true;
	this->m_Config.PotionHPThreshold = 50;
	this->m_Config.AutoPotionSD = false;
	this->m_Config.PotionSDThreshold = 50;
	this->m_Config.AutoPotionMP = true;
	this->m_Config.PotionMPThreshold = 20;
	this->m_Config.AutoAntidote = true;

	this->m_Config.BuffSkills[0] = -1;
	this->m_Config.BuffSkills[1] = -1;
	this->m_Config.BuffSkills[2] = -1;
	this->m_Config.BuffExpired = true;
	this->m_Config.BuffInterval = 0;

	this->m_Config.PickZen = true;
	this->m_Config.PickJewel = true;
	this->m_Config.PickExc = true;
	this->m_Config.PickAncient = true;
	this->m_Config.PickExtra = true;
	this->m_Config.ExtraItems.clear();

	this->m_Config.ReturnPos = true;
	this->m_Config.ReturnPosDelay = 0;
	this->m_Config.ReturnHunt = true;
	this->m_Config.ReturnHuntMin = 0;
	this->m_Config.ReturnHuntMax = 0;
	this->m_Config.RepairEquip = true;

	this->m_Config.PartySupport = false;
	this->m_Config.PartyHeal = true;
	this->m_Config.PartyHealThreshold = 50;
	this->m_Config.PartyBuffInterval = 0;
	this->m_Config.PartyFollowLeader = false;

	this->m_Config.PreferBoss = true;
	this->m_Config.SkipStuck = true;
	this->m_Config.SmartPotion = true;
	this->m_Config.StopNoPotion = true;
	this->m_Config.StopInvFull = false;
	this->m_Config.StopIdle = true;
	this->m_Config.IdleMinutes = 5;
	this->m_Config.ShowStats = false;
	this->m_Config.AutoCombo = true;
	this->m_Config.FollowTeleport = false;
	this->m_Config.AlertSound = true;
	this->m_Config.TownEscape = false;
}

void CAutoHelper::ValidateConfigForClass()
{
	BYTE baseClass = this->GetPlayerClass();

	std::vector<int> learned = this->GetPlayerLearnedSkills();
	std::vector<int> validAttackSkills = !learned.empty() ? this->GetLearnedAttackSkills() : this->GetAvailableAttackSkillsForClass(baseClass);
	std::vector<int> validBuffSkills = !learned.empty() ? this->GetLearnedBuffSkills() : this->GetAvailableBuffSkillsForClass(baseClass);

	// Validate Attack Skills
	for (int i = 0; i < MAX_AUTO_SKILLS; i++)
	{
		int sk = this->m_Config.AttackSkills[i];
		if (sk > 0)
		{
			bool valid = false;
			for (size_t v = 0; v < validAttackSkills.size(); v++)
			{
				if (validAttackSkills[v] == sk)
				{
					valid = true;
					break;
				}
			}
			if (!valid)
			{
				this->m_Config.AttackSkills[i] = (i == 0) ? 0 : -1;
			}
		}
	}

	// Validate Buff Skills
	for (int i = 0; i < MAX_AUTO_BUFFS; i++)
	{
		int sk = this->m_Config.BuffSkills[i];
		if (sk > 0)
		{
			bool valid = false;
			for (size_t v = 0; v < validBuffSkills.size(); v++)
			{
				if (validBuffSkills[v] == sk)
				{
					valid = true;
					break;
				}
			}
			if (!valid)
			{
				this->m_Config.BuffSkills[i] = -1;
			}
		}
	}

	// Default attack slot 0: if empty, default to basic attack (0)
	if (this->m_Config.AttackSkills[0] == -1)
	{
		this->m_Config.AttackSkills[0] = 0;
	}
}

void CAutoHelper::Reset(bool bClearConfig)
{
	this->m_Active = false;
	this->m_CurrentTarget = -1;
	this->m_AttackSkillIndex = 0;
	this->m_BuffSkillIndex = 0;
	this->m_LastPotionTick = 0;
	this->m_LastAttackTick = 0;
	this->m_LastBuffTick = 0;
	this->m_LastComboTick = 0;
	this->m_LastPartyBuffTick = 0;
	this->m_LastLootTick = 0;
	this->m_LastRepairTick = 0;
	this->m_LastReturnPosTick = 0;
	this->m_LastFollowTick = 0;
	this->m_LootBusyTimer = 0;
	this->m_DeadEntities.clear();
	this->m_Blacklist.clear();
	this->m_BuffCastTick.clear();
	this->m_NoPotionTick = 0;

	if (Hero)
	{
		*(BYTE*)(Hero + 748) = 0; // c->Movement = false
		*(BYTE*)(Hero + 749) = 0; // c->MovementType = 0
		*(BYTE*)(Hero + 0x2F5) = 0; // AttackTime = 0
		*(WORD*)(Hero + 0x310) = 0xFFFF; // TargetCharacter = none
		CurrentSkill = 0;
	}

	if (bClearConfig)
	{
		this->m_LoadedCharacter.clear();
		this->InitDefaultConfig();
	}
}

void CAutoHelper::OnEntityDie(WORD index)
{
	this->m_DeadEntities[index] = GetTickCount();

	if (this->m_CurrentTarget == (int)index)
	{
		this->m_CurrentTarget = -1;
	}
}

bool CAutoHelper::IsDeadEntity(WORD index)
{
	std::map<WORD, DWORD>::iterator it = this->m_DeadEntities.find(index);
	if (it == this->m_DeadEntities.end())
	{
		return false;
	}

	if (GetTickCount() - it->second > 8000)
	{
		this->m_DeadEntities.erase(it);
		return false;
	}

	return true;
}

void CAutoHelper::CheckCharacterChange()
{
	if (SceneFlag != 5 || !Hero || !CharacterAttribute)
	{
		if (this->m_Active || !this->m_LoadedCharacter.empty())
		{
			this->Reset(true);
		}
		return;
	}

	char charName[32] = { 0 };
	this->GetCharName(charName, sizeof(charName));
	if (strlen(charName) == 0) return;

	if (this->m_LoadedCharacter != charName)
	{
		// Character changed! Stop auto, clear leftover actions, reload config
		this->Reset(false);
		this->m_LoadedCharacter = charName;
		this->InitDefaultConfig(); // a character without saved settings starts from the defaults, not from the previous one
		this->LoadConfig(charName);
		this->ValidateConfigForClass();
	}
}

void CAutoHelper::Toggle()
{
	if (this->m_Active)
	{
		this->Stop();
	}
	else
	{
		this->Start();
	}
}

void CAutoHelper::Start()
{
	if (!Hero || SceneFlag != 5)
	{
		return;
	}

	this->CheckCharacterChange();

	this->m_Active = true;
	this->m_OriginX = (int)(*(float*)(Hero + 16) / 100.0f);
	this->m_OriginY = (int)(*(float*)(Hero + 20) / 100.0f);
	this->m_OriginMap = World;
	this->m_OriginSafeZone = (*(BYTE*)(Hero + 0x34E) != 0);

	this->m_LastPotionTick = 0;
	this->m_LastAttackTick = 0;
	this->m_LastBuffTick = 0;
	this->m_LastComboTick = 0;
	this->m_LastPartyBuffTick = 0;
	this->m_LastLootTick = 0;
	this->m_LastRepairTick = 0;
	this->m_LastReturnPosTick = GetTickCount();
	this->m_LastFollowTick = GetTickCount();
	this->m_LootBusyTimer = 0;

	this->m_AttackSkillIndex = 0;
	this->m_BuffSkillIndex = 0;
	this->m_CurrentTarget = -1;
	this->m_DeadEntities.clear();
	this->m_Blacklist.clear();
	this->m_BuffCastTick.clear();
	this->m_LootTries.clear();
	this->m_StatSnapTick = 0;
	this->m_StatPerHour = 0.0;
	this->m_StatEtaMinutes = -1;
	this->m_TargetProgressTick = 0;
	this->m_TargetLastHp = -2;
	this->m_NoPotionTick = 0;
	this->m_LastAntidoteTick = 0;
	this->m_LastInvCheckTick = GetTickCount();
	this->m_LastFollowMoveTick = 0;
	this->m_LastTargetSeenTick = GetTickCount();

	memset(&this->m_Stats, 0, sizeof(this->m_Stats));
	this->m_Stats.startTick = GetTickCount();

	if (gCustomComboUI.IsPKModeActive())
	{
		gCustomComboUI.SetPKMode(false);
		CreateNotice("[PK Mode] Automatically disabled when starting SmartAuto.", 1);
	}

	PlayBuffer(25, 0, 0);
	CreateNotice("[SmartAuto] Started!", 1);
}

void CAutoHelper::Stop()
{
	this->m_Active = false;
	this->m_CurrentTarget = -1;

	if (Hero)
	{
		*(BYTE*)(Hero + 748) = 0;
		*(BYTE*)(Hero + 749) = 0;
		*(BYTE*)(Hero + 0x2F5) = 0;
		*(WORD*)(Hero + 0x310) = 0xFFFF;
		CurrentSkill = 0;

		BYTE curAction = *(BYTE*)(Hero + 0x105);
		if (curAction >= 0x2E)
		{
			SetPlayerStop(Hero);
		}
	}

	PlayBuffer(25, 0, 0);
	CreateNotice("[SmartAuto] Stopped!", 0);

	if (this->m_Stats.kills > 0)
	{
		char summary[160];
		sprintf_s(summary, "[SmartAuto] Tong ket: %u quai, %llu EXP, %llu Zen, %u ngoc", this->m_Stats.kills, this->m_Stats.exp, this->m_Stats.zen, this->m_Stats.jewels);
		CreateNotice(summary, 1);
	}
}

void CAutoHelper::Work()
{
	this->CheckCharacterChange();

	if (!this->m_Active)
	{
		return;
	}

	if (!Hero || SceneFlag != 5) // In Game Scene
	{
		this->Stop();
		return;
	}

	// 0. Check death
	if (gPrintPlayer.ViewCurHP == 0)
	{
		this->Stop();
		CreateNotice("[SmartAuto] Character died - Auto stopped!", 1);
		return;
	}

	// Check return to town (SafeZone) — do not stop if actively following Party Leader
	if (!this->m_OriginSafeZone && *(BYTE*)(Hero + 0x34E) != 0 && !this->IsFollowingLeader())
	{
		this->Stop();
		CreateNotice("[SmartAuto] Returned to safe zone - Auto stopped!", 1);
		return;
	}

	// If player was in town when starting, but has stepped out to the field, mark as hunting
	if (this->m_OriginSafeZone && *(BYTE*)(Hero + 0x34E) == 0)
	{
		this->m_OriginSafeZone = false;
	}

	// Check map change (warped to another map)
	if (World != this->m_OriginMap)
	{
		if (this->IsFollowingLeader())
		{
			// Following the leader: the hunt goes on in the new map
			this->m_OriginMap = World;
			this->m_OriginX = (int)(*(float*)(Hero + 16) / 100.0f);
			this->m_OriginY = (int)(*(float*)(Hero + 20) / 100.0f);
			this->m_OriginSafeZone = (*(BYTE*)(Hero + 0x34E) != 0);
			this->m_DeadEntities.clear();
			this->m_Blacklist.clear();
			this->m_CurrentTarget = -1;
			this->m_LastTargetSeenTick = GetTickCount();
		}
		else
		{
			this->Stop();
			CreateNotice("[SmartAuto] Map changed - Auto stopped!", 1);
			return;
		}
	}

	// Inventory full / nothing to hunt for a long time
	this->CheckStopConditions();

	if (!this->m_Active)
	{
		return;
	}

	// 1. Auto Potion (Highest priority)
	this->AutoPotion();

	if (!this->m_Active)
	{
		return;
	}

	// 2. Follow the leader of the party (catching up has priority over everything else)
	bool isFollowing = this->AutoFollow();

	// 3. Auto Loot (prioritized over combat when items need picking)
	bool isLooting = false;
	if (!isFollowing)
	{
		isLooting = this->AutoLoot();
	}

	// 4. Auto Return Position (only when not looting and no mob nearby)
	bool isReturning = false;
	if (!isFollowing && !isLooting && this->m_Config.ReturnPos)
	{
		isReturning = this->AutoReturnPos();
	}

	// 5. Auto Party Support (Buff & Heal party members)
	if (this->m_Config.PartySupport || this->m_Config.PartyHeal)
	{
		this->AutoParty();
	}

	// 6. Auto Buff
	this->AutoBuff();

	// 6b. Combo: started each time it is ready while there is something to fight
	this->AutoComboTrigger();

	// 7. Auto Combat (only attack if not moving towards an item, the leader or the origin)
	if (!isFollowing && !isLooting && !isReturning)
	{
		this->AutoCombat();
	}

	// 8. Auto Repair
	if (this->m_Config.RepairEquip)
	{
		this->AutoRepair();
	}
}

// The combo of the character (a window of time with cooldown, see CustomComboUI) is started by the Auto as soon as it is ready and a
// monster is being fought: no notice, no sound, one try per 1.5 seconds at most
void CAutoHelper::AutoComboTrigger()
{
	if (!this->m_Config.AutoCombo || !this->m_Config.AttackMobs || this->m_CurrentTarget == -1)
	{
		return;
	}

	DWORD now = GetTickCount();

	if ((now - this->m_LastComboTick) < 1500)
	{
		return;
	}

	if (gCustomComboUI.IsActive() || gCustomComboUI.GetCooldownRemainSec() > 0)
	{
		return;
	}

	this->m_LastComboTick = now;

	gCustomComboUI.SendComboTrigger();
}

bool CAutoHelper::IsFollowingLeader()
{
	if (!this->m_Config.PartyFollowLeader || PartyNumber <= 1 || !Hero)
	{
		return false;
	}

	if (Party[0].Name[0] == '\0' || _strnicmp(Party[0].Name, (char*)(Hero + 0x1C1), 10) == 0)
	{
		return false; // the hero is the leader
	}

	return true;
}

// Returns true while the hero is going to the leader (then nothing else is done)
bool CAutoHelper::AutoFollow()
{
	if (!this->IsFollowingLeader())
	{
		return false;
	}

	DWORD now = GetTickCount();

	PARTY_t* leader = &Party[0];

	int heroX = (int)(*(float*)(Hero + 16) / 100.0f);
	int heroY = (int)(*(float*)(Hero + 20) / 100.0f);

	// The leader is in another map
	if (leader->map != World)
	{
		if ((now - this->m_LastFollowTick) >= 15000)
		{
			this->m_LastFollowTick = now;
			gPartyMatchingUI.RequestMoveToLeader();
		}

		return true;
	}

	// Prefer live viewport coordinates when the Leader is on screen (much fresher than Party[0].x/y)
	int targetX = (int)leader->x;
	int targetY = (int)leader->y;
	for (int i = 0; i < MAX_MAIN_VIEWPORT; i++)
	{
		DWORD vp = CharactersClient + (i * 916);
		if (*(BYTE*)(vp) == 0 || vp == Hero) continue;
		if (*(BYTE*)(vp + 0x84) == 1 && _strnicmp((char*)(vp + 0x1C1), leader->Name, 10) == 0)
		{
			targetX = (int)(*(float*)(vp + 16) / 100.0f);
			targetY = (int)(*(float*)(vp + 20) / 100.0f);
			break;
		}
	}

	int dx = targetX - heroX;
	int dy = targetY - heroY;
	int dist = (int)sqrt((float)(dx * dx + dy * dy));

	if (dist > 3)
	{
		if ((now - this->m_LastFollowMoveTick) >= 600)
		{
			this->m_LastFollowMoveTick = now;

			this->MoveTo(targetX, targetY);
		}

		return true;
	}

	// Close enough: the hunting spot follows the leader
	this->m_OriginX = targetX;
	this->m_OriginY = targetY;

	return false;
}

// A notice, and a sound when the player wants it
void CAutoHelper::Alert(const char* text)
{
	CreateNotice((char*)text, 1);

	if (this->m_Config.AlertSound)
	{
		MessageBeep(MB_ICONEXCLAMATION);
	}
}

// The profiles are sections of the same store with the name of the character and a number: "Name#1"
void CAutoHelper::SaveProfile(int n)
{
	if (n < 1 || n > 3 || this->m_LoadedCharacter.empty())
	{
		return;
	}

	std::string base = this->m_LoadedCharacter;
	char name[96];
	sprintf_s(name, sizeof(name), "%s#%d", base.c_str(), n);

	this->SaveConfig(name);
	this->m_LoadedCharacter = base;
}

void CAutoHelper::LoadProfile(int n)
{
	if (n < 1 || n > 3 || this->m_LoadedCharacter.empty())
	{
		return;
	}

	std::string base = this->m_LoadedCharacter;
	char name[96];
	sprintf_s(name, sizeof(name), "%s#%d", base.c_str(), n);

	this->LoadConfig(name);
	this->m_LoadedCharacter = base;

	// what was loaded becomes the settings of the character
	this->SaveConfig(base.c_str());
	this->m_LoadedCharacter = base;
}

void CAutoHelper::CheckStopConditions()
{
	DWORD now = GetTickCount();

	if (this->m_Config.StopInvFull && (now - this->m_LastInvCheckTick) >= 5000)
	{
		this->m_LastInvCheckTick = now;

		if (this->CountFreeInventoryCells() < 1)
		{
			this->Stop();
			this->Alert("[SmartAuto] Inventory full - Auto stopped!");
			return;
		}
	}

	if (this->m_Config.StopIdle && this->m_Config.AttackMobs && this->m_Config.IdleMinutes > 0 && this->m_LastTargetSeenTick != 0
		&& (now - this->m_LastTargetSeenTick) > (DWORD)(this->m_Config.IdleMinutes * 60000) && !this->IsFollowingLeader())
	{
		this->Stop();
		this->Alert("[SmartAuto] No monsters nearby - Auto stopped!");
	}
}

// Free 1x1 cells of the inventory (the items are stored at their first cell, the others of their cells are empty)
int CAutoHelper::CountFreeInventoryCells()
{
	bool used[64] = { false };

	for (int i = 0; i < INVENTORY_ITEM_SIZE && i < 64; i++)
	{
		ITEM* pItem = GetInventoryItem(i);

		if (!pItem || pItem->Type < 0 || pItem->Type >= MAX_ITEM)
		{
			continue;
		}

		ITEM_ATTRIBUTE* attr = (ITEM_ATTRIBUTE*)(ItemAttribute + pItem->Type * sizeof(ITEM_ATTRIBUTE));

		int w = (attr->Width > 0) ? attr->Width : 1;
		int h = (attr->Height > 0) ? attr->Height : 1;

		for (int y = 0; y < h; y++)
		{
			for (int x = 0; x < w; x++)
			{
				int cx = (i % 8) + x;
				int cy = (i / 8) + y;

				if (cx < 8 && cy < 8)
				{
					used[cy * 8 + cx] = true;
				}
			}
		}
	}

	int free = 0;

	for (int n = 0; n < 64; n++)
	{
		free += used[n] ? 0 : 1;
	}

	return free;
}

bool CAutoHelper::IsBlacklisted(WORD key)
{
	std::map<WORD, DWORD>::iterator it = this->m_Blacklist.find(key);

	if (it == this->m_Blacklist.end())
	{
		return false;
	}

	if (GetTickCount() >= it->second)
	{
		this->m_Blacklist.erase(it);
		return false;
	}

	return true;
}

void CAutoHelper::SetWorldBoss(WORD key)
{
	this->m_WorldBossKey = key;
	this->m_WorldBossTick = GetTickCount();
}

void CAutoHelper::OnExperience(WORD mobKey, DWORD exp)
{
	if (!this->m_Active)
	{
		return;
	}

	this->m_Stats.exp += exp;

	if (mobKey != 0xFFFF)
	{
		this->m_Stats.kills++;
		this->m_LastTargetSeenTick = GetTickCount();
	}
}

void CAutoHelper::OnZenPicked(int zen)
{
	if (this->m_Active && zen > 0)
	{
		this->m_Stats.zen += (unsigned long long)zen;
	}
}

void CAutoHelper::OnItemPicked(short type)
{
	if (!this->m_Active)
	{
		return;
	}

	this->m_Stats.items++;

	if (type == GET_ITEM(14, 13) || type == GET_ITEM(14, 14) || type == GET_ITEM(14, 16) || type == GET_ITEM(12, 15) || type == GET_ITEM(14, 22))
	{
		this->m_Stats.jewels++;
	}
}

static void AutoFormatNumber(unsigned long long val, char* out, size_t outSize)
{
	char raw[32];
	sprintf_s(raw, sizeof(raw), "%llu", val);
	int len = (int)strlen(raw);
	int pos = 0;
	for (int i = 0; i < len && pos + 1 < (int)outSize; i++)
	{
		if (i > 0 && ((len - i) % 3) == 0 && pos + 1 < (int)outSize)
		{
			out[pos++] = ',';
		}
		out[pos++] = raw[i];
	}
	out[pos] = '\0';
}

// Cold Steel stats panel under the top-left HUD buttons while the auto hunt runs
void CAutoHelper::RenderStats()
{
	if (!this->m_Active || !this->m_Config.ShowStats || SceneFlag != 5 || !Hero)
	{
		return;
	}

	const float x = 8.0f;
	const float y = 34.0f;
	const float w = 148.0f;
	const float h = 78.0f;
	const float pad = 7.0f;

	DWORD now = GetTickCount();
	double elapsed = (double)(now - this->m_Stats.startTick) / 1000.0;

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	((void(__thiscall*)(void*, HFONT))0x0040F650)(*(void**)0x055C9FF8, g_hFont);
	const DWORD oldTextColor = SetTextColor;
	const DWORD oldBgColor = SetBackgroundTextColor;

	// Cold Steel panel
	EnableAlphaTest(true);
	glColor4f(0.23f, 0.27f, 0.32f, 0.92f);
	RenderColor(x, y, w, h);
	glColor4f(0.055f, 0.075f, 0.098f, 0.86f);
	RenderColor(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f);
	glColor4f(0.20f, 0.85f, 0.55f, 0.95f); // emerald active bar
	RenderColor(x, y, w, 2.0f);
	glColor4f(0.10f, 0.14f, 0.19f, 0.85f);
	RenderColor(x + 1.0f, y + 2.0f, w - 2.0f, 14.0f);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	EnableAlphaTest(true);
	SetBackgroundTextColor = 0x00000000;

	char line[96], num[32];

	int total = (int)elapsed;
	sprintf_s(line, sizeof(line), "SMART AUTO  %02d:%02d:%02d", total / 3600, (total / 60) % 60, total % 60);
	SetTextColor = 0xFF70F0A0; // bright emerald
	RenderText((int)(x + pad), (int)(y + 4.0f), line, 0, RT3_SORT_LEFT, NULL);

	sprintf_s(line, sizeof(line), "Quai: %u   Ngoc: %u", this->m_Stats.kills, this->m_Stats.jewels);
	SetTextColor = 0xFFEBEFF5;
	RenderText((int)(x + pad), (int)(y + 20.0f), line, 0, RT3_SORT_LEFT, NULL);

	if (this->m_StatSnapTick == 0 || (now - this->m_StatSnapTick) >= 5000)
	{
		this->m_StatSnapTick = now;
		this->m_StatPerHour = (elapsed >= 30.0) ? ((double)this->m_Stats.exp * 3600.0 / elapsed) : 0.0;
		double remaining = ((double)gPrintPlayer.ViewNextExperience > (double)gPrintPlayer.ViewExperience) ? ((double)gPrintPlayer.ViewNextExperience - (double)gPrintPlayer.ViewExperience) : 0.0;
		double perSecond = this->m_StatPerHour / 3600.0;
		this->m_StatEtaMinutes = (perSecond > 0.0 && remaining > 0.0) ? (int)(remaining / perSecond / 60.0 + 0.5) : -1;
	}

	if (this->m_StatPerHour > 0.0)
	{
		if (this->m_StatPerHour >= 1000000.0) sprintf_s(num, sizeof(num), "%.2fM", this->m_StatPerHour / 1000000.0);
		else if (this->m_StatPerHour >= 1000.0) sprintf_s(num, sizeof(num), "%.1fK", this->m_StatPerHour / 1000.0);
		else sprintf_s(num, sizeof(num), "%d", (int)this->m_StatPerHour);
		sprintf_s(line, sizeof(line), "EXP/gio: %s", num);
	}
	else
	{
		sprintf_s(line, sizeof(line), "EXP/gio: ...");
	}
	SetTextColor = 0xFF90F0A0;
	RenderText((int)(x + pad), (int)(y + 33.0f), line, 0, RT3_SORT_LEFT, NULL);

	if (this->m_StatEtaMinutes < 0) sprintf_s(line, sizeof(line), "Len cap: ...");
	else if (this->m_StatEtaMinutes >= 6000) sprintf_s(line, sizeof(line), "Len cap: > 100 gio");
	else if (this->m_StatEtaMinutes >= 60) sprintf_s(line, sizeof(line), "Len cap: ~%dh %02dp", this->m_StatEtaMinutes / 60, this->m_StatEtaMinutes % 60);
	else sprintf_s(line, sizeof(line), "Len cap: ~%d phut", this->m_StatEtaMinutes);
	SetTextColor = 0xFFD0DCE6;
	RenderText((int)(x + pad), (int)(y + 46.0f), line, 0, RT3_SORT_LEFT, NULL);

	AutoFormatNumber(this->m_Stats.zen, num, sizeof(num));
	sprintf_s(line, sizeof(line), "Zen: %s", num);
	SetTextColor = 0xFF5AD2FF; // gold
	RenderText((int)(x + pad), (int)(y + 59.0f), line, 0, RT3_SORT_LEFT, NULL);

	SetTextColor = oldTextColor;
	SetBackgroundTextColor = oldBgColor;
	SelectObject(m_hFontDC, oldFont);
}

void CAutoHelper::AutoPotion()
{
	DWORD currentTick = GetTickCount();
	if (currentTick - this->m_LastPotionTick < 250) // 250ms cooldown between potions
	{
		return;
	}

	if (!Hero || !CharacterAttribute)
	{
		return;
	}

	DWORD curHP = gPrintPlayer.ViewCurHP;
	DWORD maxHP = gPrintPlayer.ViewMaxHP;
	DWORD curMP = gPrintPlayer.ViewCurMP;
	DWORD maxMP = gPrintPlayer.ViewMaxMP;

	if (maxHP == 0)
	{
		STRUCT_DECRYPT;
		curHP = *(WORD*)(CharacterAttribute + 0x1C);
		maxHP = *(WORD*)(CharacterAttribute + 0x20);
		if (curHP == 0 && maxHP == 0)
		{
			curHP = *(WORD*)(CharacterAttribute + 0x2E);
			maxHP = *(WORD*)(CharacterAttribute + 0x30);
		}
		STRUCT_ENCRYPT;
	}

	if (maxMP == 0)
	{
		STRUCT_DECRYPT;
		curMP = *(WORD*)(CharacterAttribute + 0x1E);
		maxMP = *(WORD*)(CharacterAttribute + 0x22);
		if (curMP == 0 && maxMP == 0)
		{
			curMP = *(WORD*)(CharacterAttribute + 0x32);
			maxMP = *(WORD*)(CharacterAttribute + 0x34);
		}
		STRUCT_ENCRYPT;
	}

	// Antidote when the hero is poisoned
	if (this->m_Config.AutoAntidote && (*(DWORD*)(Hero + 120) & STATE_POISON) != 0 && (currentTick - this->m_LastAntidoteTick) > 3000)
	{
		int slot = this->FindAntidoteSlot();
		if (slot != -1)
		{
			this->UseItem((BYTE)slot);
			this->m_LastAntidoteTick = currentTick;
			this->m_LastPotionTick = currentTick;
			return;
		}
	}

	// Auto HP Potion: the size of the potion follows the urgency (a big one when the life is really low)
	if (this->m_Config.AutoPotionHP && maxHP > 0 && curHP > 0)
	{
		int hpPercent = (int)((curHP * 100) / maxHP);
		if (hpPercent <= this->m_Config.PotionHPThreshold)
		{
			int slot = -1;

			if (this->m_Config.SmartPotion)
			{
				static const int urgent[4] = { 3, 2, 1, 0 };  // Large, Medium, Small, Apple
				static const int normal[4] = { 2, 1, 3, 0 };  // Medium, Small, Large, Apple

				bool isUrgent = (hpPercent * 2 <= this->m_Config.PotionHPThreshold);
				slot = this->FindPotionSlot(isUrgent ? urgent : normal, 4);
			}
			else
			{
				slot = this->FindHealingPotionSlot();
			}

			if (slot != -1)
			{
				this->UseItem((BYTE)slot);
				this->m_LastPotionTick = currentTick;
				this->m_NoPotionTick = 0;
				return;
			}

			// No potion left: after a few seconds the hunt stops (the hero is going to be killed otherwise)
			if (this->m_NoPotionTick == 0)
			{
				this->m_NoPotionTick = currentTick;
			}
			else if (this->m_Config.TownEscape && hpPercent <= 15 && (currentTick - this->m_NoPotionTick) > 1500)
			{
				// Life almost gone and no potion: back to the town before the hero dies
				this->Stop();
				SendChat("/move Lorencia");
				this->Alert("[SmartAuto] Low HP & no potions - Warping to Lorencia!");
				return;
			}
			else if (this->m_Config.StopNoPotion && (currentTick - this->m_NoPotionTick) > 4000)
			{
				this->Stop();
				this->Alert("[SmartAuto] Out of HP potions - Auto stopped!");
				return;
			}
		}
		else
		{
			this->m_NoPotionTick = 0;
		}
	}

	// Auto MP Potion
	if (this->m_Config.AutoPotionMP && maxMP > 0 && curMP > 0)
	{
		int mpPercent = (int)((curMP * 100) / maxMP);
		if (mpPercent <= this->m_Config.PotionMPThreshold)
		{
			int slot = -1;

			if (this->m_Config.SmartPotion)
			{
				static const int urgent[3] = { 6, 5, 4 };  // Large, Medium, Small
				static const int normal[3] = { 5, 4, 6 };  // Medium, Small, Large

				bool isUrgent = (mpPercent * 2 <= this->m_Config.PotionMPThreshold);
				slot = this->FindPotionSlot(isUrgent ? urgent : normal, 3);
			}
			else
			{
				slot = this->FindManaPotionSlot();
			}

			if (slot != -1)
			{
				this->UseItem((BYTE)slot);
				this->m_LastPotionTick = currentTick;
				return;
			}
		}
	}
}

// First potion of the list (section 14 indexes, in the order of preference) found in the inventory
int CAutoHelper::FindPotionSlot(const int* order, int count)
{
	for (int k = 0; k < count; k++)
	{
		for (int i = 0; i < INVENTORY_ITEM_SIZE; i++)
		{
			ITEM* pItem = GetInventoryItem(i);

			if (pItem && pItem->Type == GET_ITEM(14, order[k]))
			{
				return INVENTORY_WEAR_SIZE + i;
			}
		}
	}

	return -1;
}

int CAutoHelper::FindHealingPotionSlot()
{
	// Search inventory grid slots (0 to 63)
	for (int i = 0; i < INVENTORY_ITEM_SIZE; i++)
	{
		ITEM* pItem = GetInventoryItem(i);
		if (pItem && pItem->Type >= 0 && pItem->Type < MAX_ITEM)
		{
			// Apple (14,0), Small HP (14,1), Medium HP (14,2), Large HP (14,3)
			if (pItem->Type >= GET_ITEM(14, 0) && pItem->Type <= GET_ITEM(14, 3))
			{
				return INVENTORY_WEAR_SIZE + i;
			}
		}
	}
	return -1;
}

int CAutoHelper::FindManaPotionSlot()
{
	// Search inventory grid slots (0 to 63)
	for (int i = 0; i < INVENTORY_ITEM_SIZE; i++)
	{
		ITEM* pItem = GetInventoryItem(i);
		if (pItem && pItem->Type >= 0 && pItem->Type < MAX_ITEM)
		{
			// Small MP (14,4), Medium MP (14,5), Large MP (14,6)
			if (pItem->Type >= GET_ITEM(14, 4) && pItem->Type <= GET_ITEM(14, 6))
			{
				return INVENTORY_WEAR_SIZE + i;
			}
		}
	}
	return -1;
}

int CAutoHelper::FindAntidoteSlot()
{
	for (int i = 0; i < INVENTORY_ITEM_SIZE; i++)
	{
		ITEM* pItem = GetInventoryItem(i);
		if (pItem && pItem->Type >= 0 && pItem->Type < MAX_ITEM)
		{
			if (pItem->Type == GET_ITEM(14, 8))
			{
				return INVENTORY_WEAR_SIZE + i;
			}
		}
	}
	return -1;
}

void CAutoHelper::AutoCombat()
{
	DWORD currentTick = GetTickCount();

	if (!Hero || SceneFlag != 5)
	{
		return;
	}

	if (!this->m_Config.AttackMobs)
	{
		return;
	}

	DWORD attackCooldown = 400;
	if (CharacterAttribute)
	{
		STRUCT_DECRYPT;
		WORD magicSpeed = *(WORD*)(CharacterAttribute + 0x44);
		WORD physiSpeed = *(WORD*)(CharacterAttribute + 0x38);
		STRUCT_ENCRYPT;

		WORD speed = (magicSpeed > physiSpeed) ? magicSpeed : physiSpeed;
		if (speed > 500) attackCooldown = 180;
		else if (speed > 300) attackCooldown = 220;
		else if (speed > 200) attackCooldown = 260;
		else if (speed > 120) attackCooldown = 300;
		else if (speed > 80) attackCooldown = 340;
		else if (speed > 50) attackCooldown = 380;
		else attackCooldown = 420;
	}

	if (currentTick - this->m_LastAttackTick < attackCooldown)
	{
		return;
	}

	float heroX = *(float*)(Hero + 16);
	float heroY = *(float*)(Hero + 20);
	float maxDistance = (float)this->m_Config.HuntingRange * 100.0f;

	// Select skill from configured slots first so we know effective range
	std::vector<int> validSkills;
	for (int i = 0; i < MAX_AUTO_SKILLS; i++)
	{
		if (this->m_Config.AttackSkills[i] >= 0)
		{
			validSkills.push_back(this->m_Config.AttackSkills[i]);
		}
	}

	int selectedSkill = -1;
	if (!validSkills.empty())
	{
		if (this->m_Config.AttackMode == 1) // Random mode
		{
			selectedSkill = validSkills[rand() % validSkills.size()];
		}
		else // Sequential mode
		{
			this->m_AttackSkillIndex = this->m_AttackSkillIndex % validSkills.size();
			selectedSkill = validSkills[this->m_AttackSkillIndex];
			this->m_AttackSkillIndex = (this->m_AttackSkillIndex + 1) % validSkills.size();
		}
	}

	BYTE baseClass = this->GetPlayerClass(); // 0=DW, 1=DK, 2=Elf, 3=MG, 4=DL

	// Determine effective range of selected skill
	int skillMaxRange = 6;
	switch (selectedSkill)
	{
		case -1:
		case 0:
			if (baseClass == 2) // Fairy Elf basic attack with Bow/Crossbow
			{
				skillMaxRange = 6;
			}
			else
			{
				skillMaxRange = 2; // Melee classes
			}
			break;
		case 20: // Lunge
		case 21: // Uppercut
		case 22: // Cyclone
		case 23: // Slash
		case 41: // Twisting Slash
		case 43: // Death Stab
		case 55: // Fire Slash
			skillMaxRange = 2;
			break;
		case 19: // Falling Slash
		case 42: // Rageful Blow
		case 47: // Impale
		case 13: // Blast (Range 3 in Skill.txt)
		case 10: // Hellfire
		case 14: // Inferno
			skillMaxRange = 3;
			break;
		case 56: // Power Slash
			skillMaxRange = 5;
			break;
		case 51: // Ice Arrow
			skillMaxRange = 7;
			break;
		case 24: // Triple Shot
		case 52: // Penetration
		default: // Evil Spirit (9), Flame (5), Twister (8), Aqua Beam (12), Blast (13), Poison (1), etc.
			skillMaxRange = 6;
			break;
	}

	struct TargetCandidate
	{
		int key;
		float dist;
		int gridX;
		int gridY;
		float score;
		int hp;
		bool boss;
	};
	std::vector<TargetCandidate> candidates;

	for (int n = 0; n < MAX_MAIN_VIEWPORT; n++)
	{
		DWORD ViewportAddress = *(DWORD*)(0x07ABF5D0) + (n * 916);
		if (!ViewportAddress || *(BYTE*)(ViewportAddress) == 0) // Not Live
		{
			continue;
		}

		if (ViewportAddress == Hero)
		{
			continue;
		}

		// Check client character dead flag (0x2FD is set to 1 when dying/dead)
		if (*(BYTE*)(ViewportAddress + 0x2FD) != 0)
		{
			continue;
		}

		short mobKey = *(short*)(ViewportAddress + 0x1DC);
		if (mobKey <= 0 || mobKey == (short)0xFFFF || (WORD)mobKey == 0xFFFF)
		{
			continue; // Dead or invalid key
		}

		// Check internal dead entity registry (from 0x17 / 0x9C packets)
		if (this->IsDeadEntity((WORD)mobKey))
		{
			continue;
		}

		// Monsters that could not be hit or reached are left alone for a while
		if (this->IsBlacklisted((WORD)mobKey))
		{
			continue;
		}

		BYTE kind = *(BYTE*)(ViewportAddress + 0x84);
		WORD monsterType = *(WORD*)(ViewportAddress + 0x2EB);

		// Must be monster (kind == 2) or PK if allowed
		if (kind != 2 && !(this->m_Config.AttackPK && kind == 1))
		{
			continue;
		}

		// Skip non-attackable NPCs / objects (Soccer Ball = 200)
		if (monsterType == 200 || monsterType >= 500)
		{
			continue;
		}

		// Check health bar - skip dead/dying monsters
		MONSTER_HEALTH_BAR* lpHealthBar = gHealthBar.GetHealthBar((WORD)mobKey, kind);
		if (lpHealthBar && lpHealthBar->rateHP <= 0)
		{
			this->OnEntityDie((WORD)mobKey);
			continue;
		}

		float mobX = *(float*)(ViewportAddress + 0x10);
		float mobY = *(float*)(ViewportAddress + 0x14);

		float dx = mobX - heroX;
		float dy = mobY - heroY;
		float dist = sqrt(dx * dx + dy * dy);

		if (dist <= maxDistance)
		{
			bool isWorldBoss = (this->m_WorldBossTick != 0 && (currentTick - this->m_WorldBossTick) < 6000 && (WORD)mobKey == this->m_WorldBossKey);

			TargetCandidate tc;
			tc.key = (int)mobKey;
			tc.dist = dist;
			tc.gridX = (int)(mobX / 100.0f);
			tc.gridY = (int)(mobY / 100.0f);
			tc.hp = lpHealthBar ? (int)lpHealthBar->rateHP : -1;
			tc.boss = isWorldBoss;
			tc.score = dist;

			// The world boss first, then the elite monsters, then the closest ones
			if (this->m_Config.PreferBoss)
			{
				if (isWorldBoss)
				{
					tc.score -= 100000.0f;
				}
			}

			candidates.push_back(tc);
		}
	}

	if (!candidates.empty())
	{
		this->m_LastTargetSeenTick = currentTick;
	}

	if (candidates.empty())
	{
		this->m_CurrentTarget = -1;

		if (Hero)
		{
			// Immediately stop Hero attack if currently attacking
			if (*(BYTE*)(Hero + 0x2F5) != 0 || CurrentSkill != 0)
			{
				*(BYTE*)(Hero + 0x2F5) = 0; // AttackTime = 0
				*(BYTE*)(Hero + 0x2F4) = 0;
				*(WORD*)(Hero + 0x310) = 0xFFFF; // TargetCharacter = none
				CurrentSkill = 0;

				BYTE curAction = *(BYTE*)(Hero + 0x105);
				if (curAction >= 0x2E) // In attack or skill animation
				{
					SetPlayerStop(Hero);
				}
			}
		}
		return;
	}

	// Best target first (world boss, elite, then the closest)
	std::sort(candidates.begin(), candidates.end(), [](const TargetCandidate& a, const TargetCandidate& b) {
		return a.score < b.score;
	});

	int nearestTargetKey = candidates[0].key;
	float nearestDist = candidates[0].dist;
	int targetGridX = candidates[0].gridX;
	int targetGridY = candidates[0].gridY;

	// A target that loses no life and that the hero does not get closer to for a few seconds is left alone for 30 seconds
	if (this->m_Config.SkipStuck)
	{
		if (this->m_CurrentTarget != nearestTargetKey)
		{
			this->m_CurrentTarget = nearestTargetKey;
			this->m_TargetProgressTick = currentTick;
			this->m_TargetLastHp = candidates[0].hp;
			this->m_TrackX = heroX;
			this->m_TrackY = heroY;
		}
		else
		{
			float moved = sqrtf((heroX - this->m_TrackX) * (heroX - this->m_TrackX) + (heroY - this->m_TrackY) * (heroY - this->m_TrackY));

			if (candidates[0].hp != this->m_TargetLastHp || moved >= 40.0f)
			{
				this->m_TargetProgressTick = currentTick;
				this->m_TargetLastHp = candidates[0].hp;
				this->m_TrackX = heroX;
				this->m_TrackY = heroY;
			}
			else if (!candidates[0].boss && (currentTick - this->m_TargetProgressTick) > 7000)
			{
				this->m_Blacklist[(WORD)nearestTargetKey] = currentTick + 30000;
				this->m_CurrentTarget = -1;
				this->m_LastAttackTick = currentTick;
				return;
			}
		}
	}

	// Collect up to 5 closest live targets for multi-target skills (the chosen target first)
	std::vector<TargetCandidate> byDistance(candidates);
	std::sort(byDistance.begin(), byDistance.end(), [](const TargetCandidate& a, const TargetCandidate& b) {
		return a.dist < b.dist;
	});

	// Only the monsters inside the range of the skill are attacked (about one tile of margin for the radius of the area skills)
	std::vector<int> multiTargets;
	multiTargets.push_back(nearestTargetKey);
	for (size_t i = 0; i < byDistance.size() && multiTargets.size() < 5; i++)
	{
		if (byDistance[i].key != nearestTargetKey && byDistance[i].dist <= (float)((skillMaxRange + 1) * 100))
		{
			multiTargets.push_back(byDistance[i].key);
		}
	}

	// If monster is outside effective skill range, walk closer towards the monster
	if (nearestDist > (float)(skillMaxRange * 100))
	{
		this->MoveTo(targetGridX, targetGridY);
		this->m_LastAttackTick = currentTick;
		return;
	}

	this->CastSkill(selectedSkill, nearestTargetKey, targetGridX, targetGridY, multiTargets);
	this->m_LastAttackTick = currentTick;
}

// The skill of the character (c->Skill) is a protected value of the engine: the byte is written in plain and then encoded
// again with the engine function (the same two steps the engine does for its own variables). The effects of the skills
// (arrows of the Elf included) read it; when the player attacks by hand the engine sets it, the auto has to do it too.
static void SetHeroSkillByte(int skillId)
{
	if (!Hero)
	{
		return;
	}

	*(BYTE*)(Hero + 0x302) = (BYTE)((skillId > 0) ? skillId : 0);

	((unsigned int(__thiscall*)(DWORD, DWORD))0x00404040)(0x055C9BC8, (DWORD)(Hero + 0x302));
}

static bool SetElfWeaponAttackAction(DWORD c)
{
	if (!c) return false;

	short wRight = *(short*)(c + 0x270);
	short wLeft  = *(short*)(c + 0x288);
	bool bWing   = (*(short*)(c + 0x2A0) != -1);
	short pet    = *(short*)(c + 0x2B8);
	bool bRide   = (pet == 0x332 || pet == 0x333);

	bool isBow = false;
	bool isCrossbow = false;

	// Check Right Hand
	if ((wRight >= 528 && wRight <= 534) || wRight == 545 || wRight == 546 || wRight == 549 || wRight == 550)
	{
		isBow = true;
	}
	else if ((wRight >= 536 && wRight <= 542) || wRight == 544 || wRight == 547 || wRight == 548)
	{
		isCrossbow = true;
	}

	// Check Left Hand
	if (!isBow && !isCrossbow)
	{
		if ((wLeft >= 528 && wLeft <= 534) || wLeft == 545 || wLeft == 546 || wLeft == 549 || wLeft == 550)
		{
			isBow = true;
		}
		else if ((wLeft >= 536 && wLeft <= 542) || wLeft == 544 || wLeft == 547 || wLeft == 548)
		{
			isCrossbow = true;
		}
	}

	// Fallback to Ammo check if weapon model is generic/custom
	if (!isBow && !isCrossbow)
	{
		if (wRight == 543 || wLeft == 543) isBow = true; // Arrows -> Bow
		else if (wRight == 535 || wLeft == 535) isCrossbow = true; // Bolts -> Crossbow
	}

	if (isBow)
	{
		if (bRide) SetAction(c, 0x36);      // 54 = PLAYER_ATTACK_RIDE_BOW
		else if (bWing) SetAction(c, 0x30); // 48 = PLAYER_ATTACK_FLY_BOW
		else SetAction(c, 0x2E);            // 46 = PLAYER_ATTACK_BOW
	}
	else if (isCrossbow)
	{
		if (bRide) SetAction(c, 0x37);      // 55 = PLAYER_ATTACK_RIDE_CROSSBOW
		else if (bWing) SetAction(c, 0x31); // 49 = PLAYER_ATTACK_FLY_CROSSBOW
		else SetAction(c, 0x2F);            // 47 = PLAYER_ATTACK_CROSSBOW
	}
	else
	{
		SetPlayerAttack(c);
	}

	return (isBow || isCrossbow);
}

// When the player attacks by hand the engine creates the arrows right away with this function (0x00474BD0):
//   (character, object, 0, index of the skill in the skill list, 1 = three arrows, skill id)
// The auto only set the animation, so no arrow was ever created.
static void CreateElfArrows(int skillId)
{
	int index = 0;

	if (CharacterAttribute)
	{
		STRUCT_DECRYPT;

		for (int i = 0; i < 20; i++)
		{
			if (skillId > 0 && *(BYTE*)(CharacterAttribute + 0x57 + i) == (BYTE)skillId)
			{
				index = i;
				break;
			}
		}

		STRUCT_ENCRYPT;
	}

	int triple = (skillId == SKILL_TRIPLE_SHOT) ? 1 : 0;
	int skill = (skillId == SKILL_TRIPLE_SHOT || skillId <= 0) ? 0 : skillId;

	((void(__cdecl*)(DWORD, DWORD, int, int, int, int))0x00474BD0)(Hero, Hero, 0, index, triple, skill);
}

void CAutoHelper::CastSkill(int skillId, int targetIndex, int targetX, int targetY, const std::vector<int>& targets)
{
	int heroX = (int)(*(float*)(Hero + 16) / 100.0f);
	int heroY = (int)(*(float*)(Hero + 20) / 100.0f);

	// 1. Calculate direction & face Hero towards target monster
	float targetWorldX = (float)targetX * 100.0f;
	float targetWorldY = (float)targetY * 100.0f;
	float currentHeroWorldX = *(float*)(Hero + 16);
	float currentHeroWorldY = *(float*)(Hero + 20);

	float nx2 = targetWorldX - currentHeroWorldX;
	float ny2 = targetWorldY - currentHeroWorldY;
	float targetAngle = *(float*)(Hero + 0x24);
	if (fabsf(nx2) > 1.0f || fabsf(ny2) > 1.0f)
	{
		if (fabsf(nx2) < 0.0001f)
		{
			targetAngle = (ny2 < 0.0f) ? 0.0f : 180.0f;
		}
		else if (fabsf(ny2) < 0.0001f)
		{
			targetAngle = (nx2 < 0.0f) ? 270.0f : 90.0f;
		}
		else
		{
			float angle = (float)atan(ny2 / nx2) / 3.14159265358979323846f * 180.0f + 90.0f;
			targetAngle = (nx2 < 0.0f) ? (angle + 180.0f) : angle;
		}
		*(float*)(Hero + 0x24) = targetAngle; // Angle[2]
		*(float*)(Hero + 36) = targetAngle;
	}

	int heroDir = (int)(targetAngle / 360.0f * 256.0f);

	// 2. Set Hero attack & skill state in game engine
	CurrentSkill = skillId;
	SetHeroSkillByte(skillId);
	*(BYTE*)(Hero + 0x2F4) = 0;
	*(BYTE*)(Hero + 0x2F5) = 1; // AttackTime = 1 - engine counts UP to 15, triggers effects at correct frames
	*(float*)(Hero + 264) = 0.0f; // AnimationFrame = 0 - restart animation from beginning
	// TargetCharacter is a slot of the character array of the client, not the key the server uses. The arrows of the Elf
	// (and every effect that flies to the target) are created from that slot, so with the key they were never shown.
	int targetSlot = FindCharacterIndex(targetIndex);
	*(WORD*)(Hero + 0x310) = (WORD)((targetSlot >= 0 && targetSlot < MAX_MAIN_VIEWPORT) ? targetSlot : targetIndex); // TargetCharacter
	// World position of the target (c->TargetPosition): what the engine itself stores when the player attacks by hand.
	// The arrows and the effects that fly to the target start from it.
	if (targetSlot >= 0 && targetSlot < MAX_MAIN_VIEWPORT)
	{
		DWORD tc = CharactersClient + (targetSlot * 916);
		*(float*)(Hero + 0x314) = *(float*)(tc + 0x10);
		*(float*)(Hero + 0x318) = *(float*)(tc + 0x14);
		*(float*)(Hero + 0x31C) = *(float*)(tc + 0x18);
	}
	else
	{
		*(float*)(Hero + 0x314) = targetWorldX;
		*(float*)(Hero + 0x318) = targetWorldY;
	}
	*(BYTE*)(Hero + 0x308) = (BYTE)targetX;
	*(BYTE*)(Hero + 0x309) = (BYTE)targetY;
	*(BYTE*)(Hero + 0x30A) = (BYTE)((int)(targetAngle / 45.0f + 0.5f) % 8);

	// Stop Hero movement so attack animation is not cancelled by walk loop
	*(BYTE*)(Hero + 748) = 0; // c->Movement = false
	*(BYTE*)(Hero + 749) = 0; // c->MovementType = 0

	// Set attack animation speed according to character Agility/Speed
	CPrintPlayer::SetAttackSpeed();

	// Determine character base class: 0=DW, 1=DK, 2=Elf, 3=MG, 4=DL
	BYTE baseClass = this->GetPlayerClass();

	bool bowShot = false;

	// 3. Set correct animation action and play sound per skill.
	switch (skillId)
	{
		// ====== DARK KNIGHT / MAGIC GLADIATOR MELEE SKILLS ======
		case SKILL_FALLING_SLASH: // 19 = AT_SKILL_SWORD1 -> PLAYER_ATTACK_SKILL_SWORD1+0 = 56
		{
			SetAction(Hero, 56);
			PlayBuffer(112, Hero, FALSE); // SOUND_SKILL_SWORD1
			break;
		}
		case SKILL_LUNGE: // 20 = AT_SKILL_SWORD2 -> PLAYER_ATTACK_SKILL_SWORD1+1 = 57
		{
			SetAction(Hero, 57);
			PlayBuffer(113, Hero, FALSE); // SOUND_SKILL_SWORD2
			break;
		}
		case SKILL_UPPERCUT: // 21 = AT_SKILL_SWORD3 -> PLAYER_ATTACK_SKILL_SWORD1+2 = 58
		{
			SetAction(Hero, 58);
			PlayBuffer(114, Hero, FALSE); // SOUND_SKILL_SWORD3
			break;
		}
		case SKILL_CYCLONE: // 22 = AT_SKILL_SWORD4 -> PLAYER_ATTACK_SKILL_SWORD1+3 = 59
		{
			SetAction(Hero, 59);
			PlayBuffer(115, Hero, FALSE); // SOUND_SKILL_SWORD4
			break;
		}
		case SKILL_SLASH: // 23 = AT_SKILL_SWORD5 -> PLAYER_ATTACK_SKILL_SWORD1+4 = 60
		{
			SetAction(Hero, 60);
			PlayBuffer(115, Hero, FALSE); // SOUND_SKILL_SWORD4
			break;
		}
		case SKILL_TWISTING_SLASH: // 41 = AT_SKILL_WHEEL -> PLAYER_ATTACK_SKILL_WHEEL = 0x3D (61)
		{
			SetAction(Hero, 0x3D); // PLAYER_ATTACK_SKILL_WHEEL
			PlayBuffer(115, Hero, FALSE); // SOUND_SKILL_SWORD4
			break;
		}
		case SKILL_RAGEFUL_BLOW: // 42 = AT_SKILL_FURY_STRIKE -> PLAYER_ATTACK_SKILL_FURY_STRIKE = 0x3E (62)
		{
			SetAction(Hero, 0x3E); // PLAYER_ATTACK_SKILL_FURY_STRIKE
			PlayBuffer(128, Hero, FALSE); // SOUND_FURY
			break;
		}
		case SKILL_DEATH_STAB: // 43 = AT_SKILL_ONETOONE -> PLAYER_ATTACK_ONETOONE = 0x47 (71)
		{
			SetAction(Hero, 0x47); // PLAYER_ATTACK_ONETOONE
			PlayBuffer(83, NULL, FALSE); // SOUND_SKILL_SWORD2
			break;
		}
		case SKILL_IMPALE: // 47 = AT_SKILL_SPEAR -> PLAYER_ATTACK_SKILL_SPEAR = 0x48 (72)
		{
			SetAction(Hero, 0x48); // PLAYER_ATTACK_SKILL_SPEAR
			PlayBuffer(95, NULL, FALSE);
			break;
		}
		case SKILL_GREATER_LIFE: // 48 = AT_SKILL_VITALITY -> PLAYER_SKILL_VITALITY = 0x3F (63)
		{
			SetAction(Hero, 0x3F); // PLAYER_SKILL_VITALITY
			PlayBuffer(127, Hero, FALSE); // SOUND_SWELLLIFE
			break;
		}
		case SKILL_FIRE_SLASH: // 55 = AT_SKILL_REDUCEDEFENSE -> PLAYER_ATTACK_SKILL_WHEEL = 0x3D (61)
		{
			SetAction(Hero, 0x3D); // PLAYER_ATTACK_SKILL_WHEEL (same anim as Twisting Slash)
			PlayBuffer(84, NULL, FALSE);
			break;
		}
		case SKILL_POWER_SLASH: // 56 = AT_SKILL_ICE_BLADE -> PLAYER_ATTACK_TWO_HAND_SWORD_TWO = 0x51 (81)
		{
			SetAction(Hero, 0x51); // PLAYER_ATTACK_TWO_HAND_SWORD_TWO
			PlayBuffer(84, NULL, FALSE);
			break;
		}
		// ====== DARK WIZARD / MAGIC GLADIATOR MAGIC SKILLS ======
		case SKILL_EVIL_SPIRIT: // 9 = AT_SKILL_EVIL -> SetPlayerMagic
		{
			SetPlayerMagic(Hero);
			PlayBuffer(117, Hero, FALSE); // SOUND_EVIL
			break;
		}
		case SKILL_HELL_FIRE: // 10 = AT_SKILL_HELL -> PLAYER_SKILL_HELL = 0x42 (66 / 90)
		{
			SetAction(Hero, 90); // PLAYER_SKILL_HELL (0x5A)
			PlayBuffer(119, Hero, FALSE); // SOUND_HELLFIRE
			break;
		}
		case SKILL_INFERNO: // 14 = AT_SKILL_INFERNO -> PLAYER_SKILL_INFERNO = 89
		{
			SetAction(Hero, 89); // PLAYER_SKILL_INFERNO
			break;
		}
		case SKILL_FLAME: // 5 = AT_SKILL_FLAME -> SetPlayerMagic
		{
			SetPlayerMagic(Hero);
			PlayBuffer(121, Hero, FALSE); // SOUND_FLAME
			break;
		}
		case SKILL_TWISTER: // 8 = AT_SKILL_STORM -> SetPlayerMagic
		{
			SetPlayerMagic(Hero);
			PlayBuffer(116, Hero, FALSE); // SOUND_STORM
			break;
		}
		case SKILL_AQUA_BEAM: // 12 = AT_SKILL_FLASH -> SetPlayerMagic
		{
			SetPlayerMagic(Hero);
			PlayBuffer(118, Hero, FALSE);
			break;
		}

		// ====== ELF BOW / CROSSBOW SKILLS ======
		case SKILL_TRIPLE_SHOT: // 24 = AT_SKILL_CROSSBOW
		case SKILL_ICE_ARROW:   // 51 = AT_SKILL_BOW (Ice)
		case SKILL_PENETRATION: // 52 = AT_SKILL_PIERCING
		{
			bowShot = SetElfWeaponAttackAction(Hero);
			if (skillId == SKILL_ICE_ARROW) PlayBuffer(130, Hero, FALSE);
			else if (skillId == SKILL_PENETRATION) PlayBuffer(100, Hero, FALSE);
			else if (skillId == SKILL_TRIPLE_SHOT) PlayBuffer(83, NULL, FALSE);
			break;
		}
		// ====== ELF BUFF & SUMMON SKILLS ======
		case SKILL_HEAL:            // 26
		case SKILL_GREATER_DEFENSE: // 27
		case SKILL_GREATER_DAMAGE:  // 28
		case SKILL_SUMMON1:         // 30
		case SKILL_SUMMON2:         // 31
		case SKILL_SUMMON3:         // 32
		case SKILL_SUMMON4:         // 33
		case SKILL_SUMMON5:         // 34
		case SKILL_SUMMON6:         // 35
		case SKILL_SUMMON7:         // 36
		{
			SetPlayerMagic(Hero); // Native SetPlayerMagic plays Action 86 (PLAYER_SKILL_ELF1)
			PlayBuffer(118, Hero, FALSE);
			break;
		}
		default:
		{
			if (skillId <= 0)
			{
				if (baseClass == 0) // Dark Wizard basic attack
				{
					SetPlayerMagic(Hero);
				}
				else if (baseClass == 2) // Fairy Elf basic attack
				{
					bowShot = SetElfWeaponAttackAction(Hero);
				}
				else // Dark Knight, Magic Gladiator basic attack
				{
					SetPlayerAttack(Hero);
				}
			}
			else
			{
				if (baseClass == 2) // Fairy Elf generic skill
				{
					bowShot = SetElfWeaponAttackAction(Hero);
				}
				else if (baseClass == 0 || baseClass == 3) // DW / MG magic
				{
					SetPlayerMagic(Hero);
					PlayBuffer(118, Hero, FALSE);
				}
				else // DK
				{
					SetPlayerAttack(Hero);
				}
			}
			break;
		}
	}

	// The arrows (and the Ice Arrow / Penetration effects) of the Elf
	if (bowShot && baseClass == 2 && (skillId <= 0 || skillId == SKILL_TRIPLE_SHOT || skillId == SKILL_ICE_ARROW || skillId == SKILL_PENETRATION))
	{
		CreateElfArrows(skillId);
	}

	// 8-directional index for basic attack packet (0..7)
	int dir8 = 0;
	int dx = targetX - heroX;
	int dy = targetY - heroY;
	if (dx <= -1 && dy <= -1) dir8 = 0;
	else if (dx <= -1 && dy == 0) dir8 = 7;
	else if (dx <= -1 && dy >= 1) dir8 = 6;
	else if (dx == 0 && dy <= -1) dir8 = 1;
	else if (dx == 0 && dy >= 1) dir8 = 5;
	else if (dx >= 1 && dy <= -1) dir8 = 2;
	else if (dx >= 1 && dy == 0) dir8 = 3;
	else if (dx >= 1 && dy >= 1) dir8 = 4;

	if (skillId <= 0)
	{
		// Basic physical attack packet C1:[PROTOCOL_CODE2] (0x15, Encrypt = 0)
		PMSG_ATTACK_SEND pMsg;
		pMsg.header.set(0x15, sizeof(pMsg));
		pMsg.index[0] = SET_NUMBERHB(targetIndex);
		pMsg.index[1] = SET_NUMBERLB(targetIndex);
		pMsg.action = 0;
		pMsg.dir = (BYTE)dir8;
		gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
		return;
	}

	// Check if this skill is an Area / Multi-target skill
	// Evil Spirit = 9, Hellfire = 10, Aqua Beam = 12, Blast = 13, Inferno = 14,
	// Flame = 5, Twister = 8, Triple Shot = 24, Penetration = 52, Fire Slash = 55, Power Slash = 56,
	// Twisting Slash = 41, Rageful Blow = 42, Impale = 47
	bool isMultiSkill = (skillId == 9 || skillId == 10 || skillId == 12 || skillId == 13 || 
	                     skillId == 14 || skillId == 5 || skillId == 8 || skillId == 24 || 
	                     skillId == 41 || skillId == 42 || skillId == 47 || skillId == 52 || 
	                     skillId == 55 || skillId == 56);

	if (isMultiSkill)
	{
		// GameServer frustum angle matches client pos = (HeroAngle + 180.0f) / 360.0f * 256.0f
		BYTE skillAngle = (BYTE)(fmodf(targetAngle + 180.0f, 360.0f) / 360.0f * 256.0f);
		BYTE destVal = GetDestValue(heroX, heroY, targetX, targetY);

		// 1. Send Duration Skill Attack (0x1E) - triggers server-side skill animation & frustum check
		gPatchs.SendRequestMagicContinue(skillId, heroX, heroY, heroDir, destVal, skillAngle, targetIndex);

		// 2. Send Multi-Skill Attack (0x1D / PROTOCOL_CODE4) - causes server to apply damage!
		BYTE send[128];
		PMSG_MULTI_SKILL_ATTACK_SEND pMsg;
		BYTE count = (BYTE)min((int)targets.size(), 5);
		if (count == 0) count = 1;

		int totalSize = sizeof(PMSG_MULTI_SKILL_ATTACK_SEND) + count * sizeof(PMSG_MULTI_SKILL_ATTACK_TARGET);
		pMsg.header.setE(0x1D, totalSize);
		pMsg.skill = (BYTE)skillId;
		pMsg.x = (BYTE)heroX;
		pMsg.y = (BYTE)heroY;
		pMsg.serial = 0;
		pMsg.count = count;

		memcpy(send, &pMsg, sizeof(pMsg));

		int offset = sizeof(pMsg);
		for (BYTE i = 0; i < count; i++)
		{
			int tKey = (i < (BYTE)targets.size()) ? targets[i] : targetIndex;
			PMSG_MULTI_SKILL_ATTACK_TARGET t;
			t.index[0] = SET_NUMBERHB(tKey);
			t.index[1] = SET_NUMBERLB(tKey);
			memcpy(send + offset, &t, sizeof(t));
			offset += sizeof(t);
		}

		gProtocol.DataSend(send, totalSize);
	}
	else
	{
		// Targeted single-skill attack C3:19 (Energy Ball, Poison, Meteorite, Lightning, Fire Ball, Ice, Power Wave, Death Stab, etc.)
		gPatchs.SendRequestMagic(skillId, targetIndex);
	}
}

// State flag of the hero (Hero + 120) that the server sets while a buff is active
static DWORD BuffStateBit(int skill)
{
	switch (skill)
	{
		case 16: return STATE_REDUCE_MAGICDEFENSE; // Soul Barrier / Mana Shield
		case 48: return STATE_ADD_LIFE;            // Greater Fortitude
		case 28: return STATE_ATTACK;              // Greater Damage
		case 27: return STATE_DEFENSE;             // Greater Defense
	}

	return 0;
}

void CAutoHelper::CastBuffOnSelf(int buffSkill, short heroKey)
{
	CurrentSkill = buffSkill;
	SetHeroSkillByte(buffSkill);
	*(BYTE*)(Hero + 0x2F5) = 1;
	*(float*)(Hero + 264) = 0.0f;

	gPatchs.SendRequestMagic(buffSkill, (int)heroKey);

	if (buffSkill == 48) // Greater Fortitude (DK Gong Mau)
	{
		SetAction(Hero, 0x3F); // 63 = PLAYER_SKILL_VITALITY
		PlayBuffer(127, Hero, FALSE); // SOUND_SWELLLIFE
	}
	else if (buffSkill == 18 || buffSkill == 16) // Mana Shield / Soul Barrier
	{
		SetPlayerMagic(Hero);
		PlayBuffer(132, Hero, FALSE); // SOUND_SOULBARRIER
	}
	else if (buffSkill == 27 || buffSkill == 28) // Greater Defense / Greater Damage
	{
		SetAction(Hero, 86);
		PlayBuffer(118, Hero, FALSE);
	}
	else // Heal (26), etc.
	{
		SetAction(Hero, 86); // PLAYER_SKILL_ELF1 (86)
		PlayBuffer(118, Hero, FALSE);
	}
}

// "Re-buff the effects that ended": a buff is cast only when its state flag is gone from the hero (no more casting on a timer, which
// cancelled the attacks and burnt mana for buffs that were still active). "Re-buff the whole list every N seconds" casts everything.
void CAutoHelper::AutoBuff()
{
	DWORD currentTick = GetTickCount();

	std::vector<int> validBuffs;
	for (int i = 0; i < MAX_AUTO_BUFFS; i++)
	{
		if (this->m_Config.BuffSkills[i] > 0)
		{
			validBuffs.push_back(this->m_Config.BuffSkills[i]);
		}
	}

	if (validBuffs.empty())
	{
		return;
	}

	DWORD interval = (DWORD)(this->m_Config.BuffInterval > 0 ? this->m_Config.BuffInterval : 0) * 1000;
	bool periodic = (this->m_Config.BuffInterval > 0 && (this->m_LastBuffTick == 0 || (currentTick - this->m_LastBuffTick) >= interval));

	if (!periodic && !this->m_Config.BuffExpired)
	{
		return;
	}

	DWORD heroFlags = *(DWORD*)(Hero + 120);

	// Server Key cua Hero trong MU 0.97k nam o offset +0x1DC
	short heroKey = *(short*)(Hero + 0x1DC);
	if (heroKey == 0) heroKey = *(short*)(Hero + 476);

	int hpPercent = 100;
	if (gPrintPlayer.ViewMaxHP > 0)
	{
		hpPercent = (int)((gPrintPlayer.ViewCurHP * 100) / gPrintPlayer.ViewMaxHP);
	}

	for (size_t i = 0; i < validBuffs.size(); i++)
	{
		int buffSkill = validBuffs[i];

		std::map<int, DWORD>::iterator last = this->m_BuffCastTick.find(buffSkill);
		DWORD sinceLast = (last == this->m_BuffCastTick.end()) ? 0xFFFFFFFF : (currentTick - last->second);

		// The state needs a moment to arrive: no new cast of the same skill before 2.5 seconds
		if (sinceLast < 2500)
		{
			continue;
		}

		DWORD bit = BuffStateBit(buffSkill);
		bool need = false;

		if (this->m_Config.BuffExpired)
		{
			if (buffSkill == 26)
			{
				need = (hpPercent <= 75); // Heal: only when the life is low
			}
			else if (bit != 0)
			{
				need = ((heroFlags & bit) == 0);
			}
			else
			{
				need = (sinceLast >= 30000); // unknown state: every 30 seconds
			}
		}

		if (!need && periodic)
		{
			need = true;
		}

		if (need)
		{
			this->CastBuffOnSelf(buffSkill, heroKey);
			this->m_BuffCastTick[buffSkill] = currentTick;
		}
	}

	if (periodic)
	{
		this->m_LastBuffTick = currentTick;
	}
}

bool CAutoHelper::AutoLoot()
{
	DWORD currentTick = GetTickCount();

	// Dang trong thoi gian thuc hien nhat do, giu trang thai looting de khong bi AutoCombat ngat don
	if (currentTick < this->m_LootBusyTimer)
	{
		return true;
	}

	if (currentTick - this->m_LastLootTick < 100)
	{
		return false;
	}

	float heroX = *(float*)(Hero + 16);
	float heroY = *(float*)(Hero + 20);
	// The items fall around the monsters, often outside of the hunting range: they are picked up up to 4 tiles beyond it (8 at least)
	float maxDistance = ((float)this->m_Config.HuntingRange + 4.0f) * 100.0f;
	if (maxDistance < 800.0f) maxDistance = 800.0f;

	int bestItemIndex = -1;
	float bestItemDist = maxDistance + 999.0f;
	float bestItemX = 0;
	float bestItemY = 0;

	for (int i = 0; i < 1000; i++)
	{
		DWORD itemBase = 0x07E127F8 + (516 * i);
		DWORD itemObject = itemBase + 72;

		if (*(BYTE*)(itemObject + 0) && *(BYTE*)(itemObject + 352)) // Live & Visible
		{
			float itemX = *(float*)(itemObject + 16);
			float itemY = *(float*)(itemObject + 20);

			float dx = itemX - heroX;
			float dy = itemY - heroY;
			float dist = sqrt(dx * dx + dy * dy);

			std::map<int, LootTry>::iterator tryIt = this->m_LootTries.find(i);

			if (tryIt != this->m_LootTries.end())
			{
				if (tryIt->second.posX != (int)itemX || tryIt->second.posY != (int)itemY)
				{
					this->m_LootTries.erase(tryIt); // another item took the slot
				}
				else if (currentTick < tryIt->second.blockedUntil)
				{
					continue;
				}
			}

			if (dist <= maxDistance && dist < bestItemDist)
			{
				// The ITEM of a ground item starts 4 bytes in: Type +0, Option1 +27 (excellent bits), SpecialNum +36 (Structs.h)
				DWORD item = itemBase + 4;
				short itemType = *(short*)(item + 0);
				BYTE itemOption1 = *(BYTE*)(item + 27);
				BYTE specialNum = *(BYTE*)(item + 36);

				ITEM_ATTRIBUTE* itemAttr = (ITEM_ATTRIBUTE*)(ItemAttribute + itemType * sizeof(ITEM_ATTRIBUTE));
				const char* itemName = (itemAttr) ? itemAttr->Name : "";

				if (this->ShouldPickItem(itemType, itemOption1, specialNum, itemName))
				{
					bestItemDist = dist;
					bestItemIndex = i;
					bestItemX = itemX;
					bestItemY = itemY;
				}
			}
		}
	}

	if (bestItemIndex == -1)
	{
		return false;
	}

	this->m_LastLootTick = currentTick;

	if (bestItemDist <= 300.0f)
	{
		// An item that is still there after several tries belongs to somebody else (round-robin loot, protection time):
		// it is left alone for 15 seconds instead of being tried again and again
		LootTry& lt = this->m_LootTries[bestItemIndex];

		lt.posX = (int)bestItemX;
		lt.posY = (int)bestItemY;

		if (lt.firstTick == 0 || (currentTick - lt.firstTick) > 10000)
		{
			lt.count = 0;
			lt.firstTick = currentTick;
		}

		lt.count++;

		if (lt.count > 6)
		{
			lt.blockedUntil = currentTick + 15000;
			lt.count = 0;
			lt.firstTick = 0;

			return false;
		}

		this->PickItem(bestItemIndex);
		this->m_LootBusyTimer = currentTick + 400; // Khoa 400ms de Hero hoan tat nhat do
		return true;
	}
	else
	{
		// An item the hero cannot reach in 3 seconds (behind a wall, no path, crowd of monsters: Devil Square) is left alone
		// for 15 seconds, or the hero would stand there trying to walk to it instead of fighting
		LootTry& lt = this->m_LootTries[bestItemIndex];

		if (lt.posX != (int)bestItemX || lt.posY != (int)bestItemY || lt.moveTick == 0 || (currentTick - lt.moveTick) > 10000)
		{
			lt.posX = (int)bestItemX;
			lt.posY = (int)bestItemY;
			lt.moveTick = currentTick;
		}
		else if ((currentTick - lt.moveTick) > 3000)
		{
			lt.blockedUntil = currentTick + 15000;
			lt.moveTick = 0;

			return false;
		}

		// Di chuyen lai gan item
		this->MoveTo((int)(bestItemX / 100.0f), (int)(bestItemY / 100.0f));
		this->m_LootBusyTimer = currentTick + 250;
		return true;
	}
}

bool CAutoHelper::ShouldPickItem(short type, BYTE option1, BYTE specialNum, const char* name)
{
	// Items of the events and of the custom systems that are not worth walking away from: always picked (with the jewels option):
	// Muun egg and evolution stone, Devil Square and Blood Castle items, Jewel of Creation / Guardian ... (see IsJewel)
	if (this->m_Config.PickJewel)
	{
		if (type == GET_ITEM(13, 20) || type == GET_ITEM(13, 30) || type == GET_ITEM(13, 31) || // Muun egg, evolution stone, refine stone
			type == GET_ITEM(13, 16) || type == GET_ITEM(13, 17) ||                 // Archangel scroll, blood bone
			type == GET_ITEM(14, 17) || type == GET_ITEM(14, 18) || type == GET_ITEM(14, 19) || // Devil's Eye, Key, Invitation
			type == GET_ITEM(14, 28) || type == GET_ITEM(14, 29))                    // event / quest items
		{
			return true;
		}
	}

	// Zen / Money
	if (this->m_Config.PickZen && this->IsMoney(type))
	{
		return true;
	}

	// Jewels
	if (this->m_Config.PickJewel && this->IsJewel(type))
	{
		return true;
	}

	// Excellent items: the excellent options are the low bits of Option1. SpecialNum counts every option line (skill, luck, +option
	// too), so it does not tell an excellent item; this version has no ancient items, the box "Hoan hao / Than" is the excellent ones.
	if ((this->m_Config.PickExc || this->m_Config.PickAncient) && GET_ITEM_OPT_EXC(option1) != 0 && type < GET_ITEM(12, 0))
	{
		return true;
	}

	// Extra custom items by keyword
	if (this->m_Config.PickExtra && !this->m_Config.ExtraItems.empty() && name && strlen(name) > 0)
	{
		std::string itemNameLower = name;
		std::transform(itemNameLower.begin(), itemNameLower.end(), itemNameLower.begin(), ::tolower);

		for (size_t i = 0; i < this->m_Config.ExtraItems.size(); i++)
		{
			std::string keywordLower = this->m_Config.ExtraItems[i];
			std::transform(keywordLower.begin(), keywordLower.end(), keywordLower.begin(), ::tolower);

			if (!keywordLower.empty() && itemNameLower.find(keywordLower) != std::string::npos)
			{
				return true;
			}
		}
	}

	return false;
}

bool CAutoHelper::IsJewel(short type)
{
	return (type == 399 || type == 6159 || // Chaos
	        type == 461 || type == 7181 || // Bless
	        type == 462 || type == 7182 || // Soul
	        type == 464 || type == 7184 || // Life
	        type == 470 || type == 7190 || // Creation
	        type == 479 || type == 7199 || // Guardian
	        type == 7209 || type == 7210); // Harmony / Gemstone
}

bool CAutoHelper::IsMoney(short type)
{
	return (type == 463 || type == 7183 || type == 0x1CF);
}

void CAutoHelper::PickItem(int itemIndex)
{
	DWORD itemBase = 0x07E127F8 + (516 * itemIndex);
	DWORD itemObject = itemBase + 72;
	if (itemObject && *(BYTE*)(itemObject + 0) && *(BYTE*)(itemObject + 352))
	{
		*(BYTE*)(Hero + 749) = 1; // MovementType = MOVEMENT_GET
		ItemKey = itemIndex;
		Action(Hero, itemObject);
		*(BYTE*)(Hero + 749) = 0; // MovementType = MOVEMENT_MOVE

		// Gui packet C3:22 PMSG_ITEM_GET_SEND
		PMSG_ITEM_GET_SEND pMsg;
		pMsg.header.setE(0x22, sizeof(pMsg));
		pMsg.index[0] = SET_NUMBERHB(itemIndex);
		pMsg.index[1] = SET_NUMBERLB(itemIndex);
		gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
	}
}

bool CAutoHelper::AutoReturnPos()
{
	DWORD currentTick = GetTickCount();

	// Minimum 2s cooldown regardless of config to avoid spamming MoveTo every frame
	DWORD minCooldown = 2000;
	DWORD configDelay = (DWORD)(this->m_Config.ReturnPosDelay * 1000);
	DWORD cooldown = (configDelay > minCooldown) ? configDelay : minCooldown;

	if (currentTick - this->m_LastReturnPosTick < cooldown)
	{
		return false;
	}

	if (World != this->m_OriginMap)
	{
		return false;
	}

	int currentX = (int)(*(float*)(Hero + 16) / 100.0f);
	int currentY = (int)(*(float*)(Hero + 20) / 100.0f);

	int dx = currentX - this->m_OriginX;
	int dy = currentY - this->m_OriginY;
	int dist = (int)sqrt((float)(dx * dx + dy * dy));

	if (dist <= this->m_Config.HuntingRange)
	{
		return false; // Already within hunting range of origin
	}

	// Do NOT return if there are mobs nearby within attack range
	// (let AutoCombat deal with them first; we'll return once area is clear)
	float heroX = *(float*)(Hero + 16);
	float heroY = *(float*)(Hero + 20);
	float combatRange = (float)this->m_Config.HuntingRange * 100.0f;

	for (int n = 0; n < MAX_MAIN_VIEWPORT; n++)
	{
		DWORD ViewportAddress = *(DWORD*)(0x07ABF5D0) + (n * 916);
		if (!ViewportAddress || *(BYTE*)(ViewportAddress) == 0) continue;
		if (ViewportAddress == Hero) continue;
		if (*(BYTE*)(ViewportAddress + 0x2FD) != 0) continue; // Skip dead monsters

		BYTE kind = *(BYTE*)(ViewportAddress + 0x84);
		if (kind != 2) continue; // monsters only

		short mobKey = *(short*)(ViewportAddress + 0x1DC);
		if (mobKey <= 0 || (WORD)mobKey == 0xFFFF) continue;
		if (this->IsDeadEntity((WORD)mobKey)) continue; // Skip dead monsters

		WORD monsterType = *(WORD*)(ViewportAddress + 0x2EB);
		if (monsterType == 200 || monsterType >= 500) continue;

		// Check health bar - skip dead/dying monsters
		MONSTER_HEALTH_BAR* lpHealthBar = gHealthBar.GetHealthBar((WORD)mobKey, kind);
		if (lpHealthBar && lpHealthBar->rateHP <= 0) continue;

		float mobX = *(float*)(ViewportAddress + 0x10);
		float mobY = *(float*)(ViewportAddress + 0x14);
		float mdx = mobX - heroX;
		float mdy = mobY - heroY;
		float mobDist = sqrt(mdx * mdx + mdy * mdy);

		if (mobDist <= combatRange)
		{
			// Mob is nearby - do not interrupt combat to return
			return false;
		}
	}

	// No mobs nearby and out of origin range - return to origin
	this->MoveTo(this->m_OriginX, this->m_OriginY);
	this->m_LastReturnPosTick = currentTick;
	return true;
}

void CAutoHelper::AutoParty()
{
	if (PartyNumber <= 0 || !Hero)
	{
		return;
	}

	DWORD currentTick = GetTickCount();
	float heroX = *(float*)(Hero + 16);
	float heroY = *(float*)(Hero + 20);

	// 1. Check Party Healing (Elf Heal = 26)
	if (this->m_Config.PartyHeal && (currentTick - this->m_LastBuffTick > 1000))
	{
		for (int i = 0; i < PartyNumber; i++)
		{
			PARTY_t* p = &Party[i];
			if (!p || p->Name[0] == '\0') continue;

			// Skip self
			if (_strnicmp(p->Name, (char*)(Hero + 0x1C1), 10) == 0) continue;

			if (p->MaxLife > 0)
			{
				int memberHP = (p->CurLife * 100) / p->MaxLife;
				if (memberHP <= this->m_Config.PartyHealThreshold)
				{
					// Find member in viewport to get server Key
					for (int n = 0; n < MAX_MAIN_VIEWPORT; n++)
					{
						DWORD c = *(DWORD*)(0x07ABF5D0) + (n * 916);
						if (!c || *(BYTE*)c == 0 || c == Hero) continue;
						if (*(BYTE*)(c + 0x84) != 1) continue; // Player

						if (_strnicmp(p->Name, (char*)(c + 0x1C1), 10) == 0)
						{
							float mx = *(float*)(c + 0x10);
							float my = *(float*)(c + 0x14);
							float dx = mx - heroX;
							float dy = my - heroY;
							if (sqrt(dx * dx + dy * dy) <= 800.0f) // within 8 tiles
							{
								short memberKey = *(short*)(c + 0x1DC);
								gPatchs.SendRequestMagic(26, (int)memberKey); // Heal
								SetAction(Hero, 86); // PLAYER_SKILL_ELF1
								PlayBuffer(118, Hero, FALSE);
								this->m_LastBuffTick = currentTick;
								return;
							}
						}
					}
				}
			}
		}
	}

	// 2. Check Party Buffing (Greater Defense, Greater Damage, Mana Shield, etc.)
	if (this->m_Config.PartySupport)
	{
		DWORD partyInterval = (this->m_Config.PartyBuffInterval > 0) ? (DWORD)(this->m_Config.PartyBuffInterval * 1000) : 30000;

		if (this->m_LastPartyBuffTick != 0 && (currentTick - this->m_LastPartyBuffTick < partyInterval))
		{
			return;
		}

		// Gather valid targeted buff skills configured on Buff panel (e.g. 26 Heal, 27 Greater Defense, 28 Greater Damage, 16 Soul Barrier)
		std::vector<int> partyBuffSkills;
		for (int i = 0; i < MAX_AUTO_BUFFS; i++)
		{
			int sk = this->m_Config.BuffSkills[i];
			if (sk == 27 || sk == 28 || sk == 16) // Targeted party buffs
			{
				partyBuffSkills.push_back(sk);
			}
		}

		// If no specific targeted buff was placed in Buff panel, check class defaults (e.g. Elf has 27, 28)
		BYTE baseClass = this->GetPlayerClass();
		if (partyBuffSkills.empty())
		{
			if (baseClass == 2) // Elf
			{
				partyBuffSkills.push_back(27); // Greater Defense
				partyBuffSkills.push_back(28); // Greater Damage
			}
			else if (baseClass == 0) // DW
			{
				partyBuffSkills.push_back(16); // Soul Barrier
			}
		}

		if (!partyBuffSkills.empty())
		{
			for (int i = 0; i < PartyNumber; i++)
			{
				PARTY_t* p = &Party[i];
				if (!p || p->Name[0] == '\0') continue;

				// Skip self (self is buffed by AutoBuff)
				if (_strnicmp(p->Name, (char*)(Hero + 0x1C1), 10) == 0) continue;

				// Find member in viewport
				for (int n = 0; n < MAX_MAIN_VIEWPORT; n++)
				{
					DWORD c = *(DWORD*)(0x07ABF5D0) + (n * 916);
					if (!c || *(BYTE*)c == 0 || c == Hero) continue;
					if (*(BYTE*)(c + 0x84) != 1) continue; // Player

					if (_strnicmp(p->Name, (char*)(c + 0x1C1), 10) == 0)
					{
						float mx = *(float*)(c + 0x10);
						float my = *(float*)(c + 0x14);
						float dx = mx - heroX;
						float dy = my - heroY;
						if (sqrt(dx * dx + dy * dy) <= 800.0f)
						{
							short memberKey = *(short*)(c + 0x1DC);
							for (size_t b = 0; b < partyBuffSkills.size(); b++)
							{
								int sk = partyBuffSkills[b];
								gPatchs.SendRequestMagic(sk, (int)memberKey);

								if (sk == 16)
								{
									SetPlayerMagic(Hero);
									PlayBuffer(132, Hero, FALSE);
								}
								else
								{
									SetAction(Hero, 86);
									PlayBuffer(118, Hero, FALSE);
								}
							}
						}
						break;
					}
				}
			}

			this->m_LastPartyBuffTick = currentTick;
		}
	}
}

void CAutoHelper::AutoRepair()
{
	DWORD currentTick = GetTickCount();
	if (this->m_LastRepairTick != 0 && (currentTick - this->m_LastRepairTick < 5000)) // Check every 5 seconds
	{
		return;
	}

	this->m_LastRepairTick = currentTick;

	if (!Hero || !CharacterMachine || !ItemAttribute)
	{
		return;
	}

	bool needsRepair = false;

	STRUCT_DECRYPT;

	// Check equipped equipment slots (0 to 11)
	for (int i = 0; i < INVENTORY_WEAR_SIZE; i++)
	{
		// Skip Pet slot 8 unless it is Dark Horse (GET_ITEM(13, 4))
		if (i == 8)
		{
			ITEM* pPet = GetEquipmentItem(8);
			if (!pPet || pPet->Type != GET_ITEM(13, 4))
			{
				continue;
			}
		}

		ITEM* pItem = GetEquipmentItem(i);
		if (!pItem || pItem->Type < 0 || pItem->Type >= MAX_ITEM)
		{
			continue;
		}

		// Skip Arrows, Bolts, and Transformation Rings
		if (pItem->Type == GET_ITEM(4, 7) || pItem->Type == GET_ITEM(4, 15) || pItem->Type == GET_ITEM(13, 10))
		{
			continue;
		}

		ITEM_ATTRIBUTE* pAttr = (ITEM_ATTRIBUTE*)(ItemAttribute + pItem->Type * sizeof(ITEM_ATTRIBUTE));
		if (!pAttr)
		{
			continue;
		}

		WORD maxDur = gItem.myCalcMaxDurability(pItem, pAttr, GET_ITEM_OPT_LEVEL(pItem->Level));
		if (maxDur == 0)
		{
			continue;
		}

		// Check if durability has decreased from max durability
		if (pItem->Durability < maxDur)
		{
			needsRepair = true;
			break;
		}
	}

	STRUCT_ENCRYPT;

	if (needsRepair)
	{
		PMSG_ITEM_REPAIR_SEND pMsg;
		pMsg.header.setE(0x34, sizeof(pMsg));
		pMsg.slot = 0xFF; // Repair All equipped items
		pMsg.type = 0;
		gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
	}
}

void CAutoHelper::UseItem(BYTE slot)
{
	PMSG_ITEM_USE_SEND pMsg;
	pMsg.header.setE(0x26, sizeof(pMsg));
	pMsg.SourceSlot = slot;
	pMsg.TargetSlot = 0xFF;
	pMsg.type = 0;
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

// Range of the skills in tiles (Skill.txt); 0 = the skill does not need a target in front of the hero
static int SkillRangeTiles(int skillId)
{
	switch (skillId)
	{
		case 20: case 21: case 22: case 23: case 41: case 43: case 55:
			return 2;
		case 13: case 19: case 42: case 47:
			return 3;
		case 56:
			return 5;
		case 51:
			return 8;
		case 10: case 14:
			return 0;
	}

	return (skillId > 0) ? 6 : 0;
}

void CAutoHelper::ManualSkillChase()
{
	static DWORD s_LastChase = 0;

	if (this->m_Active || !Hero || SceneFlag != 5 || !CharacterAttribute || !MouseRButton || MouseOnWindow)
	{
		return;
	}

	DWORD now = GetTickCount();

	if ((now - s_LastChase) < 250)
	{
		return;
	}

	int selected = (int)SelectedCharacter;

	if (selected < 0 || selected >= MAX_MAIN_VIEWPORT)
	{
		return;
	}

	DWORD target = CharactersClient + (selected * 916);

	if (*(BYTE*)target == 0 || *(BYTE*)(target + 0x84) != 2 || *(BYTE*)(target + 0x2FD) != 0)
	{
		return; // only living monsters
	}

	// Selected skill: the slot of the skill list, or directly the skill when the list is not used
	int skillId = 0;
	DWORD slot = *(DWORD*)0x07D7809C;

	if (*(BYTE*)0x07D78098 != 0)
	{
		if (slot < 20)
		{
			STRUCT_DECRYPT;
			skillId = *(BYTE*)(CharacterAttribute + 0x57 + slot);
			STRUCT_ENCRYPT;
		}
	}
	else
	{
		skillId = (int)slot;
	}

	int range = SkillRangeTiles(skillId);

	if (range <= 0)
	{
		return;
	}

	float dx = *(float*)(target + 0x10) - *(float*)(Hero + 16);
	float dy = *(float*)(target + 0x14) - *(float*)(Hero + 20);

	if (sqrtf(dx * dx + dy * dy) <= (float)(range * 100))
	{
		return; // in range: the engine casts the skill
	}

	s_LastChase = now;

	this->MoveTo((int)(*(float*)(target + 0x10) / 100.0f), (int)(*(float*)(target + 0x14) / 100.0f));
}

void CAutoHelper::MoveTo(int x, int y)
{
	if (!Hero) return;
	int currentX = (int)(*(float*)(Hero + 16) / 100.0f);
	int currentY = (int)(*(float*)(Hero + 20) / 100.0f);
	if (currentX == x && currentY == y) return;

	if (PathFinding2(currentX, currentY, x, y, (Hero + 0x354), 0.0f))
	{
		SendMove(Hero, Hero);
	}
}

void CAutoHelper::AddExtraItem(const std::string& name)
{
	if (name.empty()) return;
	for (size_t i = 0; i < this->m_Config.ExtraItems.size(); i++)
	{
		if (_stricmp(this->m_Config.ExtraItems[i].c_str(), name.c_str()) == 0)
		{
			return; // already exists
		}
	}
	this->m_Config.ExtraItems.push_back(name);
}

void CAutoHelper::RemoveExtraItem(int index)
{
	if (index >= 0 && index < (int)this->m_Config.ExtraItems.size())
	{
		this->m_Config.ExtraItems.erase(this->m_Config.ExtraItems.begin() + index);
	}
}

BYTE CAutoHelper::GetPlayerClass()
{
	if (CharacterAttribute)
	{
		STRUCT_DECRYPT;
		BYTE rawClass = *(BYTE*)(CharacterAttribute + 0x0B);
		STRUCT_ENCRYPT;
		if (rawClass >= 16)
		{
			return (rawClass >> 4) & 7;
		}
		return rawClass & 7;
	}

	if (Hero)
	{
		BYTE c = *(BYTE*)(Hero + 0x1BC);
		if (c >= 16)
		{
			return (c >> 4) & 7;
		}
		return c & 7;
	}

	return 1; // Default DK
}

std::vector<int> CAutoHelper::GetAvailableSkillsForClass(BYTE classCode)
{
	std::vector<int> list;
	list.push_back(0); // Basic Attack

	BYTE baseClass = (classCode >= 16) ? ((classCode >> 4) & 7) : (classCode & 7);

	if (baseClass == 0) // Dark Wizard / Soul Master
	{
		list.push_back(9);  // Evil Spirit
		list.push_back(10); // Hellfire
		list.push_back(12); // Aqua Beam
		list.push_back(14); // Inferno
		list.push_back(13); // Blast
		list.push_back(11); // Power Wave
		list.push_back(16); // Soul Barrier / Mana Shield
		list.push_back(1);  // Poison
		list.push_back(2);  // Meteorite
		list.push_back(3);  // Lightning
		list.push_back(4);  // Fire Ball
		list.push_back(5);  // Flame
		list.push_back(7);  // Ice
		list.push_back(8);  // Twister
		list.push_back(38); // Decay
		list.push_back(39); // Ice Storm
		list.push_back(40); // Nova
	}
	else if (baseClass == 1) // Dark Knight / Blade Knight
	{
		list.push_back(41); // Twisting Slash
		list.push_back(42); // Rageful Blow
		list.push_back(43); // Death Stab
		list.push_back(48); // Greater Fortitude (Swell Life)
		list.push_back(22); // Cyclone
		list.push_back(23); // Slash
		list.push_back(19); // Falling Slash
		list.push_back(20); // Lunge
		list.push_back(21); // Uppercut
		list.push_back(47); // Impale
	}
	else if (baseClass == 2) // Fairy Elf / Muse Elf
	{
		list.push_back(24); // Triple Shot
		list.push_back(51); // Ice Arrow
		list.push_back(52); // Penetration
		list.push_back(26); // Heal
		list.push_back(27); // Greater Defense
		list.push_back(28); // Greater Damage
	}
	else if (baseClass == 3) // Magic Gladiator
	{
		list.push_back(41); // Twisting Slash
		list.push_back(55); // Fire Slash
		list.push_back(56); // Power Slash
		list.push_back(9);  // Evil Spirit
		list.push_back(10); // Hellfire
		list.push_back(12); // Aqua Beam
		list.push_back(14); // Inferno
		list.push_back(13); // Blast
		list.push_back(11); // Power Wave
		list.push_back(22); // Cyclone
		list.push_back(23); // Slash
		list.push_back(19); // Falling Slash
		list.push_back(20); // Lunge
		list.push_back(21); // Uppercut
		list.push_back(47); // Impale
		list.push_back(1);  // Poison
		list.push_back(2);  // Meteorite
		list.push_back(3);  // Lightning
		list.push_back(4);  // Fire Ball
		list.push_back(5);  // Flame
		list.push_back(7);  // Ice
		list.push_back(8);  // Twister
		list.push_back(38); // Decay
		list.push_back(39); // Ice Storm
		list.push_back(40); // Nova
	}

	return list;
}

std::vector<int> CAutoHelper::GetAvailableBuffSkillsForClass(BYTE rawClass)
{
	std::vector<int> list;
	BYTE baseClass = (rawClass >= 16) ? ((rawClass >> 4) & 7) : (rawClass & 7);

	if (baseClass == 0) // Dark Wizard / Soul Master
	{
		list.push_back(16); // Mana Shield / Soul Barrier (Gong Khien)
	}
	else if (baseClass == 1) // Dark Knight / Blade Knight
	{
		list.push_back(48); // Greater Fortitude (Gong Mau)
	}
	else if (baseClass == 2) // Fairy Elf / Muse Elf
	{
		list.push_back(26); // Heal (Hoi Phuc Mau)
		list.push_back(27); // Greater Defense (Tang Phong Thu)
		list.push_back(28); // Greater Damage (Tang Suc Danh)
	}
	else if (baseClass == 3) // Magic Gladiator
	{
		// MG does not have self buff skills in MU Online
	}
	else if (baseClass == 4) // Dark Lord
	{
		list.push_back(64); // Increase Critical Damage
	}
	else if (baseClass == 5) // Summoner
	{
		list.push_back(217); // Damage Reflection
		list.push_back(218); // Berserker
	}

	return list;
}

std::vector<int> CAutoHelper::GetAvailableAttackSkillsForClass(BYTE rawClass)
{
	std::vector<int> list;
	BYTE baseClass = (rawClass >= 16) ? ((rawClass >> 4) & 7) : (rawClass & 7);

	if (baseClass == 0) // Dark Wizard / Soul Master
	{
		list.push_back(9);  // Evil Spirit
		list.push_back(10); // Hellfire
		list.push_back(12); // Aqua Beam
		list.push_back(14); // Inferno
		list.push_back(13); // Blast
		list.push_back(11); // Power Wave
		list.push_back(1);  // Poison
		list.push_back(2);  // Meteorite
		list.push_back(3);  // Lightning
		list.push_back(4);  // Fire Ball
		list.push_back(5);  // Flame
		list.push_back(7);  // Ice
		list.push_back(8);  // Twister
		list.push_back(38); // Decay
		list.push_back(39); // Ice Storm
		list.push_back(40); // Nova
	}
	else if (baseClass == 1) // Dark Knight / Blade Knight
	{
		list.push_back(41); // Twisting Slash
		list.push_back(42); // Rageful Blow
		list.push_back(43); // Death Stab
		list.push_back(22); // Cyclone
		list.push_back(23); // Slash
		list.push_back(19); // Falling Slash
		list.push_back(20); // Lunge
		list.push_back(21); // Uppercut
		list.push_back(47); // Impale
	}
	else if (baseClass == 2) // Fairy Elf / Muse Elf
	{
		list.push_back(24); // Triple Shot
		list.push_back(51); // Ice Arrow
		list.push_back(52); // Penetration
	}
	else if (baseClass == 3) // Magic Gladiator
	{
		list.push_back(41); // Twisting Slash
		list.push_back(55); // Fire Slash
		list.push_back(56); // Power Slash
		list.push_back(9);  // Evil Spirit
		list.push_back(10); // Hellfire
		list.push_back(12); // Aqua Beam
		list.push_back(14); // Inferno
		list.push_back(13); // Blast
		list.push_back(11); // Power Wave
		list.push_back(22); // Cyclone
		list.push_back(23); // Slash
		list.push_back(19); // Falling Slash
		list.push_back(20); // Lunge
		list.push_back(21); // Uppercut
		list.push_back(47); // Impale
		list.push_back(1);  // Poison
		list.push_back(2);  // Meteorite
		list.push_back(3);  // Lightning
		list.push_back(4);  // Fire Ball
		list.push_back(5);  // Flame
		list.push_back(7);  // Ice
		list.push_back(8);  // Twister
		list.push_back(38); // Decay
		list.push_back(39); // Ice Storm
		list.push_back(40); // Nova
	}

	return list;
}

std::vector<int> CAutoHelper::GetPlayerLearnedSkills()
{
	std::vector<int> list;
	if (SceneFlag != 5 || !Hero || !CharacterAttribute)
	{
		return list;
	}

	STRUCT_DECRYPT;
	BYTE skillCount = *(BYTE*)(CharacterAttribute + 0x56);
	if (skillCount > 60) skillCount = 60;

	for (BYTE i = 0; i < skillCount; i++)
	{
		BYTE skillId = *(BYTE*)(CharacterAttribute + 0x57 + i);
		if (skillId > 0 && skillId != 0xFF)
		{
			if (std::find(list.begin(), list.end(), (int)skillId) == list.end())
			{
				list.push_back((int)skillId);
			}
		}
	}
	STRUCT_ENCRYPT;

	return list;
}

std::vector<int> CAutoHelper::GetLearnedBuffSkills()
{
	std::vector<int> result;
	BYTE baseClass = this->GetPlayerClass();
	std::vector<int> classBuffs = this->GetAvailableBuffSkillsForClass(baseClass);
	std::vector<int> learned = this->GetPlayerLearnedSkills();

	for (size_t i = 0; i < classBuffs.size(); i++)
	{
		int buffId = classBuffs[i];
		if (std::find(learned.begin(), learned.end(), buffId) != learned.end())
		{
			result.push_back(buffId);
		}
	}

	return result;
}

std::vector<int> CAutoHelper::GetLearnedAttackSkills()
{
	std::vector<int> result;
	BYTE baseClass = this->GetPlayerClass();
	std::vector<int> classAttacks = this->GetAvailableAttackSkillsForClass(baseClass);
	std::vector<int> learned = this->GetPlayerLearnedSkills();

	for (size_t i = 0; i < classAttacks.size(); i++)
	{
		int skillId = classAttacks[i];
		if (std::find(learned.begin(), learned.end(), skillId) != learned.end())
		{
			result.push_back(skillId);
		}
	}

	return result;
}

const char* CAutoHelper::GetSkillNameById(int skillId)
{
	switch (skillId)
	{
		case 0: return "Danh Co Ban";
		case 1: return "Phong Doc";
		case 2: return "Thien Thach";
		case 3: return "Phong Set";
		case 4: return "Qua Cau Lua";
		case 5: return "Cot Lua";
		case 7: return "Dong Bang";
		case 8: return "Loc Xoay";
		case 9: return "Trieu Tap Linh Hon";
		case 10: return "Lua Dia Nguc";
		case 11: return "Song Am";
		case 12: return "Luong Nuoc Xanh";
		case 13: return "Blast";
		case 14: return "Vong Tron Lua";
		case 16: return "Khien Mana";
		case 18: return "Phong Thu";
		case 37: return "Toc Do Chay";
		case 19: return "Chem Roi";
		case 20: return "Dam";
		case 21: return "Chem Xoay Len";
		case 22: return "Chem Xoay";
		case 23: return "Chem Anh Sang";
		case 24: return "Lien Hoan";
		case 26: return "Phuc Hoi Mau";
		case 27: return "Tang Phong Thu";
		case 28: return "Tang Tan Cong";
		case 38: return "Mua Doc To";
		case 39: return "Mua Bang Tuyet";
		case 40: return "Sieu Tan Tinh";
		case 41: return "Chem Xoay";
		case 42: return "Dia Chan";
		case 43: return "Dam Gio";
		case 44: return "Huy Hieu Quan Vuong";
		case 47: return "Dam Xoay";
		case 48: return "Tang Luong Mau";
		case 51: return "Mui Ten Bang";
		case 52: return "Xuyen Tam Tien";
		case 55: return "Chem Lua";
		case 56: return "Suc Manh Cu Chem";
		case 60: return "Cuong Luc";
		case 61: return "No Lua";
		case 62: return "Dia Chan";
		case 63: return "Trieu Hoi";
		case 64: return "Tang Sat Thuong Chi Mang";
		case 65: return "Tia Chop Dien";
		case 66: return "Song Luc";
		case 76: return "Stern";
		case 77: return "Giai Choang";
		case 78: return "Tia Mana";
		case 79: return "Don Lua";
		case 214: return "Hut Sinh Luc";
		case 215: return "Set Xich";
		case 216: return "Phan Sat Thuong";
		case 217: return "Thoi Mien";
		case 219: return "Giam Phong Thu";
		case 220: return "Giam Suc Danh";
		case 221: return "Soc Dien";
		case 222: return "Cuong No";
		case 230: return "Sam Set (RF)";
		case 232: return "Don Sat Thu";
		case 233: return "Moc Thu";
		case 234: return "Ao Anh Lien Hoan";
		case 235: return "Mat Toi";
		case 236: return "Tieng Gam Rong";
		case 237: return "Chem Rong";
		case 238: return "Xuyen Giap";
		case 239: return "Tang Mau (RF)";
		case 240: return "Tang Phong Thu (RF)";
		default: return "Chua Chon";
	}
}

void CAutoHelper::GetCharName(char* outName, int maxLen)
{
	if (!outName || maxLen <= 0) return;
	outName[0] = 0;

	if (Hero && *(char*)(Hero + 0x1C1) != '\0')
	{
		strncpy_s(outName, maxLen, (char*)(Hero + 0x1C1), _TRUNCATE);
		return;
	}

	if (CharacterAttribute)
	{
		STRUCT_DECRYPT;
		char temp[16] = { 0 };
		memcpy(temp, (char*)(CharacterAttribute + 0x00), 10);
		STRUCT_ENCRYPT;

		temp[10] = '\0';
		for (int i = 0; i < 10; i++)
		{
			if ((unsigned char)temp[i] < 32 || (unsigned char)temp[i] > 126)
			{
				temp[i] = '\0';
				break;
			}
		}
		strncpy_s(outName, maxLen, temp, _TRUNCATE);
	}
}

// ---- Settings of the characters ----------------------------------------------------------------------------------------
//
// Saved in AutoHelper.dat (not an .ini the players could edit): the text "character TAB key TAB value" lines, scrambled, with a
// checksum. A file that was changed by hand is thrown away, and every value is kept in the limits of the window anyway.
static const char* AUTOHELPER_FILE = ".\\AutoHelper.dat";
static const DWORD AUTOHELPER_MAGIC = 0x31444841; // "AHD1"

static std::map<std::string, std::map<std::string, std::string>> s_AutoStore;
static bool s_AutoStoreLoaded = false;

static DWORD AutoStoreHash(const std::string& text)
{
	DWORD h = 0x811C9DC5;
	const char* salt = "ViePN-auto-7Q3m";

	for (const char* p = salt; *p; p++)
	{
		h = (h ^ (BYTE)*p) * 0x01000193;
	}

	for (size_t i = 0; i < text.size(); i++)
	{
		h = (h ^ (BYTE)text[i]) * 0x01000193;
	}

	return h;
}

static void AutoStoreScramble(std::string& text)
{
	DWORD k = 0x5E1A2C93;

	for (size_t i = 0; i < text.size(); i++)
	{
		k = k * 1103515245 + 12345;
		text[i] ^= (char)(k >> 16);
	}
}

static void AutoStoreLoad()
{
	if (s_AutoStoreLoaded)
	{
		return;
	}

	s_AutoStoreLoaded = true;
	s_AutoStore.clear();

	FILE* fp = NULL;

	if (fopen_s(&fp, AUTOHELPER_FILE, "rb") != 0 || fp == NULL)
	{
		return;
	}

	DWORD head[2] = { 0, 0 };
	std::string text;

	if (fread(head, sizeof(head), 1, fp) == 1 && head[0] == AUTOHELPER_MAGIC)
	{
		char buf[4096];
		size_t n;

		while ((n = fread(buf, 1, sizeof(buf), fp)) > 0 && text.size() < 1024 * 1024)
		{
			text.append(buf, n);
		}
	}

	fclose(fp);

	AutoStoreScramble(text);

	if (text.empty() || AutoStoreHash(text) != head[1])
	{
		return; // edited by hand or broken: the defaults are used
	}

	std::istringstream in(text);
	std::string line;

	while (std::getline(in, line))
	{
		size_t a = line.find('\t');
		size_t b = (a == std::string::npos) ? a : line.find('\t', a + 1);

		if (b != std::string::npos)
		{
			s_AutoStore[line.substr(0, a)][line.substr(a + 1, b - a - 1)] = line.substr(b + 1);
		}
	}
}

static void AutoStoreSave()
{
	std::string text;

	for (auto& sec : s_AutoStore)
	{
		for (auto& kv : sec.second)
		{
			text += sec.first + "\t" + kv.first + "\t" + kv.second + "\n";
		}
	}

	DWORD head[2] = { AUTOHELPER_MAGIC, AutoStoreHash(text) };

	AutoStoreScramble(text);

	FILE* fp = NULL;

	if (fopen_s(&fp, AUTOHELPER_FILE, "wb") != 0 || fp == NULL)
	{
		return;
	}

	fwrite(head, sizeof(head), 1, fp);
	fwrite(text.data(), 1, text.size(), fp);
	fclose(fp);
}

static bool CleanStoreText(const char* text)
{
	for (const char* p = text; *p; p++)
	{
		if (*p == '\t' || *p == '\n' || *p == '\r')
		{
			return false;
		}
	}

	return true;
}

template <typename T> static T ClampValue(T v, T lo, T hi)
{
	return (v < lo) ? lo : ((v > hi) ? hi : v);
}

void CAutoHelper::LoadConfig(const char* characterName)
{
	if (!characterName || strlen(characterName) == 0) return;
	this->m_LoadedCharacter = characterName;

	AutoStoreLoad();

	auto sec = s_AutoStore.find(characterName);

	if (sec != s_AutoStore.end() && sec->second.count("Saved") != 0)
	{
		AutoHelperConfig& c = this->m_Config;
		std::map<std::string, std::string>& kv = sec->second;

		auto getInt = [&](const char* key, int def) -> int
		{
			auto it = kv.find(key);
			return (it == kv.end()) ? def : atoi(it->second.c_str());
		};

		#define LOAD_INT(key, field) c.field = getInt(key, (int)c.field)
		#define LOAD_BOOL(key, field) c.field = (getInt(key, c.field ? 1 : 0) != 0)

		LOAD_INT("HuntingRange", HuntingRange);
		LOAD_INT("AttackSkill0", AttackSkills[0]);
		LOAD_INT("AttackSkill1", AttackSkills[1]);
		LOAD_INT("AttackSkill2", AttackSkills[2]);
		LOAD_INT("AttackMode", AttackMode);
		LOAD_BOOL("AttackMobs", AttackMobs);
		LOAD_BOOL("SelfDefense", SelfDefense);
		LOAD_BOOL("AttackPK", AttackPK);
		LOAD_BOOL("AutoPotionHP", AutoPotionHP);
		LOAD_INT("PotionHPThreshold", PotionHPThreshold);
		LOAD_BOOL("AutoPotionSD", AutoPotionSD);
		LOAD_INT("PotionSDThreshold", PotionSDThreshold);
		LOAD_BOOL("AutoPotionMP", AutoPotionMP);
		LOAD_INT("PotionMPThreshold", PotionMPThreshold);
		LOAD_BOOL("AutoAntidote", AutoAntidote);
		LOAD_INT("BuffSkill0", BuffSkills[0]);
		LOAD_INT("BuffSkill1", BuffSkills[1]);
		LOAD_INT("BuffSkill2", BuffSkills[2]);
		LOAD_BOOL("BuffExpired", BuffExpired);
		LOAD_INT("BuffInterval", BuffInterval);
		LOAD_BOOL("PickZen", PickZen);
		LOAD_BOOL("PickJewel", PickJewel);
		LOAD_BOOL("PickExc", PickExc);
		LOAD_BOOL("PickAncient", PickAncient);
		LOAD_BOOL("PickExtra", PickExtra);
		LOAD_BOOL("ReturnPos", ReturnPos);
		LOAD_INT("ReturnPosDelay", ReturnPosDelay);
		LOAD_BOOL("ReturnHunt", ReturnHunt);
		LOAD_INT("ReturnHuntMin", ReturnHuntMin);
		LOAD_INT("ReturnHuntMax", ReturnHuntMax);
		LOAD_BOOL("RepairEquip", RepairEquip);
		LOAD_BOOL("PartySupport", PartySupport);
		LOAD_BOOL("PartyHeal", PartyHeal);
		LOAD_INT("PartyHealThreshold", PartyHealThreshold);
		LOAD_INT("PartyBuffInterval", PartyBuffInterval);
		LOAD_BOOL("PartyFollowLeader", PartyFollowLeader);
		LOAD_BOOL("PreferBoss", PreferBoss);
		LOAD_BOOL("SkipStuck", SkipStuck);
		LOAD_BOOL("SmartPotion", SmartPotion);
		LOAD_BOOL("StopNoPotion", StopNoPotion);
		LOAD_BOOL("StopInvFull", StopInvFull);
		LOAD_BOOL("StopIdle", StopIdle);
		LOAD_INT("IdleMinutes", IdleMinutes);
		LOAD_BOOL("ShowStats", ShowStats);
		LOAD_BOOL("AutoCombo", AutoCombo);
		LOAD_BOOL("FollowTeleport", FollowTeleport);
		LOAD_BOOL("AlertSound", AlertSound);
		LOAD_BOOL("TownEscape", TownEscape);

		#undef LOAD_INT
		#undef LOAD_BOOL

		c.ExtraItems.clear();

		int extraCount = ClampValue(getInt("ExtraCount", 0), 0, 64);

		for (int i = 0; i < extraCount; i++)
		{
			char key[16];
			sprintf_s(key, "Extra%d", i);

			auto it = kv.find(key);

			if (it != kv.end() && !it->second.empty() && it->second.size() < 64)
			{
				c.ExtraItems.push_back(it->second);
			}
		}

		// The limits of the window (a value out of them can only come from a changed file)
		c.HuntingRange = ClampValue(c.HuntingRange, 1, 8);
		c.AttackMode = ClampValue(c.AttackMode, 0, 1);
		c.PotionHPThreshold = ClampValue(c.PotionHPThreshold, 5, 100);
		c.PotionSDThreshold = ClampValue(c.PotionSDThreshold, 5, 100);
		c.PotionMPThreshold = ClampValue(c.PotionMPThreshold, 5, 100);
		c.PartyHealThreshold = ClampValue(c.PartyHealThreshold, 5, 100);
		c.BuffInterval = ClampValue(c.BuffInterval, 0, 300);
		c.PartyBuffInterval = ClampValue(c.PartyBuffInterval, 0, 300);
		c.ReturnPosDelay = ClampValue(c.ReturnPosDelay, 0, 300);
		c.ReturnHuntMin = ClampValue(c.ReturnHuntMin, 0, 300);
		c.ReturnHuntMax = ClampValue(c.ReturnHuntMax, c.ReturnHuntMin, 300);
		c.IdleMinutes = ClampValue(c.IdleMinutes, 1, 60);
	}

	// Validate and set default skills for the class of the character (only learned skills stay)
	this->ValidateConfigForClass();
}

void CAutoHelper::SaveConfig(const char* characterName)
{
	if (!characterName || strlen(characterName) == 0 || !CleanStoreText(characterName)) return;
	this->m_LoadedCharacter = characterName;

	AutoStoreLoad();

	AutoHelperConfig& c = this->m_Config;
	std::map<std::string, std::string>& kv = s_AutoStore[characterName];

	kv.clear();

	auto putInt = [&](const char* key, int value)
	{
		char text[16];
		sprintf_s(text, "%d", value);
		kv[key] = text;
	};

	putInt("HuntingRange", c.HuntingRange);
	putInt("AttackSkill0", c.AttackSkills[0]);
	putInt("AttackSkill1", c.AttackSkills[1]);
	putInt("AttackSkill2", c.AttackSkills[2]);
	putInt("AttackMode", c.AttackMode);
	putInt("AttackMobs", c.AttackMobs ? 1 : 0);
	putInt("SelfDefense", c.SelfDefense ? 1 : 0);
	putInt("AttackPK", c.AttackPK ? 1 : 0);
	putInt("AutoPotionHP", c.AutoPotionHP ? 1 : 0);
	putInt("PotionHPThreshold", c.PotionHPThreshold);
	putInt("AutoPotionSD", c.AutoPotionSD ? 1 : 0);
	putInt("PotionSDThreshold", c.PotionSDThreshold);
	putInt("AutoPotionMP", c.AutoPotionMP ? 1 : 0);
	putInt("PotionMPThreshold", c.PotionMPThreshold);
	putInt("AutoAntidote", c.AutoAntidote ? 1 : 0);
	putInt("BuffSkill0", c.BuffSkills[0]);
	putInt("BuffSkill1", c.BuffSkills[1]);
	putInt("BuffSkill2", c.BuffSkills[2]);
	putInt("BuffExpired", c.BuffExpired ? 1 : 0);
	putInt("BuffInterval", c.BuffInterval);
	putInt("PickZen", c.PickZen ? 1 : 0);
	putInt("PickJewel", c.PickJewel ? 1 : 0);
	putInt("PickExc", c.PickExc ? 1 : 0);
	putInt("PickAncient", c.PickAncient ? 1 : 0);
	putInt("PickExtra", c.PickExtra ? 1 : 0);
	putInt("ReturnPos", c.ReturnPos ? 1 : 0);
	putInt("ReturnPosDelay", c.ReturnPosDelay);
	putInt("ReturnHunt", c.ReturnHunt ? 1 : 0);
	putInt("ReturnHuntMin", c.ReturnHuntMin);
	putInt("ReturnHuntMax", c.ReturnHuntMax);
	putInt("RepairEquip", c.RepairEquip ? 1 : 0);
	putInt("PartySupport", c.PartySupport ? 1 : 0);
	putInt("PartyHeal", c.PartyHeal ? 1 : 0);
	putInt("PartyHealThreshold", c.PartyHealThreshold);
	putInt("PartyBuffInterval", c.PartyBuffInterval);
	putInt("PartyFollowLeader", c.PartyFollowLeader ? 1 : 0);
	putInt("PreferBoss", c.PreferBoss ? 1 : 0);
	putInt("SkipStuck", c.SkipStuck ? 1 : 0);
	putInt("SmartPotion", c.SmartPotion ? 1 : 0);
	putInt("StopNoPotion", c.StopNoPotion ? 1 : 0);
	putInt("StopInvFull", c.StopInvFull ? 1 : 0);
	putInt("StopIdle", c.StopIdle ? 1 : 0);
	putInt("IdleMinutes", c.IdleMinutes);
	putInt("ShowStats", c.ShowStats ? 1 : 0);
	putInt("AutoCombo", c.AutoCombo ? 1 : 0);
	putInt("FollowTeleport", c.FollowTeleport ? 1 : 0);
	putInt("AlertSound", c.AlertSound ? 1 : 0);
	putInt("TownEscape", c.TownEscape ? 1 : 0);

	int extra = 0;

	for (size_t i = 0; i < c.ExtraItems.size() && extra < 64; i++)
	{
		if (CleanStoreText(c.ExtraItems[i].c_str()))
		{
			char key[16];
			sprintf_s(key, "Extra%d", extra++);
			kv[key] = c.ExtraItems[i];
		}
	}

	putInt("ExtraCount", extra);
	kv["Saved"] = "1";

	AutoStoreSave();
}
