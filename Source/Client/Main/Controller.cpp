#include "stdafx.h"
#include "Controller.h"
#include "AutoHelper.h"
#include "Camera3D.h"
#include "CommandWindow.h"
#include "CustomComboUI.h"
#include "ScreenRecorder.h"
#include "JewelBankUI.h"
#include "MarketWindow.h"
#include "RankingUI.h"
#include "CashShopUI.h"
#include "PartyMatching.h"
#include "SmartAutoHelperUI.h"
#include "EventTimer.h"
#include "ItemManager.h"
#include "ItemPost.h"
#include "MiniMap.h"
#include "MoveList.h"
#include "resource.h"
#include "Window.h"

Controller gController;

Controller::Controller()
{
	this->Instance = NULL;

	this->MouseHook = NULL;

	this->KeyboardHook = NULL;

	this->AutoClick = false;

	this->AutoClickState = false;

	this->MouseWheel = 0;
}

Controller::~Controller()
{
	if (this->MouseHook)
	{
		UnhookWindowsHookEx(this->MouseHook);

		this->MouseHook = NULL;
	}

	if (this->KeyboardHook)
	{
		UnhookWindowsHookEx(this->KeyboardHook);

		this->KeyboardHook = NULL;
	}
}

void Controller::Init(HINSTANCE hins)
{
	this->Instance = hins;

	this->MouseHook = SetWindowsHookEx(WH_MOUSE, this->Mouse, this->Instance, GetCurrentThreadId());

	this->KeyboardHook = SetWindowsHookEx(WH_KEYBOARD, this->Keyboard, this->Instance, GetCurrentThreadId());

	SetDword(0x00552398, (DWORD)&GetAsyncKeyStateCall);

	SetCompleteHook(0xE8, 0x005259DD, &this->CheckKeyboardKeys);
}

LRESULT Controller::Mouse(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		MOUSEHOOKSTRUCTEX* HookStruct = (MOUSEHOOKSTRUCTEX*)lParam;

		switch (wParam)
		{
			case WM_LBUTTONDOWN:
			{
				if (GetForegroundWindow() == g_hWnd)
				{
					if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 || (GetKeyState(VK_SHIFT) & 0x8000) != 0)
					{
						ITEM* pHoverItem = gItemManager.GetHoveredItem(false);
						if (pHoverItem && pHoverItem->Type >= 0 && pHoverItem->Type < MAX_ITEM)
						{
							MouseLButtonPush = false;
							gItemPost.OnItemClickWithModifier(pHoverItem);
							return 1;
						}
					}
				}

				break;
			}

			case WM_MOUSEMOVE:
			{
				if (GetForegroundWindow() == g_hWnd)
				{
					Camera3D.Move(HookStruct);
				}

				break;
			}

			case WM_MBUTTONDOWN:
			{
				if (GetForegroundWindow() == g_hWnd)
				{
					Camera3D.SetIsMove(true);

					Camera3D.SetCursorX(HookStruct->pt.x);

					Camera3D.SetCursorY(HookStruct->pt.y);
				}

				break;
			}

			case WM_MBUTTONUP:
			{
				if (GetForegroundWindow() == g_hWnd)
				{
					Camera3D.SetIsMove(false);
				}

				break;
			}

			case WM_MBUTTONDBLCLK:
			{
				if (GetForegroundWindow() == g_hWnd)
				{
					Camera3D.Restore();
				}

				break;
			}

			case WM_RBUTTONDOWN:
			{
				if (GetForegroundWindow() == g_hWnd)
				{
					if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 || (GetKeyState(VK_SHIFT) & 0x8000) != 0)
					{
						ITEM* pHoverItem = gItemManager.GetHoveredItem(false);
						if (pHoverItem && pHoverItem->Type >= 0 && pHoverItem->Type < MAX_ITEM)
						{
							MouseRButtonPush = false;
							gItemPost.OnItemClickWithModifier(pHoverItem);
							return 1;
						}
					}

					if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
					{
						Camera3D.SetIsMove(true);

						Camera3D.SetCursorX(HookStruct->pt.x);

						Camera3D.SetCursorY(HookStruct->pt.y);
					}
				}

				break;
			}

			case WM_RBUTTONUP:
			{
				if (GetForegroundWindow() == g_hWnd)
				{
					if (Camera3D.GetIsMove())
					{
						Camera3D.SetIsMove(false);
					}
				}

				break;
			}

			case WM_MOUSEWHEEL:
			{
				if (GetForegroundWindow() == g_hWnd)
				{
					Camera3D.Zoom(HookStruct);
				}

				break;
			}
		}
	}

	return CallNextHookEx(gController.MouseHook, nCode, wParam, lParam);
}

