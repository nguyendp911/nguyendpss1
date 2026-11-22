#include "stdafx.h"
#include "ScreenRecorder.h"
#include "Interface.h"
#include "Sound.h"

#undef WINVER
#undef _WIN32_WINNT
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#undef CreateObject
#undef Action

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <cmath>
#include <cstdio>

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

CScreenRecorder gScreenRecorder;

static inline DWORD SRColor(BYTE r, BYTE g, BYTE b, BYTE a)
{
	return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

static inline void SRRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor(x, y, w, h);
}

static inline bool SRHit(float x, float y, float w, float h)
{
	return (MouseX >= (int)x && MouseX <= (int)(x + w) && MouseY >= (int)y && MouseY <= (int)(y + h));
}

static inline void SRSetFont(HFONT f)
{
	SelectObject(m_hFontDC, f);
	((void(__thiscall*)(void*, HFONT))0x0040F650)(*(void**)0x055C9FF8, f);
}

static inline float SRMeasureW(const char* text)
{
	if (!text || !text[0]) return 0.0f;
	SIZE sz = { 0, 0 };
	GetTextExtentPoint32A(m_hFontDC, text, (int)strlen(text), &sz);
	return (WindowWidth > 0) ? ((float)sz.cx * 640.0f / (float)WindowWidth) : (float)strlen(text) * 5.0f;
}

static inline void SRText(float x, float y, const char* text, DWORD color, bool bold = false)
{
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	EnableAlphaTest(true);
	SRSetFont(bold ? g_hFontBold : g_hFont);
	SetBackgroundTextColor = 0x00000000;
	SetTextColor = color;
	RenderText((int)x, (int)y, text, 0, RT3_SORT_LEFT, NULL);
}

static void GetRecorderHudLayout(float* btnW, float* btnH, float* recX, float* y)
{
	const float asp = (g_fScreenRate_x > 0.0f) ? (g_fScreenRate_y / g_fScreenRate_x) : 0.75f;
	*btnH = 22.0f;
	*btnW = (*btnH) * asp;
	*y = 8.0f;
	// Collapsible slot 8: Pinned(0..5), PK(6), Market(7), Recorder(8)
	*recX = 8.0f + (*btnW + 3.0f) * 8.0f;
}

CScreenRecorder::CScreenRecorder()
{
	this->m_Recording = false;
	this->m_StopRequested = false;
	this->m_EncoderReady = false;
	this->m_EncoderFailed = false;

	this->m_Width = 0;
	this->m_Height = 0;
	this->m_Fps = 30;

	this->m_StartTick = 0;
	this->m_LastCaptureTick = 0;
	this->m_FrameCount = 0;

	memset(this->m_OutputPathA, 0, sizeof(this->m_OutputPathA));
	memset(this->m_OutputPathW, 0, sizeof(this->m_OutputPathW));

	this->m_Thread = NULL;
	this->m_FrameEvent = NULL;
	InitializeCriticalSection(&this->m_Lock);

	for (int i = 0; i < MAX_QUEUE; i++)
	{
		this->m_Queue[i].pixels = NULL;
		this->m_Queue[i].timestamp100ns = 0;
	}
	this->m_QueueHead = 0;
	this->m_QueueTail = 0;
	this->m_QueueCount = 0;

	this->m_SinkWriter = NULL;
	this->m_StreamIndex = 0;
	this->m_MfStarted = false;
	this->m_TexRecCS = 0;
}

CScreenRecorder::~CScreenRecorder()
{
	this->Shutdown();
	DeleteCriticalSection(&this->m_Lock);
}

void CScreenRecorder::Init()
{
}

void CScreenRecorder::LoadImages()
{
	if (this->m_TexRecCS != 0)
	{
		return;
	}

	int w = 0, h = 0, ch = 0;
	unsigned char* data = stbi_load("Data\\Interface\\RecCS.png", &w, &h, &ch, 4);
	if (data && w > 0 && h > 0)
	{
		GLuint tex = 0;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);

		int mw = w, mh = h;
		unsigned char* cur = data;
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
				for (int x = 0; x < nw; x++)
				{
					int sx = x * 2;
					int sy = y * 2;
					int sx1 = (sx + 1 < mw) ? sx + 1 : sx;
					int sy1 = (sy + 1 < mh) ? sy + 1 : sy;
					const unsigned char* p00 = cur + ((sy * mw + sx) * 4);
					const unsigned char* p10 = cur + ((sy * mw + sx1) * 4);
					const unsigned char* p01 = cur + ((sy1 * mw + sx) * 4);
					const unsigned char* p11 = cur + ((sy1 * mw + sx1) * 4);
					unsigned char* out = dst + ((y * nw + x) * 4);
					for (int c = 0; c < 4; c++)
					{
						out[c] = (unsigned char)((p00[c] + p10[c] + p01[c] + p11[c] + 2) >> 2);
					}
				}
			}
			if (cur != data) free(cur);
			cur = dst;
			mw = nw;
			mh = nh;
		}
		if (cur != data) free(cur);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

		this->m_TexRecCS = (unsigned int)tex;
	}
	if (data) stbi_image_free(data);
}

