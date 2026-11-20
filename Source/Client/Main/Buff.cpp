#include "stdafx.h"
#include "Buff.h"
#include "LoadModels.h"
#include "Util.h"

CBuff gBuff;

CBuff::CBuff()
{

}

CBuff::~CBuff()
{

}

void CBuff::Init()
{
	SetCompleteHook(0xE9, 0x0043BDE0, &this->InsertBuffPhysicalEffect);

	SetCompleteHook(0xE9, 0x0043C070, &this->ClearBuffPhysicalEffect);
}

void CBuff::AddActiveBuff(int skillId, const char* name, int durationSec, bool isDebuff)
{
	DWORD currentTick = GetTickCount();
	DWORD durMs = (durationSec > 0) ? (DWORD)(durationSec * 1000) : 60000;

	for (size_t i = 0; i < this->m_ActiveBuffs.size(); i++)
	{
		if (this->m_ActiveBuffs[i].skillId == skillId)
		{
			this->m_ActiveBuffs[i].startTick = currentTick;
			this->m_ActiveBuffs[i].durationMs = durMs;
			this->m_ActiveBuffs[i].isDebuff = isDebuff;
			if (name && name[0] != '\0')
			{
				this->m_ActiveBuffs[i].name = name;
			}
			return;
		}
	}

	ActiveBuffInfo info;
	info.skillId = skillId;
	info.name = name ? name : "Buff";
	info.isDebuff = isDebuff;
	info.startTick = currentTick;
	info.durationMs = durMs;
	this->m_ActiveBuffs.push_back(info);
}

void CBuff::RemoveActiveBuff(int skillId)
{
	for (size_t i = 0; i < this->m_ActiveBuffs.size(); i++)
	{
		if (this->m_ActiveBuffs[i].skillId == skillId)
		{
			this->m_ActiveBuffs.erase(this->m_ActiveBuffs.begin() + i);
			return;
		}
	}
}

void CBuff::ClearAll()
{
	this->m_ActiveBuffs.clear();
}

static void RenderBuffTooltip(float anchorX, float anchorY, const char* text, DWORD textColor)
{
	if (!text || text[0] == '\0')
	{
		return;
	}

	HFONT backupFont = (HFONT)GetCurrentObject(m_hFontDC, OBJ_FONT);
	SelectObject(m_hFontDC, g_hFontBold);

	DWORD backupTextColor = SetTextColor;
	DWORD backupBgColor = SetBackgroundTextColor;
	SetBackgroundTextColor = Color4b(0, 0, 0, 0);

	int textX = CenterTextPosX((char*)text, (int)anchorX);
	if (textX < 6) textX = 6;
	int textY = (int)anchorY;

	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	DisableAlphaBlend();
	glColor3f(1.0f, 1.0f, 1.0f);

	SetTextColor = Color4b(0, 0, 0, 255);
	RenderText(textX + 1, textY + 1, (char*)text, 0, RT3_SORT_LEFT, NULL);
	RenderText(textX - 1, textY, (char*)text, 0, RT3_SORT_LEFT, NULL);
	RenderText(textX + 1, textY, (char*)text, 0, RT3_SORT_LEFT, NULL);
	RenderText(textX, textY - 1, (char*)text, 0, RT3_SORT_LEFT, NULL);
	RenderText(textX, textY + 1, (char*)text, 0, RT3_SORT_LEFT, NULL);

	SetTextColor = textColor;
	RenderText(textX, textY, (char*)text, 0, RT3_SORT_LEFT, NULL);

	SelectObject(m_hFontDC, backupFont ? backupFont : g_hFont);
	SetTextColor = backupTextColor;
	SetBackgroundTextColor = backupBgColor;
}

