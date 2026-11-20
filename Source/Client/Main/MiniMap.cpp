#include "stdafx.h"
#include "MiniMap.h"
#include "EventTimer.h"
#include "LoadModels.h"
#include "MapManager.h"
#include "MoveList.h"
#include "stb.h"

CMiniMap gMiniMap;

CMiniMap::CMiniMap()
{
	memset(this->MiniMapTable, 0, sizeof(this->MiniMapTable));

	this->MiniMapSwitch = true;

	this->Alpha = GetPrivateProfileInt("MiniMap", "Alpha", 10, ".\\Config.ini");

	this->ZoomLevel = GetPrivateProfileInt("MiniMap", "ZoomLevel", 2, ".\\Config.ini");

	this->VisualRange = 128 - (this->ZoomLevel * 16);

	this->MiniMapFont = CreateFont(10 + (this->ZoomLevel * 2), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, "Verdana");

	this->m_GatesIndex.clear();
}

CMiniMap::~CMiniMap()
{
	char Text[33] = { 0 };

	wsprintf(Text, "%d", this->Alpha);

	WritePrivateProfileString("MiniMap", "Alpha", Text, ".\\Config.ini");

	wsprintf(Text, "%d", this->ZoomLevel);

	WritePrivateProfileString("MiniMap", "ZoomLevel", Text, ".\\Config.ini");
}

void CMiniMap::Init()
{
	SetCompleteHook(0xE8, 0x0050E6F8, &this->MiniMapLoad);
}

void CMiniMap::ChangeZoom(int i)
{
	if (i == -1)
	{
		this->ZoomLevel = (this->ZoomLevel + 1 >= 7) ? 0 : this->ZoomLevel + 1;
	}
	else
	{
		this->ZoomLevel = i;
	}

	DeleteObject((HGDIOBJ)this->MiniMapFont);

	this->MiniMapFont = CreateFont(10 + (this->ZoomLevel * 2), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, "Verdana");
}

void CMiniMap::ChangeAlpha()
{
	this->Alpha = (this->Alpha + 1 > 10) ? 3 : this->Alpha + 1;
}

void CMiniMap::MiniMapLoad(char* FileName)
{
	OpenTerrainLight(FileName);

	if (MAP_RANGE(World))
	{
		gMiniMap.MiniMapTable[World] = (gMapManager.GetMiniMap(World) != 0);

		if (!gMiniMap.MiniMapTable[World])
		{
			gMiniMap.MiniMapSwitch = false;
		}

		gMiniMap.LoadMiniMapImage();

		gMiniMap.LoadMiniMapGates();
	}
}

void CMiniMap::LoadMiniMapImage()
{
	BITMAP_t* pNewBitmap = gLoadModels.GetBitmap(MINIMAP_TEXTURE);

	int MapWidth = MAP_SIZE;

	int MapHeight = MAP_SIZE;

	pNewBitmap->Width = (float)MapWidth;

	pNewBitmap->Height = (float)MapHeight;

	pNewBitmap->Components = 4;

	pNewBitmap->TextureNumber = this->MapTextureID;

	size_t BufferSize = MapWidth * MapHeight * 4;

	if (!pNewBitmap->Buffer)
	{
		pNewBitmap->Buffer = (BYTE*)operator_new(BufferSize);

		m_dwUsedTextureMemory += BufferSize;

		pNewBitmap->Ref++;
	}

	memset(pNewBitmap->Buffer, 0, BufferSize);

	int index;

	unsigned char* dst = NULL;

	int dstIndex;

	for (int sy = 0; sy < TERRAIN_SIZE; sy++)
	{
		for (int sx = 0; sx < TERRAIN_SIZE; sx++)
		{
			index = (TERRAIN_SIZE - 1 - sy) * TERRAIN_SIZE + sx;

			if ((TerrainWall[index] >> 2) & 0x1) // TW_NOMOVE
			{
				continue;
			}

			if ((TerrainWall[index] >> 3) & 0x1) // TW_NOGROUND
			{
				continue;
			}

			if ((TerrainWall[index] >> 4) & 0x1) // TW_WATER
			{
				continue;
			}

			if ((TerrainWall[index] >> 5) & 0x1) // TW_ACTION
			{
				continue;
			}

			if ((TerrainWall[index] >> 6) & 0x1) // TW_HEIGHT
			{
				continue;
			}

			if ((TerrainWall[index] >> 7) & 0x1) // TW_CAMERA_UP
			{
				continue;
			}

			if ((TerrainWall[index] == 0) // HUNT_ZONE
			    || ((TerrainWall[index] >> 0) & 0x1) // TW_SAFEZONE
			    || ((TerrainWall[index] >> 1) & 0x1)) // TW_CHARACTER
			{
				this->PaintBorder(sx, sy, pNewBitmap->Buffer);

				const bool isSafe = ((TerrainWall[index] >> 0) & 0x1) != 0;
				const unsigned char tr = isSafe ? 52 : 34;
				const unsigned char tg = isSafe ? 84 : 50;
				const unsigned char tb = isSafe ? 106 : 64;
				const unsigned char ta = isSafe ? 230 : 210;

				dstIndex = (((sy * 2) * MapWidth) + (sx * 2)) * 4;
				dst = &pNewBitmap->Buffer[dstIndex];

				dst[0] = tr; dst[1] = tg; dst[2] = tb; dst[3] = ta;
				dst[4] = tr; dst[5] = tg; dst[6] = tb; dst[7] = (unsigned char)(ta - 12);

				dstIndex = ((((sy * 2) + 1) * MapWidth) + (sx * 2)) * 4;
				dst = &pNewBitmap->Buffer[dstIndex];

				dst[0] = tr; dst[1] = tg; dst[2] = tb; dst[3] = ta;
				dst[4] = tr; dst[5] = tg; dst[6] = tb; dst[7] = (unsigned char)(ta - 12);
			}
		}
	}

	glBindTexture(GL_TEXTURE_2D, pNewBitmap->TextureNumber); // Bind the texture

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MapWidth, MapHeight, GL_RGBA, GL_UNSIGNED_BYTE, pNewBitmap->Buffer); // Update the texture

	glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture
}

