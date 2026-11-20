#include "stdafx.h"
#include "LoginRemember.h"
#include "LoginScale.h"
#include <wincrypt.h>
#include <stdio.h>
#include <string>

#pragma comment(lib, "crypt32.lib")

CLoginRemember gLoginRemember;

#define ACCOUNT_BUFFER ((char*)0x07DB8710)   // text typed in the account box (index 0)
#define PASSWORD_BUFFER ((char*)0x07DB8810)  // text typed in the password box (index 1)

#define STORE_FILE ".\\LoginAccounts.dat"

// frame-relative rectangles (units of the original 250x216 art)
static const float AF_X0 = 60.0f, AF_Y0 = 76.0f, AF_X1 = 190.0f, AF_Y1 = 91.0f;       // account field
static const float ARROW_X0 = 173.0f, ARROW_Y0 = 76.5f, ARROW_X1 = 189.5f, ARROW_Y1 = 90.5f;
static const float CHK_X0 = 60.0f, CHK_Y0 = 127.0f, CHK_SIZE = 8.5f;                  // check box
static const float CHK_HIT_X1 = 138.0f, CHK_HIT_Y0 = 125.0f, CHK_HIT_Y1 = 138.0f;     // box + label hit area
static const float LIST_X0 = 60.0f, LIST_X1 = 190.0f, LIST_Y0 = 92.5f, ROW_H = 15.0f, LIST_PAD = 2.0f;

CLoginRemember::CLoginRemember()
{
	this->m_Remember = false;

	this->m_Open = false;

	this->m_Prefilled = false;

	this->m_Count = 0;

	memset(this->m_SnapAccount, 0, sizeof(this->m_SnapAccount));

	memset(this->m_SnapPassword, 0, sizeof(this->m_SnapPassword));

	memset(this->m_List, 0, sizeof(this->m_List));
}

void CLoginRemember::Init()
{
	this->Load();
}

// ---------------------------------------------------------------------------------------------------- storage

static std::string ToHex(const BYTE* data, DWORD size)
{
	static const char* digits = "0123456789ABCDEF";

	std::string out;

	for (DWORD i = 0; i < size; i++)
	{
		out += digits[data[i] >> 4];

		out += digits[data[i] & 15];
	}

	return out;
}

static int HexVal(char c)
{
	if (c >= '0' && c <= '9') return c - '0';

	if (c >= 'A' && c <= 'F') return c - 'A' + 10;

	if (c >= 'a' && c <= 'f') return c - 'a' + 10;

	return -1;
}

static BYTE g_Entropy[] = { 'M', 'u', 'E', 'm', 'u', '9', '7', 'k' };

static bool ProtectPassword(const char* password, std::string& hex)
{
	DATA_BLOB in = { (DWORD)strlen(password), (BYTE*)password };

	DATA_BLOB entropy = { sizeof(g_Entropy), g_Entropy };

	DATA_BLOB out = { 0, NULL };

	if (!CryptProtectData(&in, L"MuLogin", &entropy, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &out))
	{
		return false;
	}

	hex = ToHex(out.pbData, out.cbData);

	LocalFree(out.pbData);

	return true;
}

static bool UnprotectPassword(const char* hex, char* password11)
{
	size_t len = strlen(hex);

	if (len == 0 || (len % 2) != 0 || len > 1024)
	{
		return false;
	}

	std::string raw;

	for (size_t i = 0; i < len; i += 2)
	{
		int hi = HexVal(hex[i]);

		int lo = HexVal(hex[i + 1]);

		if (hi < 0 || lo < 0)
		{
			return false;
		}

		raw += (char)((hi << 4) | lo);
	}

	DATA_BLOB in = { (DWORD)raw.size(), (BYTE*)raw.data() };

	DATA_BLOB entropy = { sizeof(g_Entropy), g_Entropy };

	DATA_BLOB out = { 0, NULL };

	if (!CryptUnprotectData(&in, NULL, &entropy, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &out))
	{
		return false;
	}

	bool ok = (out.cbData <= 10);

	if (ok)
	{
		memset(password11, 0, 11);

		memcpy(password11, out.pbData, out.cbData);
	}

	LocalFree(out.pbData);

	return ok;
}

