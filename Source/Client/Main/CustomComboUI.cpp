#include "stdafx.h"
#include "CustomComboUI.h"
#include "Interface.h"
#include "Protocol.h"
#include "PrintPlayer.h"
#include "stb.h"
#include "ComboMeshData.h"
#include <math.h>
#include <vector>

CCustomComboUI gCustomComboUI;

static BYTE DecodeCharClass(DWORD charPtr)
{
	if (!charPtr)
	{
		return 1;
	}
	BYTE c = *(BYTE*)(charPtr + 0x1BC);
	return (c >= 16) ? ((c >> 4) & 7) : (c & 7);
}

static void RenderComboWorldEffectsHook()
{
	((void(*)())0x0046BBA0)();
	gCustomComboUI.RenderWorldEffects();
}

static void DrawComboMeshSpikes()
{
	glBegin(GL_TRIANGLES);
	for (int tri = 0; tri < 28; tri++)
	{
		for (int k = 0; k < 3; k++)
		{
			short vi = s_ComboTriangles[tri].v[k];
			short ti = s_ComboTriangles[tri].t[k];
			glTexCoord2f(s_ComboTexCoords[ti][0], s_ComboTexCoords[ti][1]);
			glVertex3f(s_ComboVertices[vi][0], s_ComboVertices[vi][1], s_ComboVertices[vi][2]);
		}
	}
	glEnd();
}

static void DrawComboSlot(GLuint texId, int slot, float x, float y, float w, float h, float r, float g, float b, float alpha)
{
	if (texId == 0)
	{
		return;
	}

	const float u0 = (float)slot * 0.25f;
	const float u1 = u0 + 0.25f;

	const float sx = x * g_fScreenRate_x;
	const float sy = (float)WindowHeight - y * g_fScreenRate_y;
	const float sw = w * g_fScreenRate_x;
	const float sh = h * g_fScreenRate_y;

	EnableAlphaTest(true);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texId);
	glColor4f(r, g, b, alpha);

	glBegin(GL_QUADS);
	glTexCoord2f(u0, 0.0f); glVertex2f(sx, sy);
	glTexCoord2f(u1, 0.0f); glVertex2f(sx + sw, sy);
	glTexCoord2f(u1, 1.0f); glVertex2f(sx + sw, sy - sh);
	glTexCoord2f(u0, 1.0f); glVertex2f(sx, sy - sh);
	glEnd();

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

static bool InComboRect(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static void GetComboButtonLayout(float* btnW, float* btnH, float* comboX, float* pkX, float* y)
{
	const float asp = (g_fScreenRate_x > 0.0f) ? (g_fScreenRate_y / g_fScreenRate_x) : 0.75f;
	*btnH = 22.0f;
	*btnW = (*btnH) * asp;
	*y = 8.0f;
	// Combo is pinned at slot 2 (after Camera 0,1); PK is collapsible at slot 6 (after Auto 3,4 and JewelBank 5)
	*comboX = 8.0f + (*btnW + 3.0f) * 2.0f;
	*pkX = 8.0f + (*btnW + 3.0f) * 6.0f;
}

CCustomComboUI::CCustomComboUI()
{
	this->m_LoadedCharacter[0] = 0;
	this->m_Active = false;
	this->m_ActiveEndTick = 0;
	this->m_ActiveTotalMs = 0;
	this->m_CooldownEndTick = 0;
	this->m_CooldownTotalMs = 0;
	this->m_PKMode = false;
	this->m_LastComboDamageTick = 0;
	this->m_LastComboPos[0] = this->m_LastComboPos[1] = this->m_LastComboPos[2] = 0.0f;
	this->m_TexComboCS = 0;
	memset(&this->m_TexComboAura, 0, sizeof(this->m_TexComboAura));
	memset(&this->m_TexComboCircle, 0, sizeof(this->m_TexComboCircle));
	memset(&this->m_TexCombo2, 0, sizeof(this->m_TexCombo2));
	memset(&this->m_TexCombo3, 0, sizeof(this->m_TexCombo3));
	memset(&this->m_TexComboFlare, 0, sizeof(this->m_TexComboFlare));
	memset(this->m_Bursts, 0, sizeof(this->m_Bursts));
}

CCustomComboUI::~CCustomComboUI()
{
}

void CCustomComboUI::Init()
{
	SetCompleteHook(0xE8, 0x00525C55, &RenderComboWorldEffectsHook);
}

void CCustomComboUI::ToggleCombo()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		return;
	}

	DWORD curTick = GetTickCount();

	if (this->m_Active && curTick < this->m_ActiveEndTick)
	{
		CreateNotice("[Combo] Combo dang duoc kich hoat!", 1);
		return;
	}

	if (curTick < this->m_CooldownEndTick)
	{
		int sec = (this->m_CooldownEndTick - curTick) / 1000 + 1;
		char msg[80];
		sprintf_s(msg, sizeof(msg), "[Combo] Dang trong thoi gian hoi chieu! Con %d giay.", sec);
		CreateNotice(msg, 1);
		return;
	}

	PlayBuffer(25, 0, 0);
	this->SendComboTrigger();
}

bool CCustomComboUI::IsPKModeActive()
{
	return this->m_PKMode;
}

void CCustomComboUI::SetPKMode(bool active)
{
	if (this->m_PKMode != active)
	{
		this->m_PKMode = active;
		this->SendPKModeState();
	}
}

