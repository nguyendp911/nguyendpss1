#include "stdafx.h"
#include "LoginScale.h"
#include "LoginRemember.h"
#include "Interface.h"
#include <intrin.h>

CLoginScale gLoginScale;

#define LOGIN_STATE (*(int*)0x083A7C14)   // 2..3 while the login window is shown
#define LOGIN_FRAME_Y (*(int*)0x005616A4) // animated top of the login frame (virtual 640x480 units)
#define LOGIN_FRAME_X 195.0f              // left of the login frame (the original art is 250x216 units)

static bool g_LoginActive = false;        // true while the login window block of RenderLogInScene is running

static bool g_MouseSaved = false;

static int g_SavedMouseX = 0;

static int g_SavedMouseY = 0;

static float g_RelX = -10000.0f;   // mouse position relative to the login frame (unscaled art units), valid during the block

static float g_RelY = -10000.0f;

static bool g_PrevDown = false;

typedef void(_cdecl* RENDERBITMAP_T)(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight, bool Scale, bool StartScale);

typedef int(_cdecl* RENDERTEXT_T)(int iPos_x, int iPos_y, const char* pszText, int iBoxWidth, int iSort, SIZE* lpTextSize);

static RENDERBITMAP_T g_OrigRenderBitmap = 0;

static RENDERTEXT_T g_OrigRenderText = 0;

// ---------------------------------------------------------------------------------------------------------------------
// Re-arranged layout. Rectangles are relative to the login frame, in the original 250x216 art units.
// The original code keeps drawing / hit-testing at the "Old" rectangles; we draw the matching art at the "New" ones,
// move the captions by the difference of the centres and remap the mouse from New back to Old.
// order: account field, password field, Connect, Exit
// ---------------------------------------------------------------------------------------------------------------------
struct LAYOUT_RECT
{
	float x0, y0, x1, y1;
};

static const LAYOUT_RECT g_Old[4] =
{
	{  99.0f,  46.0f, 223.0f,  64.0f },
	{  99.0f,  68.0f, 223.0f,  86.0f },
	{ 129.0f,  95.0f, 199.0f, 114.0f },
	{  90.0f, 180.0f, 160.0f, 200.0f },
};

static const LAYOUT_RECT g_New[4] =
{
	{  60.0f,  76.0f, 190.0f,  91.0f },
	{  60.0f, 105.0f, 190.0f, 120.0f },
	{  60.0f, 147.0f, 121.0f, 162.0f },
	{ 129.0f, 147.0f, 190.0f, 162.0f },
};

// Copies the first `stolen` bytes of `target` (whole instructions, position independent) and jumps back after them.
static void* MakeTrampoline(DWORD target, int stolen)
{
	BYTE* mem = (BYTE*)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	memcpy(mem, (void*)target, stolen);

	mem[stolen] = 0xE9;

	*(DWORD*)(mem + stolen + 1) = (target + stolen) - ((DWORD)mem + stolen + 5);

	return mem;
}

static void ScalePoint(float* x, float* y)
{
	*x = gLoginScale.m_PivotX + (*x - gLoginScale.m_PivotX) * gLoginScale.m_Scale;

	*y = gLoginScale.m_PivotY + (*y - gLoginScale.m_PivotY) * gLoginScale.m_Scale;
}

// Cold Steel HUD: while the bottom interface (Interface.cpp RenderInterfaceShifted) runs, every draw primitive is moved
// HUD_SHIFT_Y units up. Done in the primitives themselves instead of a GL translation, which leaked into the windows.
bool g_HudShiftActive = false;
bool g_InvScaleActive = false;

static inline float ScaleInvX(float x) { return INV_PIVOT_X - (640.0f - x) * INV_SCALE; }
static inline float ScaleInvY(float y) { return INV_PIVOT_Y + y * INV_SCALE; }

typedef void(_cdecl* RENDERCOLOR_T)(float x, float y, float width, float height);
typedef void(_cdecl* RENDERTIP_T)(int sx, int sy, char* text);
typedef void(_cdecl* INPUTBOX_T)(int x, int y, int index, int flag);