void CMiniMap::PaintBorder(int sx, int sy, BYTE* bitmap)
{
	unsigned char* dst = NULL;

	if ((sx - 1) > 0)
	{
		if (((TerrainWall[(TERRAIN_SIZE - 1 - sy) * TERRAIN_SIZE + (sx - 1)] >> 0) & 0xFC) != 0)
		{
			dst = &bitmap[((sy * 2) * MAP_SIZE + ((sx - 1) * 2)) * 4];

			dst[4] = 192;
			dst[5] = 254;
			dst[6] = 243;
			dst[7] = 128;

			dst = &bitmap[(((sy * 2) + 1) * MAP_SIZE + ((sx - 1) * 2)) * 4];

			dst[4] = 192;
			dst[5] = 254;
			dst[6] = 243;
			dst[7] = 128;
		}
	}

	if ((sx + 1) < TERRAIN_SIZE)
	{
		if (((TerrainWall[(TERRAIN_SIZE - 1 - sy) * TERRAIN_SIZE + (sx + 1)] >> 0) & 0xFC) != 0)
		{
			dst = &bitmap[((sy * 2) * MAP_SIZE + ((sx + 1) * 2)) * 4];

			dst[0] = 192;
			dst[1] = 254;
			dst[2] = 243;
			dst[3] = 128;

			dst = &bitmap[(((sy * 2) + 1) * MAP_SIZE + ((sx + 1) * 2)) * 4];

			dst[0] = 192;
			dst[1] = 254;
			dst[2] = 243;
			dst[3] = 128;
		}
	}

	if ((sy - 1) > 0)
	{
		if (((TerrainWall[(TERRAIN_SIZE - 1 - (sy - 1)) * TERRAIN_SIZE + sx] >> 0) & 0xFC) != 0)
		{
			dst = &bitmap[((((sy - 1) * 2) + 1) * MAP_SIZE + (sx * 2)) * 4];

			dst[0] = 192;
			dst[1] = 254;
			dst[2] = 243;
			dst[3] = 128;

			dst[4] = 192;
			dst[5] = 254;
			dst[6] = 243;
			dst[7] = 128;
		}
	}

	if ((sy + 1) < TERRAIN_SIZE)
	{
		if (((TerrainWall[(TERRAIN_SIZE - 1 - (sy + 1)) * TERRAIN_SIZE + sx] >> 0) & 0xFC) != 0)
		{
			dst = &bitmap[((((sy + 1) * 2)) * MAP_SIZE + (sx * 2)) * 4];

			dst[0] = 192;
			dst[1] = 254;
			dst[2] = 243;
			dst[3] = 128;

			dst[4] = 192;
			dst[5] = 254;
			dst[6] = 243;
			dst[7] = 128;
		}
	}
}

void CMiniMap::LoadMiniMapGates()
{
	this->m_GatesIndex.clear();

	for (BYTE i = 0; i < MAX_GATES; i++)
	{
		if (GateAttribute[i].Flag == 1 && GateAttribute[i].Map == World)
		{
			this->m_GatesIndex.push_back(i);
		}
	}
}

void CMiniMap::LoadImages()
{
	// Load Cold Steel Minimap Atlas (1024x1024 RGBA) with mipmaps into MINIMAP_FRAME_TEXTURE
	BITMAP_t* pFrameBmp = gLoadModels.GetBitmap(MINIMAP_FRAME_TEXTURE);
	if (pFrameBmp)
	{
		int w = 0, h = 0, ch = 0;
		unsigned char* pixels = stbi_load("Data\\Interface\\MiniMapCS.png", &w, &h, &ch, 4);
		if (pixels && w > 0 && h > 0)
		{
			GLuint frameTexId = 0;
			glGenTextures(1, &frameTexId);
			glBindTexture(GL_TEXTURE_2D, frameTexId);

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
			glBindTexture(GL_TEXTURE_2D, 0);

			pFrameBmp->Width = (float)w;
			pFrameBmp->Height = (float)h;
			pFrameBmp->Components = 4;
			pFrameBmp->TextureNumber = frameTexId;
			pFrameBmp->Ref = 1;

			stbi_image_free(pixels);
		}
		else
		{
			OpenTGA(MINIMAP_FRAME_TEXTURE_NAME, MINIMAP_FRAME_TEXTURE, GL_NEAREST, GL_CLAMP, 0, 1);
		}
	}

	// Map Texture
	glGenTextures(1, &this->MapTextureID); // Reserve the texture ID

	glBindTexture(GL_TEXTURE_2D, this->MapTextureID); // Bind the texture

	glTexImage2D(GL_TEXTURE_2D, 0, 4, MAP_SIZE, MAP_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)NULL); // Reserve memory space

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Smooth terrain scaling

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); // Set the texture wrap

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); // Set the texture wrap

	GLfloat borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };

	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor); // Specify the border color

	glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture
}

