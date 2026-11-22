#include "stdafx.h"
#include "Duel.h"
#include "CommandManager.h"
#include "DSProtocol.h"
#include "EffectManager.h"
#include "ItemManager.h"
#include "Map.h"
#include "Notice.h"
#include "ObjectManager.h"
#include "Party.h"
#include "Protocol.h"
#include "ServerInfo.h"
#include "Util.h"

CDuel gDuel;

CDuel::CDuel()
{
}

CDuel::~CDuel()
{
}

void CDuel::MainProc()
{
	DWORD currentTick = GetTickCount();

	for (int n = OBJECT_START_USER; n < MAX_OBJECT; n++)
	{
		if (gObjIsConnectedGP(n) == 0)
		{
			continue;
		}

		LPOBJ lpObj = &gObj[n];

		// 1. Check Duel Request Timeout (20 seconds)
		if (lpObj->DuelRequestedBy != -1)
		{
			if ((currentTick - lpObj->DuelRequestTick) >= DUEL_REQUEST_TIMEOUT)
			{
				lpObj->DuelRequestedBy = -1;
				gNotice.GCNoticeSend(n, 1, "[DUEL] Duel challenge request expired.");
			}
		}

		// 2. Check active duel validity
		if (lpObj->DuelUser != -1)
		{
			// 5-second grace period after duel start for map teleport
			if ((currentTick - lpObj->DuelTick) < 5000)
			{
				continue;
			}

			int targetIndex = lpObj->DuelUser;

			if (OBJECT_USER_RANGE(targetIndex) == 0 || gObjIsConnectedGP(targetIndex) == 0)
			{
				this->CancelDuel(n);
				continue;
			}

			LPOBJ lpTarget = &gObj[targetIndex];

			if (lpTarget->DuelUser != n || lpObj->Map != lpTarget->Map || lpObj->Map != MAP_ARENA)
			{
				this->CancelDuel(n);
				continue;
			}
		}
	}
}

