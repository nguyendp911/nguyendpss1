#include "stdafx.h"
#include "Interface.h"
#include "Buff.h"
#include "Camera3D.h"
#include "CustomComboUI.h"
#include "LoginScale.h"
#include "Camera3D.h"
#include "CustomComboUI.h"
#include "EventTimer.h"
#include "LoadModels.h"
#include "MiniMap.h"
#include "MoveList.h"
#include "Protect.h"
#include "stb.h"
#include "Window.h"

Interface gInterface;

Interface::Interface()
{
	memset(this->Data, 0, sizeof(this->Data));
}

Interface::~Interface()
{

}

// ---------------------------------------------------------------------------------------------------------------------
// Chat layout (virtual 640x480 units), bottom left, above the interface bar and clear of the HP orb:
//
//     chat log header  x 0..272,  y 214 .. 228
//     chat log body    x 0..272,  y 228 .. 320  (fits 6 lines cleanly, clipped at top)
//     chat input box   x 0..272,  y 320 .. 349  (CHAT_INPUT_Y 360 - HUD_SHIFT_Y 40)
//
// The floating chat log (chat mode 0) is anchored at (-10, 81) in the original client, i.e. the top left corner.
// The log window position is set at 0x005258E9 (every frame) and at 0x004C05D9 (when the chat mode is toggled); the
// input box is drawn by the function at 0x004BE4F0 with the coordinates hard coded as push immediates.
// ---------------------------------------------------------------------------------------------------------------------
#define CHAT_LOG_POS_X (-2) // +10 offset inside RenderMessage puts text at x = 8 (pill starts at x = 3)
#define CHAT_LOG_POS_Y 316  // bottom of floating chat log (just above the 320 input bar)
#define CHAT_INPUT_Y 360    // shifted by -HUD_SHIFT_Y (40) -> 320..349 on screen
#define CHAT_LOG_WIDTH 272
#define CHAT_INPUT_TEXT_DY 3 // typed text a little lower inside the fields

static __declspec(naked) void ChatLogPositionStub()
{
	static DWORD jmpBack = 0x005258ED;

	_asm
	{
		Push CHAT_LOG_POS_Y;
		Push CHAT_LOG_POS_X;
		Jmp[jmpBack];
	}
}

static __declspec(naked) void ChatLogPositionToggleStub()
{
	static DWORD jmpBack = 0x004C06E6;

	_asm
	{
		Push CHAT_LOG_POS_Y;
		Push CHAT_LOG_POS_X;
		Jmp[jmpBack];
	}
}

static DWORD FloatBits(float value)
{
	DWORD bits;

	memcpy(&bits, &value, sizeof(bits));

	return bits;
}

static void PatchChatPositions()
{
	// 0x005254CE: jne 0x005258E9 (SetPosition(-10, 81) for chat mode 0) -> redirect to our position
	SetDword(0x005254CE + 2, (DWORD)&ChatLogPositionStub - (0x005254CE + 6));

	// 0x004C05D9: jne 0x004C06E2 (same thing when the chat mode is toggled)
	SetDword(0x004C05D9 + 2, (DWORD)&ChatLogPositionToggleStub - (0x004C05D9 + 6));

	const float dx = 0.0f - 186.0f;

	const float dy = (float)CHAT_INPUT_Y - 415.0f;

	// input box background: RenderBitmap(248, 186, 415, 134, 29) and RenderBitmap(249, 320, 415, 134, 29)
	SetDword(0x004BE554, FloatBits(415.0f + dy));
	SetDword(0x004BE559, FloatBits(186.0f + dx));
	SetDword(0x004BE584, FloatBits(415.0f + dy));
	SetDword(0x004BE589, FloatBits(320.0f + dx));

	// typed text of the chat / whisper boxes: 0x0047F0B0(191, 422, 0, 0) and 0x0047F0B0(376, 422, 1, 1)
	// 4 units of padding inside each flat field (chat field starts at x = 5, whisper field at x = 192)
	SetDword(0x004BE5D6, (DWORD)(422 + (int)dy + CHAT_INPUT_TEXT_DY));
	SetDword(0x004BE5DB, (DWORD)(9));
	SetDword(0x004BE5EE, (DWORD)(422 + (int)dy + CHAT_INPUT_TEXT_DY));
	SetDword(0x004BE5F6, (DWORD)(196));

	// whisper history list: 0x0047F7A0(376, 422 + 15 * n, ...)
	SetDword(0x004BE662, (DWORD)(422 + (int)dy + CHAT_INPUT_TEXT_DY));
	SetDword(0x004BE668, (DWORD)(196));

	// button next to the whisper box: RenderBitmap(239, 432, 420, 15, 16) -> 20x23 aligned with input fields
	SetDword(0x004BE6E9, FloatBits(23.0f));
	SetDword(0x004BE6EE, FloatBits(20.0f));
	SetDword(0x004BE6F3, FloatBits((float)CHAT_INPUT_Y + 3.0f));
	SetDword(0x004BE6F8, FloatBits(247.0f));
}

static void ChatPanelRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);

	glColor4f(r, g, b, a);

	RenderColor(x, y, w, h);
}

// ---------------------------------------------------------------------------------------------------------------------
// Floating chat log (chat mode 0), drawn entirely by us:
//   * CUIChatLogWindow vtable +0x10 (0x00411920, frame + message loop) is replaced: in mode 0 it only keeps the frame /
//     IME part (+0x58) and marks the log visible; the stock message loop (which drew black GDI boxes) is skipped.
//   * The messages are read straight from the window's std::list (head at +0x5C, node: +0 next, +4 prev,
//     +0x08 strID, +0x13 strText, +0x114 type) and drawn in ChatLogRenderHook, the unshifted 2D pass used by the
//     notices, so the pill and the text always line up.
// ---------------------------------------------------------------------------------------------------------------------
#define CHAT_WND_PTR (*(BYTE**)0x055C9FF0)
#define CHAT_LINE_H 15
#define CHAT_MAX_LINES 8

static DWORD g_ChatLogVisibleTick = 0;

static void RenderChatLogFrame()
{
}

static void __fastcall ChatLogWindowRenderHook(void* pThis, void* /*edx*/)
{
	// Suppress the original MU chat frame, dark backdrop, scrollbar (vtbl +0x58) and F4 top-left log.
	// Our Cold Steel floating chat pills are rendered cleanly in RenderFloatingChatLog().
	g_ChatLogVisibleTick = GetTickCount();
}

