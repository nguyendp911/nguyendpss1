#include "stdafx.h"
#include "Camera3D.h"
#include "MapManager.h"
#include "stb.h"

CCamera3D Camera3D;

static void DrawCameraSlot(GLuint texId, int slot, float x, float y, float w, float h, float alpha)
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
	glColor4f(1.0f, 1.0f, 1.0f, alpha);

	glBegin(GL_QUADS);
	glTexCoord2f(u0, 0.0f); glVertex2f(sx, sy);
	glTexCoord2f(u1, 0.0f); glVertex2f(sx + sw, sy);
	glTexCoord2f(u1, 1.0f); glVertex2f(sx + sw, sy - sh);
	glTexCoord2f(u0, 1.0f); glVertex2f(sx, sy - sh);
	glEnd();

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

static bool InCameraRect(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

CCamera3D::CCamera3D()
{
	this->m_Enable = false;

	this->m_IsMove = false;

	this->m_CursorX = 0;
	this->m_CursorY = 0;

	this->m_Zoom.MinPercent = 35.0f;

	this->m_Zoom.MaxPercent = 300.0f;

	this->m_Zoom.Precision = 3.5f;

	this->m_Default.IsLoad = false;

	this->m_TexCameraCS = 0;
}

CCamera3D::~CCamera3D()
{

}

void CCamera3D::Init()
{
	this->m_Address.Zoom = (float*)0x00524CC1;

	this->m_Address.RotX = (float*)0x0052538A; // Immediate operand of mov [0x083A42C0], -45.0f

	this->m_Address.RotY = (float*)0x00552D54;

	this->m_Address.PosZ = (float*)0x0055297C;

	this->m_Address.ClipX[0] = (float*)0x004F8ED8;

	this->m_Address.ClipX[1] = (float*)0x004F8EEE;

	this->m_Address.ClipY[0] = (float*)0x004F8EFE;

	this->m_Address.ClipY[1] = (float*)0x004F8F0E;

	this->m_Address.ClipZ = (float*)0x00552CBC;

	this->m_Address.ClipGL = (float*)0x00524D32;

	// Fix ground item billboard orientation
	SetDword(0x00511332 + 2, 0x005528CC);
	SetDword(0x00511360 + 2, 0x005528CC);
	SetDword(0x0051136C + 2, 0x005528CC);

	SetCompleteHook(0xE9, 0x005120C0, &this->MyRenderNumber);

	if (!this->m_Default.IsLoad)
	{
		this->m_Default.Zoom = (*this->m_Address.Zoom);

		this->m_Default.RotX = (*this->m_Address.RotX);

		this->m_Default.RotY = (*this->m_Address.RotY);

		this->m_Default.PosZ = (*this->m_Address.PosZ);

		this->m_Default.ClipX[0] = (*this->m_Address.ClipX[0]);

		this->m_Default.ClipX[1] = (*this->m_Address.ClipX[1]);

		this->m_Default.ClipY[0] = (*this->m_Address.ClipY[0]);

		this->m_Default.ClipY[1] = (*this->m_Address.ClipY[1]);

		this->m_Default.ClipZ = (*this->m_Address.ClipZ);

		this->m_Default.ClipGL = (*this->m_Address.ClipGL);

		this->m_Default.IsLoad = true;
	}

	this->m_Enable = false;

	this->SetDefaultValue();
}

void CCamera3D::MyRenderNumber(vec3_t Position, int Num, vec3_t Color, float Alpha, float Scale)
{
	vec3_t p;
	VectorCopy(Position, p);

	vec3_t Light[4];
	for (int i = 0; i < 4; i++)
	{
		VectorCopy(Color, Light[i]);
	}

	if (Num == -1)
	{
		float UV[4][2] =
		{
			{ 0.0f, 32.0f / 32.0f },
			{ 32.0f / 256.0f, 32.0f / 32.0f },
			{ 32.0f / 256.0f, 17.0f / 32.0f },
			{ 0.0f, 17.0f / 32.0f }
		};

		RenderSpriteUV(1, p, 45.0f, 20.0f, UV, Light, Alpha);

		return;
	}

	char Text[32];
	_itoa_s(Num, Text, 10);

	unsigned int Length = strlen(Text);

	float textOffset = Length * Scale * 0.125f;
	p[0] -= textOffset;
	p[1] -= textOffset;

	float yawRad = DegToRad(CameraAngle[2]);
	float cosYaw = cos(yawRad);
	float sinYaw = sin(yawRad);
	float spacing = Scale * (float)sqrt(2.0f) * 0.5f;

	for (unsigned int i = 0; i < Length; i++)
	{
		float texOffsetX = (float)(Text[i] - '0') * 16.0f / 256.0f;
		float UV[4][2] =
		{
			{ texOffsetX, 16.0f / 32.0f },
			{ texOffsetX + 16.0f / 256.0f, 16.0f / 32.0f },
			{ texOffsetX + 16.0f / 256.0f, 0.0f },
			{ texOffsetX, 0.0f }
		};

		RenderSpriteUV(1, p, Scale, Scale, UV, Light, Alpha);

		p[0] += spacing * cosYaw;
		p[1] -= spacing * sinYaw;
	}
}

void CCamera3D::Toggle()
{
	if (SceneFlag == MAIN_SCENE)
	{
		this->m_Enable ^= 1;

		if (this->m_Enable && !this->m_Default.IsLoad)
		{
			this->m_Default.Zoom = (*this->m_Address.Zoom);
			this->m_Default.RotX = (*this->m_Address.RotX);
			this->m_Default.RotY = (*this->m_Address.RotY);
			this->m_Default.PosZ = (*this->m_Address.PosZ);
			this->m_Default.ClipX[0] = (*this->m_Address.ClipX[0]);
			this->m_Default.ClipX[1] = (*this->m_Address.ClipX[1]);
			this->m_Default.ClipY[0] = (*this->m_Address.ClipY[0]);
			this->m_Default.ClipY[1] = (*this->m_Address.ClipY[1]);
			this->m_Default.ClipZ = (*this->m_Address.ClipZ);
			this->m_Default.ClipGL = (*this->m_Address.ClipGL);
			this->m_Default.IsLoad = true;
		}

		CreateNotice(((this->m_Enable) ? "Camera 3D: Da Bat (Zoom / Xoay 360)" : "Camera 3D: Da Tat (Khoa goc nhin)"), 1);
	}
}

void CCamera3D::Restore()
{
	if (SceneFlag == MAIN_SCENE)
	{
		this->SetDefaultValue();

		CreateNotice("Camera 3D: Da khoi phuc goc mac dinh", 1);
	}
}

void CCamera3D::SetIsMove(bool IsMove)
{
	if (this->m_Enable && SceneFlag == MAIN_SCENE)
	{
		this->m_IsMove = IsMove;
	}
}

void CCamera3D::SetCursorX(LONG CursorX)
{
	if (this->m_Enable && SceneFlag == MAIN_SCENE)
	{
		this->m_CursorX = CursorX;
	}
}

void CCamera3D::SetCursorY(LONG CursorY)
{
	if (this->m_Enable && SceneFlag == MAIN_SCENE)
	{
		this->m_CursorY = CursorY;
	}
}

void CCamera3D::Zoom(MOUSEHOOKSTRUCTEX* lpMouse)
{
	if (!this->m_Enable || this->m_IsMove || SceneFlag != MAIN_SCENE)
	{
		return;
	}

	short wheelDelta = GET_WHEEL_DELTA_WPARAM(lpMouse->mouseData);
	if (wheelDelta == 0)
	{
		return;
	}

	float currentZoom = *this->m_Address.Zoom;
	const float MIN_ZOOM = 20.0f;
	const float MAX_ZOOM = 100.0f;
	const float ZOOM_STEP = 3.5f;

	if (wheelDelta < 0)
	{
		float newZoom = currentZoom + ZOOM_STEP;
		if (newZoom > MAX_ZOOM)
		{
			newZoom = MAX_ZOOM;
		}
		SetFloat((DWORD)this->m_Address.Zoom, newZoom);
	}
	else if (wheelDelta > 0)
	{
		float newZoom = currentZoom - ZOOM_STEP;
		if (newZoom < MIN_ZOOM)
		{
			newZoom = MIN_ZOOM;
		}
		SetFloat((DWORD)this->m_Address.Zoom, newZoom);
	}

	this->SetCurrentValue();
}

void CCamera3D::Move(MOUSEHOOKSTRUCTEX* lpMouse)
{
	if (!this->m_Enable || SceneFlag != MAIN_SCENE)
	{
		return;
	}

	bool isMiddleHeld = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
	bool isShiftRightHeld = ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0 && (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);

	if (isMiddleHeld || isShiftRightHeld)
	{
		if (!this->m_IsMove)
		{
			this->m_IsMove = true;
			this->m_CursorX = lpMouse->pt.x;
			this->m_CursorY = lpMouse->pt.y;
			return;
		}
	}
	else
	{
		this->m_IsMove = false;
		return;
	}

	int diffX = lpMouse->pt.x - this->m_CursorX;
	int diffY = lpMouse->pt.y - this->m_CursorY;

	if (diffX == 0 && diffY == 0)
	{
		return;
	}

	// Smooth horizontal 360-degree rotation (Yaw)
	if (diffX != 0)
	{
		float currentRotX = *this->m_Address.RotX;
		float newRotX = currentRotX + ((float)diffX * 0.40f);

		while (newRotX > 180.0f)
		{
			newRotX -= 360.0f;
		}
		while (newRotX <= -180.0f)
		{
			newRotX += 360.0f;
		}

		SetFloat((DWORD)this->m_Address.RotX, newRotX);
		SetFloat(0x083A42C0, newRotX);
	}

	// Smooth vertical pitch and camera height
	if (diffY != 0)
	{
		float currentRotY = *this->m_Address.RotY;
		float currentPosZ = *this->m_Address.PosZ;

		float newRotY = currentRotY + ((float)diffY * 0.25f);
		float newPosZ = currentPosZ + ((float)diffY * 4.5f);

		if (newRotY < 15.0f) newRotY = 15.0f;
		if (newRotY > 90.0f) newRotY = 90.0f;

		if (newPosZ < -150.0f) newPosZ = -150.0f;
		if (newPosZ > 600.0f) newPosZ = 600.0f;

		SetFloat((DWORD)this->m_Address.RotY, newRotY);
		SetFloat((DWORD)this->m_Address.PosZ, newPosZ);
	}

	this->m_CursorX = lpMouse->pt.x;
	this->m_CursorY = lpMouse->pt.y;

	this->SetCurrentValue();
}

void CCamera3D::SetCurrentValue()
{
	float zoom = *this->m_Address.Zoom;
	float posZ = *this->m_Address.PosZ;
	float extra = max(0.0f, (zoom - 35.0f) * 60.0f) + abs(posZ - 150.0f) * 5.0f;

	SetFloat((DWORD)this->m_Address.ClipX[0], 1272.0f + extra + 2500.0f);
	SetFloat((DWORD)this->m_Address.ClipX[1], 1272.0f + extra + 2500.0f);
	SetFloat((DWORD)this->m_Address.ClipY[0], -672.0f - extra - 4000.0f);
	SetFloat((DWORD)this->m_Address.ClipY[1], -672.0f - extra - 4000.0f);
	SetFloat((DWORD)this->m_Address.ClipZ, 1190.0f + extra + 4000.0f);
	SetFloat((DWORD)this->m_Address.ClipGL, 4000.0f + extra + 4000.0f);
}

void CCamera3D::SetDefaultValue()
{
	if (this->m_Default.IsLoad)
	{
		SetFloat((DWORD)this->m_Address.Zoom, this->m_Default.Zoom);
		SetFloat((DWORD)this->m_Address.RotX, this->m_Default.RotX);
		SetFloat(0x083A42C0, this->m_Default.RotX);
		SetFloat((DWORD)this->m_Address.RotY, this->m_Default.RotY);
		SetFloat((DWORD)this->m_Address.PosZ, this->m_Default.PosZ);

		this->SetCurrentValue();
	}
}

void CCamera3D::LoadImages()
{
	if (this->m_TexCameraCS != 0)
	{
		return;
	}

	int w = 0, h = 0, ch = 0;
	unsigned char* pixels = stbi_load("Data\\Interface\\CameraCS.png", &w, &h, &ch, 4);
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
	this->m_TexCameraCS = texId;
}

void CCamera3D::Render()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		return;
	}

	if (this->m_TexCameraCS == 0)
	{
		this->LoadImages();
		if (this->m_TexCameraCS == 0)
		{
			return;
		}
	}

	const float asp = (g_fScreenRate_x > 0.0f) ? (g_fScreenRate_y / g_fScreenRate_x) : 0.75f;
	const float btnH = 22.0f;
	const float btnW = btnH * asp;
	const float x = 8.0f;
	const float y = 8.0f;

	// Button 1: Toggle Camera (Slot 0 = ON cyan, Slot 1 = OFF crimson)
	bool hoverToggle = InCameraRect(x, y, btnW, btnH);
	{
		float a = hoverToggle ? 1.0f : 0.90f;
		float scale = hoverToggle ? 1.06f : 1.0f;
		float rw = btnW * scale;
		float rh = btnH * scale;
		float rx = x - (rw - btnW) * 0.5f;
		float ry = y - (rh - btnH) * 0.5f;
		DrawCameraSlot(this->m_TexCameraCS, this->m_Enable ? 0 : 1, rx, ry, rw, rh, a);
	}

	// Button 2: Reset Camera (Slot 2 = circular arrow)
	const float resetX = x + btnW + 3.0f;
	const float resetY = y;
	bool hoverReset = InCameraRect(resetX, resetY, btnW, btnH);
	{
		float a = hoverReset ? 1.0f : 0.90f;
		float scale = hoverReset ? 1.06f : 1.0f;
		float rw = btnW * scale;
		float rh = btnH * scale;
		float rx = resetX - (rw - btnW) * 0.5f;
		float ry = resetY - (rh - btnH) * 0.5f;
		DrawCameraSlot(this->m_TexCameraCS, 2, rx, ry, rw, rh, a);
	}
}

void CCamera3D::UpdateMouse()
{
	if (SceneFlag != MAIN_SCENE || !Hero)
	{
		return;
	}

	const float asp = (g_fScreenRate_x > 0.0f) ? (g_fScreenRate_y / g_fScreenRate_x) : 0.75f;
	const float btnH = 22.0f;
	const float btnW = btnH * asp;
	const float x = 8.0f;
	const float y = 8.0f;

	if (InCameraRect(x, y, btnW, btnH))
	{
		MouseOnWindow = true;
		if (MouseLButtonPush)
		{
			MouseLButtonPush = false;
			MouseLButton = false;
			MouseLButtonPop = false;
			PlayBuffer(25, 0, 0);
			this->Toggle();
			return;
		}
	}

	const float resetX = x + btnW + 3.0f;
	const float resetY = y;
	if (InCameraRect(resetX, resetY, btnW, btnH))
	{
		MouseOnWindow = true;
		if (MouseLButtonPush)
		{
			MouseLButtonPush = false;
			MouseLButton = false;
			MouseLButtonPop = false;
			PlayBuffer(25, 0, 0);
			this->Restore();
			return;
		}
	}
}