void CBuff::Render()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		this->m_ActiveBuffs.clear();
		return;
	}

	DWORD currentTick = GetTickCount();
	DWORD heroBuffFlags = *(DWORD*)(Hero + 120);

	struct BuffFlagMapping
	{
		int skillId;
		const char* name;
		DWORD stateBit;
		int defaultDurSec;
		bool isDebuff;
	};

	static const BuffFlagMapping s_mappings[] =
	{
		{ 16, "Mana Shield",       STATE_REDUCE_MAGICDEFENSE, 60, false },
		{ 48, "Greater Fortitude", STATE_ADD_LIFE,            60, false },
		{ 28, "Greater Damage",    STATE_ATTACK,              60, false },
		{ 27, "Greater Defense",   STATE_DEFENSE,             60, false },
		{  1, "Poison",            STATE_POISON,              20, true  },
		{  7, "Freeze",            STATE_FREEZE,              20, true  },
	};

	for (size_t m = 0; m < sizeof(s_mappings) / sizeof(s_mappings[0]); m++)
	{
		bool isActiveOnHero = (heroBuffFlags & s_mappings[m].stateBit) != 0;
		bool isInList = false;
		for (size_t b = 0; b < this->m_ActiveBuffs.size(); b++)
		{
			if (this->m_ActiveBuffs[b].skillId == s_mappings[m].skillId)
			{
				isInList = true;
				break;
			}
		}

		if (!isActiveOnHero && isInList)
		{
			this->RemoveActiveBuff(s_mappings[m].skillId);
		}
		else if (isActiveOnHero && !isInList)
		{
			this->AddActiveBuff(s_mappings[m].skillId, s_mappings[m].name, s_mappings[m].defaultDurSec, s_mappings[m].isDebuff);
		}
	}

	// Remove expired buffs
	for (auto it = this->m_ActiveBuffs.begin(); it != this->m_ActiveBuffs.end();)
	{
		if (currentTick - it->startTick >= it->durationMs)
		{
			it = this->m_ActiveBuffs.erase(it);
		}
		else
		{
			++it;
		}
	}

	if (this->m_ActiveBuffs.empty())
	{
		return;
	}

	BITMAP_t* pSkillBmp = gLoadModels.GetBitmap(0x12A);
	if (!pSkillBmp || pSkillBmp->TextureNumber == 0)
	{
		return;
	}

	const float asp = (g_fScreenRate_x > 0.0f) ? (g_fScreenRate_y / g_fScreenRate_x) : 0.75f;
	const int count = (int)this->m_ActiveBuffs.size();
	const float iconH = 21.0f;
	const float iconW = iconH * 0.82f * asp;
	const float gap = 4.0f;

	const float totalW = count * (iconW + gap) - gap;
	const float startX = (640.0f - totalW) * 0.5f;
	const float startY = 46.0f;

	int hoverIdx = -1;
	float hoverX = 0.0f;

	float cursorX = startX;

	for (int i = 0; i < count; i++)
	{
		const float bx = cursorX;
		const float by = startY;
		cursorX += iconW + gap;

		const ActiveBuffInfo& buff = this->m_ActiveBuffs[i];
		const bool hovered = (MouseX >= (int)(bx - 1.0f) && MouseX <= (int)(bx + iconW + 1.0f) && MouseY >= (int)(by - 1.0f) && MouseY <= (int)(by + iconH + 4.0f));
		if (hovered)
		{
			hoverIdx = i;
			hoverX = bx + iconW * 0.5f;
		}

		// Cold Steel outer frame
		EnableAlphaTest(true);
		glColor4f(0.06f, 0.08f, 0.11f, 0.92f);
		RenderColor(bx - 1.2f, by - 1.2f, iconW + 2.4f, iconH + 2.4f);

		if (buff.isDebuff)
		{
			glColor4f(0.88f, 0.30f, 0.36f, hovered ? 1.0f : 0.90f);
		}
		else
		{
			glColor4f(0.20f, 0.80f, 0.95f, hovered ? 1.0f : 0.85f);
		}
		RenderColor(bx - 1.2f, by - 1.2f, iconW + 2.4f, 1.0f);
		RenderColor(bx - 1.2f, by + iconH + 0.2f, iconW + 2.4f, 1.0f);
		RenderColor(bx - 1.2f, by - 1.2f, 1.0f, iconH + 2.4f);
		RenderColor(bx + iconW + 0.2f, by - 1.2f, 1.0f, iconH + 2.4f);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		DisableAlphaBlend();

		// HD skill icon from SkillHD.png (0x12A)
		int sIndex = buff.skillId - 1;
		if (sIndex >= 0)
		{
			int sCol = sIndex % 8;
			int sRow = sIndex / 8;

			float u = (sCol * 32.0f + 7.0f) / 256.0f;
			float v = (sRow * 36.0f + 4.0f) / 256.0f;
			float uWidth = 18.0f / 256.0f;
			float vHeight = 25.0f / 256.0f;

			float rx = bx * g_fScreenRate_x;
			float ry = by * g_fScreenRate_y;
			float rw = iconW * g_fScreenRate_x;
			float rh = iconH * g_fScreenRate_y;

			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, pSkillBmp->TextureNumber);
			EnableAlphaTest(true);
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

			glBegin(GL_QUADS);
			glTexCoord2f(u, v);                    glVertex2f(rx, (float)WindowHeight - ry);
			glTexCoord2f(u + uWidth, v);           glVertex2f(rx + rw, (float)WindowHeight - ry);
			glTexCoord2f(u + uWidth, v + vHeight); glVertex2f(rx + rw, (float)WindowHeight - (ry + rh));
			glTexCoord2f(u, v + vHeight);          glVertex2f(rx, (float)WindowHeight - (ry + rh));
			glEnd();

			DisableAlphaBlend();
		}

		// Cold Steel countdown bar below icon
		DWORD elapsedMs = currentTick - buff.startTick;
		float progress = 0.0f;
		if (buff.durationMs > 0 && elapsedMs < buff.durationMs)
		{
			progress = 1.0f - ((float)elapsedMs / (float)buff.durationMs);
		}
		if (progress < 0.0f) progress = 0.0f;
		if (progress > 1.0f) progress = 1.0f;

		const float barX = bx - 1.0f;
		const float barY = by + iconH + 2.0f;
		const float barW = iconW + 2.0f;
		const float barH = 2.5f;

		EnableAlphaTest(true);
		glColor4f(0.05f, 0.07f, 0.10f, 0.92f);
		RenderColor(barX, barY, barW, barH);
		if (buff.isDebuff)
		{
			glColor4f(0.92f, 0.30f, 0.36f, 1.0f);
		}
		else
		{
			glColor4f(0.18f, 0.85f, 0.96f, 1.0f);
		}
		RenderColor(barX, barY, barW * progress, barH);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		DisableAlphaBlend();
	}

	if (hoverIdx >= 0)
	{
		const ActiveBuffInfo& b = this->m_ActiveBuffs[hoverIdx];
		DWORD remainSec = (currentTick < b.startTick + b.durationMs) ? ((b.startTick + b.durationMs - currentTick) / 1000 + 1) : 0;
		char tip[96];
		sprintf_s(tip, sizeof(tip), "%s (%ds)", b.name.c_str(), (int)remainSec);
		DWORD col = b.isDebuff ? Color4b(255, 110, 120, 255) : Color4b(150, 235, 255, 255);
		RenderBuffTooltip(hoverX, startY + iconH + 8.0f, tip, col);
	}
}