static void ChatTypeStyle(int type, DWORD* textColor, float* r, float* g, float* b)
{
	switch (type)
	{
	case 0: // whisper -> gold
		*textColor = 0xFF66D8FF; *r = 0.95f; *g = 0.76f; *b = 0.28f;
		break;
	case 1: // system / exp / zen / items -> sky blue
		*textColor = 0xFFFFD29A; *r = 0.38f; *g = 0.75f; *b = 0.98f;
		break;
	case 2: // error -> coral red
		*textColor = 0xFF7880FF; *r = 0.95f; *g = 0.32f; *b = 0.28f;
		break;
	case 4: // party -> aqua
		*textColor = 0xFFE8F070; *r = 0.30f; *g = 0.88f; *b = 0.85f;
		break;
	case 5: // guild -> green
		*textColor = 0xFF90FFA0; *r = 0.35f; *g = 0.88f; *b = 0.50f;
		break;
	default: // normal chat -> white
		*textColor = 0xFFF2F2F2; *r = 0.80f; *g = 0.85f; *b = 0.90f;
		break;
	}
}

static void RenderFloatingChatLog()
{
	if (GetTickCount() - g_ChatLogVisibleTick > 250)
	{
		return;
	}

	BYTE* wnd = CHAT_WND_PTR;

	if (wnd == NULL)
	{
		return;
	}

	BYTE* head = *(BYTE**)(wnd + 0x5C);

	if (head == NULL)
	{
		return;
	}

	int maxLines = *(int*)(wnd + 0x8C);

	if (maxLines <= 0 || maxLines > CHAT_MAX_LINES)
	{
		maxLines = 6;
	}

	// collect the newest visible lines (new messages are pushed at the front: head->next is the newest)
	BYTE* nodes[CHAT_MAX_LINES];
	int count = 0;
	int guard = 0;

	for (BYTE* node = *(BYTE**)(head + 0); node != NULL && node != head && count < maxLines && guard < 512; node = *(BYTE**)(node + 0), guard++)
	{
		if (*(char*)(node + 0x08) == '\0' && *(char*)(node + 0x13) == '\0')
		{
			continue;
		}

		nodes[count++] = node;
	}

	if (count == 0)
	{
		return;
	}

	// Use the same font for GDI measurement and the text renderer
	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	((void(__thiscall*)(void*, HFONT)) 0x0040F650)(*(void**)0x055C9FF8, g_hFont);
	const DWORD oldTextColor = SetTextColor;
	const DWORD oldBgColor = SetBackgroundTextColor;

	for (int i = 0; i < count; i++)
	{
		BYTE* node = nodes[i];
		const char* strID = (const char*)(node + 0x08);
		const char* strText = (const char*)(node + 0x13);
		const int type = *(int*)(node + 0x114);

		char line[300];

		if (strID[0] != '\0')
		{
			sprintf_s(line, sizeof(line), "%.10s : %.270s", strID, strText);
		}
		else
		{
			sprintf_s(line, sizeof(line), "%.290s", strText);
		}

		DWORD textColor;
		float r, g, b;

		ChatTypeStyle(type, &textColor, &r, &g, &b);

		SIZE sz = { 0, 0 };
		GetTextExtentPoint32A(m_hFontDC, line, (int)strlen(line), &sz);

		float textW = (WindowWidth > 0) ? ((float)sz.cx * 640.0f / (float)WindowWidth) : (float)strlen(line) * 5.0f;

		if (textW > (float)CHAT_LOG_WIDTH - 16.0f)
		{
			textW = (float)CHAT_LOG_WIDTH - 16.0f;
		}

		const int textY = CHAT_LOG_POS_Y - (i + 1) * CHAT_LINE_H + 2;
		const float bx = 2.0f;
		const float by = (float)textY - 2.0f;
		const float bw = textW + 14.0f;
		const float bh = 14.0f;

		// same look as the notice banners: steel frame, dark body, colored accent bar on the left
		ChatPanelRect(bx, by, bw, bh, 0.23f, 0.27f, 0.32f, 0.85f);
		ChatPanelRect(bx + 1.0f, by + 1.0f, bw - 2.0f, bh - 2.0f, 0.05f, 0.07f, 0.09f, 0.75f);
		ChatPanelRect(bx, by, 2.0f, bh, r, g, b, 1.0f);

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		DisableAlphaBlend();

		SetTextColor = textColor;
		SetBackgroundTextColor = 0x00000000;
		RenderText((int)bx + 8, textY, line, 0, RT3_SORT_LEFT, NULL);
	}

	SetTextColor = oldTextColor;
	SetBackgroundTextColor = oldBgColor;
	SelectObject(m_hFontDC, oldFont);
}


// ---------------------------------------------------------------------------------------------------------------------
// Chat input box, flat digital style. The original function (0x004BE4F0) draws it with three RenderBitmap calls:
//   0x004BE562 texture 248 (left half), 0x004BE592 texture 249 (right half), 0x004BE701 texture 239 (button).
// The text of the two fields (chat / whisper target) is drawn separately, so only the artwork is replaced.
// ---------------------------------------------------------------------------------------------------------------------
static void _cdecl ChatInputBackgroundHook(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight, bool Scale, bool StartScale)
{
	if (Texture != 248) // the right half (249) is covered by the single flat panel drawn with the left half
	{
		return;
	}

	const float X = 0.0f;

	const float Y = (float)CHAT_INPUT_Y;

	const float W = (float)CHAT_LOG_WIDTH;

	const float H = 29.0f;

	// frame + accent bar
	ChatPanelRect(X, Y, W, H, 0.23f, 0.27f, 0.32f, 1.0f);

	ChatPanelRect(X + 1.0f, Y + 1.0f, W - 2.0f, H - 2.0f, 0.09f, 0.11f, 0.13f, 0.97f);

	ChatPanelRect(X, Y, 2.0f, H, 0.435f, 0.714f, 0.769f, 1.0f);

	// chat text field and whisper target field
	ChatPanelRect(X + 4.0f, Y + 3.0f, 184.0f, 23.0f, 0.17f, 0.21f, 0.25f, 1.0f);

	ChatPanelRect(X + 5.0f, Y + 4.0f, 182.0f, 21.0f, 0.03f, 0.04f, 0.05f, 1.0f);

	// Subtle cyan top highlight on active chat field
	if (*(BYTE*)0x00559C84 != 0)
	{
		ChatPanelRect(X + 5.0f, Y + 4.0f, 182.0f, 1.0f, 0.435f, 0.714f, 0.769f, 0.55f);
	}

	ChatPanelRect(X + 191.0f, Y + 3.0f, 53.0f, 23.0f, 0.17f, 0.21f, 0.25f, 1.0f);

	ChatPanelRect(X + 192.0f, Y + 4.0f, 51.0f, 21.0f, 0.07f, 0.09f, 0.11f, 1.0f);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	DisableAlphaBlend();
}