void CLoginRemember::Load()
{
	this->m_Count = 0;

	FILE* f = fopen(STORE_FILE, "r");

	if (!f)
	{
		return;
	}

	char line[2048];

	while (fgets(line, sizeof(line), f))
	{
		size_t n = strlen(line);

		while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
		{
			line[--n] = 0;
		}

		if (strncmp(line, "R|", 2) == 0)
		{
			this->m_Remember = (line[2] == '1');
		}
		else if (strncmp(line, "A|", 2) == 0 && this->m_Count < LOGIN_SAVED_MAX)
		{
			char* sep = strchr(line + 2, '|');

			if (!sep)
			{
				continue;
			}

			*sep = 0;

			LOGIN_SAVED_INFO info;

			memset(&info, 0, sizeof(info));

			if (strlen(line + 2) == 0 || strlen(line + 2) > 10 || !UnprotectPassword(sep + 1, info.password))
			{
				continue;
			}

			strcpy_s(info.account, line + 2);

			this->m_List[this->m_Count++] = info;
		}
	}

	fclose(f);
}

void CLoginRemember::Save()
{
	FILE* f = fopen(STORE_FILE, "w");

	if (!f)
	{
		return;
	}

	fprintf(f, "R|%d\n", this->m_Remember ? 1 : 0);

	for (int i = 0; i < this->m_Count; i++)
	{
		std::string hex;

		if (ProtectPassword(this->m_List[i].password, hex))
		{
			fprintf(f, "A|%s|%s\n", this->m_List[i].account, hex.c_str());
		}
	}

	fclose(f);
}

void CLoginRemember::Upsert(const char* account, const char* password)
{
	LOGIN_SAVED_INFO info;

	memset(&info, 0, sizeof(info));

	strncpy_s(info.account, account, 10);

	strncpy_s(info.password, password, 10);

	// remove the same account (case-insensitive), then put it first
	int out = 0;

	for (int i = 0; i < this->m_Count; i++)
	{
		if (_stricmp(this->m_List[i].account, info.account) != 0)
		{
			this->m_List[out++] = this->m_List[i];
		}
	}

	this->m_Count = out;

	if (this->m_Count >= LOGIN_SAVED_MAX)
	{
		this->m_Count = LOGIN_SAVED_MAX - 1;
	}

	for (int i = this->m_Count; i > 0; i--)
	{
		this->m_List[i] = this->m_List[i - 1];
	}

	this->m_List[0] = info;

	this->m_Count++;
}

void CLoginRemember::Fill(int index)
{
	if (index < 0 || index >= this->m_Count)
	{
		return;
	}

	memset(ACCOUNT_BUFFER, 0, 256);

	memset(PASSWORD_BUFFER, 0, 256);

	strcpy_s(ACCOUNT_BUFFER, 256, this->m_List[index].account);

	strcpy_s(PASSWORD_BUFFER, 256, this->m_List[index].password);

	// keep the original input handler consistent: per-box insert position (0x7D780A8 + 4*box) and the active box (0x7E11D78)
	*(int*)(0x07D780A8 + 0) = (int)strlen(ACCOUNT_BUFFER);

	*(int*)(0x07D780A8 + 4) = (int)strlen(PASSWORD_BUFFER);

	*(int*)0x07E11D78 = 1;
}

void CLoginRemember::OnConnectResult(BYTE result)
{
	if (result != 1 || !this->m_Remember || this->m_SnapAccount[0] == 0 || this->m_SnapPassword[0] == 0)
	{
		return;
	}

	this->Upsert(this->m_SnapAccount, this->m_SnapPassword);

	this->Save();
}

// ---------------------------------------------------------------------------------------------------- hit tests

bool CLoginRemember::InArrow(float rx, float ry)
{
	return (rx >= ARROW_X0 && rx < ARROW_X1 && ry >= ARROW_Y0 && ry < ARROW_Y1);
}

bool CLoginRemember::InCheck(float rx, float ry)
{
	return (rx >= CHK_X0 && rx < CHK_HIT_X1 && ry >= CHK_HIT_Y0 && ry < CHK_HIT_Y1);
}

bool CLoginRemember::InList(float rx, float ry)
{
	if (!this->m_Open || this->m_Count == 0)
	{
		return false;
	}

	float y1 = LIST_Y0 + LIST_PAD * 2 + ROW_H * this->m_Count;

	return (rx >= LIST_X0 && rx < LIST_X1 && ry >= LIST_Y0 && ry < y1);
}