void CCustomComboUI::TogglePKMode()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		return;
	}

	this->m_PKMode = !this->m_PKMode;
	this->SendPKModeState();
	PlayBuffer(25, 0, 0);

	if (this->m_PKMode)
	{
		CreateNotice("[PK Mode] DA BAT! Danh truc tiep nguoi choi. Combo bat buoc 3 Skill.", 1);
	}
	else
	{
		CreateNotice("[PK Mode] DA TAT! Can giu phim Ctrl. Combo danh quai 1 Skill.", 1);
	}
}

void CCustomComboUI::SendPKModeState()
{
	if (SceneFlag != MAIN_SCENE || !Hero || !g_bGameServerConnected)
	{
		return;
	}

	PMSG_CUSTOM_COMBO_PKMODE_SEND pMsg;
	pMsg.header.set(0xF3, 0xEC, sizeof(pMsg));
	pMsg.pkMode = this->m_PKMode ? 1 : 0;
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

bool CCustomComboUI::IsRecentComboDamage(float pos[3])
{
	if (GetTickCount() - this->m_LastComboDamageTick > 400)
	{
		return false;
	}

	if (pos == NULL)
	{
		return true;
	}

	float dx = pos[0] - this->m_LastComboPos[0];
	float dy = pos[1] - this->m_LastComboPos[1];
	return ((dx * dx + dy * dy) < (280.0f * 280.0f));
}

void CCustomComboUI::LoadImages()
{
	if (this->m_TexComboCS != 0)
	{
		return;
	}

	int w = 0, h = 0, ch = 0;
	unsigned char* pixels = stbi_load("Data\\Interface\\ComboCS.png", &w, &h, &ch, 4);
	if (!pixels || w <= 0 || h <= 0)
	{
		if (pixels) stbi_image_free(pixels);
		return;
	}

	GLuint texId = 0;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);

	int mw = w, mh = h;
	unsigned char* cur = pixels;
	unsigned char* nextBuf = NULL;

	for (int level = 0; ; level++)
	{
		glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, mw, mh, 0, GL_RGBA, GL_UNSIGNED_BYTE, cur);
		if (mw == 1 && mh == 1) break;

		int nw = (mw > 1) ? (mw / 2) : 1;
		int nh = (mh > 1) ? (mh / 2) : 1;
		unsigned char* dst = (unsigned char*)malloc((size_t)nw * nh * 4);
		if (!dst) break;

		for (int y = 0; y < nh; y++)
		{
			int sy0 = y * 2;
			int sy1 = (sy0 + 1 < mh) ? (sy0 + 1) : sy0;
			for (int x = 0; x < nw; x++)
			{
				int sx0 = x * 2;
				int sx1 = (sx0 + 1 < mw) ? (sx0 + 1) : sx0;
				const unsigned char* p00 = cur + (sy0 * mw + sx0) * 4;
				const unsigned char* p10 = cur + (sy0 * mw + sx1) * 4;
				const unsigned char* p01 = cur + (sy1 * mw + sx0) * 4;
				const unsigned char* p11 = cur + (sy1 * mw + sx1) * 4;
				unsigned char* out = dst + (y * nw + x) * 4;
				for (int c = 0; c < 4; c++)
				{
					out[c] = (unsigned char)(((int)p00[c] + p10[c] + p01[c] + p11[c] + 2) >> 2);
				}
			}
		}

		if (nextBuf) free(nextBuf);
		nextBuf = dst;
		cur = dst;
		mw = nw;
		mh = nh;
	}

	if (nextBuf) free(nextBuf);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	stbi_image_free(pixels);
	this->m_TexComboCS = texId;
}

void CCustomComboUI::RenderCustomTooltip(float anchorX, float anchorY, const char* text, DWORD textColor)
{
	if (text == NULL || text[0] == '\0')
	{
		return;
	}

	HFONT backupFont = (HFONT)GetCurrentObject(m_hFontDC, OBJ_FONT);
	SelectObject(m_hFontDC, g_hFontBold);

	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	DisableAlphaBlend();
	glColor3f(1.0f, 1.0f, 1.0f);

	DWORD backupTextColor = SetTextColor;
	DWORD backupBgColor = SetBackgroundTextColor;
	SetBackgroundTextColor = Color4b(0, 0, 0, 0);

	int textX = CenterTextPosX((char*)text, (int)anchorX);
	if (textX < 6) textX = 6;
	int textY = (int)anchorY;

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

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	DisableAlphaBlend();
}

bool CCustomComboUI::IsActive()
{
	return (this->m_Active && GetTickCount() < this->m_ActiveEndTick);
}

DWORD CCustomComboUI::GetActiveRemainSec()
{
	DWORD curTick = GetTickCount();
	if (this->m_Active && curTick < this->m_ActiveEndTick)
	{
		return (this->m_ActiveEndTick - curTick) / 1000 + 1;
	}
	return 0;
}

DWORD CCustomComboUI::GetCooldownRemainSec()
{
	DWORD curTick = GetTickCount();
	if (curTick < this->m_CooldownEndTick)
	{
		return (this->m_CooldownEndTick - curTick) / 1000 + 1;
	}
	return 0;
}