static RENDERCOLOR_T g_OrigRenderColor = 0;
static RENDERTIP_T g_OrigRenderTip = 0;
static INPUTBOX_T g_OrigInputBox = 0;

static void _cdecl HookRenderColor(float x, float y, float width, float height)
{
	if (g_HudShiftActive)
	{
		y -= HUD_SHIFT_Y;
	}
	else if (g_InvScaleActive)
	{
		x = ScaleInvX(x);
		y = ScaleInvY(y);
		width *= INV_SCALE;
		height *= INV_SCALE;
	}

	g_OrigRenderColor(x, y, width, height);
}

// These two call lower level helpers that are not hooked, but RenderTipText also calls RenderColor: shift the argument once and
// suspend the automatic shift while they run.
static void _cdecl HookRenderTip(int sx, int sy, char* text)
{
	bool was = g_HudShiftActive;

	if (was)
	{
		sy -= (int)HUD_SHIFT_Y;

		g_HudShiftActive = false;
	}

	g_OrigRenderTip(sx, sy, text);

	g_HudShiftActive = was;
}

static void _cdecl HookInputBoxEntry(int x, int y, int index, int flag)
{
	bool wasHud = g_HudShiftActive;
	bool wasInv = g_InvScaleActive;

	if (wasHud)
	{
		y -= (int)HUD_SHIFT_Y;

		g_HudShiftActive = false;
	}
	else if (wasInv)
	{
		x = (int)ScaleInvX((float)x);
		y = (int)ScaleInvY((float)y);

		g_InvScaleActive = false;
	}

	g_OrigInputBox(x, y, index, flag);

	g_HudShiftActive = wasHud;
	g_InvScaleActive = wasInv;
}

static void _cdecl HookRenderBitmap(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight, bool Scale, bool StartScale)
{
	// Cold Steel HUD: the pressed-state icons of the three bottom buttons (party / character / inventory, 24x24, drawn over the
	// bar at y ~452 in the original coordinates) are replaced by a highlight on the new buttons
	if ((Texture == 236 || Texture == 237 || Texture == 238) && y >= 440.0f && y < 470.0f && x >= 335.0f && x < 440.0f)
	{
		HudButtonActive(x < 372.0f ? 0 : (x < 404.0f ? 1 : 2));

		return;
	}

	if (g_HudShiftActive)
	{
		y -= HUD_SHIFT_Y;
	}
	else if (g_InvScaleActive && Scale)
	{
		x = ScaleInvX(x);
		y = ScaleInvY(y);
		Width *= INV_SCALE;
		Height *= INV_SCALE;
	}

#ifdef BITMAP_TRACE
	{
		static DWORD lastCheck = 0;

		static bool traceOn = false;

		DWORD now = GetTickCount();

		if (now - lastCheck > 1000)
		{
			lastCheck = now;

			traceOn = (GetFileAttributesA("trace.on") != INVALID_FILE_ATTRIBUTES);
		}

		if (traceOn)
		{
			static char seen[16384];

			static int seenCount = 0;

			char key[96];

			wsprintfA(key, "%d|%d|%d|%d|%d", Texture, (int)x, (int)y, (int)Width, (int)Height);

			bool found = false;

			for (int i = 0; i < seenCount && !found; i++)
			{
				found = (strncmp(seen + i * 48, key, 47) == 0);
			}

			if (!found && seenCount < 340)
			{
				strncpy_s(seen + seenCount * 48, 48, key, 47);

				seenCount++;

				FILE* f = fopen("trace.txt", "a");

				if (f)
				{
					fprintf(f, "tex=%d x=%.1f y=%.1f w=%.1f h=%.1f u=%.3f v=%.3f uw=%.3f vh=%.3f scale=%d ret=%08X\n", Texture, x, y, Width, Height, u, v, uWidth, vHeight, (int)Scale, (DWORD)_ReturnAddress());

					fclose(f);
				}
			}
		}
	}
#endif

	if (g_LoginActive && Scale)
	{
		// hover / pressed plates of the Connect (texture 13) and Exit (texture 14) buttons: draw them over the new button rectangles
		DWORD ret = (DWORD)_ReturnAddress();

		if (ret == 0x00521802 || ret == 0x005218B0)
		{
			const LAYOUT_RECT& n = (ret == 0x00521802) ? g_New[2] : g_New[3];

			x = LOGIN_FRAME_X + n.x0;

			y = (float)LOGIN_FRAME_Y + n.y0;

			Width = (n.x1 - n.x0);

			Height = (n.y1 - n.y0);
		}

		ScalePoint(&x, &y);

		Width *= gLoginScale.m_Scale;

		Height *= gLoginScale.m_Scale;
	}

	g_OrigRenderBitmap(Texture, x, y, Width, Height, u, v, uWidth, vHeight, Scale, StartScale);
}

