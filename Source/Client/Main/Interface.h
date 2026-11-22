#pragma once

#define MAX_OBJECT 250

#define MAX_WIN_WIDTH 640

#define MAX_WIN_HEIGHT 480

struct InterfaceObject
{
	DWORD ModelID;
	float Width;
	float Height;
	float X;
	float Y;
	float MaxX;
	float MaxY;
	float ScaleX;
	float ScaleY;
	DWORD EventTick;
	bool OnClick;
	bool OnShow;
	BYTE Attribute;
};

enum ObjectID
{

};

// Cold Steel HUD helpers (Interface.cpp). Coordinates are FINAL screen units (the original code runs HUD_SHIFT_Y units lower).
extern bool g_HudShiftActive; // LoginScale.cpp: the bottom interface is being drawn / updated HUD_SHIFT_Y units higher
extern bool g_InvScaleActive; // LoginScale.cpp: the inventory window (V) is being scaled down above the HUD
#define INV_SCALE 0.82f
#define INV_PIVOT_X 636.0f
#define INV_PIVOT_Y 20.0f
void HudSprite(float ax, float ay, float aw, float ah, float x, float y, float w, float h);
void HudBegin();
void HudEnd();
void HudDrawExpBar(float frac, float* hx, float* hy, float* hw, float* hh);
void HudDrawAgBar(float frac);
void HudButtonActive(int index);
bool HudIconBarCollapsed();
void HudIconBarUpdateMouse();
void HudIconBarRender();

class Interface
{
public:

	Interface();

	virtual ~Interface();

	void Init();

	void AddExpPopup(DWORD exp);

private:

	static void RenderLogInScene(HDC Hdc);

	static void RenderCharacterScene(HDC Hdc);

	static void RenderMainScene();

	static void LoadImages();

	static void MyUpdateWindowsMouse();

	static void MyRenderWindows();

	static void RenderLeftDragon(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight, bool Scale, bool StartScale);

	static void RenderRightDragon(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight, bool Scale, bool StartScale);

	void BindObject(short ObjectID, DWORD ModelID, float Width, float Height, float X, float Y);

	void DrawIMG(short ObjectID, float PosX, float PosY, float ScaleX, float ScaleY);

	void DrawIMG(short ObjectID, float PosX, float PosY, float u, float v, float ScaleX, float ScaleY);

	bool IsWorkZone(short ObjectID);

public:

	InterfaceObject Data[MAX_OBJECT];
};

extern Interface gInterface;