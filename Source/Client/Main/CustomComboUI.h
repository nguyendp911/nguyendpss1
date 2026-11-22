#pragma once

#include "ProtocolDefines.h"

//**********************************************//
//************ Custom Combo Packets ************//
//**********************************************//

struct PMSG_CUSTOM_COMBO_TRIGGER_SEND
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

struct COMBO_TEX_INFO
{
	GLuint id;
	int width;
	int height;
	float maxU;
	float maxV;
};

struct COMBO_BURST_INFO
{
	bool active;
	DWORD startTick;
	DWORD duration;
	vec3_t pos;
	vec3_t angle;
	BYTE userClass;
};

//**********************************************//
//************ Custom Combo UI Class ***********//
//**********************************************//

class CCustomComboUI
{
public:
	CCustomComboUI();
	virtual ~CCustomComboUI();

	void Init();
	void LoadImages();
	void Render();
	void UpdateMouse();

	bool IsActive();
	DWORD GetActiveRemainSec();
	DWORD GetCooldownRemainSec();

	void ToggleCombo();
	void SendComboTrigger();
	void SendPKModeState();
	void GCComboStateRecv(PMSG_CUSTOM_COMBO_STATE_SEND* lpMsg);
	void GCComboEffectRecv(PMSG_CUSTOM_COMBO_EFFECT_SEND* lpMsg);
	void PlayComboVisual(int targetIndex, int userIndex);

	bool IsPKModeActive();
	void TogglePKMode();
	void SetPKMode(bool active);

	bool IsRecentComboDamage(float pos[3]);

	void RenderWorldEffects();
	void CheckCharacterChange();

private:
	void LoadComboTextures();
	bool LoadSkillOzjTexture(const char* filename, COMBO_TEX_INFO* pTex);
	void RenderCustomTooltip(float anchorX, float anchorY, const char* text, DWORD textColor);

private:
	char m_LoadedCharacter[32];
	bool m_Active;
	DWORD m_ActiveEndTick;
	DWORD m_ActiveTotalMs;
	DWORD m_CooldownEndTick;
	DWORD m_CooldownTotalMs;

	bool m_PKMode;

	DWORD m_LastComboDamageTick;
	vec3_t m_LastComboPos;

	GLuint m_TexComboCS;
	COMBO_TEX_INFO m_TexComboAura;
	COMBO_TEX_INFO m_TexComboCircle;
	COMBO_TEX_INFO m_TexCombo2;
	COMBO_TEX_INFO m_TexCombo3;
	COMBO_TEX_INFO m_TexComboFlare;

	COMBO_BURST_INFO m_Bursts[16];
};

extern CCustomComboUI gCustomComboUI;