static int _cdecl HookRenderText(int iPos_x, int iPos_y, const char* pszText, int iBoxWidth, int iSort, SIZE* lpTextSize)
{
	if (g_HudShiftActive)
	{
		iPos_y -= (int)HUD_SHIFT_Y;
	}

#ifdef BITMAP_TRACE
	if (pszText && strstr(pszText, "test1mg: ") && GetFileAttributesA("trace.on") != INVALID_FILE_ATTRIBUTES)
	{
		FILE* tf = fopen("trace_text.txt", "a");

		if (tf)
		{
			fprintf(tf, "ret=%08X x=%d y=%d %s\n", (DWORD)_ReturnAddress(), iPos_x, iPos_y, pszText);

			fclose(tf);
		}
	}
#endif

	if (g_LoginActive)
	{
		DWORD ret = (DWORD)_ReturnAddress();

		if (ret == 0x0052176B || ret == 0x00521796)
		{
			// "Account" and "Password" labels positioned cleanly above their input boxes
			const LAYOUT_RECT& n = (ret == 0x0052176B) ? g_New[0] : g_New[1];

			float x = LOGIN_FRAME_X + n.x0;

			float y = (float)LOGIN_FRAME_Y + n.y0 - 7.2f;

			ScalePoint(&x, &y);

			iPos_x = (int)(x + 0.5f);

			iPos_y = (int)(y + 0.5f);

			iBoxWidth = (int)(iBoxWidth * gLoginScale.m_Scale);
		}
		else if (ret == 0x00521850 || ret == 0x005218FD)
		{
			// "Connect" (orig center 357) and "Exit" (orig center 319): exact visual centering inside each button
			bool isConnect = (ret == 0x00521850);

			const LAYOUT_RECT& n = isConnect ? g_New[2] : g_New[3];

			float halfW = (isConnect ? 357.0f : 319.0f) - (float)iPos_x;

			float sx = LOGIN_FRAME_X + 0.5f * (n.x0 + n.x1);

			float sy = (float)LOGIN_FRAME_Y + 0.5f * (n.y0 + n.y1);

			ScalePoint(&sx, &sy);

			iPos_x = (int)(sx - halfW + 0.5f);

			iPos_y = (int)(sy - 2.6f + 0.5f);

			iBoxWidth = (int)(iBoxWidth * gLoginScale.m_Scale);
		}
		else
		{
			float x = (float)iPos_x;

			float y = (float)iPos_y;

			ScalePoint(&x, &y);

			iPos_x = (int)(x + 0.5f);

			iPos_y = (int)(y + 0.5f);

			iBoxWidth = (int)(iBoxWidth * gLoginScale.m_Scale);
		}
	}

	if (g_InvScaleActive)
	{
		iPos_x = (int)ScaleInvX((float)iPos_x);
		iPos_y = (int)ScaleInvY((float)iPos_y);
		if (iBoxWidth > 0)
		{
			iBoxWidth = (int)((float)iBoxWidth * INV_SCALE);
		}
	}

	return g_OrigRenderText(iPos_x, iPos_y, pszText, iBoxWidth, iSort, lpTextSize);
}