LRESULT Controller::Keyboard(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		if (GetForegroundWindow() == g_hWnd)
		{
			if (wParam == 'C' && ((GetKeyState(VK_SHIFT) & 0x8000) != 0))
			{
				if (!CheckInputInterfaces() && !gPartyMatchingUI.IsEditingInput())
				{
					if (((DWORD)lParam & (1 << 31)) == 0 && ((DWORD)lParam & (1 << 30)) == 0)
					{
						gCustomComboUI.ToggleCombo();
					}
					return 1;
				}
			}

			if (wParam == 'P' && ((GetKeyState(VK_SHIFT) & 0x8000) != 0))
			{
				if (!CheckInputInterfaces() && !gSmartAutoHelperUI.IsItemInputFocused() && !gPartyMatchingUI.IsEditingInput())
				{
					if (((DWORD)lParam & (1 << 31)) == 0 && ((DWORD)lParam & (1 << 30)) == 0)
					{
						gCustomComboUI.TogglePKMode();
					}
					return 1;
				}
			}

			if ((wParam == VK_HOME || wParam == 'Z') && SceneFlag == 5 && Hero)
			{
				if (!CheckInputInterfaces() && !gSmartAutoHelperUI.IsItemInputFocused() && !gPartyMatchingUI.IsEditingInput())
				{
					if (((DWORD)lParam & (1 << 31)) == 0 && ((DWORD)lParam & (1 << 30)) == 0)
					{
						gAutoHelper.Toggle();
					}
					return 1;
				}
			}

			if (wParam == VK_END && SceneFlag == 5 && Hero)
			{
				if (!CheckInputInterfaces() && !gSmartAutoHelperUI.IsItemInputFocused() && !gPartyMatchingUI.IsEditingInput())
				{
					if (((DWORD)lParam & (1 << 31)) == 0 && ((DWORD)lParam & (1 << 30)) == 0)
					{
						gSmartAutoHelperUI.Toggle();
					}
					return 1;
				}
			}

			if (wParam == 'J' && SceneFlag == 5 && Hero)
			{
				if (!CheckInputInterfaces() && !gSmartAutoHelperUI.IsItemInputFocused() && !gMarketWindow.IsEditingPrice() && !gPartyMatchingUI.IsEditingInput())
				{
					if (((DWORD)lParam & (1 << 31)) == 0 && ((DWORD)lParam & (1 << 30)) == 0)
					{
						gJewelBankUI.Toggle();
					}
					return 1;
				}
			}

			if (wParam == VK_F5 && SceneFlag == 5 && Hero)
			{
				if (!CheckInputInterfaces() && !gSmartAutoHelperUI.IsItemInputFocused() && !gMarketWindow.IsEditingPrice() && !gPartyMatchingUI.IsEditingInput())
				{
					if (((DWORD)lParam & (1 << 31)) == 0 && ((DWORD)lParam & (1 << 30)) == 0)
					{
						gMarketWindow.Toggle();
					}
					return 1;
				}
			}

			if (wParam == VK_F6 && SceneFlag == 5 && Hero)
			{
				if (!CheckInputInterfaces() && !gSmartAutoHelperUI.IsItemInputFocused() && !gMarketWindow.IsEditingPrice() && !gPartyMatchingUI.IsEditingInput())
				{
					if (((DWORD)lParam & (1 << 31)) == 0 && ((DWORD)lParam & (1 << 30)) == 0)
					{
						gRankingUI.Toggle();
					}
					return 1;
				}
			}

			if (wParam == VK_F7 && SceneFlag == 5 && Hero)
			{
				if (!CheckInputInterfaces() && !gSmartAutoHelperUI.IsItemInputFocused() && !gMarketWindow.IsEditingPrice() && !gPartyMatchingUI.IsEditingInput())
				{
					if (((DWORD)lParam & (1 << 31)) == 0 && ((DWORD)lParam & (1 << 30)) == 0)
					{
						gRankingUI.ToggleVipShop();
					}
					return 1;
				}
			}

			if (wParam == 'D' && SceneFlag == 5 && Hero)
			{
				if (!CheckInputInterfaces() && !gSmartAutoHelperUI.IsItemInputFocused() && !gMarketWindow.IsEditingPrice() && !gPartyMatchingUI.IsEditingInput())
				{
					if (((DWORD)lParam & (1 << 31)) == 0 && ((DWORD)lParam & (1 << 30)) == 0)
					{
						gCommandWindow.Toggle();
					}
					return 1;
				}
			}

			if (wParam == 'X' && SceneFlag == 5 && Hero)
			{
				if (!CheckInputInterfaces() && !gSmartAutoHelperUI.IsItemInputFocused() && !gMarketWindow.IsEditingPrice() && !gPartyMatchingUI.IsEditingInput())
				{
					if (((DWORD)lParam & (1 << 31)) == 0 && ((DWORD)lParam & (1 << 30)) == 0)
					{
						gCashShopUI.Toggle();
					}
					return 1;
				}
			}

			if (wParam == VK_ESCAPE && SceneFlag == 5 && Hero)
			{
				if (gPartyMatchingUI.IsOpen())
				{
					if (((DWORD)lParam & (1 << 31)) == 0 && ((DWORD)lParam & (1 << 30)) == 0)
					{
						gPartyMatchingUI.Close();
					}
					return 1;
				}
				if (gCommandWindow.IsOpen() || gCommandWindow.IsActiveMode())
				{
					if (((DWORD)lParam & (1 << 31)) == 0 && ((DWORD)lParam & (1 << 30)) == 0)
					{
						gCommandWindow.Show(false);
					}
					return 1;
				}
			}
		}

		if (((DWORD)lParam & (1 << 30)) != 0 && ((DWORD)lParam & (1 << 31)) != 0)
		{
			switch (wParam)
			{
				case VK_F8:
				{
					if (GetForegroundWindow() == g_hWnd && !CheckInputInterfaces() && !gSmartAutoHelperUI.IsItemInputFocused())
					{
						gScreenRecorder.Toggle();
					}

					break;
				}

				case VK_F9:
				{
					if (GetForegroundWindow() == g_hWnd)
					{
						gController.AutoClick ^= 1;

						if (gController.AutoClick)
						{
							SetTimer(g_hWnd, WM_AUTOCLICKTIMER, 100, 0);
						}
						else
						{
							KillTimer(g_hWnd, WM_AUTOCLICKTIMER);

							gController.AutoClickState = false;

							MouseRButtonPush = false;

							MouseRButton = false;
						}

						CreateNotice(((gController.AutoClick) ? "Autoclick Enabled" : "Autoclick Disabled"), 1);
					}

					break;
				}

				case VK_F10:
				{
					if (GetForegroundWindow() == g_hWnd)
					{
						Camera3D.Toggle();
					}

					break;
				}

				case VK_F11:
				{
					if (GetForegroundWindow() == g_hWnd)
					{
						Camera3D.Restore();
					}

					break;
				}

				case VK_F12:
				{
					if (GetForegroundWindow() == g_hWnd)
					{
						gWindow.ToggleTrayMode();
					}

					break;
				}
			}
		}
	}

	return CallNextHookEx(gController.KeyboardHook, nCode, wParam, lParam);
}

