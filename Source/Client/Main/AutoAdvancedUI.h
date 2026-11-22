#pragma once

// Window "Nâng cao" of the Smart Auto: the smart options that do not fit in the picture of the main window.
// Built with the MU UI Basic Bronze pieces (CustomUiKit).
class CAutoAdvancedUI
{
public:
	CAutoAdvancedUI();

	bool IsOpen() const { return this->m_Open; }
	void Open();
	void Close();

	void Render();

	// Takes every click while the window is open; returns true when the window is open (the mouse is taken)
	bool UpdateMouse();

private:
	bool m_Open;
};

extern CAutoAdvancedUI gAutoAdvancedUI;