// The typed text of the account / password boxes is drawn by 0x0047F0B0(x, y, index, flag), called twice inside the block.
// It shares the coordinate space of the RenderText captions.
static void _cdecl HookInputBox(int x, int y, int index, int flag)
{
	if (g_LoginActive)
	{
		const LAYOUT_RECT& n = g_New[(index == 0) ? 0 : 1];

		float fx = LOGIN_FRAME_X + n.x0 + 5.5f;

		float fy = (float)LOGIN_FRAME_Y + 0.5f * (n.y0 + n.y1);

		ScalePoint(&fx, &fy);

		x = (int)(fx + 0.5f);

		y = (int)(fy - 2.6f + 0.5f);
	}

	((void(_cdecl*)(int, int, int, int)) 0x0047F0B0)(x, y, index, flag);
}

// Maps a mouse position (virtual units, as seen on screen) to what the original code expects:
// undo the enlargement, then map the moved elements from their new rectangle back to the old one.
static void ComputeRel(int mx, int my, float* rx, float* ry)
{
	float ux = gLoginScale.m_PivotX + (mx - gLoginScale.m_PivotX) / gLoginScale.m_Scale;

	float uy = gLoginScale.m_PivotY + (my - gLoginScale.m_PivotY) / gLoginScale.m_Scale;

	*rx = ux - LOGIN_FRAME_X;

	*ry = uy - (float)LOGIN_FRAME_Y;
}

static void MapMouse(int mx, int my, int* outX, int* outY, bool hide)
{
	float frameY = (float)LOGIN_FRAME_Y;

	float rx, ry;

	ComputeRel(mx, my, &rx, &ry);

	bool hit = false;

	for (int i = 0; i < 4 && !hit; i++)
	{
		const LAYOUT_RECT& n = g_New[i];

		const LAYOUT_RECT& o = g_Old[i];

		if (rx >= n.x0 && rx < n.x1 && ry >= n.y0 && ry < n.y1)
		{
			rx = o.x0 + (rx - n.x0) * (o.x1 - o.x0) / (n.x1 - n.x0);

			ry = o.y0 + (ry - n.y0) * (o.y1 - o.y0) / (n.y1 - n.y0);

			hit = true;
		}
	}

	if (!hit)
	{
		// nothing is drawn at the old rectangles any more, so they must not react
		for (int i = 0; i < 4; i++)
		{
			const LAYOUT_RECT& o = g_Old[i];

			if (rx >= o.x0 && rx < o.x1 && ry >= o.y0 && ry < o.y1)
			{
				rx = -10000.0f;

				break;
			}
		}
	}

	if (hide)
	{
		rx = -10000.0f;
	}

	*outX = (int)(LOGIN_FRAME_X + rx);

	*outY = (int)(frameY + ry);
}

static void _cdecl BeginMouseMap()
{
	g_SavedMouseX = MouseX;

	g_SavedMouseY = MouseY;

	g_MouseSaved = true;

	ComputeRel(g_SavedMouseX, g_SavedMouseY, &g_RelX, &g_RelY);

	int nx, ny;

	MapMouse(g_SavedMouseX, g_SavedMouseY, &nx, &ny, gLoginRemember.OverList(g_RelX, g_RelY));

	MouseX = nx;

	MouseY = ny;
}

static void _cdecl DrawLoginExtras()
{
	int state = LOGIN_STATE;

	if (state == 2 || state == 3)
	{
		gLoginRemember.Render(LOGIN_FRAME_X, (float)LOGIN_FRAME_Y, g_RelX, g_RelY, state);
	}
}

static void _cdecl EndMouseMap()
{
	if (g_MouseSaved)
	{
		MouseX = g_SavedMouseX;

		MouseY = g_SavedMouseY;

		g_MouseSaved = false;
	}
}

