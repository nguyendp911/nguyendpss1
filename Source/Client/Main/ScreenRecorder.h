#pragma once

#include <windows.h>

class CScreenRecorder
{
public:
	CScreenRecorder();
	~CScreenRecorder();

	void Init();
	void LoadImages();
	void Shutdown();

	void Toggle();
	bool Start();
	void Stop();
	bool IsRecording() const { return this->m_Recording; }

	// Call once per frame after game & HUD have rendered (before REC button overlay)
	void CaptureFrame();

	// Top-left Cold Steel HUD button + live REC timer
	void Render();
	bool UpdateMouse();

private:
	static DWORD WINAPI EncoderThreadProc(LPVOID param);
	void EncoderLoop();
	bool InitMediaFoundation(const wchar_t* outPath, UINT32 width, UINT32 height, UINT32 fps);
	void CloseMediaFoundation();

private:
	volatile bool m_Recording;
	volatile bool m_StopRequested;
	volatile bool m_EncoderReady;
	volatile bool m_EncoderFailed;

	UINT32 m_Width;
	UINT32 m_Height;
	UINT32 m_Fps;

	DWORD m_StartTick;
	DWORD m_LastCaptureTick;
	DWORD m_FrameCount;

	char m_OutputPathA[MAX_PATH];
	wchar_t m_OutputPathW[MAX_PATH];

	HANDLE m_Thread;
	HANDLE m_FrameEvent;
	CRITICAL_SECTION m_Lock;

	struct PendingFrame
	{
		BYTE* pixels;
		LONGLONG timestamp100ns;
	};

	static const int MAX_QUEUE = 4;
	PendingFrame m_Queue[MAX_QUEUE];
	int m_QueueHead;
	int m_QueueTail;
	int m_QueueCount;

	// Opaque Media Foundation pointers (kept as void* so header stays lightweight)
	void* m_SinkWriter;
	DWORD m_StreamIndex;
	bool m_MfStarted;

	unsigned int m_TexRecCS;
};

extern CScreenRecorder gScreenRecorder;