static void _cdecl ChatInputButtonHook(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight, bool Scale, bool StartScale)
{
	const bool hover = IsWorkZone((int)x, (int)y, (int)Width, (int)Height);

	ChatPanelRect(x, y, Width, Height, 0.23f, 0.27f, 0.32f, 1.0f);

	if (hover)
	{
		ChatPanelRect(x + 1.0f, y + 1.0f, Width - 2.0f, Height - 2.0f, 0.30f, 0.56f, 0.62f, 1.0f);
	}
	else
	{
		ChatPanelRect(x + 1.0f, y + 1.0f, Width - 2.0f, Height - 2.0f, 0.13f, 0.16f, 0.19f, 1.0f);
	}

	// right pointing arrow made of 4 columns
	const float step = 1.5f;

	const float cx = x + Width / 2.0f;

	const float cy = y + Height / 2.0f;

	if (hover)
	{
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else
	{
		glColor4f(0.435f, 0.714f, 0.769f, 1.0f);
	}

	for (int k = 0; k < 4; k++)
	{
		float hh = (float)(4 - k) * 2.0f * step;

		RenderColor(cx - 2.0f * step + (float)k * step, cy - hh / 2.0f, step, hh);
	}

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	DisableAlphaBlend();
}

// ---------------------------------------------------------------------------------------------------------------------
// Cold Steel HUD. The original bottom interface (0x004BBFB0, called once from 0x00525CB9) is drawn HUD_SHIFT_Y units higher:
// the 2D projection is translated, the mouse is shifted the opposite way for its hit tests, and the one function that
// does not go through the 2D projection (RenderItem3D, 0x004E1BE0 called from 0x004BFF1D for the potion slots) is shifted by
// hand. The old bar textures (230..232) are replaced by the Cold Steel frame, slots and buttons drawn from texture 233.
// All coordinates of the art below are FINAL screen units (what the player sees); HudSprite adds the shift because the
// drawing runs inside the shifted region.
// ---------------------------------------------------------------------------------------------------------------------
void HudBegin()
{
	glDisable(GL_FOG);

	glEnable(GL_ALPHA_TEST);

	glAlphaFunc(GL_GREATER, 0.15f);

	glEnable(GL_BLEND);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glColor3f(1.0f, 1.0f, 1.0f);
}

void HudEnd()
{
	glDisable(GL_BLEND);

	glDisable(GL_ALPHA_TEST);
}

void HudSprite(float ax, float ay, float aw, float ah, float x, float y, float w, float h)
{
	RenderBitmap(233, x, y + HUD_SHIFT_Y, w, h, ax / HUD_ATLAS_W, ay / HUD_ATLAS_H, aw / HUD_ATLAS_W, ah / HUD_ATLAS_H, true, true);
}

// EXP bar (final rect 208,447 110x11.4) and AG bar (final rect 326,444 110x17.3): the pack meter frames with their fills inside
#define EXP_X 208.0f
#define EXP_Y 447.0f
#define EXP_W 110.0f
#define EXP_H 11.4f

#define AG_X 326.0f
#define AG_Y 444.0f
#define AG_W 110.0f
#define AG_H 17.3f

void HudDrawExpBar(float frac, float* hx, float* hy, float* hw, float* hh)
{
	HudBegin();

	HudSprite(0.0f, 860.0f, 348.0f, 27.0f, EXP_X, EXP_Y, EXP_W, EXP_H);

	frac = (frac < 0.0f) ? 0.0f : ((frac > 1.0f) ? 1.0f : frac);

	const float ix = EXP_X + EXP_W * 0.058f;

	const float iw = EXP_W * 0.884f;

	const float iy = EXP_Y + EXP_H * 0.31f;

	const float ih = EXP_H * 0.38f;

	if (frac > 0.0f)
	{
		RenderBitmap(233, ix, iy + HUD_SHIFT_Y, iw * frac, ih, 270.0f / HUD_ATLAS_W, 900.0f / HUD_ATLAS_H, (244.0f * frac) / HUD_ATLAS_W, 39.0f / HUD_ATLAS_H, true, true);
	}

	HudEnd();

	*hx = EXP_X;

	*hy = EXP_Y + HUD_SHIFT_Y;

	*hw = EXP_W;

	*hh = EXP_H;
}

void HudDrawAgBar(float frac)
{
	HudBegin();

	HudSprite(0.0f, 810.0f, 358.0f, 42.0f, AG_X, AG_Y, AG_W, AG_H);

	frac = (frac < 0.0f) ? 0.0f : ((frac > 1.0f) ? 1.0f : frac);

	const float ix = AG_X + AG_W * 0.09f;

	const float iw = AG_W * 0.82f;

	const float iy = AG_Y + AG_H * 0.33f;

	const float ih = AG_H * 0.34f;

	if (frac > 0.0f)
	{
		RenderBitmap(233, ix, iy + HUD_SHIFT_Y, iw * frac, ih, 0.0f / HUD_ATLAS_W, 900.0f / HUD_ATLAS_H, (253.0f * frac) / HUD_ATLAS_W, 46.0f / HUD_ATLAS_H, true, true);
	}

	HudEnd();
}

static void HudSquare(float ax, float ay, float aw, float ah, float cx, float cy, float h, float asp)
{
	// a sprite centred at (cx, cy), h units tall, wide enough to look undistorted on a wide window
	const float w = h * (aw / ah) * asp;

	HudSprite(ax, ay, aw, ah, cx - w / 2.0f, cy - h / 2.0f, w, h);
}

void HudButtonActive(int index)
{
	const float asp = (g_fScreenRate_x > 0.0f) ? (g_fScreenRate_y / g_fScreenRate_x) : 0.75f;

	const float bx[3] = { 358.0f, 391.0f, 422.0f };

	if (index < 0 || index > 2)
	{
		return;
	}

	HudBegin();

	// additive cyan glow over the button frame = "window is open"
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glColor4f(0.15f, 0.75f, 1.0f, 0.6f);

	HudSquare(1250.0f, 570.0f, 199.0f, 196.0f, bx[index], 424.0f, 31.0f, asp);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	HudEnd();
}

// 0x00525BD8: BeginOpengl(0, 0, width, height) for the 3D world. While inventory / character / ... windows are open the width
// shrinks (190 units per window) and the rest of the screen was black behind the windows. Render the world over the whole width
// instead; the windows are drawn on top of it (a negative viewport origin does not work: the whole view turns black).
static void _cdecl HudBeginOpenglHook(int x, int y, int w, int h)
{
	if (x == 0 && y == 0 && w > 0 && w < 640)
	{
		w = 640;
	}

	((void(_cdecl*)(int, int, int, int)) 0x005119B0)(x, y, w, h);
}

static void _cdecl HudBarHook(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight, bool Scale, bool StartScale)
{
	const float asp = (g_fScreenRate_x > 0.0f) ? (g_fScreenRate_y / g_fScreenRate_x) : 0.75f;

	HudBegin();

	// hotbar frame: the pack's full backplate, 3-sliced so the corner ornaments keep their shape
	const float FX0 = 170.0f, FX1 = 470.0f, FY = 374.0f, FH = 106.0f;

	const float capw = 165.0f * (FH / 232.0f) * asp;

	HudSprite(0.0f, 570.0f, 165.0f, 232.0f, FX0, FY, capw, FH);

	HudSprite(741.0f - 165.0f, 570.0f, 165.0f, 232.0f, FX1 - capw, FY, capw, FH);

	HudSprite(165.0f, 570.0f, 411.0f, 232.0f, FX0 + capw - 0.3f, FY, (FX1 - FX0) - 2.0f * capw + 0.6f, FH);

	// potion slots Q W E, selected skill slot, three buttons
	const float cy = 424.0f;

	const float cxs[3] = { 217.0f, 249.0f, 281.0f };

	for (int i = 0; i < 3; i++)
	{
		HudSquare(760.0f, 570.0f, 234.0f, 229.0f, cxs[i], cy, 31.0f, asp);
	}

	HudSquare(1010.0f, 570.0f, 225.0f, 224.0f, 319.5f, 417.0f, 38.0f, asp);

	const float bx[3] = { 358.0f, 391.0f, 422.0f };

	for (int i = 0; i < 3; i++)
	{
		HudSquare(1250.0f, 570.0f, 199.0f, 196.0f, bx[i], cy, 31.0f, asp);

		HudSquare(1470.0f + 70.0f * i, 710.0f, 64.0f, 64.0f, bx[i], cy, 17.0f, asp);
	}

	HudEnd();

	// hotkey letters
	DWORD backupBg = SetBackgroundTextColor;

	DWORD backupText = SetTextColor;

	EnableAlphaTest(true);

	SetBackgroundTextColor = Color4b(255, 255, 255, 0);

	SetTextColor = Color4b(32, 216, 245, 255);

	RenderText((int)(cxs[0] - 12.0f), (int)(cy + 4.0f + HUD_SHIFT_Y), "Q", 0, RT3_SORT_LEFT, NULL);

	RenderText((int)(cxs[1] - 12.0f), (int)(cy + 4.0f + HUD_SHIFT_Y), "W", 0, RT3_SORT_LEFT, NULL);

	RenderText((int)(cxs[2] - 12.0f), (int)(cy + 4.0f + HUD_SHIFT_Y), "E", 0, RT3_SORT_LEFT, NULL);

	SetBackgroundTextColor = backupBg;

	SetTextColor = backupText;

	DisableAlphaBlend();
}

// 0x004B14F0 is the big interface update function (hot keys C / I / P ..., clicks on the bottom buttons and slots), called from
// 0x005254C2. It hit-tests in the ORIGINAL coordinates, HUD_SHIFT_Y units lower than the HUD is drawn now: while the cursor is
// over the new HUD, present the mouse to it at its original place.
static void _cdecl HudUpdateHook()
{
	const int backupMouseY = MouseY;

	// (y >= 396: below the bottom edge of the right / left windows, so their own hit tests are not displaced)
	// the skill list that pops up above the skill slot is drawn HUD_SHIFT_Y units higher too, so its click areas follow it
	// (only while no window is open: the windows' own hit tests are in plain coordinates)
	const bool overSkillPopup = (!CheckRightInterfaces() && MouseX >= 250 && MouseX < 390 && MouseY >= 318 && MouseY < 396);

	if (MouseX >= 570 && MouseX < 640 && MouseY >= 432 && MouseY < 480)
	{
		// Bottom-right Guild HUD button was removed: prevent ghost clicks on its original hitbox
		MouseY = -100;
	}
	else if (overSkillPopup || (MouseY >= 396 && MouseY < 480 && MouseX >= 170 && MouseX < 470))
	{
		MouseY += (int)HUD_SHIFT_Y;
	}

	((void(_cdecl*)()) 0x004B14F0)();

	MouseY = backupMouseY;
}

static float _cdecl HudNoNumberHook(float x, float y, int Num, float Width, float Height)
{
	// the map coordinates ("x y" at the left of the old bar) are not shown any more
	return 0.0f;
}

static void _cdecl HudSkipHook(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight, bool Scale, bool StartScale)
{
	// the other two pieces of the old bar are not drawn any more
}

static void _cdecl HudItem3DHook(float sx, float sy, float Width, float Height, int Type, int Level, int Option1, bool PickUp)
{
	RenderItem3D(sx, sy - HUD_SHIFT_Y, Width, Height, Type, Level, Option1, PickUp);
}

// Map real screen mouse coords into unscaled Inventory coords (450..640, 0..432) while Inventory (V) is scaled down.
static void MapMouseForScaledInv(int origX, int origY, int* outX, int* outY)
{
	const float scaledBottom = INV_PIVOT_Y + 432.0f * INV_SCALE;
	if ((float)origY >= INV_PIVOT_Y && (float)origY <= scaledBottom + 2.0f)
	{
		*outX = (int)(640.0f - (INV_PIVOT_X - (float)origX) / INV_SCALE);
		*outY = (int)(((float)origY - INV_PIVOT_Y) / INV_SCALE);
	}
	else if ((float)origY > scaledBottom + 2.0f)
	{
		// Keep clicks on the bottom HUD / MP orb from hitting the old unscaled inventory bottom
		*outX = origX;
		*outY = 460;
	}
	else
	{
		*outX = origX;
		*outY = origY;
	}
}

static inline bool HasScaledSideWindow()
{
	return (InventoryOpened != 0 || CharacterOpened != 0 || PartyOpened != 0 || GuildOpened != 0 || GuildCreatorOpened != 0);
}

// Sealed metallic top & bottom frame caps for scaled floating side windows (V / C / P / G and companion left panels).
static void RenderScaledInvFrameCaps()
{
	if (!HasScaledSideWindow())
	{
		return;
	}

	const bool hasLeftWin = (GetScreenWidth() <= 260);
	const float totalW = (hasLeftWin ? 380.0f : 190.0f) * INV_SCALE;
	const float x0 = INV_PIVOT_X - totalW;
	const float y0 = INV_PIVOT_Y;
	const float y1 = INV_PIVOT_Y + 432.0f * INV_SCALE;

	// Outer left & right 1px steel trim connecting top and bottom caps
	ChatPanelRect(x0 - 1.0f, y0 - 3.0f, 1.0f, (y1 - y0) + 6.0f, 0.42f, 0.47f, 0.53f, 0.92f);
	ChatPanelRect(x0 + totalW, y0 - 3.0f, 1.0f, (y1 - y0) + 6.0f, 0.42f, 0.47f, 0.53f, 0.92f);

	// Top cap (steel rim + dark gunmetal core + subtle cyan center accent)
	ChatPanelRect(x0 - 1.0f, y0 - 3.0f, totalW + 2.0f, 4.0f, 0.45f, 0.50f, 0.56f, 0.96f);
	ChatPanelRect(x0, y0 - 2.0f, totalW, 2.0f, 0.10f, 0.12f, 0.15f, 0.98f);
	ChatPanelRect(x0 + totalW * 0.22f, y0 - 2.0f, totalW * 0.56f, 1.0f, 0.435f, 0.714f, 0.769f, 0.85f);

	// Bottom cap (steel rim + dark gunmetal core + subtle cyan center accent)
	ChatPanelRect(x0 - 1.0f, y1 - 1.0f, totalW + 2.0f, 4.0f, 0.45f, 0.50f, 0.56f, 0.96f);
	ChatPanelRect(x0, y1, totalW, 2.0f, 0.10f, 0.12f, 0.15f, 0.98f);
	ChatPanelRect(x0 + totalW * 0.22f, y1 + 1.0f, totalW * 0.56f, 1.0f, 0.435f, 0.714f, 0.769f, 0.85f);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

// The three last calls of the bottom interface function (0x004F5820, 0x004F6050, 0x004EB070, called from 0x004BC0DE, 0x004BC10C and
// 0x004BC111) draw the inventory / character / party / guild windows. When any of V/C/P/G is open, scale calls 1 and 2 down
// cleanly above the Cold Steel HUD; call 3 (tooltips) stays unscaled for crisp readability.
#define HUD_INV_SCALED_CALL(NAME, ADDRESS, DRAW_CAPS)                          \
static void _cdecl NAME()                                                      \
{                                                                              \
	const bool was = g_HudShiftActive;                                         \
	const int backupMouseX = MouseX;                                           \
	const int backupMouseY = MouseY;                                           \
	int realY = was ? (MouseY - (int)HUD_SHIFT_Y) : MouseY;                    \
	g_HudShiftActive = false;                                                  \
	if (HasScaledSideWindow())                                                 \
	{                                                                          \
		int mx, my;                                                            \
		MapMouseForScaledInv(backupMouseX, realY, &mx, &my);                   \
		MouseX = mx;                                                           \
		MouseY = my;                                                           \
		g_InvScaleActive = true;                                               \
	}                                                                          \
	else                                                                       \
	{                                                                          \
		MouseY = realY;                                                        \
	}                                                                          \
	((void(_cdecl*)()) ADDRESS)();                                             \
	g_InvScaleActive = false;                                                  \
	if (DRAW_CAPS)                                                             \
	{                                                                          \
		RenderScaledInvFrameCaps();                                            \
	}                                                                          \
	g_HudShiftActive = was;                                                    \
	MouseX = backupMouseX;                                                     \
	MouseY = backupMouseY;                                                     \
}

#define HUD_UNSHIFTED_CALL(NAME, ADDRESS)                                      \
static void _cdecl NAME()                                                      \
{                                                                              \
	const bool was = g_HudShiftActive;                                         \
	const int backupMouseY = MouseY;                                           \
	if (was)                                                                   \
	{                                                                          \
		g_HudShiftActive = false;                                              \
		MouseY -= (int)HUD_SHIFT_Y;                                            \
	}                                                                          \
	((void(_cdecl*)()) ADDRESS)();                                             \
	g_HudShiftActive = was;                                                    \
	MouseY = backupMouseY;                                                     \
}

HUD_INV_SCALED_CALL(HudWindowsCall1, 0x004F5820, false)
HUD_INV_SCALED_CALL(HudWindowsCall2, 0x004F6050, true)
HUD_UNSHIFTED_CALL(HudWindowsCall3, 0x004EB070)
HUD_UNSHIFTED_CALL(HudPickedItem3DCall, 0x004F6420)

// 0x004BFEB4: call 0x004F5CE0 (inside 0x004BBFB0) draws all 3D equipment & inventory/shop/warehouse/trade items.
static void _cdecl HudWindows3DItemsCall()
{
	const bool wasShift = g_HudShiftActive;
	const int backupMouseY = MouseY;

	if (wasShift)
	{
		g_HudShiftActive = false;
		MouseY -= (int)HUD_SHIFT_Y;
	}

	g_InvScaleActive = (InventoryOpened != 0);

	((void(_cdecl*)()) 0x004F5CE0)();

	g_InvScaleActive = false;
	g_HudShiftActive = wasShift;
	MouseY = backupMouseY;
}

// 0x004E1BCB: call 0x00505A10 inside RenderObjectScreen — scale 3D item model size to match the scaled slots.
static void _cdecl HookInvRenderObject(DWORD o, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7, DWORD a8, DWORD a9, DWORD a10, DWORD a11, DWORD a12)
{
	if (g_InvScaleActive && o)
	{
		*(float*)(o + 0xC) *= INV_SCALE;
	}

	((void(_cdecl*)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)) 0x00505A10)(o, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12);
}

static void _cdecl RenderInterfaceShifted(int arg)
{
	// Every draw primitive (RenderBitmap / RenderText / RenderColor / tooltips / input box text, see LoginScale.cpp) moves its y
	// by -HUD_SHIFT_Y while g_HudShiftActive is set; the mouse is shifted the opposite way for the hit tests.
	const int backupMouseY = MouseY;

	MouseY += (int)HUD_SHIFT_Y;

	g_HudShiftActive = true;

	((void(_cdecl*)(int)) 0x004BBFB0)(arg);

	g_HudShiftActive = false;

	MouseY = backupMouseY;
}

// Floating "+N Exp" popups above the hero (inspired by Muemu97) and chat EXP-spam filter
struct EXP_POPUP
{
	bool used;
	DWORD exp;
	DWORD tick;
};

static EXP_POPUP g_ExpPopups[8] = { 0 };
static BYTE* g_pAddTextTrampoline = NULL;

void Interface::AddExpPopup(DWORD exp)
{
	if (exp == 0)
	{
		return;
	}

	int slot = -1;
	DWORD oldest = 0xFFFFFFFF;

	for (int i = 0; i < 8; i++)
	{
		if (!g_ExpPopups[i].used)
		{
			slot = i;
			break;
		}
		if (g_ExpPopups[i].tick < oldest)
		{
			oldest = g_ExpPopups[i].tick;
			slot = i;
		}
	}

	if (slot >= 0)
	{
		g_ExpPopups[slot].used = true;
		g_ExpPopups[slot].exp = exp;
		g_ExpPopups[slot].tick = GetTickCount();
	}
}

static void RenderExpPopups()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		return;
	}

	vec3_t pos = {
		*(float*)(Hero + 0x10),
		*(float*)(Hero + 0x14),
		*(float*)(Hero + 0x12C) + *(float*)(Hero + 0x18) + 95.0f
	};

	int sx = 0, sy = 0;
	Projection(pos, &sx, &sy);

	const DWORD now = GetTickCount();

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFontBold);
	const DWORD oldTextColor = SetTextColor;
	const DWORD oldBgColor = SetBackgroundTextColor;

	EnableAlphaTest(true);
	DisableAlphaBlend();
	glColor3f(1.0f, 1.0f, 1.0f);
	SetBackgroundTextColor = 0x00000000;

	for (int i = 0; i < 8; i++)
	{
		EXP_POPUP& p = g_ExpPopups[i];
		if (!p.used)
		{
			continue;
		}

		const DWORD age = now - p.tick;
		if (age > 1600)
		{
			p.used = false;
			continue;
		}

		char text[48];
		sprintf_s(text, sizeof(text), "+%u Exp", p.exp);

		SIZE sz = { 0, 0 };
		GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
		const int textW = (WindowWidth > 0) ? (int)((float)sz.cx * 640.0f / (float)WindowWidth) : 50;

		const int tx = sx - textW / 2;
		const int ty = sy - (int)(age * 0.035f);

		SetTextColor = 0xFF000000;
		RenderText(tx + 1, ty + 1, text, 0, RT3_SORT_LEFT, NULL);
		SetTextColor = 0xFF78FF5A; // bright green (ABGR: R=90, G=255, B=120)
		RenderText(tx, ty, text, 0, RT3_SORT_LEFT, NULL);
	}

	SelectObject(m_hFontDC, oldFont);
	SetTextColor = oldTextColor;
	SetBackgroundTextColor = oldBgColor;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