void CScreenRecorder::Shutdown()
{
	if (this->m_Recording || this->m_Thread != NULL)
	{
		this->Stop();
	}
}

void CScreenRecorder::Toggle()
{
	if (this->m_Recording)
	{
		this->Stop();
	}
	else
	{
		this->Start();
	}
}

bool CScreenRecorder::Start()
{
	if (this->m_Recording || this->m_Thread != NULL)
	{
		return false;
	}

	UINT32 w = (WindowWidth > 0) ? (UINT32)(WindowWidth & ~1) : 640;
	UINT32 h = (WindowHeight > 0) ? (UINT32)(WindowHeight & ~1) : 480;
	if (w < 64 || h < 64)
	{
		return false;
	}

	CreateDirectoryA("Recordings", NULL);

	SYSTEMTIME st;
	GetLocalTime(&st);
	sprintf_s(this->m_OutputPathA, sizeof(this->m_OutputPathA),
		"Recordings\\MU_%04d-%02d-%02d_%02d-%02d-%02d.mp4",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	MultiByteToWideChar(CP_ACP, 0, this->m_OutputPathA, -1, this->m_OutputPathW, MAX_PATH);

	this->m_Width = w;
	this->m_Height = h;
	this->m_Fps = 30;
	this->m_StopRequested = false;
	this->m_EncoderReady = false;
	this->m_EncoderFailed = false;
	this->m_FrameCount = 0;
	this->m_QueueHead = 0;
	this->m_QueueTail = 0;
	this->m_QueueCount = 0;

	this->m_FrameEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (!this->m_FrameEvent)
	{
		return false;
	}

	this->m_Thread = CreateThread(NULL, 0, &CScreenRecorder::EncoderThreadProc, this, 0, NULL);
	if (!this->m_Thread)
	{
		CloseHandle(this->m_FrameEvent);
		this->m_FrameEvent = NULL;
		return false;
	}

	// Wait briefly for Media Foundation SinkWriter initialization
	DWORD waitStart = GetTickCount();
	while (!this->m_EncoderReady && !this->m_EncoderFailed && (GetTickCount() - waitStart < 3000))
	{
		Sleep(10);
	}

	if (!this->m_EncoderReady || this->m_EncoderFailed)
	{
		this->Stop();
		CreateNotice("Failed to initialize MP4 video recorder!", 1);
		return false;
	}

	this->m_StartTick = GetTickCount();
	this->m_LastCaptureTick = 0;
	this->m_Recording = true;

	char msg[160];
	sprintf_s(msg, sizeof(msg), "Recording started (%ux%u @ %u FPS)...", this->m_Width, this->m_Height, this->m_Fps);
	CreateNotice(msg, 1);
	return true;
}

void CScreenRecorder::Stop()
{
	bool wasRecording = this->m_Recording;
	this->m_Recording = false;
	this->m_StopRequested = true;

	if (this->m_FrameEvent)
	{
		SetEvent(this->m_FrameEvent);
	}

	if (this->m_Thread)
	{
		WaitForSingleObject(this->m_Thread, 5000);
		CloseHandle(this->m_Thread);
		this->m_Thread = NULL;
	}

	if (this->m_FrameEvent)
	{
		CloseHandle(this->m_FrameEvent);
		this->m_FrameEvent = NULL;
	}

	EnterCriticalSection(&this->m_Lock);
	for (int i = 0; i < MAX_QUEUE; i++)
	{
		if (this->m_Queue[i].pixels)
		{
			free(this->m_Queue[i].pixels);
			this->m_Queue[i].pixels = NULL;
		}
	}
	this->m_QueueHead = 0;
	this->m_QueueTail = 0;
	this->m_QueueCount = 0;
	LeaveCriticalSection(&this->m_Lock);

	if (wasRecording)
	{
		char msg[320];
		sprintf_s(msg, sizeof(msg), "Video saved: %s", this->m_OutputPathA);
		CreateNotice(msg, 1);
	}
}

void CScreenRecorder::CaptureFrame()
{
	if (!this->m_Recording || !this->m_EncoderReady || this->m_EncoderFailed)
	{
		return;
	}

	DWORD now = GetTickCount();
	const DWORD frameIntervalMs = 1000 / this->m_Fps; // ~33ms for 30 FPS
	if (this->m_LastCaptureTick != 0 && (now - this->m_LastCaptureTick) < frameIntervalMs)
	{
		return;
	}

	// If window size changed mid-recording, stop cleanly to avoid corrupted video
	UINT32 curW = (WindowWidth > 0) ? (UINT32)(WindowWidth & ~1) : 0;
	UINT32 curH = (WindowHeight > 0) ? (UINT32)(WindowHeight & ~1) : 0;
	if (curW != this->m_Width || curH != this->m_Height)
	{
		this->Stop();
		return;
	}

	// Drop frame if encoder queue is full (never stall the game loop)
	EnterCriticalSection(&this->m_Lock);
	bool full = (this->m_QueueCount >= MAX_QUEUE);
	LeaveCriticalSection(&this->m_Lock);
	if (full)
	{
		return;
	}

	const size_t bufSize = (size_t)this->m_Width * (size_t)this->m_Height * 4;
	BYTE* pixels = (BYTE*)malloc(bufSize);
	if (!pixels)
	{
		return;
	}

	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glReadPixels(0, 0, (GLsizei)this->m_Width, (GLsizei)this->m_Height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);

	LONGLONG ts100ns = (LONGLONG)(now - this->m_StartTick) * 10000LL;
	this->m_LastCaptureTick = now;
	this->m_FrameCount++;

	EnterCriticalSection(&this->m_Lock);
	this->m_Queue[this->m_QueueTail].pixels = pixels;
	this->m_Queue[this->m_QueueTail].timestamp100ns = ts100ns;
	this->m_QueueTail = (this->m_QueueTail + 1) % MAX_QUEUE;
	this->m_QueueCount++;
	LeaveCriticalSection(&this->m_Lock);

	if (this->m_FrameEvent)
	{
		SetEvent(this->m_FrameEvent);
	}
}

DWORD WINAPI CScreenRecorder::EncoderThreadProc(LPVOID param)
{
	CScreenRecorder* self = (CScreenRecorder*)param;
	self->EncoderLoop();
	return 0;
}

bool CScreenRecorder::InitMediaFoundation(const wchar_t* outPath, UINT32 width, UINT32 height, UINT32 fps)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	bool comInit = SUCCEEDED(hr);
	if (hr == RPC_E_CHANGED_MODE)
	{
		hr = S_OK;
		comInit = false;
	}
	if (FAILED(hr)) return false;

	hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	if (FAILED(hr))
	{
		if (comInit) CoUninitialize();
		return false;
	}
	this->m_MfStarted = true;

	IMFAttributes* pAttr = NULL;
	MFCreateAttributes(&pAttr, 2);
	if (pAttr)
	{
		pAttr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
		pAttr->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
	}

	IMFSinkWriter* pWriter = NULL;
	hr = MFCreateSinkWriterFromURL(outPath, NULL, pAttr, &pWriter);
	if (pAttr) pAttr->Release();
	if (FAILED(hr) || !pWriter)
	{
		return false;
	}

	// Output media type: H.264 MP4
	IMFMediaType* pTypeOut = NULL;
	hr = MFCreateMediaType(&pTypeOut);
	if (SUCCEEDED(hr))
	{
		pTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		pTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
		pTypeOut->SetUINT32(MF_MT_AVG_BITRATE, 4500000); // 4.5 Mbps crisp gameplay
		pTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		MFSetAttributeSize(pTypeOut, MF_MT_FRAME_SIZE, width, height);
		MFSetAttributeRatio(pTypeOut, MF_MT_FRAME_RATE, fps, 1);
		MFSetAttributeRatio(pTypeOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		hr = pWriter->AddStream(pTypeOut, &this->m_StreamIndex);
		pTypeOut->Release();
	}

	if (FAILED(hr))
	{
		pWriter->Release();
		return false;
	}

	// Input media type: RGB32 (BGRA) bottom-up (OpenGL glReadPixels native layout!)
	IMFMediaType* pTypeIn = NULL;
	hr = MFCreateMediaType(&pTypeIn);
	if (SUCCEEDED(hr))
	{
		pTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		pTypeIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
		pTypeIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		MFSetAttributeSize(pTypeIn, MF_MT_FRAME_SIZE, width, height);
		MFSetAttributeRatio(pTypeIn, MF_MT_FRAME_RATE, fps, 1);
		MFSetAttributeRatio(pTypeIn, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		hr = pWriter->SetInputMediaType(this->m_StreamIndex, pTypeIn, NULL);
		pTypeIn->Release();
	}

	if (FAILED(hr))
	{
		pWriter->Release();
		return false;
	}

	hr = pWriter->BeginWriting();
	if (FAILED(hr))
	{
		pWriter->Release();
		return false;
	}

	this->m_SinkWriter = pWriter;
	return true;
}

void CScreenRecorder::CloseMediaFoundation()
{
	if (this->m_SinkWriter)
	{
		IMFSinkWriter* pWriter = (IMFSinkWriter*)this->m_SinkWriter;
		pWriter->Finalize();
		pWriter->Release();
		this->m_SinkWriter = NULL;
	}

	if (this->m_MfStarted)
	{
		MFShutdown();
		this->m_MfStarted = false;
	}

	CoUninitialize();
}

void CScreenRecorder::EncoderLoop()
{
	if (!this->InitMediaFoundation(this->m_OutputPathW, this->m_Width, this->m_Height, this->m_Fps))
	{
		this->m_EncoderFailed = true;
		this->CloseMediaFoundation();
		return;
	}

	this->m_EncoderReady = true;

	IMFSinkWriter* pWriter = (IMFSinkWriter*)this->m_SinkWriter;
	const LONG stride = (LONG)(this->m_Width * 4);
	const DWORD frameBytes = (DWORD)(this->m_Width * this->m_Height * 4);
	const LONGLONG defaultDur = 10000000LL / (LONGLONG)this->m_Fps;
	LONGLONG lastTs = -1;

	while (true)
	{
		PendingFrame item = { NULL, 0 };

		EnterCriticalSection(&this->m_Lock);
		if (this->m_QueueCount > 0)
		{
			item = this->m_Queue[this->m_QueueHead];
			this->m_Queue[this->m_QueueHead].pixels = NULL;
			this->m_QueueHead = (this->m_QueueHead + 1) % MAX_QUEUE;
			this->m_QueueCount--;
		}
		bool done = (this->m_StopRequested && this->m_QueueCount == 0 && item.pixels == NULL);
		LeaveCriticalSection(&this->m_Lock);

		if (done)
		{
			break;
		}

		if (!item.pixels)
		{
			WaitForSingleObject(this->m_FrameEvent, 50);
			continue;
		}

		LONGLONG ts = item.timestamp100ns;
		if (ts <= lastTs)
		{
			ts = lastTs + defaultDur;
		}
		LONGLONG dur = (lastTs >= 0) ? (ts - lastTs) : defaultDur;
		if (dur < 100000LL) dur = defaultDur;
		lastTs = ts;

		IMFMediaBuffer* pBuffer = NULL;
		if (SUCCEEDED(MFCreateMemoryBuffer(frameBytes, &pBuffer)) && pBuffer)
		{
			BYTE* pDst = NULL;
			if (SUCCEEDED(pBuffer->Lock(&pDst, NULL, NULL)) && pDst)
			{
				// MFVideoFormat_RGB32 is bottom-up by default, matching OpenGL glReadPixels directly!
				memcpy(pDst, item.pixels, frameBytes);
				pBuffer->Unlock();
				pBuffer->SetCurrentLength(frameBytes);

				IMFSample* pSample = NULL;
				if (SUCCEEDED(MFCreateSample(&pSample)) && pSample)
				{
					pSample->AddBuffer(pBuffer);
					pSample->SetSampleTime(ts);
					pSample->SetSampleDuration(dur);
					pWriter->WriteSample(this->m_StreamIndex, pSample);
					pSample->Release();
				}
			}
			pBuffer->Release();
		}

		free(item.pixels);
	}

	this->CloseMediaFoundation();
}

static void DrawRecIconQuad(GLuint tex, float x, float y, float w, float h, float u0, float u1, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex);
	glColor4f(r, g, b, a);

	const float sx0 = x * g_fScreenRate_x;
	const float sx1 = (x + w) * g_fScreenRate_x;
	const float sy0 = (float)WindowHeight - y * g_fScreenRate_y;
	const float sy1 = (float)WindowHeight - (y + h) * g_fScreenRate_y;

	glBegin(GL_QUADS);
	glTexCoord2f(u0, 0.0f); glVertex2f(sx0, sy0);
	glTexCoord2f(u1, 0.0f); glVertex2f(sx1, sy0);
	glTexCoord2f(u1, 1.0f); glVertex2f(sx1, sy1);
	glTexCoord2f(u0, 1.0f); glVertex2f(sx0, sy1);
	glEnd();

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

// =========================================================================
// Top-Left Cold Steel HUD Button (7th icon, right next to SmartAuto)
// =========================================================================
void CScreenRecorder::Render()
{
	if (SceneFlag != 5 || !Hero) return;

	const bool collapsed = HudIconBarCollapsed();
	if (collapsed && !this->m_Recording) return;

	if (this->m_TexRecCS == 0)
	{
		this->LoadImages();
	}

	float btnW, btnH, recX, y;
	GetRecorderHudLayout(&btnW, &btnH, &recX, &y);

	const bool rec = this->m_Recording;
	const bool hov = !collapsed && SRHit(recX, y, btnW, btnH);
	const DWORD now = GetTickCount();

	if (!collapsed && this->m_TexRecCS != 0)
	{
		// Tile 0 (0.00..0.50): Idle Cold Steel camcorder | Tile 1 (0.50..1.00): Active crimson Stop
		float u0 = rec ? 0.50f : 0.00f;
		float u1 = rec ? 1.00f : 0.50f;
		float r = 1.0f, g = 1.0f, b = 1.0f;
		if (rec)
		{
			float pulse = 0.85f + 0.15f * sinf((float)now * 0.008f);
			r = 1.0f; g = pulse; b = pulse;
		}
		else
		{
			r = g = b = hov ? 1.0f : 0.90f;
		}
		DrawRecIconQuad((GLuint)this->m_TexRecCS, recX, y, btnW, btnH, u0, u1, r, g, b, 0.98f);
	}

	HFONT oldFont = (HFONT)SelectObject(m_hFontDC, g_hFont);
	const DWORD oldTC = SetTextColor;
	const DWORD oldBG = SetBackgroundTextColor;

	// Live REC timer pill next to the button while recording
	if (rec)
	{
		DWORD sec = (now - this->m_StartTick) / 1000;
		char tbuf[32];
		sprintf_s(tbuf, sizeof(tbuf), "REC %02u:%02u", sec / 60, sec % 60);
		SRSetFont(g_hFontBold);
		float tw = SRMeasureW(tbuf);
		float px = collapsed ? (8.0f + (btnW + 3.0f) * 7.0f + 1.0f) : (8.0f + (btnW + 3.0f) * 13.0f + 1.0f);
		float py = y + 3.0f;
		SRRect(px, py, tw + 10.0f, 16.0f, 0.75f, 0.14f, 0.18f, 0.92f);
		SRRect(px + 1.0f, py + 1.0f, tw + 8.0f, 14.0f, 0.12f, 0.04f, 0.06f, 0.90f);
		SRText(px + 5.0f, py + 3.0f, tbuf, SRColor(255, 110, 115, 255), true);
	}

	// Hover tooltip
	if (hov)
	{
		const char* tip = rec ? "Stop Recording (F8)" : "Record Screen Video (F8)";
		SRSetFont(g_hFont);
		float tw = SRMeasureW(tip);
		float ty = y + btnH + 4.0f;
		SRRect(recX, ty, tw + 10.0f, 15.0f, 0.23f, 0.27f, 0.32f, 0.95f);
		SRRect(recX + 1.0f, ty + 1.0f, tw + 8.0f, 13.0f, 0.055f, 0.075f, 0.098f, 0.92f);
		SRText(recX + 5.0f, ty + 2.5f, tip, SRColor(230, 242, 248, 255), false);
	}

	SRSetFont(oldFont ? oldFont : g_hFont);
	SetTextColor = oldTC;
	SetBackgroundTextColor = oldBG;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	DisableAlphaBlend();
}

bool CScreenRecorder::UpdateMouse()
{
	if (SceneFlag != 5 || !Hero || HudIconBarCollapsed()) return false;

	float btnW, btnH, recX, y;
	GetRecorderHudLayout(&btnW, &btnH, &recX, &y);

	if (!SRHit(recX, y, btnW, btnH)) return false;

	MouseOnWindow = true;

	if (MouseLButtonPush)
	{
		MouseLButtonPush = false;
		MouseLButton = false;
		MouseLButtonPop = false;
		PlayBuffer(25, 0, 0);
		this->Toggle();
		return true;
	}

	return false;
}
