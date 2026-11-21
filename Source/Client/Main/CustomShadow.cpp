#include "stdafx.h"
#include "CustomShadow.h"
#include "LoadModels.h"
#include "Offsets.h"
#include "OptionsMenu.h"
#include "Util.h"

CCustomShadow gCustomShadow;

CCustomShadow::CCustomShadow()
{
}

CCustomShadow::~CCustomShadow()
{
}

void CCustomShadow::Init()
{
	// 1. Hook character body parts shadow in RenderCharacter
	// Male player at 0x00456A3E (51 bytes up to 0x00456A71)
	SetCompleteHook(0xE9, 0x00456A3E, &CCustomShadow::HookRenderCharacterShadowMale);
	for (DWORD addr = 0x00456A3E + 5; addr < 0x00456A71; ++addr)
	{
		SetByte(addr, 0x90);
	}

	// Female player / Elf at 0x004578C8 (52 bytes up to 0x004578FC)
	SetCompleteHook(0xE9, 0x004578C8, &CCustomShadow::HookRenderCharacterShadowFemale);
	for (DWORD addr = 0x004578C8 + 5; addr < 0x004578FC; ++addr)
	{
		SetByte(addr, 0x90);
	}

	// 2. Hook calls to RenderLinkObject for in-hand weapons and shields in main.exe
	SetCompleteHook(0xE8, 0x00457380, &CCustomShadow::MyRenderLinkObject);
	SetCompleteHook(0xE8, 0x004573F6, &CCustomShadow::MyRenderLinkObject);
	SetCompleteHook(0xE8, 0x004574B4, &CCustomShadow::MyRenderLinkObject);
	SetCompleteHook(0xE8, 0x004584AC, &CCustomShadow::MyRenderLinkObject);
	SetCompleteHook(0xE8, 0x00458541, &CCustomShadow::MyRenderLinkObject);
	SetCompleteHook(0xE8, 0x004585A0, &CCustomShadow::MyRenderLinkObject);
	SetCompleteHook(0xE8, 0x004585F0, &CCustomShadow::MyRenderLinkObject);
	SetCompleteHook(0xE8, 0x00458CD6, &CCustomShadow::MyRenderLinkObject);
}

_declspec(naked) void CCustomShadow::HookRenderCharacterShadowMale()
{
	static DWORD jmpBack = 0x00456A71;
	_asm
	{
		Pushad;
		Push Ebp; // o (OBJECT*)
		Push Ebx; // c (CHARACTER*)
		Call CCustomShadow::RenderCharacterShadowParts;
		Add Esp, 8;
		Popad;
		Jmp[jmpBack];
	}
}

_declspec(naked) void CCustomShadow::HookRenderCharacterShadowFemale()
{
	static DWORD jmpBack = 0x004578FC;
	_asm
	{
		Pushad;
		Push Ebp; // o (OBJECT*)
		Push Ebx; // c (CHARACTER*)
		Call CCustomShadow::RenderCharacterShadowParts;
		Add Esp, 8;
		Popad;
		Jmp[jmpBack];
	}
}

void CCustomShadow::RenderCharacterShadowParts(DWORD c, DWORD o)
{
	if (gOptionsMenu.GetDeleteShadows())
	{
		return;
	}

	if (World == MAP_ICARUS)
	{
		return;
	}

	if (o == 0 || c == 0)
	{
		return;
	}

	float alpha = *(float*)(o + 0x168);
	if (alpha < 0.2f)
	{
		return;
	}

	// Enable shadow mode on object
	*(BYTE*)(o + 0x8C) = 1;

	// In Blood Castle check (adjust dead height if needed)
	if (World >= MAP_BLOOD_CASTLE1 && World <= MAP_BLOOD_CASTLE6)
	{
		if (*(BYTE*)(o + 0x64) && *(int*)(c + 0x14) > 0)
		{
			float groundH = RequestTerrainHeight(*(float*)(o + 0x10), *(float*)(o + 0x14));
			if (groundH < *(float*)(o + 0x18))
			{
				*(float*)(o + 0x18) = groundH;
			}
		}
	}

	// Loop through all 5 body parts: Helm (0), Armor (1), Pants (2), Gloves (3), Boots (4)
	for (int i = 4; i >= 0; --i)
	{
		DWORD pPart = c + 0x1E0 + (i * 24);
		short sType = *(short*)pPart;

		if (sType != -1)
		{
			RenderPartObject(o, sType, (void*)pPart, (float*)(c + 0x320), alpha, 0, 0, false, false, true, 0, 2);
		}
		else
		{
			RenderPartObject(o, *(short*)(o + 2), NULL, (float*)(c + 0x320), alpha, 0, 0, false, false, true, 0, 2);
		}
	}

	// Restore shadow mode flag
	*(BYTE*)(o + 0x8C) = 0;
}

void CCustomShadow::MyRenderLinkObject(float x, float y, float z, DWORD c, DWORD f, int Type, int Level, int Option1, bool Link, bool Translate, int RenderType)
{
	// 1. Call original RenderLinkObject to draw the 3D model
	((void(_cdecl*)(float, float, float, DWORD, DWORD, int, int, int, bool, bool, int))0x00455430)(x, y, z, c, f, Type, Level, Option1, Link, Translate, RenderType);

	// 2. Render shadow for linked items (wings, weapons, shields)
	if (gOptionsMenu.GetDeleteShadows())
	{
		return;
	}

	if (World == MAP_ICARUS)
	{
		return;
	}

	if (Type < 0 || Type >= TOTAL_MODELS)
	{
		return;
	}

	// Candidate check: Wings, Weapons & Shields
	bool isCandidate = false;

	if (Type >= MODEL_WING && Type < MODEL_HELPER)
	{
		isCandidate = true;
	}
	else if (Type >= MODEL_SWORD && Type < MODEL_HELM)
	{
		isCandidate = true;
	}

	if (!isCandidate)
	{
		return;
	}

	DWORD b = (DWORD)(gLoadModels.GetModels() + (0xBC * Type));
	if (b == 0)
	{
		return;
	}

	if (*(short*)(b + 0x24) <= 0) // NumMesh <= 0
	{
		return;
	}

	DWORD o = f ? f : c;
	if (o != 0 && c != 0)
	{
		float posX = *(float*)(c + 0x10);
		float posY = *(float*)(c + 0x14);
		float groundZ = RequestTerrainHeight(posX, posY);

		*(float*)(b + 0x6C) = posX;
		*(float*)(b + 0x70) = posY;
		*(float*)(b + 0x74) = groundZ;

		float alpha = *(float*)(c + 0x168);
		if (alpha > 1.0f) alpha = 1.0f;
		if (alpha < 0.2f) return;

		EnableAlphaTest(true);
		glColor4f(0.0f, 0.0f, 0.0f, 0.25f * alpha);

		_asm
		{
			push -1;
			push -1;
			mov ecx, b;
			mov eax, 0x00441F00;
			call eax;
		}

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	}
}