static void __cdecl AddTextFilterHook(const char* strID, const char* strText, int MsgType)
{
	if (g_pAddTextTrampoline)
	{
		((void(__cdecl*)(const char*, const char*, int))g_pAddTextTrampoline)(strID, strText, MsgType);
	}
}

// 0x00525CA9: call 0x004CB6F0, the first call of the 2D pass (right after 0x005123C0); draw the frame here so that all
// the 2D text (including the floating chat log) ends up on top of it.
static void _cdecl ChatLogRenderHook()
{
	RenderExpPopups();

	RenderFloatingChatLog();

	((void(_cdecl*)()) 0x004CB6F0)();
}

// 0x00525CCB: RenderNotices (0x0047FCE0) — render system/event notices as sleek top-center Cold Steel banners
// instead of blocking the middle of the screen (y = 300).
static void _cdecl RenderNoticesHook()
{
	if (*(BYTE*)0x0047FCE0 == 0xC3) // respect OptionsMenu "Disable Notices"
	{
		return;
	}

	struct NoticeEntry
	{
		char Text[260];
		BYTE Color;
		BYTE Pad[3];
	};

	NoticeEntry* notices = (NoticeEntry*)0x07DB80D8;
	int& noticeFrame = *(int*)0x07E11DA0;
	noticeFrame++;

	// Auto-expire each notice after 8 seconds instead of lingering for ~72s across all 6 ring slots
	struct NoticeSeen
	{
		bool used;
		char text[260];
		DWORD firstSeen;
	};
	static NoticeSeen s_seen[6] = { 0 };
	const DWORD now = GetTickCount();

	// Evict cache entries that are no longer present in the notice buffer
	for (int s = 0; s < 6; s++)
	{
		if (!s_seen[s].used) continue;
		bool stillPresent = false;
		for (int i = 0; i < 6; i++)
		{
			if (notices[i].Text[0] != '\0' && strcmp(notices[i].Text, s_seen[s].text) == 0)
			{
				stillPresent = true;
				break;
			}
		}
		if (!stillPresent)
		{
			s_seen[s].used = false;
		}
	}

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFontBold);
	DWORD oldTextColor = SetTextColor;
	DWORD oldBgColor = SetBackgroundTextColor;

	int curY = 44; // cleanly below the monster HP bar and clear of the center character

	for (int i = 0; i < 6; i++)
	{
		if (notices[i].Text[0] == '\0')
		{
			continue;
		}

		// Find or register first-seen timestamp for this notice text
		int slot = -1;
		int freeSlot = -1;
		for (int s = 0; s < 6; s++)
		{
			if (s_seen[s].used && strcmp(s_seen[s].text, notices[i].Text) == 0)
			{
				slot = s;
				break;
			}
			if (!s_seen[s].used && freeSlot < 0)
			{
				freeSlot = s;
			}
		}

		if (slot < 0)
		{
			slot = (freeSlot >= 0) ? freeSlot : 0;
			s_seen[slot].used = true;
			strcpy_s(s_seen[slot].text, sizeof(s_seen[slot].text), notices[i].Text);
			s_seen[slot].firstSeen = now;
		}

		if (now - s_seen[slot].firstSeen >= 8000)
		{
			notices[i].Text[0] = '\0';
			s_seen[slot].used = false;
			continue;
		}

		SIZE sz = { 0, 0 };
		GetTextExtentPoint32A(m_hFontDC, notices[i].Text, (int)strlen(notices[i].Text), &sz);

		const float textW = (WindowWidth > 0) ? ((float)sz.cx * 640.0f / (float)WindowWidth) : 140.0f;
		float bannerW = textW + 24.0f;
		if (bannerW < 110.0f)
		{
			bannerW = 110.0f;
		}
		const float bannerH = 16.0f;
		const float bx = 320.0f - bannerW * 0.5f;
		const float by = (float)curY - 2.0f;

		ChatPanelRect(bx, by, bannerW, bannerH, 0.23f, 0.27f, 0.32f, 0.88f);
		ChatPanelRect(bx + 1.0f, by + 1.0f, bannerW - 2.0f, bannerH - 2.0f, 0.05f, 0.07f, 0.09f, 0.78f);

		if (notices[i].Color == 0)
		{
			ChatPanelRect(bx, by, 2.0f, bannerH, 0.95f, 0.76f, 0.28f, 0.95f);
			SetTextColor = ((noticeFrame % 20) < 10) ? 0xFF55D2FF : 0xFF8CE2FF;
		}
		else
		{
			ChatPanelRect(bx, by, 2.0f, bannerH, 0.435f, 0.714f, 0.769f, 0.95f);
			SetTextColor = 0xFFDCE8EE;
		}

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		DisableAlphaBlend();

		SetBackgroundTextColor = 0x00000000;
		RenderText((int)(320.0f - textW * 0.5f), curY, notices[i].Text, 0, RT3_SORT_LEFT, NULL);

		curY += 18;
	}

	SetTextColor = oldTextColor;
	SetBackgroundTextColor = oldBgColor;
	SelectObject(m_hFontDC, oldFont);
}