bool CLoginRemember::OverList(float rx, float ry)
{
	return this->InList(rx, ry);
}

int CLoginRemember::RowAt(float ry)
{
	int row = (int)((ry - (LIST_Y0 + LIST_PAD)) / ROW_H);

	return (row >= 0 && row < this->m_Count) ? row : -1;
}

bool CLoginRemember::Update(float rx, float ry, bool click, int state)
{
	if (state != 2)
	{
		this->m_Open = false;

		return false;
	}

	// keep the last typed values; they are what gets saved once the server accepts the login
	if (ACCOUNT_BUFFER[0] != 0 && PASSWORD_BUFFER[0] != 0)
	{
		strncpy_s(this->m_SnapAccount, ACCOUNT_BUFFER, 10);

		strncpy_s(this->m_SnapPassword, PASSWORD_BUFFER, 10);
	}

	// first time the window shows: fill in the most recent saved account
	if (!this->m_Prefilled)
	{
		this->m_Prefilled = true;

		if (this->m_Remember && this->m_Count > 0 && ACCOUNT_BUFFER[0] == 0 && PASSWORD_BUFFER[0] == 0)
		{
			this->Fill(0);
		}
	}

	if (click)
	{
		if (this->m_Open)
		{
			if (this->InList(rx, ry))
			{
				int row = this->RowAt(ry);

				if (row >= 0)
				{
					if (rx >= LIST_X1 - 18.0f)
					{
						// delete this entry
						for (int i = row; i < this->m_Count - 1; i++)
						{
							this->m_List[i] = this->m_List[i + 1];
						}

						this->m_Count--;

						this->Save();

						if (this->m_Count == 0)
						{
							this->m_Open = false;
						}
					}
					else
					{
						this->Fill(row);

						this->m_Open = false;
					}
				}

				return true;
			}

			this->m_Open = false;

			if (this->InArrow(rx, ry))
			{
				return true;
			}

			return false;
		}

		if (this->InArrow(rx, ry))
		{
			this->m_Open = (this->m_Count > 0);

			return true;
		}

		if (this->InCheck(rx, ry))
		{
			this->m_Remember = !this->m_Remember;

			this->Save();

			return true;
		}
	}

	return this->InList(rx, ry);
}

// ---------------------------------------------------------------------------------------------------- drawing

static void Fill4(float rx0, float ry0, float rx1, float ry1, float frameX, float frameY, float r, float g, float b, float a)
{
	float s = gLoginScale.m_Scale;

	float px = gLoginScale.m_PivotX;

	float py = gLoginScale.m_PivotY;

	float x0 = px + ((frameX + rx0) - px) * s;

	float y0 = py + ((frameY + ry0) - py) * s;

	float x1 = px + ((frameX + rx1) - px) * s;

	float y1 = py + ((frameY + ry1) - py) * s;

	EnableAlphaTest(true);

	glColor4f(r, g, b, a);

	RenderColor(x0, y0, x1 - x0, y1 - y0);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	DisableAlphaBlend();

	EnableAlphaTest(true);
}

static void Frame4(float rx0, float ry0, float rx1, float ry1, float frameX, float frameY, float t, float r, float g, float b, float a)
{
	Fill4(rx0, ry0, rx1, ry0 + t, frameX, frameY, r, g, b, a);

	Fill4(rx0, ry1 - t, rx1, ry1, frameX, frameY, r, g, b, a);

	Fill4(rx0, ry0 + t, rx0 + t, ry1 - t, frameX, frameY, r, g, b, a);

	Fill4(rx1 - t, ry0 + t, rx1, ry1 - t, frameX, frameY, r, g, b, a);
}

static void Text4(float rx, float ry, const char* text, float frameX, float frameY, DWORD color)
{
	DWORD backupBg = SetBackgroundTextColor;

	DWORD backupColor = SetTextColor;

	SelectObject(m_hFontDC, g_hFont);

	SetBackgroundTextColor = Color4b(0, 0, 0, 0);

	SetTextColor = color;

	RenderText((int)(frameX + rx + 0.5f), (int)(frameY + ry + 0.5f), text, 0, 0, NULL);

	SetBackgroundTextColor = backupBg;

	SetTextColor = backupColor;
}