bool CMiniMap::GetMiniMapState()
{
	return this->MiniMapSwitch;
}

void CMiniMap::Toggle()
{
	if (!this->MiniMapTable[World])
	{
		this->MiniMapSwitch = false;

		return;
	}

	if (CheckInputInterfaces())
	{
		return;
	}

	if (CheckRightInterfaces())
	{
		this->MiniMapSwitch = false;

		return;
	}

	if (gEventTimer.GetEventTimerState())
	{
		gEventTimer.Toggle();
	}

	if (gMoveList.GetMoveListState())
	{
		gMoveList.Toggle();
	}

	this->MiniMapSwitch ^= 1;

	PlayBuffer(25, 0, 0);
}

// Cold Steel Minimap layout (virtual 640x480, top-right, aspect-matched to 16:9)
#define CS_MM_ATLAS_SIZE 1024.0f
#define CS_MM_W          104.0f
#define CS_MM_H          136.0f
#define CS_MM_X          (640.0f - CS_MM_W - 8.0f)
#define CS_MM_Y          8.0f
#define CS_MM_MAP_X      (CS_MM_X + CS_MM_W * 0.13f)
#define CS_MM_MAP_Y      (CS_MM_Y + CS_MM_H * 0.14f)
#define CS_MM_MAP_W      (CS_MM_W * 0.73f)
#define CS_MM_MAP_H      (CS_MM_H * 0.72f)
#define CS_MM_PLATE_Y    (CS_MM_Y + CS_MM_H + 1.0f)
#define CS_MM_PLATE_H    34.0f
#define CS_MM_BTN_Y      (CS_MM_PLATE_Y + CS_MM_PLATE_H + 2.0f)
#define CS_MM_BTN_H      13.0f
#define CS_MM_COLL_H     30.0f

static void MiniMapFlatRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
}

static void MiniMapCenterText(float cx, int y, const char* text, DWORD color, HFONT font)
{
	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, font);
	DWORD oldColor = SetTextColor;
	DWORD oldBg = SetBackgroundTextColor;

	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	float textW = (WindowWidth > 0) ? ((float)sz.cx * 640.0f / (float)WindowWidth) : (float)strlen(text) * 4.5f;

	SetTextColor = color;
	SetBackgroundTextColor = 0x00000000;
	RenderText((int)(cx - textW * 0.5f), y, text, 0, RT3_SORT_LEFT, NULL);

	SetTextColor = oldColor;
	SetBackgroundTextColor = oldBg;
	SelectObject(m_hFontDC, oldFont);
}

void CMiniMap::UpdateMouse()
{
	if (!this->MiniMapTable[World])
	{
		return;
	}

	if (CheckRightInterfaces())
	{
		return;
	}

	if (!this->MiniMapSwitch)
	{
		// Collapsed plate click -> expand minimap
		if (IsWorkZone((int)CS_MM_X, (int)CS_MM_Y, (int)CS_MM_W, (int)CS_MM_COLL_H))
		{
			MouseOnWindow = true;
			if (MouseLButton && MouseLButtonPush)
			{
				MouseLButtonPush = false;
				MouseUpdateTime = 0;
				MouseUpdateTimeMax = 6;
				PlayBuffer(25, 0, 0);
				this->MiniMapSwitch = true;
			}
		}
		return;
	}

	this->UpdateMouseMiniMap();
}

void CMiniMap::Render()
{
	if (!this->MiniMapTable[World])
	{
		return;
	}

	if (CheckRightInterfaces())
	{
		return;
	}

	if (!this->MiniMapSwitch)
	{
		// Render sleek Cold Steel collapsed plate with Map Name & Coordinates
		EnableAlphaTest(true);
		glColor4f(1.0f, 1.0f, 1.0f, 0.95f);
		RenderBitmap(MINIMAP_FRAME_TEXTURE, CS_MM_X, CS_MM_Y, CS_MM_W, CS_MM_COLL_H,
			0.0f / CS_MM_ATLAS_SIZE, 780.0f / CS_MM_ATLAS_SIZE,
			512.0f / CS_MM_ATLAS_SIZE, 172.0f / CS_MM_ATLAS_SIZE, true, true);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		DisableAlphaBlend();

		int hx = Hero ? (int)(*(float*)(Hero + 0x10) / 100.0f) : 0;
		int hy = Hero ? (int)(*(float*)(Hero + 0x14) / 100.0f) : 0;
		char label[64];
		sprintf_s(label, sizeof(label), "%s  %d,%d", gMapManager.GetMapName(World), hx, hy);
		MiniMapCenterText(CS_MM_X + CS_MM_W * 0.5f, (int)(CS_MM_Y + 10.0f), label, 0xFFE6EEF2, g_hFontBold);
		return;
	}

	this->RenderMiniMap();
}