void CCustomComboUI::Render()
{
	this->CheckCharacterChange();
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		return;
	}

	if (this->m_TexComboCS == 0)
	{
		this->LoadImages();
	}

	float btnW, btnH, x, pkX, y;
	GetComboButtonLayout(&btnW, &btnH, &x, &pkX, &y);

	DWORD curTick = GetTickCount();
	bool isActive = (this->m_Active && curTick < this->m_ActiveEndTick);
	bool isCooldown = (!isActive && curTick < this->m_CooldownEndTick);
	bool isHover = InComboRect(x, y, btnW, btnH);

	float scale = isHover ? 1.06f : 1.0f;
	float rw = btnW * scale;
	float rh = btnH * scale;
	float rx = x - (rw - btnW) * 0.5f;
	float ry = y - (rh - btnH) * 0.5f;

	// 1. Combo Button (Slot 0 = Ready cyan, Slot 1 = Active gold)
	if (this->m_TexComboCS != 0)
	{
		if (isActive)
		{
			float pulse = 0.88f + 0.12f * sinf((float)curTick * 0.008f);
			DrawComboSlot(this->m_TexComboCS, 1, rx, ry, rw, rh, 1.0f, pulse, 0.85f, 1.0f);
		}
		else if (isCooldown)
		{
			DrawComboSlot(this->m_TexComboCS, 0, rx, ry, rw, rh, 0.55f, 0.58f, 0.65f, 0.78f);
		}
		else
		{
			DrawComboSlot(this->m_TexComboCS, 0, rx, ry, rw, rh, 1.0f, 1.0f, 1.0f, isHover ? 1.0f : 0.90f);
		}
	}

	// Progress / Cooldown bar right under Combo button
	if (isActive || isCooldown)
	{
		float progress = 0.0f;
		if (isActive && this->m_ActiveTotalMs > 0 && curTick < this->m_ActiveEndTick)
		{
			progress = (float)(this->m_ActiveEndTick - curTick) / (float)this->m_ActiveTotalMs;
		}
		else if (isCooldown && this->m_CooldownTotalMs > 0 && curTick < this->m_CooldownEndTick)
		{
			progress = (float)(this->m_CooldownEndTick - curTick) / (float)this->m_CooldownTotalMs;
		}
		if (progress < 0.0f) progress = 0.0f;
		if (progress > 1.0f) progress = 1.0f;

		float barW = btnW - 2.0f;
		float barH = 2.5f;
		float barX = x + 1.0f;
		float barY = y + btnH + 2.0f;

		EnableAlphaTest(true);
		glColor4f(0.06f, 0.08f, 0.10f, 0.90f);
		RenderColor(barX, barY, barW, barH);
		if (isActive)
		{
			glColor4f(1.0f, 0.84f, 0.25f, 1.0f);
		}
		else
		{
			glColor4f(0.20f, 0.72f, 0.88f, 0.95f);
		}
		RenderColor(barX, barY, barW * progress, barH);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		DisableAlphaBlend();
	}

	// 2. PK Mode Button (collapsible slot 6: Slot 2 = OFF steel swords, Slot 3 = ON crimson swords)
	const bool showPkBtn = !HudIconBarCollapsed();
	bool isPkActive = this->IsPKModeActive();
	bool isPkHover = showPkBtn && InComboRect(pkX, y, btnW, btnH);

	if (showPkBtn && this->m_TexComboCS != 0)
	{
		float pkScale = isPkHover ? 1.06f : 1.0f;
		float pkrw = btnW * pkScale;
		float pkrh = btnH * pkScale;
		float pkrx = pkX - (pkrw - btnW) * 0.5f;
		float pkry = y - (pkrh - btnH) * 0.5f;

		if (isPkActive)
		{
			float pulse = 0.82f + 0.18f * sinf((float)curTick * 0.010f);
			DrawComboSlot(this->m_TexComboCS, 3, pkrx, pkry, pkrw, pkrh, 1.0f, pulse, pulse, 1.0f);
		}
		else
		{
			DrawComboSlot(this->m_TexComboCS, 2, pkrx, pkry, pkrw, pkrh, 1.0f, 1.0f, 1.0f, isPkHover ? 1.0f : 0.90f);
		}
	}

	// 3. Tooltips
	if (isHover)
	{
		char tooltip[80] = { 0 };
		if (isActive)
		{
			int sec = (this->m_ActiveEndTick - curTick) / 1000 + 1;
			sprintf_s(tooltip, sizeof(tooltip), "Combo: Dang Bat (%ds) [Shift+C]", sec);
			this->RenderCustomTooltip(x + btnW * 0.5f, y + btnH + 9.0f, tooltip, Color4b(255, 230, 80, 255));
		}
		else if (isCooldown)
		{
			int sec = (this->m_CooldownEndTick - curTick) / 1000 + 1;
			sprintf_s(tooltip, sizeof(tooltip), "Combo: Hoi Chieu (%ds)", sec);
			this->RenderCustomTooltip(x + btnW * 0.5f, y + btnH + 9.0f, tooltip, Color4b(140, 210, 240, 255));
		}
		else
		{
			sprintf_s(tooltip, sizeof(tooltip), "Combo All Class [Shift+C]");
			this->RenderCustomTooltip(x + btnW * 0.5f, y + btnH + 5.0f, tooltip, Color4b(235, 245, 250, 255));
		}
	}
	else if (isPkHover)
	{
		if (isPkActive)
		{
			this->RenderCustomTooltip(pkX + btnW * 0.5f, y + btnH + 5.0f, "PK: DANG BAT [Shift+P]", Color4b(255, 95, 105, 255));
		}
		else
		{
			this->RenderCustomTooltip(pkX + btnW * 0.5f, y + btnH + 5.0f, "PK: TAT [Shift+P]", Color4b(225, 235, 240, 255));
		}
	}

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	EnableAlphaTest(true);
	DisableAlphaBlend();
}