void Interface::Init()
{
	PatchChatPositions();

	SetCompleteHook(0xE8, 0x00525CA9, &ChatLogRenderHook);

	SetDword(0x005525CC + 0x10, (DWORD)&ChatLogWindowRenderHook);

	SetCompleteHook(0xE8, 0x00525CCB, &RenderNoticesHook);

	g_pAddTextTrampoline = (BYTE*)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (g_pAddTextTrampoline)
	{
		memcpy(g_pAddTextTrampoline, (void*)0x00480620, 6);
		g_pAddTextTrampoline[6] = 0xE9;
		*(DWORD*)(g_pAddTextTrampoline + 7) = (0x00480620 + 6) - ((DWORD)g_pAddTextTrampoline + 6 + 5);
		SetCompleteHook(0xE9, 0x00480620, &AddTextFilterHook);
		SetByte(0x00480620 + 5, 0x90);
	}

	// Cold Steel HUD
	SetCompleteHook(0xE8, 0x00525CB9, &RenderInterfaceShifted);

	// Extend Tarkan/underwater fullscreen RenderBitmapUV overlays (0x494/0x495) from 435.0f to full 480.0f height
	SetDword(0x004BC032, FloatBits(480.0f));
	SetDword(0x004BC07E, FloatBits(480.0f));
	SetDword(0x004BC1A2, FloatBits(480.0f));
	SetDword(0x004BC1EE, FloatBits(480.0f));

	SetCompleteHook(0xE8, 0x004BD364, &HudBarHook);

	SetCompleteHook(0xE8, 0x004BD602, &HudSkipHook);

	SetCompleteHook(0xE8, 0x004BD632, &HudSkipHook);

	SetCompleteHook(0xE8, 0x004BFF1D, &HudItem3DHook);

	SetCompleteHook(0xE8, 0x00525BD8, &HudBeginOpenglHook);

	SetCompleteHook(0xE8, 0x004BC0DE, &HudWindowsCall1);

	SetCompleteHook(0xE8, 0x004BC10C, &HudWindowsCall2);

	SetCompleteHook(0xE8, 0x004BC111, &HudWindowsCall3);

	SetCompleteHook(0xE8, 0x004BFEB4, &HudWindows3DItemsCall);

	SetCompleteHook(0xE8, 0x004BFECB, &HudPickedItem3DCall);

	SetCompleteHook(0xE8, 0x004E1BCB, &HookInvRenderObject);

	SetCompleteHook(0xE8, 0x005254C2, &HudUpdateHook);

	SetCompleteHook(0xE8, 0x004BE0F9, &HudNoNumberHook);

	SetCompleteHook(0xE8, 0x004BE1CC, &HudNoNumberHook);

	// The click areas: the original client treats the mouse as being over the interface bar when MouseY >= 432 (0x1B0) and over
	// the game world when MouseY < 433 (0x1B1). The new HUD starts at y = 374 (frame top), so move those limits up.
	SetDword(0x0052548A + 4, 374);        // cmp [MouseY], 0x1B0 -> 374 (mouse is on the interface)
	SetDword(0x00402886 + 9, 375);        // cmp eax, 0x1B1 -> 375 (mouse is in the world)
	SetDword(0x00402F57 + 9, 375);

	SetCompleteHook(0xE8, 0x004BE562, &ChatInputBackgroundHook);

	SetCompleteHook(0xE8, 0x004BE592, &ChatInputBackgroundHook);

	SetCompleteHook(0xE8, 0x004BE701, &ChatInputButtonHook);

	SetCompleteHook(0xE8, 0x0052698A, &this->RenderLogInScene);

	SetCompleteHook(0xE8, 0x005269A2, &this->RenderCharacterScene);

	SetCompleteHook(0xE8, 0x005269B6, &this->RenderMainScene);

	SetCompleteHook(0xE8, 0x00510DEA, &this->LoadImages);

	SetCompleteHook(0xE8, 0x005254B2, &this->MyUpdateWindowsMouse);

	SetCompleteHook(0xE8, 0x00525CEC, &this->MyRenderWindows);

	SetCompleteHook(0xE8, 0x004BD300, &this->RenderLeftDragon);

	SetCompleteHook(0xE8, 0x004BD332, &this->RenderRightDragon);
}