void CLoginRemember::Render(float frameX, float frameY, float rx, float ry, int state)
{
	const float ar = 111.0f / 255.0f, ag = 182.0f / 255.0f, ab = 196.0f / 255.0f;   // accent

	// --- dropdown arrow, inside the right end of the account field
	bool hotArrow = (state == 2 && this->InArrow(rx, ry));

	Fill4(ARROW_X0 - 0.5f, ARROW_Y0 + 1.0f, ARROW_X0 + 0.3f, ARROW_Y1 - 1.0f, frameX, frameY, 0.23f, 0.27f, 0.32f, 1.0f);   // separator

	if (hotArrow || this->m_Open)
	{
		Fill4(ARROW_X0 + 0.5f, ARROW_Y0 + 0.5f, ARROW_X1 - 0.5f, ARROW_Y1 - 0.5f, frameX, frameY, 1.0f, 1.0f, 1.0f, 0.08f);
	}

	{
		float cx = (ARROW_X0 + ARROW_X1) * 0.5f;

		float cy = (ARROW_Y0 + ARROW_Y1) * 0.5f;

		for (int i = 0; i < 4; i++)
		{
			float half = 3.4f - i * 0.9f;

			float y = this->m_Open ? (cy + 1.2f - i * 0.95f) : (cy - 1.8f + i * 0.95f);

			Fill4(cx - half, y, cx + half, y + 0.95f, frameX, frameY, ar, ag, ab, 1.0f);
		}
	}

	// --- remember check box
	bool hotCheck = (state == 2 && this->InCheck(rx, ry));

	Frame4(CHK_X0, CHK_Y0, CHK_X0 + CHK_SIZE, CHK_Y0 + CHK_SIZE, frameX, frameY, 0.7f, hotCheck ? 0.9f : 0.24f, hotCheck ? 0.91f : 0.29f, hotCheck ? 0.93f : 0.34f, 1.0f);

	Fill4(CHK_X0 + 0.7f, CHK_Y0 + 0.7f, CHK_X0 + CHK_SIZE - 0.7f, CHK_Y0 + CHK_SIZE - 0.7f, frameX, frameY, 0.05f, 0.07f, 0.09f, 1.0f);

	if (this->m_Remember)
	{
		Fill4(CHK_X0 + 1.8f, CHK_Y0 + 1.8f, CHK_X0 + CHK_SIZE - 1.8f, CHK_Y0 + CHK_SIZE - 1.8f, frameX, frameY, ar, ag, ab, 1.0f);
	}

	Text4(CHK_X0 + 12.5f, CHK_Y0 + 2.0f, "Remember me", frameX, frameY, Color4b(228, 233, 238, 255));

	// --- list of saved accounts (drawn last, on top of the password field)
	if (this->m_Open && this->m_Count > 0)
	{
		float y1 = LIST_Y0 + LIST_PAD * 2 + ROW_H * this->m_Count;

		Fill4(LIST_X0, LIST_Y0, LIST_X1, y1, frameX, frameY, 0.09f, 0.11f, 0.13f, 0.98f);

		Frame4(LIST_X0, LIST_Y0, LIST_X1, y1, frameX, frameY, 0.7f, ar, ag, ab, 1.0f);

		int hover = (state == 2 && this->InList(rx, ry)) ? this->RowAt(ry) : -1;

		for (int i = 0; i < this->m_Count; i++)
		{
			float top = LIST_Y0 + LIST_PAD + ROW_H * i;

			if (i == hover)
			{
				Fill4(LIST_X0 + 1.0f, top, LIST_X1 - 1.0f, top + ROW_H, frameX, frameY, ar, ag, ab, 0.18f);

				Fill4(LIST_X0 + 1.0f, top, LIST_X0 + 2.2f, top + ROW_H, frameX, frameY, ar, ag, ab, 1.0f);
			}

			Text4(LIST_X0 + 5.5f, top + 5.3f, this->m_List[i].account, frameX, frameY, Color4b(228, 233, 238, 255));

			bool hotX = (i == hover && rx >= LIST_X1 - 18.0f);

			Text4(LIST_X1 - 12.0f, top + 5.3f, "x", frameX, frameY, hotX ? Color4b(217, 112, 127, 255) : Color4b(138, 149, 161, 255));
		}
	}
}