void CBuff::InsertBuffPhysicalEffect(eEffectState buff, DWORD o)
{
	WORD Type = *(WORD*)(o + 2);

	float Position[3], Angle[3], Light[3];

	if (FindEffect(buff, STATE_POISON) != 0)
	{
		AddEffect(*(DWORD*)(o + 120), STATE_POISON);
	}

	if (FindEffect(buff, STATE_FREEZE) != 0)
	{
		AddEffect(*(DWORD*)(o + 120), STATE_FREEZE);
	}

	if (FindEffect(buff, STATE_ATTACK) != 0)
	{
		AddEffect(*(DWORD*)(o + 120), STATE_ATTACK);
	}

	if (FindEffect(buff, STATE_DEFENSE) != 0)
	{
		AddEffect(*(DWORD*)(o + 120), STATE_DEFENSE);

		DeleteJoint(266, o, 4);

		for (int i = 0; i < 5; i++)
		{
			CreateJoint(266, Position, Position, Angle, 4, o, 20.0f, -1, 0);
		}
	}

	if (FindEffect(buff, STATE_ADD_LIFE) != 0)
	{
		if (Type == 390)
		{
			DeleteEffect(1150, o, 1);

			VectorCopy((float*)(o + 16), Position);

			VectorCopy((float*)(o + 28), Angle);

			VectorCopy((float*)(o + 232), Light);

			CreateEffect(1150, Position, Angle, Light, 1, o, -1, 0, 0);
		}

		AddEffect(*(DWORD*)(o + 120), STATE_ADD_LIFE);
	}

	if (FindEffect(buff, STATE_HARDEN) != 0)
	{
		DeleteEffect(190, o, 1);

		VectorCopy((float*)(o + 16), Position);

		VectorCopy((float*)(o + 28), Angle);

		VectorCopy((float*)(o + 232), Light);

		CreateEffect(190, Position, Angle, Light, 1, o, -1, 0, 0);

		Angle[2] += 180.0f;

		CreateEffect(190, Position, Angle, Light, 2, o, -1, 0, 0);

		AddEffect(*(DWORD*)(o + 120), STATE_HARDEN);
	}

	if (FindEffect(buff, STATE_REDUCE_DEFENSE) != 0)
	{
		DeleteEffect(1274, o, 0);

		VectorCopy((float*)(o + 16), Position);

		VectorCopy((float*)(o + 28), Angle);

		Vector(1.0f, 1.0f, 1.0f, Light);

		CreateEffect(1274, Position, Angle, Light, 0, o, -1, 0, 0);

		PlayBuffer(104, o, 0);

		AddEffect(*(DWORD*)(o + 120), STATE_REDUCE_DEFENSE);
	}

	if (FindEffect(buff, STATE_REDUCE_ATTACKDAMAGE) != 0)
	{
		DeleteEffect(1274, o, 3);

		VectorCopy((float*)(o + 16), Position);

		VectorCopy((float*)(o + 28), Angle);

		Vector(1.0f, 1.0f, 1.0f, Light);

		CreateEffect(1274, Position, Angle, Light, 3, o, -1, 0, 0);

		AddEffect(*(DWORD*)(o + 120), STATE_REDUCE_ATTACKDAMAGE);
	}

	if (FindEffect(buff, STATE_REDUCE_MAGICDEFENSE) != 0)
	{
		if (Type != 325)
		{
			PlayBuffer(103, NULL, 0);

			DeleteJoint(266, o, 0);

			VectorCopy((float*)(o + 16), Position);

			VectorCopy((float*)(o + 28), Angle);

			VectorCopy((float*)(o + 232), Light);

			for (int i = 0; i < 5; i++)
			{
				CreateJoint(266, Position, Position, Angle, 0, o, 50.0f, -1, 0);
			}
		}

		AddEffect(*(DWORD*)(o + 120), STATE_REDUCE_MAGICDEFENSE);
	}

	if (FindEffect(buff, STATE_REDUCE_MAGICPOWER) != 0)
	{
		AddEffect(*(DWORD*)(o + 120), STATE_REDUCE_MAGICPOWER);
	}

	// Register Active Buffs for Hero
	if (Hero && o == Hero)
	{
		if (FindEffect(buff, STATE_REDUCE_MAGICDEFENSE) != 0)
		{
			gBuff.AddActiveBuff(16, "Mana Shield", 60, false);
		}
		if (FindEffect(buff, STATE_ADD_LIFE) != 0)
		{
			gBuff.AddActiveBuff(48, "Greater Fortitude", 60, false);
		}
		if (FindEffect(buff, STATE_ATTACK) != 0)
		{
			gBuff.AddActiveBuff(28, "Greater Damage", 60, false);
		}
		if (FindEffect(buff, STATE_DEFENSE) != 0)
		{
			gBuff.AddActiveBuff(27, "Greater Defense", 60, false);
		}
		if (FindEffect(buff, STATE_POISON) != 0)
		{
			gBuff.AddActiveBuff(1, "Poison", 20, true);
		}
		if (FindEffect(buff, STATE_FREEZE) != 0)
		{
			gBuff.AddActiveBuff(7, "Freeze", 20, true);
		}
	}
}