void Controller::CheckKeyboardKeys()
{
	if (gSmartAutoHelperUI.IsItemInputFocused() || gMarketWindow.IsEditingPrice() || gPartyMatchingUI.IsEditingInput())
	{
		return;
	}

	if ((GetAsyncKeyStateCall('M') >> 0x0F)) // M Pressed
	{
		if (!KeyState['M'])
		{
			KeyState['M'] = 1;

			gMoveList.Toggle();
		}
	}
	else
	{
		KeyState['M'] = 0;
	}

	if ((GetAsyncKeyStateCall('H') >> 0x0F)) // H Pressed
	{
		if (!KeyState['H'])
		{
			KeyState['H'] = 1;

			gEventTimer.Toggle();
		}
	}
	else
	{
		KeyState['H'] = 0;
	}

	if ((GetAsyncKeyStateCall(VK_TAB) >> 0x0F)) // Tab Pressed
	{
		if (!KeyState[VK_TAB])
		{
			KeyState[VK_TAB] = 1;

			gMiniMap.Toggle();
		}
	}
	else
	{
		KeyState[VK_TAB] = 0;
	}

	if ((GetAsyncKeyStateCall(VK_SPACE) >> 0x0F)) // Space Pressed
	{
		if (!KeyState[VK_SPACE])
		{
			KeyState[VK_SPACE] = 1;

			if (!InputEnable && !MouseOnWindow)
			{
				for (int i = 0; i < 1000; ++i)
				{
					DWORD ItemObject = (0x07E127F8 + 516 * i) + 72; // Items[i].Object

					if (*(BYTE*)(ItemObject + 0) // ItemObject->Live
					    && *(BYTE*)(ItemObject + 352)) // ItemObject->Visible
					{
						vec3_t vDir;

						VectorSubtract((float*)(ItemObject + 16), // ItemObject->Position
							       (float*)(Hero + 16), // Hero->Object.Position
							       vDir);

						if (sqrt(vDir[0] * vDir[0] + vDir[1] * vDir[1] + vDir[2] * vDir[2]) <= 300.0f)
						{
							*(BYTE*)(Hero + 749) = 1; // Hero->MovementType = MOVEMENT_GET;

							ItemKey = i;

							Action(Hero, ItemObject);

							*(BYTE*)(Hero + 749) = 0; // Hero->MovementType = MOVEMENT_MOVE;

							break;
						}
					}
				}
			}
		}
	}
	else
	{
		KeyState[VK_SPACE] = 0;
	}

	((void(__cdecl*)()) 0x004C04A0)(); // Original Function
}