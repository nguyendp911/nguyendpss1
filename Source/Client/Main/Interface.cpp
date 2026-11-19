#include "stdafx.h"
#include "Interface.h"
#include "LoginScale.h"
#include "Camera3D.h"
#include "EventTimer.h"
#include "MiniMap.h"
#include "MoveList.h"
#include "Protect.h"
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
// Chat layout (virtual 640x480 units), bottom left, above the interface bar and clear of the left dragon (y >= 387):
//
//     chat log frame   x 0..268,  y CHAT_LOG_POS_Y-70 .. CHAT_LOG_POS_Y+2
//     chat input box   x 0..266,  y CHAT_INPUT_Y .. CHAT_INPUT_Y+29
//
// The floating chat log (chat mode 0) is anchored at (-10, 81) in the original client, i.e. the top left corner.
// The log window position is set at 0x005258E9 (every frame) and at 0x004C05D9 (when the chat mode is toggled); the
// input box is drawn by the function at 0x004BE4F0 with the coordinates hard coded as push immediates.
// ---------------------------------------------------------------------------------------------------------------------
#define CHAT_LOG_POS_X (-4) // the original value (-10) puts the text against the left edge; +6 gives it some padding
#define CHAT_LOG_POS_Y 354
#define CHAT_INPUT_Y 358
#define CHAT_LOG_WIDTH 268
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

	// button next to the whisper box: RenderBitmap(239, 432, 420, 15, 16)
	SetDword(0x004BE6F3, FloatBits(420.0f + dy));
	SetDword(0x004BE6F8, FloatBits(432.0f + dx));
}

// Flat frame behind the chat log (same digital style as the other panels)
static void RenderChatLogFrame()
{
	if (*(DWORD*)0x005590AC == 1) // bottom anchored chat mode brings its own window
	{
		return;
	}

	// only show the background while the chat input is open (same condition the input box uses at 0x004BE518),
	// otherwise the idle chat is just text over the game and the frame does not cover the screen
	if (*(BYTE*)0x00559C84 == 0 && *(BYTE*)0x007E11D71 == 0)
	{
		return;
	}

	const float x = 0.0f;

	// the log shows 6 lines of 13.3 units, anchored at the bottom
	const float y = (float)(CHAT_LOG_POS_Y - 82);

	const float w = (float)CHAT_LOG_WIDTH;

	const float h = 84.0f;

	EnableAlphaTest(true);

	glColor4f(0.23f, 0.27f, 0.32f, 0.5f);

	RenderColor(x, y - 1.0f, w + 1.0f, h + 2.0f);

	glColor4f(0.05f, 0.07f, 0.09f, 0.35f);

	RenderColor(x, y, w, h);

	glColor4f(0.435f, 0.714f, 0.769f, 0.8f);

	RenderColor(x, y, 2.0f, h);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	DisableAlphaBlend();
}

// ---------------------------------------------------------------------------------------------------------------------
// Chat input box, flat digital style. The original function (0x004BE4F0) draws it with three RenderBitmap calls:
//   0x004BE562 texture 248 (left half), 0x004BE592 texture 249 (right half), 0x004BE701 texture 239 (button).
// The text of the two fields (chat / whisper target) is drawn separately, so only the artwork is replaced.
// ---------------------------------------------------------------------------------------------------------------------
static void ChatPanelRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);

	glColor4f(r, g, b, a);

	RenderColor(x, y, w, h);
}

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

	ChatPanelRect(X + 191.0f, Y + 3.0f, 52.0f, 23.0f, 0.17f, 0.21f, 0.25f, 1.0f);

	ChatPanelRect(X + 192.0f, Y + 4.0f, 50.0f, 21.0f, 0.07f, 0.09f, 0.11f, 1.0f);

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

// 0x00525CA9: call 0x004CB6F0, the first call of the 2D pass (right after 0x005123C0); draw the frame here so that all
// the 2D text (including the floating chat log) ends up on top of it.
static void _cdecl ChatLogRenderHook()
{
	RenderChatLogFrame();

	((void(_cdecl*)()) 0x004CB6F0)();
}

void Interface::Init()
{
	PatchChatPositions();

	SetCompleteHook(0xE8, 0x00525CA9, &ChatLogRenderHook);

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

	gMiniMap.LoadImages();
}

void Interface::MyUpdateWindowsMouse()
{
	UpdateWindowsMouse();

	gMiniMap.UpdateMouse();

	gMoveList.UpdateMouse();

	gEventTimer.UpdateMouse();
}

void Interface::MyRenderWindows()
{
	((void(_cdecl*)()) 0x004C3530)();

	gMiniMap.Render();

	gMoveList.Render();

	gEventTimer.Render();
}

void Interface::RenderLeftDragon(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight, bool Scale, bool StartScale)
{
	if (gMiniMap.GetMiniMapState() && gMapManager.GetMiniMap(World) == 2)
	{
		return;
	}

	RenderBitmap(Texture, x, y, Width, Height, u, v, uWidth, vHeight, Scale, StartScale);
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

	RenderBitmap(Texture, x, y, Width, Height, u, v, uWidth, vHeight, Scale, StartScale);
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