/*******************************************************/
/*********************** MINIMAP ***********************/
/*******************************************************/

void CMiniMap::UpdateMouseMiniMap()
{
	const float totalH = (CS_MM_BTN_Y + CS_MM_BTN_H) - CS_MM_Y;
	if (!IsWorkZone((int)CS_MM_X, (int)CS_MM_Y, (int)CS_MM_W, (int)totalH))
	{
		return;
	}

	MouseOnWindow = true;

	// 4 compact control buttons below the coordinate plate: [-], [+], [A], [^]
	const float gap = 2.0f;
	const float bw = (CS_MM_W - gap * 3.0f) / 4.0f;

	for (int i = 0; i < 4; i++)
	{
		float bx = CS_MM_X + i * (bw + gap);
		if (IsWorkZone((int)bx, (int)CS_MM_BTN_Y, (int)bw, (int)CS_MM_BTN_H))
		{
			if (MouseLButton && MouseLButtonPush)
			{
				MouseLButtonPush = false;
				MouseUpdateTime = 0;
				MouseUpdateTimeMax = 6;
				PlayBuffer(25, 0, 0);

				if (i == 0) // Zoom out
				{
					int z = (this->ZoomLevel > 0) ? (this->ZoomLevel - 1) : 0;
					this->ChangeZoom(z);
				}
				else if (i == 1) // Zoom in
				{
					int z = (this->ZoomLevel < 6) ? (this->ZoomLevel + 1) : 6;
					this->ChangeZoom(z);
				}
				else if (i == 2) // Alpha
				{
					this->ChangeAlpha();
				}
				else if (i == 3) // Collapse
				{
					this->MiniMapSwitch = false;
				}
			}
			return;
		}
	}
}

void CMiniMap::RenderMiniMap()
{
	if (!Hero)
	{
		return;
	}

	this->VisualRange = 8 * (8 - this->ZoomLevel);

	this->MaxSize = CS_MM_MAP_W;

	this->Scale = 256.0f / (this->VisualRange * 2);

	this->Size = 512.0f / this->Scale;

	this->CharPosX = *(float*)(Hero + 0x10) / 100.0f;

	this->CharPosY = *(float*)(Hero + 0x14) / 100.0f;

	this->MapOffsetX = min(max((this->CharPosX - this->VisualRange), 0.0f), 256.0f - (this->VisualRange * 2));
	this->MapOffsetX /= 256.0f;

	this->MapOffsetY = min(max(((256.0f - this->CharPosY) - this->VisualRange), 0.0f), 256.0f - (this->VisualRange * 2));
	this->MapOffsetY /= 256.0f;

	this->RenderMiniMapBackground();

	this->RenderMiniMapCharacters();

	this->RenderMiniMapGates();

	this->RenderMiniMapFrame();

	this->RenderMiniMapButtons();
}