void CBuff::ClearBuffPhysicalEffect(eEffectState buff, DWORD o)
{
	WORD Type = *(WORD*)(o + 2);

	if (FindEffect(buff, STATE_POISON) != 0)
	{
		DelEffect(*(DWORD*)(o + 120), STATE_POISON);
	}

	if (FindEffect(buff, STATE_FREEZE) != 0)
	{
		DelEffect(*(DWORD*)(o + 120), STATE_FREEZE);
	}

	if (FindEffect(buff, STATE_ATTACK) != 0)
	{
		DelEffect(*(DWORD*)(o + 120), STATE_ATTACK);
	}

	if (FindEffect(buff, STATE_DEFENSE) != 0)
	{
		DelEffect(*(DWORD*)(o + 120), STATE_DEFENSE);

		DeleteJoint(266, o, 4);
	}

	if (FindEffect(buff, STATE_ADD_LIFE) != 0)
	{
		DelEffect(*(DWORD*)(o + 120), STATE_ADD_LIFE);

		if (Type == 390)
		{
			DeleteEffect(1150, o, 1);
		}
	}

	if (FindEffect(buff, STATE_HARDEN) != 0)
	{
		DelEffect(*(DWORD*)(o + 120), STATE_HARDEN);
	}

	if (FindEffect(buff, STATE_REDUCE_DEFENSE) != 0)
	{
		DelEffect(*(DWORD*)(o + 120), STATE_REDUCE_DEFENSE);

		DeleteEffect(1274, o, 0);
	}

	if (FindEffect(buff, STATE_REDUCE_ATTACKDAMAGE) != 0)
	{
		DelEffect(*(DWORD*)(o + 120), STATE_REDUCE_ATTACKDAMAGE);

		DeleteEffect(1274, o, 3);
	}

	if (FindEffect(buff, STATE_REDUCE_MAGICDEFENSE) != 0)
	{
		DelEffect(*(DWORD*)(o + 120), STATE_REDUCE_MAGICDEFENSE);

		DeleteJoint(266, o, 0);
	}

	if (FindEffect(buff, STATE_REDUCE_MAGICPOWER) != 0)
	{
		DelEffect(*(DWORD*)(o + 120), STATE_REDUCE_MAGICPOWER);
	}

	// Remove Active Buffs for Hero
	if (Hero && o == Hero)
	{
		if (FindEffect(buff, STATE_REDUCE_MAGICDEFENSE) != 0)
		{
			gBuff.RemoveActiveBuff(16);
		}
		if (FindEffect(buff, STATE_ADD_LIFE) != 0)
		{
			gBuff.RemoveActiveBuff(48);
		}
		if (FindEffect(buff, STATE_ATTACK) != 0)
		{
			gBuff.RemoveActiveBuff(28);
		}
		if (FindEffect(buff, STATE_DEFENSE) != 0)
		{
			gBuff.RemoveActiveBuff(27);
		}
		if (FindEffect(buff, STATE_POISON) != 0)
		{
			gBuff.RemoveActiveBuff(1);
		}
		if (FindEffect(buff, STATE_FREEZE) != 0)
		{
			gBuff.RemoveActiveBuff(7);
		}
	}
}
