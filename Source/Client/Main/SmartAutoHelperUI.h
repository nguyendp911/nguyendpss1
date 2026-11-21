#pragma once

#include <string>
#include <vector>
#include "Util.h"

class CSmartAutoHelperUI
{
public:
	CSmartAutoHelperUI();
	virtual ~CSmartAutoHelperUI();

	void Init();
	void LoadImages();
	void Toggle();
	void Show(bool show);
	bool IsOpen() const { return this->m_IsOpen; }
	bool IsItemInputFocused() const { return (this->m_IsOpen && (this->m_ItemInputFocused || this->m_FocusedNumberField != 0)); }

	void Render();
	void UpdateMouse();
	void UpdateKeyboard();
	void RenderHudButtons();
	bool UpdateHudButtonsMouse();

private:
	void RenderSkillIcon(int skill, float x, float y, float w, float h);
	void RenderSkillPicker();
	bool HandleSkillPickerClick();
	std::vector<int> BuildPickerSkills();
	void CommitNumberInput();

private:
	bool m_IsOpen;
	float m_PosX;
	float m_PosY;
	float m_Width;
	float m_Height;

	// Skill Picker
	bool m_ShowSkillPicker;
	int m_PickingSlotType; // 0 = Attack (0..2), 1 = Buff (0..2)
	int m_PickingSlotIndex;
	std::vector<int> m_AvailableSkills;

	// Extra Items Input & List
	char m_ItemInputBuffer[64];
	bool m_ItemInputFocused;
	int m_SelectedExtraItemIndex;
	int m_ExtraItemScrollOffset;

	// Number Input Focus (0=None, 1=ReturnPosDelay, 2=BuffInterval, 3=PartyBuffInterval, 4=ReturnMin, 5=ReturnMax)
	int m_FocusedNumberField;
	char m_NumberInputBuffer[16];

	// Textures
	GLuint m_TexAutoCS;
	GLuint m_SkillTextureID;

	// Window Dragging
	bool m_IsDragging;
	float m_DragOffsetX;
	float m_DragOffsetY;
};

extern CSmartAutoHelperUI gSmartAutoHelperUI;