bool CDuel::RequestDuel(int aIndex, int bIndex)
{
	BYTE emptySlots[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
	return this->RequestDuelWithSetup(aIndex, bIndex, DUEL_RULE_NONE, 0, emptySlots);
}

bool CDuel::RequestDuelWithSetup(int aIndex, int bIndex, BYTE rules, BYTE betCount, BYTE* betSlots)
{
	if (gObjIsConnectedGP(aIndex) == 0 || gObjIsConnectedGP(bIndex) == 0)
	{
		return false;
	}

	LPOBJ lpObj = &gObj[aIndex];
	LPOBJ lpTarget = &gObj[bIndex];

	if (lpObj->Type != OBJECT_USER || lpTarget->Type != OBJECT_USER)
	{
		return false;
	}

	if (aIndex == bIndex)
	{
		gNotice.GCNoticeSend(aIndex, 1, "[DUEL] You cannot duel yourself!");
		return false;
	}

	if (lpObj->DuelUser != -1)
	{
		gNotice.GCNoticeSend(aIndex, 1, "[DUEL] You are already in an active duel!");
		return false;
	}

	if (lpTarget->DuelUser != -1)
	{
		gNotice.GCNoticeSend(aIndex, 1, "[DUEL] Target player is already in an active duel!");
		return false;
	}

	if (BC_MAP_RANGE(lpObj->Map) || DS_MAP_RANGE(lpObj->Map) || BC_MAP_RANGE(lpTarget->Map) || DS_MAP_RANGE(lpTarget->Map))
	{
		gNotice.GCNoticeSend(aIndex, 1, "[DUEL] Cannot start a duel inside Blood Castle or Devil Square!");
		return false;
	}

	if (lpTarget->DuelRequestedBy != -1 && (GetTickCount() - lpTarget->DuelRequestTick) < DUEL_REQUEST_TIMEOUT)
	{
		gNotice.GCNoticeSend(aIndex, 1, "[DUEL] Target player already has a pending duel challenge!");
		return false;
	}

	if (lpTarget->Interface.use != 0)
	{
		gNotice.GCNoticeSend(aIndex, 1, "[DUEL] Target player is currently busy!");
		return false;
	}

	if ((lpTarget->Option & 1) == 0)
	{
		gNotice.GCNoticeSend(aIndex, 1, "[DUEL] Target player has requests disabled.");
		return false;
	}

	if (betCount > 4) betCount = 4;
	BYTE validBetCount = 0;
	DUEL_BET_SLOT betItems[4];
	DUEL_BET_ITEM_INFO betInfos[4];
	memset(betItems, 0, sizeof(betItems));
	memset(betInfos, 0, sizeof(betInfos));

	if (betSlots)
	{
		for (BYTE i = 0; i < betCount; i++)
		{
			BYTE slot = betSlots[i];
			if (INVENTORY_MAIN_RANGE(slot) != 0)
			{
				// Prevent duplicate slot selection
				bool dup = false;
				for (BYTE k = 0; k < validBetCount; k++)
				{
					if (betItems[k].slot == slot) { dup = true; break; }
				}
				if (dup) continue;

				CItem* pItem = &lpObj->Inventory[slot];
				if (pItem->IsItem())
				{
					betItems[validBetCount].slot = slot;
					betItems[validBetCount].index = (WORD)pItem->m_Index;
					betItems[validBetCount].level = (BYTE)pItem->m_Level;
					betItems[validBetCount].serial = pItem->m_Serial;

					betInfos[validBetCount].index = (WORD)pItem->m_Index;
					betInfos[validBetCount].level = (BYTE)pItem->m_Level;
					betInfos[validBetCount].dur = (BYTE)pItem->m_Durability;
					betInfos[validBetCount].skill = (BYTE)pItem->m_SkillOption;
					betInfos[validBetCount].luck = (BYTE)pItem->m_LuckOption;
					betInfos[validBetCount].add = (BYTE)pItem->m_AddOption;
					betInfos[validBetCount].exce = (BYTE)pItem->m_ExceOption;
					validBetCount++;
				}
			}
		}
	}

	lpObj->DuelPendingRules = rules;
	lpObj->DuelPendingBetCount = validBetCount;
	memcpy(lpObj->DuelPendingBetItems, betItems, sizeof(betItems));

	lpTarget->DuelRequestedBy = aIndex;
	lpTarget->DuelRequestTick = GetTickCount();
	lpTarget->DuelPendingRules = rules;
	lpTarget->DuelPendingBetCount = validBetCount;

	gNotice.GCNoticeSend(aIndex, 1, "[DUEL] Challenge sent to [%s]! Waiting for response...", lpTarget->Name);
	gNotice.GCNoticeSend(bIndex, 1, "[DUEL] [%s] challenged you to a PvP Duel! Accept or type /okduel.", lpObj->Name);

	PMSG_DUEL_REQUEST_RECV pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.header.set(0xAA, 0x01, sizeof(pMsg));
	pMsg.index[0] = SET_NUMBERHB(aIndex);
	pMsg.index[1] = SET_NUMBERLB(aIndex);
	memcpy(pMsg.name, lpObj->Name, sizeof(pMsg.name));
	pMsg.rules = rules;
	pMsg.betItemCount = validBetCount;
	memcpy(pMsg.betItems, betInfos, sizeof(betInfos));

	DataSend(bIndex, (BYTE*)&pMsg, pMsg.header.size);
	return true;
}

void CDuel::CGDuelRequestRecv(PMSG_DUEL_REQUEST_SEND* lpMsg, int aIndex)
{
	int bIndex = MAKE_NUMBERW(lpMsg->index[0], lpMsg->index[1]);
	this->RequestDuelWithSetup(aIndex, bIndex, lpMsg->rules, lpMsg->betItemCount, lpMsg->betSlots);
}

void CDuel::CGDuelAnswerRecv(PMSG_DUEL_ANSWER_RECV* lpMsg, int aIndex)
{
	if (gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if (lpObj->DuelRequestedBy == -1)
	{
		return;
	}

	int challengerIndex = lpObj->DuelRequestedBy;

	if ((GetTickCount() - lpObj->DuelRequestTick) >= DUEL_REQUEST_TIMEOUT)
	{
		lpObj->DuelRequestedBy = -1;
		gNotice.GCNoticeSend(aIndex, 1, "[DUEL] Duel challenge request expired.");
		return;
	}

	if (lpMsg->result == 0)
	{
		lpObj->DuelRequestedBy = -1;

		if (gObjIsConnectedGP(challengerIndex) != 0)
		{
			gNotice.GCNoticeSend(challengerIndex, 1, "[DUEL] [%s] declined your duel challenge.", lpObj->Name);
		}

		gNotice.GCNoticeSend(aIndex, 1, "[DUEL] You declined the duel challenge.");
		return;
	}

	if (gObjIsConnectedGP(challengerIndex) == 0)
	{
		lpObj->DuelRequestedBy = -1;
		gNotice.GCNoticeSend(aIndex, 1, "[DUEL] Challenger is no longer online.");
		return;
	}

	LPOBJ lpChallenger = &gObj[challengerIndex];

	if (lpChallenger->DuelUser != -1)
	{
		lpObj->DuelRequestedBy = -1;
		gNotice.GCNoticeSend(aIndex, 1, "[DUEL] Challenger is already in another duel.");
		return;
	}

	// Process Target's wager items
	if (lpMsg->betItemCount > 4) lpMsg->betItemCount = 4;
	BYTE validTargetBetCount = 0;
	DUEL_BET_SLOT targetBetItems[4];
	memset(targetBetItems, 0, sizeof(targetBetItems));

	for (BYTE i = 0; i < lpMsg->betItemCount; i++)
	{
		BYTE slot = lpMsg->betSlots[i];
		if (INVENTORY_MAIN_RANGE(slot) != 0)
		{
			bool dup = false;
			for (BYTE k = 0; k < validTargetBetCount; k++)
			{
				if (targetBetItems[k].slot == slot) { dup = true; break; }
			}
			if (dup) continue;

			CItem* pItem = &lpObj->Inventory[slot];
			if (pItem->IsItem())
			{
				targetBetItems[validTargetBetCount].slot = slot;
				targetBetItems[validTargetBetCount].index = (WORD)pItem->m_Index;
				targetBetItems[validTargetBetCount].level = (BYTE)pItem->m_Level;
				targetBetItems[validTargetBetCount].serial = pItem->m_Serial;
				validTargetBetCount++;
			}
		}
	}

	// Copy pending rules and wager items to active duel session
	lpChallenger->DuelRules = lpChallenger->DuelPendingRules;
	lpChallenger->DuelBetCount = lpChallenger->DuelPendingBetCount;
	memcpy(lpChallenger->DuelBetItems, lpChallenger->DuelPendingBetItems, sizeof(lpChallenger->DuelBetItems));
	lpChallenger->DuelIsChallenger = true;

	lpObj->DuelRules = lpChallenger->DuelPendingRules;
	lpObj->DuelBetCount = validTargetBetCount;
	memcpy(lpObj->DuelBetItems, targetBetItems, sizeof(lpObj->DuelBetItems));
	lpObj->DuelIsChallenger = false;

	this->StartDuel(challengerIndex, aIndex);
}

void CDuel::StartDuel(int aIndex, int bIndex)
{
	if (gObjIsConnectedGP(aIndex) == 0 || gObjIsConnectedGP(bIndex) == 0)
	{
		return;
	}

	LPOBJ lpObjA = &gObj[aIndex];
	LPOBJ lpObjB = &gObj[bIndex];

	// If duelists are in the same party, remove from party so PvP works cleanly
	if (lpObjA->PartyNumber >= 0 && lpObjA->PartyNumber == lpObjB->PartyNumber)
	{
		gParty.DelMember(lpObjA->PartyNumber, bIndex);
	}

	lpObjA->DuelUser = bIndex;
	lpObjA->DuelScore = 0;
	lpObjA->DuelRequestedBy = -1;
	lpObjA->DuelTick = GetTickCount();

	lpObjB->DuelUser = aIndex;
	lpObjB->DuelScore = 0;
	lpObjB->DuelRequestedBy = -1;
	lpObjB->DuelTick = GetTickCount();

	// Teleport both players into Stadium PvP Duel Ring (MAP_ARENA, monster-free X:56..68, Y:150..170)
	gObjTeleport(aIndex, MAP_ARENA, 62, 152);
	gObjTeleport(bIndex, MAP_ARENA, 62, 168);

	lpObjA->State = OBJECT_PLAYING;
	lpObjB->State = OBJECT_PLAYING;
	lpObjA->Teleport = 0;
	lpObjB->Teleport = 0;

	// Clear existing buffs on entering Duel Arena for fair fight
	gEffectManager.ClearAllEffect(lpObjA);
	gEffectManager.ClearAllEffect(lpObjB);
	gObjectManager.CharacterCalcAttribute(aIndex);
	gObjectManager.CharacterCalcAttribute(bIndex);

	lpObjA->Life = lpObjA->MaxLife + lpObjA->AddLife;
	lpObjA->Mana = lpObjA->MaxMana + lpObjA->AddMana;
	lpObjA->BP = lpObjA->MaxBP + lpObjA->AddBP;
	GCLifeSend(aIndex, 0xFF, (int)lpObjA->Life);
	GCManaSend(aIndex, 0xFF, (int)lpObjA->Mana, lpObjA->BP);

	lpObjB->Life = lpObjB->MaxLife + lpObjB->AddLife;
	lpObjB->Mana = lpObjB->MaxMana + lpObjB->AddMana;
	lpObjB->BP = lpObjB->MaxBP + lpObjB->AddBP;
	GCLifeSend(bIndex, 0xFF, (int)lpObjB->Life);
	GCManaSend(bIndex, 0xFF, (int)lpObjB->Mana, lpObjB->BP);

	gNotice.GCNoticeSend(aIndex, 1, "[DUEL] Fight! [%s] vs [%s] - First to %d kills wins!", lpObjA->Name, lpObjB->Name, MAX_DUEL_SCORE);
	gNotice.GCNoticeSend(bIndex, 1, "[DUEL] Fight! [%s] vs [%s] - First to %d kills wins!", lpObjA->Name, lpObjB->Name, MAX_DUEL_SCORE);

	char szRules[128] = "[DUEL] Rules:";
	if (lpObjA->DuelRules & DUEL_RULE_NO_HP) strcat_s(szRules, " [No HP Pot]");
	if (lpObjA->DuelRules & DUEL_RULE_NO_BUFF) strcat_s(szRules, " [No Buff]");
	if (lpObjA->DuelRules & DUEL_RULE_NO_MANA) strcat_s(szRules, " [No Mana Pot]");
	if (lpObjA->DuelRules == 0) strcat_s(szRules, " [Free PvP]");
	gNotice.GCNoticeSend(aIndex, 1, szRules);
	gNotice.GCNoticeSend(bIndex, 1, szRules);

	PMSG_DUEL_START_RECV pMsgA;
	memset(&pMsgA, 0, sizeof(pMsgA));
	pMsgA.header.set(0xAA, 0x03, sizeof(pMsgA));
	pMsgA.index[0] = SET_NUMBERHB(bIndex);
	pMsgA.index[1] = SET_NUMBERLB(bIndex);
	memcpy(pMsgA.name, lpObjB->Name, sizeof(pMsgA.name));
	pMsgA.rules = lpObjA->DuelRules;
	DataSend(aIndex, (BYTE*)&pMsgA, pMsgA.header.size);

	PMSG_DUEL_START_RECV pMsgB;
	memset(&pMsgB, 0, sizeof(pMsgB));
	pMsgB.header.set(0xAA, 0x03, sizeof(pMsgB));
	pMsgB.index[0] = SET_NUMBERHB(aIndex);
	pMsgB.index[1] = SET_NUMBERLB(aIndex);
	memcpy(pMsgB.name, lpObjA->Name, sizeof(pMsgB.name));
	pMsgB.rules = lpObjB->DuelRules;
	DataSend(bIndex, (BYTE*)&pMsgB, pMsgB.header.size);

	PMSG_DUEL_SCORE_RECV pScore;
	pScore.header.set(0xAA, 0x05, sizeof(pScore));
	pScore.myScore = 0;
	pScore.targetScore = 0;
	DataSend(aIndex, (BYTE*)&pScore, pScore.header.size);
	DataSend(bIndex, (BYTE*)&pScore, pScore.header.size);
}

void CDuel::CheckDuelScore(LPOBJ lpObj, LPOBJ lpTarget)
{
	if (!lpObj || !lpTarget) return;
	if (lpObj->DuelUser != lpTarget->Index || lpTarget->DuelUser != lpObj->Index) return;

	lpTarget->KillerType = 3;
	lpObj->DuelScore++;

	gNotice.GCNoticeSend(lpObj->Index, 1, "[DUEL] Score: %s (%d) - (%d) %s", lpObj->Name, lpObj->DuelScore, lpTarget->DuelScore, lpTarget->Name);
	gNotice.GCNoticeSend(lpTarget->Index, 1, "[DUEL] Score: %s (%d) - (%d) %s", lpObj->Name, lpObj->DuelScore, lpTarget->DuelScore, lpTarget->Name);

	PMSG_DUEL_SCORE_RECV pScoreKiller;
	pScoreKiller.header.set(0xAA, 0x05, sizeof(pScoreKiller));
	pScoreKiller.myScore = (BYTE)lpObj->DuelScore;
	pScoreKiller.targetScore = (BYTE)lpTarget->DuelScore;
	DataSend(lpObj->Index, (BYTE*)&pScoreKiller, pScoreKiller.header.size);

	PMSG_DUEL_SCORE_RECV pScoreVictim;
	pScoreVictim.header.set(0xAA, 0x05, sizeof(pScoreVictim));
	pScoreVictim.myScore = (BYTE)lpTarget->DuelScore;
	pScoreVictim.targetScore = (BYTE)lpObj->DuelScore;
	DataSend(lpTarget->Index, (BYTE*)&pScoreVictim, pScoreVictim.header.size);

	if (lpObj->DuelScore >= MAX_DUEL_SCORE)
	{
		gNotice.GCNoticeSendToAll(0, "[DUEL] [%s] defeated [%s] in PvP Duel (%d - %d)!", lpObj->Name, lpTarget->Name, lpObj->DuelScore, lpTarget->DuelScore);
		this->EndDuel(lpObj->Index, lpTarget->Index, 1);
	}
}

void CDuel::RespawnDuelUser(LPOBJ lpObj)
{
	if (!lpObj || lpObj->DuelUser == -1) return;

	int targetIndex = lpObj->DuelUser;
	LPOBJ lpTarget = (OBJECT_USER_RANGE(targetIndex) && gObjIsConnectedGP(targetIndex) != 0) ? &gObj[targetIndex] : NULL;

	// Random spot inside Stadium Duel Ring (X: 56..68, Y: 150..170)
	short spawnX = 62;
	short spawnY = 152;

	for (int n = 0; n < 100; n++)
	{
		short randX = 56 + (GetLargeRand() % 13);
		short randY = 150 + (GetLargeRand() % 21);

		if (gMap[MAP_ARENA].CheckAttr(randX, randY, 4) == 0 && gMap[MAP_ARENA].CheckAttr(randX, randY, 8) == 0)
		{
			if (lpTarget != NULL)
			{
				int dist = abs(randX - lpTarget->X) + abs(randY - lpTarget->Y);
				if (dist < 6 && n < 80) continue;
			}
			spawnX = randX;
			spawnY = randY;
			break;
		}
	}

	lpObj->Map = MAP_ARENA;
	lpObj->X = spawnX;
	lpObj->Y = spawnY;
	lpObj->Life = lpObj->MaxLife + lpObj->AddLife;
	lpObj->Mana = lpObj->MaxMana + lpObj->AddMana;
	lpObj->BP = lpObj->MaxBP + lpObj->AddBP;

	GCLifeSend(lpObj->Index, 0xFF, (int)lpObj->Life);
	GCManaSend(lpObj->Index, 0xFF, (int)lpObj->Mana, lpObj->BP);

	if (lpTarget != NULL)
	{
		lpTarget->Life = lpTarget->MaxLife + lpTarget->AddLife;
		lpTarget->Mana = lpTarget->MaxMana + lpTarget->AddMana;
		lpTarget->BP = lpTarget->MaxBP + lpTarget->AddBP;
		GCLifeSend(lpTarget->Index, 0xFF, (int)lpTarget->Life);
		GCManaSend(lpTarget->Index, 0xFF, (int)lpTarget->Mana, lpTarget->BP);
	}
}

void CDuel::EndDuel(int aIndex, int bIndex, int reason)
{
	LPOBJ lpObjA = (OBJECT_USER_RANGE(aIndex) && gObjIsConnectedGP(aIndex) != 0) ? &gObj[aIndex] : NULL;
	LPOBJ lpObjB = (OBJECT_USER_RANGE(bIndex) && gObjIsConnectedGP(bIndex) != 0) ? &gObj[bIndex] : NULL;

	if (lpObjA && lpObjB && reason == 1)
	{
		// Transfer loser's wagered items to winner
		if (lpObjB->DuelBetCount > 0)
		{
			int transferred = 0;
			for (BYTE i = 0; i < lpObjB->DuelBetCount; i++)
			{
				DUEL_BET_SLOT bet = lpObjB->DuelBetItems[i];
				int foundSlot = -1;

				if (INVENTORY_MAIN_RANGE(bet.slot) != 0 &&
					lpObjB->Inventory[bet.slot].IsItem() &&
					lpObjB->Inventory[bet.slot].m_Index == bet.index &&
					lpObjB->Inventory[bet.slot].m_Level == bet.level &&
					(bet.serial == 0 || lpObjB->Inventory[bet.slot].m_Serial == bet.serial))
				{
					foundSlot = bet.slot;
				}
				else
				{
					for (int s = INVENTORY_WEAR_SIZE; s < INVENTORY_SIZE; s++)
					{
						if (lpObjB->Inventory[s].IsItem() &&
							lpObjB->Inventory[s].m_Index == bet.index &&
							lpObjB->Inventory[s].m_Level == bet.level &&
							(bet.serial == 0 || lpObjB->Inventory[s].m_Serial == bet.serial))
						{
							foundSlot = s;
							break;
						}
					}
				}

				if (foundSlot != -1)
				{
					CItem betItem = lpObjB->Inventory[foundSlot];
					gItemManager.InventoryDelItem(bIndex, foundSlot);
					gItemManager.GCItemDeleteSend(bIndex, foundSlot, 1);

					BYTE newSlot = gItemManager.InventoryInsertItem(aIndex, betItem);
					if (newSlot != 0xFF)
					{
						gItemManager.GCItemModifySend(aIndex, newSlot);
						transferred++;
					}
					else
					{
						gMap[lpObjA->Map].ItemDrop(betItem.m_Index, betItem.m_Level, betItem.m_Durability,
							lpObjA->X, lpObjA->Y,
							betItem.m_SkillOption, betItem.m_LuckOption, betItem.m_AddOption, betItem.m_ExceOption,
							betItem.m_Serial, aIndex);
						transferred++;
					}
				}
			}

			gObjInventoryCommit(aIndex);
			gItemManager.GCItemListSend(aIndex);
			GDCharacterInfoSaveSend(aIndex);

			gObjInventoryCommit(bIndex);
			gItemManager.GCItemListSend(bIndex);
			GDCharacterInfoSaveSend(bIndex);

			if (transferred > 0)
			{
				gNotice.GCNoticeSendToAll(0, "[DUEL] [%s] won %d wagered item(s) from [%s]!", lpObjA->Name, transferred, lpObjB->Name);
			}
		}

		if (lpObjA->DuelBetCount > 0)
		{
			gNotice.GCNoticeSend(aIndex, 1, "[DUEL] You successfully defended your %d wagered item(s)!", lpObjA->DuelBetCount);
		}
	}

	if (lpObjA)
	{
		PMSG_DUEL_END_RECV pMsg;
		memset(&pMsg, 0, sizeof(pMsg));
		pMsg.header.set(0xAA, 0x04, sizeof(pMsg));
		pMsg.result = (BYTE)reason;
		if (lpObjB)
		{
			memcpy(pMsg.winner, lpObjA->Name, sizeof(pMsg.winner));
			memcpy(pMsg.loser, lpObjB->Name, sizeof(pMsg.loser));
		}
		DataSend(aIndex, (BYTE*)&pMsg, pMsg.header.size);

		lpObjA->DuelUser = -1;
		lpObjA->DuelScore = 0;
		lpObjA->DuelRequestedBy = -1;
		lpObjA->DuelRules = 0;
		lpObjA->DuelBetCount = 0;
		memset(lpObjA->DuelBetItems, 0, sizeof(lpObjA->DuelBetItems));
		lpObjA->DuelIsChallenger = false;

		gObjTeleport(aIndex, MAP_LORENCIA, 125, 125);
	}

	if (lpObjB)
	{
		PMSG_DUEL_END_RECV pMsg;
		memset(&pMsg, 0, sizeof(pMsg));
		pMsg.header.set(0xAA, 0x04, sizeof(pMsg));
		pMsg.result = (BYTE)(reason == 1 ? 2 : 0);
		if (lpObjA)
		{
			memcpy(pMsg.winner, lpObjA->Name, sizeof(pMsg.winner));
			memcpy(pMsg.loser, lpObjB->Name, sizeof(pMsg.loser));
		}
		DataSend(bIndex, (BYTE*)&pMsg, pMsg.header.size);

		lpObjB->DuelUser = -1;
		lpObjB->DuelScore = 0;
		lpObjB->DuelRequestedBy = -1;
		lpObjB->DuelRules = 0;
		lpObjB->DuelBetCount = 0;
		memset(lpObjB->DuelBetItems, 0, sizeof(lpObjB->DuelBetItems));
		lpObjB->DuelIsChallenger = false;

		gObjTeleport(bIndex, MAP_LORENCIA, 125, 125);
	}
}

void CDuel::CancelDuel(int aIndex)
{
	if (OBJECT_USER_RANGE(aIndex) == 0) return;
	this->ResetDuel(&gObj[aIndex]);
}

void CDuel::ResetDuel(LPOBJ lpObj)
{
	if (!lpObj) return;

	if (lpObj->DuelUser != -1)
	{
		int targetIndex = lpObj->DuelUser;

		if (OBJECT_USER_RANGE(targetIndex) && gObjIsConnectedGP(targetIndex) != 0)
		{
			LPOBJ lpTarget = &gObj[targetIndex];
			if (lpTarget->DuelUser == lpObj->Index)
			{
				lpTarget->DuelUser = -1;
				lpTarget->DuelScore = 0;
				lpTarget->DuelRequestedBy = -1;
				lpTarget->DuelRules = 0;
				lpTarget->DuelBetCount = 0;
				memset(lpTarget->DuelBetItems, 0, sizeof(lpTarget->DuelBetItems));
				lpTarget->DuelIsChallenger = false;
				lpTarget->DuelPendingRules = 0;
				lpTarget->DuelPendingBetCount = 0;
				memset(lpTarget->DuelPendingBetItems, 0, sizeof(lpTarget->DuelPendingBetItems));

				PMSG_DUEL_END_RECV pMsg;
				memset(&pMsg, 0, sizeof(pMsg));
				pMsg.header.set(0xAA, 0x04, sizeof(pMsg));
				pMsg.result = 0;
				memcpy(pMsg.winner, lpTarget->Name, sizeof(pMsg.winner));
				memcpy(pMsg.loser, lpObj->Name, sizeof(pMsg.loser));
				DataSend(targetIndex, (BYTE*)&pMsg, pMsg.header.size);

				gNotice.GCNoticeSend(targetIndex, 1, "[DUEL] Duel ended because opponent left the match.");
				gObjTeleport(targetIndex, MAP_LORENCIA, 125, 125);
			}
		}
	}

	if (gObjIsConnectedGP(lpObj->Index) != 0 && lpObj->DuelUser != -1)
	{
		PMSG_DUEL_END_RECV pMsg;
		memset(&pMsg, 0, sizeof(pMsg));
		pMsg.header.set(0xAA, 0x04, sizeof(pMsg));
		pMsg.result = 0;
		DataSend(lpObj->Index, (BYTE*)&pMsg, pMsg.header.size);
	}

	lpObj->DuelUser = -1;
	lpObj->DuelScore = 0;
	lpObj->DuelRequestedBy = -1;
	lpObj->DuelRules = 0;
	lpObj->DuelBetCount = 0;
	memset(lpObj->DuelBetItems, 0, sizeof(lpObj->DuelBetItems));
	lpObj->DuelIsChallenger = false;
	lpObj->DuelPendingRules = 0;
	lpObj->DuelPendingBetCount = 0;
	memset(lpObj->DuelPendingBetItems, 0, sizeof(lpObj->DuelPendingBetItems));
}

bool CDuel::CheckDuel(LPOBJ lpObj, LPOBJ lpTarget)
{
	if (!lpObj || !lpTarget) return false;
	if (lpObj->Type != OBJECT_USER || lpTarget->Type != OBJECT_USER) return false;
	return (lpObj->DuelUser != -1 && lpObj->DuelUser == lpTarget->Index && lpTarget->DuelUser == lpObj->Index);
}

void CDuel::CommandDuel(LPOBJ lpObj, char* arg)
{
	if (!lpObj) return;

	char targetName[11] = { 0 };
	gCommandManager.GetString(arg, targetName, sizeof(targetName), 0);

	if (targetName[0] == '\0')
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, "[DUEL] Usage: /duel <PlayerName>  (or press D -> Duel PvP)");
		return;
	}

	for (int n = OBJECT_START_USER; n < MAX_OBJECT; n++)
	{
		if (gObjIsConnectedGP(n) != 0 && _stricmp(gObj[n].Name, targetName) == 0)
		{
			if (n == lpObj->Index)
			{
				gNotice.GCNoticeSend(lpObj->Index, 1, "[DUEL] You cannot duel yourself!");
				return;
			}

			PMSG_DUEL_OPEN_SETUP_SEND pSetup;
			memset(&pSetup, 0, sizeof(pSetup));
			pSetup.header.set(0xAA, 0x06, sizeof(pSetup));
			pSetup.index[0] = SET_NUMBERHB(n);
			pSetup.index[1] = SET_NUMBERLB(n);
			memcpy(pSetup.name, gObj[n].Name, sizeof(pSetup.name));
			DataSend(lpObj->Index, (BYTE*)&pSetup, pSetup.header.size);
			return;
		}
	}

	gNotice.GCNoticeSend(lpObj->Index, 1, "[DUEL] Player [%s] not found online!", targetName);
}

void CDuel::CommandDuelAccept(LPOBJ lpObj)
{
	if (!lpObj) return;
	PMSG_DUEL_ANSWER_RECV pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.result = 1;
	this->CGDuelAnswerRecv(&pMsg, lpObj->Index);
}

void CDuel::CommandDuelRefuse(LPOBJ lpObj)
{
	if (!lpObj) return;
	if (lpObj->DuelUser != -1)
	{
		this->CancelDuel(lpObj->Index);
		gNotice.GCNoticeSend(lpObj->Index, 1, "[DUEL] You forfeited the duel.");
		return;
	}
	PMSG_DUEL_ANSWER_RECV pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.result = 0;
	this->CGDuelAnswerRecv(&pMsg, lpObj->Index);
}