void Interface::RenderLogInScene(HDC Hdc)
{
	gWindow.ChangeWindowText();

	((void(__cdecl*)(HDC Hdc)) 0x00521630)(Hdc);

	CLoginScale::ResetRender();
}

void Interface::RenderCharacterScene(HDC Hdc)
{
	gWindow.ChangeWindowText();

	((void(__cdecl*)(HDC Hdc)) 0x00523B30)(Hdc);
}

void Interface::RenderMainScene()
{
	gWindow.ChangeWindowText();

	((void(__cdecl*)()) 0x00525A00)();
}

void Interface::LoadImages()
{
	((void(__cdecl*)()) 0x0050EB80)(); // OpenImages

	int w = 0, h = 0, ch = 0;
	unsigned char* pixels = stbi_load("Data\\Interface\\SkillHD.png", &w, &h, &ch, 4);
	if (pixels && w > 0 && h > 0)
	{
		BITMAP_t* bmp = gLoadModels.GetBitmap(0x12A);
		if (bmp && bmp->TextureNumber != 0)
		{
			glBindTexture(GL_TEXTURE_2D, bmp->TextureNumber);

			// Upload base level + all mipmap levels so downscaling to 24x32 skill slots stays razor-sharp
			int mw = w;
			int mh = h;
			unsigned char* cur = pixels;
			unsigned char* nextBuf = NULL;

			for (int level = 0; ; level++)
			{
				glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, mw, mh, 0, GL_RGBA, GL_UNSIGNED_BYTE, cur);
				if (mw == 1 && mh == 1)
				{
					break;
				}

				int nw = (mw > 1) ? (mw / 2) : 1;
				int nh = (mh > 1) ? (mh / 2) : 1;
				unsigned char* dst = (unsigned char*)malloc((size_t)nw * nh * 4);
				if (!dst)
				{
					break;
				}

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

				if (nextBuf)
				{
					free(nextBuf);
				}
				nextBuf = dst;
				cur = dst;
				mw = nw;
				mh = nh;
			}

			if (nextBuf)
			{
				free(nextBuf);
			}

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		}
		stbi_image_free(pixels);
	}

	gMiniMap.LoadImages();

	Camera3D.LoadImages();

	gCustomComboUI.LoadImages();
}