void CMiniMap::RenderMiniMapBackground()
{
	const float a = this->Alpha * 0.1f;

	// Dark Cold Steel backing inside the square cutout
	MiniMapFlatRect(CS_MM_MAP_X, CS_MM_MAP_Y, CS_MM_MAP_W, CS_MM_MAP_H, 0.04f, 0.06f, 0.09f, a * 0.85f);

	// Terrain texture cropped to current visual range
	EnableAlphaTest(true);
	glColor4f(1.0f, 1.0f, 1.0f, a);
	RenderBitmap(
		MINIMAP_TEXTURE,
		CS_MM_MAP_X,
		CS_MM_MAP_Y,
		CS_MM_MAP_W,
		CS_MM_MAP_H,
		this->MapOffsetX,
		this->MapOffsetY,
		this->Size / 512.0f,
		this->Size / 512.0f,
		true,
		true
	);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

void CMiniMap::RenderMiniMapCharacters()
{
	const float a = this->Alpha * 0.1f;
	const float uvSpan = this->Size / 512.0f;
	if (uvSpan <= 0.0001f)
	{
		return;
	}

	for (int i = 0; i < MAX_MAIN_VIEWPORT; i++)
	{
		DWORD ViewportAddress = CharactersClient + (i * 916);
		if (ViewportAddress == 0 || *(BYTE*)(ViewportAddress) == 0)
		{
			continue;
		}

		float normX = ((*(float*)(ViewportAddress + 0x10) / 100.0f) / 256.0f - this->MapOffsetX) / uvSpan;
		float normY = ((256.0f - (*(float*)(ViewportAddress + 0x14) / 100.0f)) / 256.0f - this->MapOffsetY) / uvSpan;

		if (normX < 0.03f || normX > 0.97f || normY < 0.03f || normY > 0.97f)
		{
			continue;
		}

		float px = CS_MM_MAP_X + normX * CS_MM_MAP_W;
		float py = CS_MM_MAP_Y + normY * CS_MM_MAP_H;
		BYTE kind = *(BYTE*)(ViewportAddress + 0x84);

		if (kind == 1) // Player
		{
			if (ViewportAddress == Hero)
			{
				MiniMapFlatRect(px - 2.0f, py - 2.5f, 4.0f, 5.0f, 0.15f, 0.95f, 0.45f, a);
				MiniMapFlatRect(px - 1.0f, py - 1.0f, 2.0f, 2.0f, 1.0f, 1.0f, 1.0f, a);
			}
			else
			{
				MiniMapFlatRect(px - 1.5f, py - 2.0f, 3.0f, 4.0f, 1.0f, 0.88f, 0.25f, a);
			}
		}
		else if (kind == 2) // Monster
		{
			MiniMapFlatRect(px - 1.2f, py - 1.6f, 2.4f, 3.2f, 0.92f, 0.22f, 0.22f, a * 0.9f);
		}
		else if (kind == 4) // NPC
		{
			MiniMapFlatRect(px - 1.5f, py - 2.0f, 3.0f, 4.0f, 0.435f, 0.714f, 0.769f, a);
		}
	}

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

void CMiniMap::RenderMiniMapGates()
{
	const float a = this->Alpha * 0.1f;
	const float uvSpan = this->Size / 512.0f;
	if (uvSpan <= 0.0001f)
	{
		return;
	}

	char hoverText[64] = { 0 };
	float hoverX = 0.0f, hoverY = 0.0f;

	for each (int index in this->m_GatesIndex)
	{
		GATE_ATTRIBUTE* gate = &GateAttribute[index];
		float gx = gate->StartX + ceil((gate->EndX - gate->StartX) / 2.0f);
		float gy = gate->StartY + ceil((gate->EndY - gate->StartY) / 2.0f);

		float normX = ((gx / 256.0f) - this->MapOffsetX) / uvSpan;
		float normY = (((256.0f - gy) / 256.0f) - this->MapOffsetY) / uvSpan;

		if (normX < 0.03f || normX > 0.97f || normY < 0.03f || normY > 0.97f)
		{
			continue;
		}

		float px = CS_MM_MAP_X + normX * CS_MM_MAP_W;
		float py = CS_MM_MAP_Y + normY * CS_MM_MAP_H;

		MiniMapFlatRect(px - 2.0f, py - 2.5f, 4.0f, 5.0f, 0.20f, 0.90f, 1.0f, a);

		if (IsWorkZone((int)(px - 4.0f), (int)(py - 4.0f), 8, 8))
		{
			GATE_ATTRIBUTE* targetGate = &GateAttribute[gate->Target];
			sprintf_s(hoverText, sizeof(hoverText), "-> %s", gMapManager.GetMapName(targetGate->Map));
			hoverX = px;
			hoverY = py - 12.0f;
		}
	}

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();

	if (hoverText[0] != '\0')
	{
		MiniMapCenterText(hoverX, (int)hoverY, hoverText, 0xFF64F0FF, g_hFontBold);
	}
}

void CMiniMap::RenderMiniMapFrame()
{
	EnableAlphaTest(true);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	// Cold Steel Square Frame overlaid cleanly on top of the map edges
	RenderBitmap(
		MINIMAP_FRAME_TEXTURE,
		CS_MM_X,
		CS_MM_Y,
		CS_MM_W,
		CS_MM_H,
		0.0f / CS_MM_ATLAS_SIZE,
		0.0f / CS_MM_ATLAS_SIZE,
		571.0f / CS_MM_ATLAS_SIZE,
		563.0f / CS_MM_ATLAS_SIZE,
		true,
		true
	);

	// Cold Steel Coordinate Plate directly below the minimap
	RenderBitmap(
		MINIMAP_FRAME_TEXTURE,
		CS_MM_X,
		CS_MM_PLATE_Y,
		CS_MM_W,
		CS_MM_PLATE_H,
		0.0f / CS_MM_ATLAS_SIZE,
		584.0f / CS_MM_ATLAS_SIZE,
		512.0f / CS_MM_ATLAS_SIZE,
		178.0f / CS_MM_ATLAS_SIZE,
		true,
		true
	);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();

	// Map Name + Hero Coordinates centered inside the coordinate plate
	const float plateCX = CS_MM_X + CS_MM_W * 0.5f;
	MiniMapCenterText(plateCX, (int)(CS_MM_PLATE_Y + 7.0f), gMapManager.GetMapName(World), 0xFFF0F4F8, g_hFontBold);

	char coords[32];
	sprintf_s(coords, sizeof(coords), "%d, %d", (int)this->CharPosX, (int)this->CharPosY);
	MiniMapCenterText(plateCX, (int)(CS_MM_PLATE_Y + 18.0f), coords, 0xFFC4B66F, g_hFont);
}

void CMiniMap::RenderMiniMapButtons()
{
	const float gap = 2.0f;
	const float bw = (CS_MM_W - gap * 3.0f) / 4.0f;
	const char* labels[4] = { "-", "+", "A", "^" };

	for (int i = 0; i < 4; i++)
	{
		float bx = CS_MM_X + i * (bw + gap);
		bool hover = IsWorkZone((int)bx, (int)CS_MM_BTN_Y, (int)bw, (int)CS_MM_BTN_H);

		MiniMapFlatRect(bx, CS_MM_BTN_Y, bw, CS_MM_BTN_H, 0.23f, 0.27f, 0.32f, 0.92f);
		if (hover)
		{
			MiniMapFlatRect(bx + 1.0f, CS_MM_BTN_Y + 1.0f, bw - 2.0f, CS_MM_BTN_H - 2.0f, 0.22f, 0.42f, 0.48f, 0.95f);
		}
		else
		{
			MiniMapFlatRect(bx + 1.0f, CS_MM_BTN_Y + 1.0f, bw - 2.0f, CS_MM_BTN_H - 2.0f, 0.08f, 0.11f, 0.14f, 0.88f);
		}

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		DisableAlphaBlend();

		MiniMapCenterText(bx + bw * 0.5f, (int)(CS_MM_BTN_Y + 2.0f), labels[i], hover ? 0xFFFFFFFF : 0xFFC4DDE6, g_hFontBold);
	}
}

/*******************************************************/
/*********************** FULLMAP ***********************/
/*******************************************************/

void CMiniMap::UpdateMouseFullMap()
{
	float StartPosX = 0.0f;

	float StartPosY = 411.0f;

	if (IsWorkZone((int)StartPosX, (int)StartPosY, 96, 21)) // Zoom
	{
		MouseOnWindow = true;

		StartPosX += 75.0f;

		if (IsWorkZone((int)StartPosX, (int)StartPosY, 21, 21)) // Zoom Button
		{
			if (MouseLButton && MouseLButtonPush)
			{
				MouseLButtonPush = false;

				MouseUpdateTime = 0;

				MouseUpdateTimeMax = 6;

				PlayBuffer(25, 0, 0);

				this->ChangeZoom();
			}
		}

		return;
	}

	StartPosX = (640.0f - 96.0f);

	if (IsWorkZone((int)StartPosX, (int)StartPosY, 96, 21)) // Alpha
	{
		MouseOnWindow = true;

		if (IsWorkZone((int)StartPosX, (int)StartPosY, 21, 21)) // Alpha Button
		{
			if (MouseLButton && MouseLButtonPush)
			{
				MouseLButtonPush = false;

				MouseUpdateTime = 0;

				MouseUpdateTimeMax = 6;

				PlayBuffer(25, 0, 0);

				this->ChangeAlpha();
			}
		}

		return;
	}
}

void CMiniMap::RenderFullMap()
{
	this->VisualRange = 128 - (this->ZoomLevel * 16);

	this->MaxSize = 640.0f;

	this->Scale = 256.0f / (this->VisualRange * 2);

	this->Size = 512.0f / this->Scale;

	this->CharPosX = *(float*)(Hero + 0x10) / 100.0f;

	this->CharPosY = *(float*)(Hero + 0x14) / 100.0f;

	this->MapOffsetX = this->CharPosX - this->VisualRange;
	this->MapOffsetX /= 256.0f;

	this->MapOffsetY = (256.0f - this->CharPosY) - this->VisualRange;
	this->MapOffsetY /= 256.0f;

	this->RenderFullMapBackground();

	this->RenderFullMapCharacters();

	this->RenderFullMapGates();

	this->RenderFullMapZoom();

	this->RenderFullMapAlpha();
}

void CMiniMap::RenderFullMapBackground()
{
	EnableAlphaTest(true);

	glColor4f(1.0f, 1.0f, 1.0f, this->Alpha * 0.1f);

	MyRenderBitRotate
	(
		MINIMAP_TEXTURE,
		320.0f - 320.0f + (this->MaxSize * 0.5f),
		192.0f - 240.0f + (this->MaxSize * 0.5f),
		this->MaxSize,
		this->MaxSize,
		-45.0f,
		((this->MapOffsetX * 512.0f) / 512.0f),
		((this->MapOffsetY * 512.0f) / 512.0f),
		(this->Size / 512.0f),
		(this->Size / 512.0f)
	);
}

void CMiniMap::RenderFullMapCharacters()
{
	DWORD ViewportAddress;

	float TargetPosX;

	float TargetPosY;

	DWORD backupBgTextColor = SetBackgroundTextColor;

	DWORD backupTextColor = SetTextColor;

	for (int i = 0; i < MAX_MAIN_VIEWPORT; i++)
	{
		if ((ViewportAddress = (CharactersClient + (i * 916))) == 0)
		{
			continue;
		}

		if (*(BYTE*)(ViewportAddress) == 0)
		{
			continue;
		}

		TargetPosX = (((((*(float*)(ViewportAddress + 0x10) / 100.0f) / 256.0f) - this->MapOffsetX) * (this->MaxSize * this->Scale)));

		TargetPosY = (((((256.0f - (*(float*)(ViewportAddress + 0x14) / 100.0f)) / 256.0f) - this->MapOffsetY) * (this->MaxSize * this->Scale)));

		if (*(BYTE*)(ViewportAddress + 0x84) == 1) // Player
		{
			if (ViewportAddress == Hero)
			{
				EnableAlphaTest(true);

				glColor4f(0.0f, 1.0f, 0.0f, this->Alpha * 0.1f);

				MyRenderPointRotate
				(
					0x9,
					TargetPosX,
					TargetPosY,
					this->Scale * 7,
					this->Scale * 7,
					320.0f - 320.0f + (this->MaxSize * 0.5f),
					192.0f - 240.0f + (this->MaxSize * 0.5f),
					this->MaxSize,
					this->MaxSize,
					-45.0f,
					*(float*)(ViewportAddress + 0x24) - 45.0f,
					(2.0f / 32.0f),
					(14.0f / 32.0f),
					((32.0f - 2.0f) / 32.0f),
					((32.0f - 14.0f) / 32.0f),
					NULL
				);
			}
			else
			{
				EnableAlphaTest(true);

				glColor4f(1.0f, 1.0f, 0.0f, this->Alpha * 0.1f);

				MyRenderPointRotate
				(
					0x9,
					TargetPosX,
					TargetPosY,
					this->Scale * 7,
					this->Scale * 7,
					320.0f - 320.0f + (this->MaxSize * 0.5f),
					192.0f - 240.0f + (this->MaxSize * 0.5f),
					this->MaxSize,
					this->MaxSize,
					-45.0f,
					*(float*)(ViewportAddress + 0x24) - 45.0f,
					(2.0f / 32.0f),
					(14.0f / 32.0f),
					((32.0f - 2.0f) / 32.0f),
					((32.0f - 14.0f) / 32.0f),
					NULL
				);
			}
		}
		else if (*(BYTE*)(ViewportAddress + 0x84) == 2) // Monster
		{
			EnableAlphaTest(true);

			glColor4f(0.5f, 0.0f, 0.0f, this->Alpha * 0.1f);

			MyRenderPointRotate
			(
				0x9,
				TargetPosX,
				TargetPosY,
				this->Scale * 6,
				this->Scale * 6,
				320.0f - 320.0f + (this->MaxSize * 0.5f),
				192.0f - 240.0f + (this->MaxSize * 0.5f),
				this->MaxSize,
				this->MaxSize,
				-45.0f,
				*(float*)(ViewportAddress + 0x24) - 45.0f,
				(2.0f / 32.0f),
				(14.0f / 32.0f),
				((32.0f - 2.0f) / 32.0f),
				((32.0f - 14.0f) / 32.0f),
				NULL
			);
		}
		else if (*(BYTE*)(ViewportAddress + 0x84) == 4) // NPC
		{
			EnableAlphaTest(true);

			glColor4f(1.0f, 0.0f, 1.0f, this->Alpha * 0.1f);

			SelectObject(m_hFontDC, this->MiniMapFont);

			MyRenderPointRotate
			(
				0x9,
				TargetPosX,
				TargetPosY,
				this->Scale * 7,
				this->Scale * 7,
				320.0f - 320.0f + (this->MaxSize * 0.5f),
				192.0f - 240.0f + (this->MaxSize * 0.5f),
				this->MaxSize,
				this->MaxSize,
				-45.0f,
				*(float*)(ViewportAddress + 0x24) - 45.0f,
				(2.0f / 32.0f),
				(14.0f / 32.0f),
				((32.0f - 2.0f) / 32.0f),
				((32.0f - 14.0f) / 32.0f),
				(char*)(ViewportAddress + 0x1C1)
			);
		}
	}

	DisableAlphaBlend();

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	SelectObject(m_hFontDC, g_hFont);

	SetBackgroundTextColor = backupBgTextColor;

	SetTextColor = backupTextColor;
}

void CMiniMap::RenderFullMapGates()
{
	float TargetPosX;
	float TargetPosY;

	float DifX;
	float DifY;

	GATE_ATTRIBUTE* gate;
	GATE_ATTRIBUTE* targetGate;

	char text[64];

	for each (int index in this->m_GatesIndex)
	{
		gate = &GateAttribute[index];

		DifX = ceil((gate->EndX - gate->StartX) / 2.0f);
		DifY = ceil((gate->EndY - gate->StartY) / 2.0f);

		TargetPosX = gate->StartX + DifX;
		TargetPosY = gate->StartY + DifY;

		if (TargetPosX > (this->CharPosX + this->VisualRange) || TargetPosX < (this->CharPosX - this->VisualRange))
		{
			continue;
		}

		if (TargetPosY > (this->CharPosY + this->VisualRange) || TargetPosY < (this->CharPosY - this->VisualRange))
		{
			continue;
		}

		TargetPosX = (((((gate->StartX + DifX) / 256.0f) - this->MapOffsetX) * (this->MaxSize * this->Scale)));
		TargetPosY = (((((256.0f - (gate->StartY + DifY)) / 256.0f) - this->MapOffsetY) * (this->MaxSize * this->Scale)));

		targetGate = &GateAttribute[gate->Target];

		sprintf_s(text, "Gate to %s", gMapManager.GetMapName(targetGate->Map));

		EnableAlphaTest(true);

		glColor4f(0.0f, 1.0f, 1.0f, this->Alpha * 0.1f);

		SelectObject(m_hFontDC, this->MiniMapFont);

		MyRenderPointRotate
		(
			0x9,
			TargetPosX,
			TargetPosY,
			this->Scale * 10,
			this->Scale * 10,
			320.0f - 320.0f + (this->MaxSize * 0.5f),
			192.0f - 240.0f + (this->MaxSize * 0.5f),
			this->MaxSize,
			this->MaxSize,
			-45.0f,
			0.0f,
			(0.0f / 32.0f),
			(0.0f / 32.0f),
			(32.0f / 32.0f),
			(32.0f / 32.0f),
			text
		);
	}

	DisableAlphaBlend();

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	SelectObject(m_hFontDC, g_hFont);
}

void CMiniMap::RenderFullMapZoom()
{
	float StartPosX = 0.0f;

	float StartPosY = 411.0f;

	float alpha = 0.3f;

	if (IsWorkZone((int)StartPosX, (int)StartPosY, 96, 21))
	{
		alpha = 1.0f;
	}

	EnableAlphaTest(true);

	glColor4f(1.0f, 1.0f, 1.0f, alpha);

	RenderBitmap(0xF5, StartPosX, StartPosY, 75.0f, 21.0f, (0.0f / 128.0f), (0.0f / 32.0f), (75.0f / 128.0f), (21.0f / 32.0f), true, true);

	StartPosX += 75.0f;

	if (IsWorkZone((int)StartPosX, (int)StartPosY, 21, 21) && MouseLButton)
	{
		RenderBitmap(0x121, StartPosX, StartPosY, 21.0f, 21.0f, (0.0f / 32.0f), (0.0f / 32.0f), (24.0f / 32.0f), (24.0f / 32.0f), true, true);
	}
	else
	{
		RenderBitmap(0x120, StartPosX, StartPosY, 21.0f, 21.0f, (0.0f / 32.0f), (0.0f / 32.0f), (24.0f / 32.0f), (24.0f / 32.0f), true, true);
	}

	DisableAlphaBlend();

	glColor3f(1.0f, 1.0f, 1.0f);

	SelectObject(m_hFontDC, g_hFontBold);

	EnableAlphaTest(true);

	char Text[128] = { 0 };

	wsprintf(Text, "Zoom Level: %d", this->ZoomLevel);

	DWORD backupBgTextColor = SetBackgroundTextColor;

	SetBackgroundTextColor = Color4b(255, 255, 255, 0);

	DWORD backupTextColor = SetTextColor;

	SetTextColor = Color4b(255, 0, 0, (int)(alpha * 255));

	RenderText(5, CenterTextPosY(Text, 421), Text, REAL_WIDTH(60), RT3_SORT_LEFT, NULL);

	SetBackgroundTextColor = backupBgTextColor;

	SetTextColor = backupTextColor;

	SelectObject(m_hFontDC, g_hFont);

	DisableAlphaBlend();
}

void CMiniMap::RenderFullMapAlpha()
{
	float StartPosX = (640.0f - 75.0f);

	float StartPosY = 411.0f;

	float alpha = 0.3f;

	if (IsWorkZone((int)StartPosX - 21, (int)StartPosY, 96, 21))
	{
		alpha = 1.0f;
	}

	EnableAlphaTest(true);

	glColor4f(1.0f, 1.0f, 1.0f, alpha);

	RenderBitmap(0xF5, StartPosX, StartPosY, 75.0f, 21.0f, (75.0f / 128.0f), (0.0f / 32.0f), (-75.0f / 128.0f), (21.0f / 32.0f), true, true);

	StartPosX -= 21.0f;

	if (IsWorkZone((int)StartPosX, (int)StartPosY, 21, 21) && MouseLButton)
	{
		RenderBitmap(0x121, StartPosX, StartPosY, 21.0f, 21.0f, (0.0f / 32.0f), (0.0f / 32.0f), (24.0f / 32.0f), (24.0f / 32.0f), true, true);
	}
	else
	{
		RenderBitmap(0x120, StartPosX, StartPosY, 21.0f, 21.0f, (0.0f / 32.0f), (0.0f / 32.0f), (24.0f / 32.0f), (24.0f / 32.0f), true, true);
	}

	DisableAlphaBlend();

	glColor3f(1.0f, 1.0f, 1.0f);

	StartPosX += 31.0f;

	SelectObject(m_hFontDC, g_hFontBold);

	EnableAlphaTest(true);

	char Text[128] = { 0 };

	wsprintf(Text, "Alpha: %d", this->Alpha * 10);

	DWORD backupBgTextColor = SetBackgroundTextColor;

	SetBackgroundTextColor = Color4b(255, 255, 255, 0);

	DWORD backupTextColor = SetTextColor;

	SetTextColor = Color4b(255, 0, 0, (int)(alpha * 255));

	RenderText((int)StartPosX, CenterTextPosY(Text, 421), Text, REAL_WIDTH(60), RT3_SORT_LEFT, NULL);

	SetBackgroundTextColor = backupBgTextColor;

	SetTextColor = backupTextColor;

	SelectObject(m_hFontDC, g_hFont);

	DisableAlphaBlend();
}