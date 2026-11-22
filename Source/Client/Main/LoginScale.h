#pragma once

// Enlarges the original login window (frame, input boxes, buttons, captions) without touching its logic:
//  - while the login window block of RenderLogInScene runs, RenderBitmap/RenderText coordinates are scaled around a pivot
//  - while the login scene update runs, the mouse position is mapped back so the original hit tests keep working.

class CLoginScale
{
public:

	CLoginScale();

	void Init();

	float m_Scale;

	float m_PivotX;

	float m_PivotY;

	static void _cdecl UpdateLoginScene();

	static void ResetRender();
};

extern CLoginScale gLoginScale;