void Interface::MyUpdateWindowsMouse()
{
	if (HasScaledSideWindow())
	{
		const int origX = MouseX;
		const int origY = MouseY;
		int mx, my;
		MapMouseForScaledInv(origX, origY, &mx, &my);
		MouseX = mx;
		MouseY = my;
		UpdateWindowsMouse();
		MouseX = origX;
		MouseY = origY;
	}
	else
	{
		UpdateWindowsMouse();
	}

	gMiniMap.UpdateMouse();

	Camera3D.UpdateMouse();

	gCustomComboUI.UpdateMouse();

	gMoveList.UpdateMouse();

	gEventTimer.UpdateMouse();
}

void Interface::MyRenderWindows()
{
	((void(_cdecl*)()) 0x004C3530)();

	gMiniMap.Render();

	Camera3D.Render();

	gCustomComboUI.Render();

	gBuff.Render();

	gMoveList.Render();

	gEventTimer.Render();
}

void Interface::RenderLeftDragon(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight, bool Scale, bool StartScale)
{
	if (gMiniMap.GetMiniMapState() && gMapManager.GetMiniMap(World) == 2)
	{
		return;
	}

	// the dragon corner art (texture 233) is now the orb atlas: the orb frames are drawn by CPrintPlayer::RenderCircleHPMP
}