// Inside RenderLogInScene (0x00521630): the login window block starts at 0x00521718 (mov esi,[0x5616A4]) ...
__declspec(naked) static void BeginLoginWindow()
{
	static DWORD back = 0x0052171E;

	__asm
	{
		pushfd
		pushad
		call BeginMouseMap
		popad
		popfd
		mov byte ptr[g_LoginActive], 1
		mov esi, dword ptr ds : [0x005616A4]
		jmp back
	}
}

// ... and ends at 0x00521900 (push 0x3F19999A), which is also the jump target when the window is not shown.
__declspec(naked) static void EndLoginWindow()
{
	static DWORD back = 0x00521905;

	__asm
	{
		cmp byte ptr[g_LoginActive], 0
		je skip_extras
		pushfd
		pushad
		call DrawLoginExtras
		popad
		popfd
	skip_extras:
		pushfd
		pushad
		call EndMouseMap
		popad
		popfd
		mov byte ptr[g_LoginActive], 0
		push 0x3F19999A
		jmp back
	}
}

CLoginScale::CLoginScale()
{
	this->m_Scale = 1.18f;

	this->m_PivotX = 320.0f;

	this->m_PivotY = 460.0f;
}

void CLoginScale::Init()
{
	gLoginRemember.Init();

	g_OrigRenderBitmap = (RENDERBITMAP_T)MakeTrampoline(0x005125A0, 7);   // mov al,[esp+2C] ; sub esp,48

	g_OrigRenderText = (RENDERTEXT_T)MakeTrampoline(0x0047F650, 5);       // mov eax,[esp+18] ; push esi

	g_OrigRenderColor = (RENDERCOLOR_T)MakeTrampoline(0x005124C0, 6);   // sub esp,28 ; push ebx ; push esi ; push edi

	g_OrigRenderTip = (RENDERTIP_T)MakeTrampoline(0x0047F7F0, 8);       // sub esp,18 ; mov eax,[55CA00C]

	g_OrigInputBox = (INPUTBOX_T)MakeTrampoline(0x0047F0B0, 6);         // sub esp,100

	SetCompleteHook(0xE9, 0x005124C0, &HookRenderColor);

	SetCompleteHook(0xE9, 0x0047F7F0, &HookRenderTip);

	SetCompleteHook(0xE9, 0x0047F0B0, &HookInputBoxEntry);

	SetCompleteHook(0xE9, 0x005125A0, &HookRenderBitmap);

	SetCompleteHook(0xE9, 0x0047F650, &HookRenderText);

	SetCompleteHook(0xE9, 0x00521718, &BeginLoginWindow);

	SetByte(0x0052171D, 0x90);

	SetCompleteHook(0xE9, 0x00521900, &EndLoginWindow);

	SetCompleteHook(0xE8, 0x00521778, &HookInputBox);

	SetCompleteHook(0xE8, 0x005217A0, &HookInputBox);

	SetCompleteHook(0xE8, 0x0052624A, &CLoginScale::UpdateLoginScene);
}

void _cdecl CLoginScale::UpdateLoginScene()
{
	int state = LOGIN_STATE;

	if (state == 2 || state == 3)
	{
		int oldX = MouseX;

		int oldY = MouseY;

		float rx, ry;

		ComputeRel(oldX, oldY, &rx, &ry);

		bool down = MouseLButton;

		bool click = (down && !g_PrevDown);

		g_PrevDown = down;

		bool hide = gLoginRemember.Update(rx, ry, click, state);

		int nx, ny;

		MapMouse(oldX, oldY, &nx, &ny, hide);

		MouseX = nx;

		MouseY = ny;

		((void(_cdecl*)()) 0x0051F900)();

		MouseX = oldX;

		MouseY = oldY;
	}
	else
	{
		g_PrevDown = MouseLButton;

		((void(_cdecl*)()) 0x0051F900)();
	}
}

void CLoginScale::ResetRender()
{
	g_LoginActive = false;

	EndMouseMap();
}