void CCustomComboUI::UpdateMouse()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		return;
	}

	float btnW, btnH, x, pkX, y;
	GetComboButtonLayout(&btnW, &btnH, &x, &pkX, &y);

	if (InComboRect(x, y, btnW, btnH))
	{
		MouseOnWindow = true;
		if (MouseLButtonPush)
		{
			MouseLButtonPush = false;
			MouseLButton = false;
			MouseLButtonPop = false;
			this->ToggleCombo();
			return;
		}
	}

	if (!HudIconBarCollapsed() && InComboRect(pkX, y, btnW, btnH))
	{
		MouseOnWindow = true;
		if (MouseLButtonPush)
		{
			MouseLButtonPush = false;
			MouseLButton = false;
			MouseLButtonPop = false;
			this->TogglePKMode();
			return;
		}
	}
}

void CCustomComboUI::SendComboTrigger()
{
	PMSG_CUSTOM_COMBO_TRIGGER_SEND pMsg;
	pMsg.header.set(0xF3, 0xEA, sizeof(pMsg));
	gProtocol.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CCustomComboUI::GCComboStateRecv(PMSG_CUSTOM_COMBO_STATE_SEND* lpMsg)
{
	this->m_Active = (lpMsg->active != 0);

	DWORD curTick = GetTickCount();

	if (this->m_Active)
	{
		this->m_ActiveEndTick = curTick + lpMsg->activeRemainMs;
		if (lpMsg->activeRemainMs > this->m_ActiveTotalMs || this->m_ActiveTotalMs == 0)
		{
			this->m_ActiveTotalMs = lpMsg->activeRemainMs;
		}
	}
	else
	{
		this->m_ActiveEndTick = 0;
		this->m_ActiveTotalMs = 0;
	}

	if (lpMsg->cooldownRemainMs > 0)
	{
		this->m_CooldownEndTick = curTick + lpMsg->cooldownRemainMs;
		if (lpMsg->cooldownRemainMs > this->m_CooldownTotalMs || this->m_CooldownTotalMs == 0)
		{
			this->m_CooldownTotalMs = lpMsg->cooldownRemainMs;
		}
	}
	else
	{
		this->m_CooldownEndTick = 0;
		this->m_CooldownTotalMs = 0;
	}

	this->SendPKModeState();
}

void CCustomComboUI::LoadComboTextures()
{
	this->LoadSkillOzjTexture("Magic_Ground2.OZJ", &this->m_TexComboAura);
	if (this->m_TexComboAura.id == 0)
	{
		this->LoadSkillOzjTexture("combo2.OZJ", &this->m_TexComboAura);
	}
	this->LoadSkillOzjTexture("ComboAura.OZJ", &this->m_TexComboCircle);
	this->LoadSkillOzjTexture("combo2.OZJ", &this->m_TexCombo2);
	this->LoadSkillOzjTexture("combo3.OZJ", &this->m_TexCombo3);
	this->LoadSkillOzjTexture("flare01.OZJ", &this->m_TexComboFlare);
}

bool CCustomComboUI::LoadSkillOzjTexture(const char* filename, COMBO_TEX_INFO* pTex)
{
	if (!pTex || pTex->id != 0)
	{
		return true;
	}

	char path[260];
	sprintf_s(path, "Data\\Skill\\%s", filename);

	FILE* fp = NULL;
	fopen_s(&fp, path, "rb");
	if (!fp)
	{
		sprintf_s(path, "Skill\\%s", filename);
		fopen_s(&fp, path, "rb");
	}

	if (!fp)
	{
		return false;
	}

	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (sz <= 24)
	{
		fclose(fp);
		return false;
	}

	std::vector<BYTE> buf(sz);
	fread(buf.data(), 1, sz, fp);
	fclose(fp);

	int w = 0, h = 0, ch = 0;
	unsigned char* data = stbi_load_from_memory(buf.data() + 24, sz - 24, &w, &h, &ch, 4);
	if (!data)
	{
		data = stbi_load_from_memory(buf.data(), sz, &w, &h, &ch, 4);
	}
	if (!data)
	{
		return false;
	}

	int potW = 1; while (potW < w) potW <<= 1;
	int potH = 1; while (potH < h) potH <<= 1;

	std::vector<unsigned char> potData(potW * potH * 4, 0);
	for (int row = 0; row < h; row++)
	{
		memcpy(&potData[(row * potW) * 4], &data[(row * w) * 4], w * 4);
	}

	for (int i = 0; i < potW * potH; i++)
	{
		BYTE r = potData[i * 4 + 0];
		BYTE g = potData[i * 4 + 1];
		BYTE b = potData[i * 4 + 2];
		BYTE lum = (BYTE)max(r, max(g, b));
		potData[i * 4 + 3] = lum;
	}

	pTex->width = w;
	pTex->height = h;
	pTex->maxU = (float)w / (float)potW;
	pTex->maxV = (float)h / (float)potH;

	glGenTextures(1, &pTex->id);
	glBindTexture(GL_TEXTURE_2D, pTex->id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, potW, potH, 0, GL_RGBA, GL_UNSIGNED_BYTE, potData.data());
	glBindTexture(GL_TEXTURE_2D, 0);

	stbi_image_free(data);
	return true;
}

void CCustomComboUI::PlayComboVisual(int targetIndex, int userIndex)
{
	__try
	{
		if (SceneFlag != MAIN_SCENE || !Hero)
		{
			return;
		}

		static DWORD s_dwLastVisualTick = 0;
		DWORD dwCurTick = GetTickCount();

		if (dwCurTick < s_dwLastVisualTick + 100)
		{
			return;
		}
		s_dwLastVisualTick = dwCurTick;

		vec3_t targetPos = { 0.0f, 0.0f, 0.0f };
		vec3_t targetAngle = { 0.0f, 0.0f, 0.0f };
		bool found = false;

		if (targetIndex >= 0)
		{
			for (int i = 0; i < MAX_MAIN_VIEWPORT; i++)
			{
				DWORD c = CharactersClient + (i * 916);
				if (*(short*)(c + 0x1DC) == (short)targetIndex)
				{
					VectorCopy((float*)(c + 16), targetPos);
					VectorCopy((float*)(c + 28), targetAngle);
					found = true;
					break;
				}
			}
		}

		if (!found && Hero)
		{
			VectorCopy((float*)(Hero + 16), targetPos);
			VectorCopy((float*)(Hero + 28), targetAngle);
			float rad = targetAngle[2] * 0.0174532925f;
			targetPos[0] += sinf(rad) * 150.0f;
			targetPos[1] -= cosf(rad) * 150.0f;
		}

		this->m_LastComboDamageTick = dwCurTick;
		VectorCopy(targetPos, this->m_LastComboPos);

		vec3_t userPos = { 0.0f, 0.0f, 0.0f };
		vec3_t userAngle = { 0.0f, 0.0f, 0.0f };
		bool userFound = false;
		BYTE attackerClass = 1;

		if (userIndex == gPrintPlayer.ViewIndex || *(short*)(Hero + 0x1DC) == (short)userIndex || userIndex < 0)
		{
			VectorCopy((float*)(Hero + 16), userPos);
			VectorCopy((float*)(Hero + 28), userAngle);
			attackerClass = DecodeCharClass(Hero);
			userFound = true;
		}
		else
		{
			for (int i = 0; i < MAX_MAIN_VIEWPORT; i++)
			{
				DWORD c = CharactersClient + (i * 916);
				if (*(short*)(c + 0x1DC) == (short)userIndex)
				{
					VectorCopy((float*)(c + 16), userPos);
					VectorCopy((float*)(c + 28), userAngle);
					attackerClass = DecodeCharClass(c);
					userFound = true;
					break;
				}
			}
		}

		if (!userFound && Hero)
		{
			VectorCopy((float*)(Hero + 16), userPos);
			VectorCopy((float*)(Hero + 28), userAngle);
			attackerClass = DecodeCharClass(Hero);
		}

		int heroSlot = -1;
		DWORD oldestTick = 0xFFFFFFFF;
		int oldestSlot = 0;

		for (int i = 0; i < 16; i++)
		{
			if (!this->m_Bursts[i].active)
			{
				heroSlot = i;
				break;
			}
			if (this->m_Bursts[i].startTick < oldestTick)
			{
				oldestTick = this->m_Bursts[i].startTick;
				oldestSlot = i;
			}
		}
		if (heroSlot == -1) heroSlot = oldestSlot;

		this->m_Bursts[heroSlot].active = true;
		this->m_Bursts[heroSlot].startTick = dwCurTick;
		this->m_Bursts[heroSlot].duration = 700;
		this->m_Bursts[heroSlot].userClass = attackerClass;
		VectorCopy(userPos, this->m_Bursts[heroSlot].pos);
		VectorCopy(userAngle, this->m_Bursts[heroSlot].angle);

		PlayBuffer(92, NULL, FALSE);
		PlayBuffer(84, NULL, FALSE);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

static void GetComboClassColor(BYTE userClass, float* rgb)
{
	switch (userClass & 7)
	{
	case 0: rgb[0] = 0.75f; rgb[1] = 0.30f; rgb[2] = 1.00f; break;
	case 1: rgb[0] = 0.40f; rgb[1] = 0.90f; rgb[2] = 1.00f; break;
	case 2: rgb[0] = 1.00f; rgb[1] = 0.85f; rgb[2] = 0.20f; break;
	case 3: rgb[0] = 1.00f; rgb[1] = 0.30f; rgb[2] = 0.12f; break;
	case 4: rgb[0] = 1.00f; rgb[1] = 0.25f; rgb[2] = 0.65f; break;
	case 5: rgb[0] = 0.35f; rgb[1] = 1.00f; rgb[2] = 0.35f; break;
	default: rgb[0] = 1.00f; rgb[1] = 1.00f; rgb[2] = 1.00f; break;
	}
}

void CCustomComboUI::CheckCharacterChange()
{
	char name[32] = { 0 };
	if (SceneFlag == MAIN_SCENE && Hero)
	{
		if (*(char*)(Hero + 0x1C1) != '\0')
		{
			strncpy_s(name, sizeof(name), (const char*)(Hero + 0x1C1), _TRUNCATE);
		}
		else if (CharacterAttribute)
		{
			strncpy_s(name, sizeof(name), (const char*)CharacterAttribute, 10);
		}
	}

	if (strcmp(name, this->m_LoadedCharacter) != 0)
	{
		strcpy_s(this->m_LoadedCharacter, sizeof(this->m_LoadedCharacter), name);
		this->m_Active = false;
		this->m_ActiveEndTick = 0;
		this->m_ActiveTotalMs = 0;
		this->m_CooldownEndTick = 0;
		this->m_CooldownTotalMs = 0;
		memset(this->m_Bursts, 0, sizeof(this->m_Bursts));
	}
}

void CCustomComboUI::RenderWorldEffects()
{
	this->CheckCharacterChange();
	__try
	{
		if (SceneFlag != MAIN_SCENE || !Hero)
		{
			return;
		}

		if (this->m_TexComboCircle.id == 0 || this->m_TexComboAura.id == 0 || this->m_TexCombo2.id == 0 || this->m_TexCombo3.id == 0 || this->m_TexComboFlare.id == 0)
		{
			this->LoadComboTextures();
		}

		DWORD curTick = GetTickCount();
		bool hasActive = this->IsActive();

		for (int i = 0; i < 16; i++)
		{
			if (this->m_Bursts[i].active)
			{
				if (curTick >= this->m_Bursts[i].startTick + this->m_Bursts[i].duration)
				{
					this->m_Bursts[i].active = false;
				}
				else
				{
					hasActive = true;
				}
			}
		}

		if (!hasActive)
		{
			return;
		}

		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glDisable(GL_LIGHTING);
		glDisable(GL_CULL_FACE);
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_FOG);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glDepthMask(GL_FALSE);

		// 0. Active Hero Combo Aura (ComboAura.OZJ magic circle + 72 rising sparkles)
		bool ownColors = (this->m_TexComboCircle.id != 0);
		COMBO_TEX_INFO* pAuraTex = ownColors ? &this->m_TexComboCircle : ((this->m_TexComboAura.id != 0) ? &this->m_TexComboAura : &this->m_TexCombo2);
		if (this->IsActive() && Hero && pAuraTex->id != 0)
		{
			float hx = *(float*)(Hero + 16);
			float hy = *(float*)(Hero + 20);
			float hz = *(float*)(Hero + 24);

			glBindTexture(GL_TEXTURE_2D, pAuraTex->id);

			float u = pAuraTex->maxU;
			float v = pAuraTex->maxV;

			float auraPulse = 0.70f + 0.30f * sinf((float)curTick * 0.005f);
			float auraSize = 130.0f + 20.0f * auraPulse;

			float ac[3];
			GetComboClassColor(DecodeCharClass(Hero), ac);
			float classCol[3] = { ac[0], ac[1], ac[2] };

			if (ownColors)
			{
				ac[0] = ac[1] = ac[2] = 1.0f;
			}

			glColor4f(ac[0] * auraPulse, ac[1] * auraPulse, ac[2] * auraPulse, 0.90f * auraPulse);

			float centerH = RequestTerrainHeight(hx, hy);
			auto drawTerrainQuad = [&](float size, float rotDeg, float lift)
			{
				const int N = 16;
				float rad = rotDeg * 3.14159265f / 180.0f;
				float cs = cosf(rad), sn = sinf(rad);
				float step = (size * 2.0f) / N;
				for (int gy = 0; gy < N; gy++)
				{
					glBegin(GL_QUAD_STRIP);
					for (int gx = 0; gx <= N; gx++)
					{
						for (int k = 0; k < 2; k++)
						{
							float lx = -size + gx * step;
							float ly = -size + (gy + k) * step;
							float wx = hx + lx * cs - ly * sn;
							float wy = hy + lx * sn + ly * cs;
							float wz = RequestTerrainHeight(wx, wy);
							if (wz < centerH - 120.0f) wz = centerH - 120.0f;
							if (wz > centerH + 120.0f) wz = centerH + 120.0f;
							wz += lift;
							glTexCoord2f(u * (gx / (float)N), v * ((gy + k) / (float)N));
							glVertex3f(wx, wy, wz);
						}
					}
					glEnd();
				}
			};

			// Depth test on: hero stays cleanly in front of the magic circle
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LEQUAL);
			drawTerrainQuad(auraSize, (float)curTick * 0.07f, 6.0f);

			float innerSize = auraSize * 0.78f;
			glColor4f((ac[0] * 0.65f + 0.35f) * auraPulse, (ac[1] * 0.65f + 0.35f) * auraPulse, (ac[2] * 0.65f + 0.35f) * auraPulse, 0.75f * auraPulse);
			if (!ownColors)
			{
				drawTerrainQuad(innerSize, (float)curTick * 0.07f - (float)curTick * 0.14f, 12.0f);
			}

			glPushMatrix();
			glTranslatef(hx, hy, hz + 3.0f);
			glRotatef((float)curTick * 0.07f - (float)curTick * 0.14f + (float)curTick * 0.20f, 0.0f, 0.0f, 1.0f);

			float waistSize = auraSize * 0.50f;
			glTranslatef(0.0f, 0.0f, 33.0f);
			glColor4f(ac[0] * auraPulse, ac[1] * auraPulse, ac[2] * auraPulse, 0.60f * auraPulse);

			glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 0.0f); glVertex3f(-waistSize, -waistSize, 0.0f);
			glTexCoord2f(u,    0.0f); glVertex3f( waistSize, -waistSize, 0.0f);
			glTexCoord2f(u,    v   ); glVertex3f( waistSize,  waistSize, 0.0f);
			glTexCoord2f(0.0f, v   ); glVertex3f(-waistSize,  waistSize, 0.0f);
			glEnd();

			glPopMatrix();

			// Layer 4: 72 rising twinkling class-colored sparkles around the hero
			if (this->m_TexComboFlare.id != 0)
			{
				float mv[16];
				glGetFloatv(GL_MODELVIEW_MATRIX, mv);

				float rx = mv[0], ry = mv[4], rz = mv[8];
				float ux = mv[1], uy = mv[5], uz = mv[9];

				glEnable(GL_DEPTH_TEST);
				glBindTexture(GL_TEXTURE_2D, this->m_TexComboFlare.id);

				float fu = this->m_TexComboFlare.maxU;
				float fv = this->m_TexComboFlare.maxV;

				float sc[3] = { classCol[0], classCol[1], classCol[2] };
				float cc[3] = { classCol[0] * 0.3f + 0.7f, classCol[1] * 0.3f + 0.7f, classCol[2] * 0.3f + 0.7f };

				glBegin(GL_QUADS);

				for (int k = 0; k < 72; k++)
				{
					unsigned int h = (unsigned int)k * 2654435761u;
					float r1 = (float)((h >> 4) & 1023) / 1023.0f;
					float r2 = (float)((h >> 14) & 1023) / 1023.0f;
					float r3 = (float)((h >> 22) & 1023) / 1023.0f;

					float life = 1600.0f + r3 * 1400.0f;
					float lt = fmodf((float)curTick + r1 * life * 7.0f, life) / life;

					float ang = r1 * 6.2831853f + lt * (0.8f + r2 * 0.8f) * ((k & 1) ? 1.0f : -1.0f);
					float rad = auraSize * (0.25f + 0.75f * r2) * (1.0f - lt * 0.25f);

					float px = hx + cosf(ang) * rad;
					float py = hy + sinf(ang) * rad;
					float pz = centerH + 10.0f + lt * (140.0f + r3 * 80.0f);

					float fade = (lt < 0.15f) ? (lt / 0.15f) : ((1.0f - lt) / 0.85f);
					float twinkle = 0.65f + 0.35f * sinf((float)curTick * 0.02f + r2 * 40.0f);
					float a = fade * twinkle;
					float base = (9.0f + r3 * 10.0f) * (0.6f + 0.4f * twinkle);

					for (int pass = 0; pass < 2; pass++)
					{
						float sz = (pass == 0) ? base * 1.8f : base * 0.7f;
						float* col = (pass == 0) ? sc : cc;
						float pa = (pass == 0) ? a : (a < 0.5f ? a * 2.0f : 1.0f) * fade;

						glColor4f(col[0] * pa, col[1] * pa, col[2] * pa, pa);

						glTexCoord2f(0.0f, 0.0f); glVertex3f(px + (-rx - ux) * sz, py + (-ry - uy) * sz, pz + (-rz - uz) * sz);
						glTexCoord2f(fu,   0.0f); glVertex3f(px + ( rx - ux) * sz, py + ( ry - uy) * sz, pz + ( rz - uz) * sz);
						glTexCoord2f(fu,   fv  ); glVertex3f(px + ( rx + ux) * sz, py + ( ry + uy) * sz, pz + ( rz + uz) * sz);
						glTexCoord2f(0.0f, fv  ); glVertex3f(px + (-rx + ux) * sz, py + (-ry + uy) * sz, pz + (-rz + uz) * sz);
					}
				}

				glEnd();
			}
		}

		// Bursts lie flat just above the ground: disable depth test so grass never hides them
		glDisable(GL_DEPTH_TEST);

		for (int i = 0; i < 16; i++)
		{
			if (!this->m_Bursts[i].active)
			{
				continue;
			}

			DWORD elapsed = curTick - this->m_Bursts[i].startTick;
			float t = (float)elapsed / (float)this->m_Bursts[i].duration;
			if (t < 0.0f) t = 0.0f;
			if (t > 1.0f) t = 1.0f;

			float alpha = (t < 0.15f) ? (t / 0.15f) : ((1.0f - t) / 0.85f);
			alpha = powf(alpha, 1.2f);

			float cx = this->m_Bursts[i].pos[0];
			float cy = this->m_Bursts[i].pos[1];
			float cz = this->m_Bursts[i].pos[2];

			// 1. Expanding electric shockwave rings
			if (this->m_TexCombo2.id != 0)
			{
				glBindTexture(GL_TEXTURE_2D, this->m_TexCombo2.id);

				float bc[3];
				GetComboClassColor(this->m_Bursts[i].userClass, bc);

				float expandFactor = 1.0f - powf(1.0f - t, 2.5f);
				float r2 = 50.0f + 330.0f * expandFactor;
				float rot2 = this->m_Bursts[i].angle[2] + t * 270.0f;

				glColor4f(bc[0] * alpha, bc[1] * alpha, bc[2] * alpha, alpha * 0.8f);

				glPushMatrix();
				glTranslatef(cx, cy, cz + 4.0f);
				glRotatef(rot2, 0.0f, 0.0f, 1.0f);

				glBegin(GL_QUADS);
				glTexCoord2f(0.0f, 0.0f); glVertex3f(-r2, -r2, 0.0f);
				glTexCoord2f(1.0f, 0.0f); glVertex3f( r2, -r2, 0.0f);
				glTexCoord2f(1.0f, 1.0f); glVertex3f( r2,  r2, 0.0f);
				glTexCoord2f(0.0f, 1.0f); glVertex3f(-r2,  r2, 0.0f);
				glEnd();

				float r2Inner = r2 * 0.70f;
				glRotatef(-rot2 * 1.5f, 0.0f, 0.0f, 1.0f);
				glTranslatef(0.0f, 0.0f, 15.0f);
				glColor4f((bc[0] * 0.5f + 0.5f) * alpha, (bc[1] * 0.5f + 0.5f) * alpha, (bc[2] * 0.5f + 0.5f) * alpha, alpha * 0.60f);

				glBegin(GL_QUADS);
				glTexCoord2f(0.0f, 0.0f); glVertex3f(-r2Inner, -r2Inner, 0.0f);
				glTexCoord2f(1.0f, 0.0f); glVertex3f( r2Inner, -r2Inner, 0.0f);
				glTexCoord2f(1.0f, 1.0f); glVertex3f( r2Inner,  r2Inner, 0.0f);
				glTexCoord2f(0.0f, 1.0f); glVertex3f(-r2Inner,  r2Inner, 0.0f);
				glEnd();

				glPopMatrix();
			}

			// 2. 3D Spiky Starburst Energy Explosion
			if (this->m_TexCombo3.id != 0)
			{
				glBindTexture(GL_TEXTURE_2D, this->m_TexCombo3.id);

				float expandFactor3 = 1.0f - powf(1.0f - t, 2.2f);
				float spikeScale = 0.80f + 2.80f * expandFactor3;

				float bc3[3];
				GetComboClassColor(this->m_Bursts[i].userClass, bc3);
				glColor4f((bc3[0] * 0.25f + 0.75f) * alpha, (bc3[1] * 0.25f + 0.75f) * alpha, (bc3[2] * 0.25f + 0.75f) * alpha, alpha);

				float yaw = this->m_Bursts[i].angle[2];
				const float lifts[3] = { 8.0f, 16.0f, 24.0f };
				const float spins[3] = { 360.0f, -450.0f, 500.0f };
				const float sizes[3] = { 1.00f, 1.25f, 1.05f };
				for (int k = 0; k < 3; k++)
				{
					glPushMatrix();
					glTranslatef(cx, cy, cz + lifts[k]);
					glRotatef(yaw + k * 30.0f, 0.0f, 0.0f, 1.0f);
					glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
					glRotatef(t * spins[k], 1.0f, 0.0f, 0.0f);
					float sk = spikeScale * sizes[k];
					glScalef(sk, sk, sk);
					DrawComboMeshSpikes();
					glPopMatrix();
				}
			}

			// 3. Detonation Star Flare (flat on ground)
			if (this->m_TexComboFlare.id != 0 && t < 0.75f)
			{
				glBindTexture(GL_TEXTURE_2D, this->m_TexComboFlare.id);

				float ft = (t < 0.20f) ? (t / 0.20f) : ((0.75f - t) / 0.55f);
				float flareAlpha = max(0.0f, min(1.0f, ft));
				float flareSize = 60.0f + 260.0f * (t / 0.75f);
				float flareRot = t * 360.0f;

				float fc[3];
				GetComboClassColor(this->m_Bursts[i].userClass, fc);
				glColor4f((fc[0] * 0.2f + 0.8f) * flareAlpha, (fc[1] * 0.2f + 0.8f) * flareAlpha, (fc[2] * 0.2f + 0.8f) * flareAlpha, flareAlpha);

				glPushMatrix();
				glTranslatef(cx, cy, cz + 20.0f);
				glRotatef(this->m_Bursts[i].angle[2] + flareRot, 0.0f, 0.0f, 1.0f);

				glRotatef(45.0f, 0.0f, 0.0f, 1.0f);
				float discSize = flareSize * 0.90f;
				glBegin(GL_QUADS);
				glTexCoord2f(0.0f, 0.0f); glVertex3f(-discSize, -discSize, 0.0f);
				glTexCoord2f(1.0f, 0.0f); glVertex3f( discSize, -discSize, 0.0f);
				glTexCoord2f(1.0f, 1.0f); glVertex3f( discSize,  discSize, 0.0f);
				glTexCoord2f(0.0f, 1.0f); glVertex3f(-discSize,  discSize, 0.0f);
				glEnd();

				glPopMatrix();
			}
		}

		glPopAttrib();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

void CCustomComboUI::GCComboEffectRecv(PMSG_CUSTOM_COMBO_EFFECT_SEND* lpMsg)
{
	__try
	{
		if (lpMsg == NULL)
		{
			return;
		}

		this->PlayComboVisual(lpMsg->targetIndex, lpMsg->userIndex);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}
