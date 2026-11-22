#pragma once
#include <windows.h>

class CCustomShadow
{
public:
	CCustomShadow();
	virtual ~CCustomShadow();

	void Init();

	static void RenderCharacterShadowParts(DWORD c, DWORD o);
	static void MyRenderLinkObject(float x, float y, float z, DWORD c, DWORD f, int Type, int Level, int Option1, bool Link, bool Translate, int RenderType);

private:
	static void HookRenderCharacterShadowMale();
	static void HookRenderCharacterShadowFemale();
};

extern CCustomShadow gCustomShadow;