void Interface::RenderRightDragon(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight, bool Scale, bool StartScale)
{
	if (CheckRightInterfaces())
	{
		return;
	}

	if (gMiniMap.GetMiniMapState())
	{
		return;
	}

	// the dragon corner art (texture 233) is now the orb atlas: the orb frames are drawn by CPrintPlayer::RenderCircleHPMP
}

void Interface::BindObject(short MonsterID, DWORD ModelID, float Width, float Height, float X, float Y)
{
	this->Data[MonsterID].EventTick = 0;

	this->Data[MonsterID].OnClick = false;

	this->Data[MonsterID].OnShow = false;

	this->Data[MonsterID].ModelID = ModelID;

	this->Data[MonsterID].Width = Width;

	this->Data[MonsterID].Height = Height;

	this->Data[MonsterID].X = X;

	this->Data[MonsterID].Y = Y;

	this->Data[MonsterID].MaxX = X + Width;

	this->Data[MonsterID].MaxY = Y + Height;

	this->Data[MonsterID].Attribute = 0;
}

void Interface::DrawIMG(short ObjectID, float PosX, float PosY, float ScaleX, float ScaleY)
{
	if (this->Data[ObjectID].X == -1 || this->Data[ObjectID].Y == -1)
	{
		this->Data[ObjectID].X = PosX;

		this->Data[ObjectID].Y = PosY;

		this->Data[ObjectID].MaxX = PosX + this->Data[ObjectID].Width;

		this->Data[ObjectID].MaxY = PosY + this->Data[ObjectID].Height;
	}

	RenderBitmap(this->Data[ObjectID].ModelID, PosX, PosY, this->Data[ObjectID].Width, this->Data[ObjectID].Height, 0, 0, ScaleX, ScaleY, true, true);
}

void Interface::DrawIMG(short ObjectID, float PosX, float PosY, float u, float v, float ScaleX, float ScaleY)
{
	if (this->Data[ObjectID].X == -1 || this->Data[ObjectID].Y == -1)
	{
		this->Data[ObjectID].X = PosX;

		this->Data[ObjectID].Y = PosY;

		this->Data[ObjectID].MaxX = PosX + this->Data[ObjectID].Width;

		this->Data[ObjectID].MaxY = PosY + this->Data[ObjectID].Height;
	}

	RenderBitmap(this->Data[ObjectID].ModelID, PosX, PosY, this->Data[ObjectID].Width, this->Data[ObjectID].Height, u, v, ScaleX, ScaleY, true, true);
}

bool Interface::IsWorkZone(short ObjectID)
{
	if ((MouseX < this->Data[ObjectID].X || MouseX > this->Data[ObjectID].MaxX) || (MouseY < this->Data[ObjectID].Y || MouseY > this->Data[ObjectID].MaxY))
	{
		return false;
	}

	return true;
}