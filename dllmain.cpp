// Need for Speed Underground & Underground 2 - Xtended Input plugin
// Bringing native XInput to NFS
// by Xan/Tenjoin

// TODO: BACKPORT STUFF FROM NFS_XtendedInput! Lots of stuff had been developed since this code was last updated!
// TODO: bring rumble/vibration function
// TODO: remapping? -- partially done, but only 1 event per key and 1 key per event
// TODO: kill DInput enough so that it doesn't detect XInput controllers but still detects wheels
// TODO: proper raw input for keyboard (and maybe non XInput gamepads?)
// TODO (UG 1/2): properly restore console FrontEnd objects (help messages, controller icons) -- partially done, HELP menu still needs to be restored
// TODO (UG): reassigned button textures -- so when you set Y button for BUTTON1 in FE, it should draw Y and not keep drawing X
// TODO (UG2): maybe button hash assignments and FE stuff
// TODO (UG2): ingame settings menu
// TODO (UG): maybe rewrite the drag steering handler, DPad input for steering is currently not possible with the analog steering at the same time!

#include "stdafx.h"
#include "stdio.h"
#include <windows.h>
#include "includes\injector\injector.hpp"
#include "includes\IniReader.h"

#ifdef XINPUT_OLD
#include <XInput.h>
#pragma comment(lib,"xinput9_1_0.lib")
#else
#include <XInput.h>
#pragma comment(lib,"xinput.lib")
#endif

#ifdef GAME_UG
#include "NFSU_ConsoleButtonHashes.h"
#include "NFSU_EventNames.h"
#include "NFSU_XtendedInput_FEng.h"
#include "NFSU_Addresses.h"
#include <timeapi.h>
#endif

#ifdef GAME_UG2
#include "NFSU2_EventNames.h"
#include "NFSU2_Addresses.h"
#include "NFSU2_FEng.h"
#endif

#include "NFSU_XtendedInput_XInputConfig.h"
#include "NFSU_XtendedInput_VKHash.h"

#define MAX_CONTROLLERS 4  // XInput handles up to 4 controllers 

WORD INPUT_DEADZONE_LS = (0.24f * FLOAT(0x7FFF));  // Default to 24% of the +/- 32767 range.   This is a reasonable default value but can be altered if needed.
WORD INPUT_DEADZONE_RS = (0.24f * FLOAT(0x7FFF));  // Default to 24% of the +/- 32767 range.   This is a reasonable default value but can be altered if needed.
WORD INPUT_DEADZONE_LS_P2 = (0.24f * FLOAT(0x7FFF));  // Default to 24% of the +/- 32767 range.   This is a reasonable default value but can be altered if needed.
WORD INPUT_DEADZONE_RS_P2 = (0.24f * FLOAT(0x7FFF));  // Default to 24% of the +/- 32767 range.   This is a reasonable default value but can be altered if needed.
WORD SHIFT_ANALOG_THRESHOLD = (0.75f * FLOAT(0x7FFF));  // 75% for shifting
WORD FEUPDOWN_ANALOG_THRESHOLD = (0.50f * FLOAT(0x7FFF));  // 50% for analog sticks digital activation
WORD TRIGGER_ACTIVATION_THRESHOLD = (0.12f * FLOAT(0xFF));  // 12% for analog triggers digital activation

// for triggering the over-zelaous inputs once in a tick...
WORD bQuitButtonOldState = 0;
WORD bYButtonOldState = 0;
WORD bXButtonOldState = 0;

// steering axis & buttons
WORD SteerLeftButton = 0; // defaulting digital steer to 0 because of predefined functions
WORD SteerRightButton = 0;
WORD SteerLeftAxis = XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF;
WORD SteerRightAxis = XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF;

struct CONTROLLER_STATE
{
	XINPUT_STATE state;
	bool bConnected;
}g_Controllers[MAX_CONTROLLERS];

// KB Input declarations
//unbuffered -- calls GetAsyncKeyState during scanning process itself -- probably more taxing, but very good input latency (default)
#define KB_READINGMODE_UNBUFFERED_ASYNC 0
// buffered -- calls GetKeyboardState right after reading joypads, updates VKeyStates
#define KB_READINGMODE_BUFFERED 1
// unbuffered -- reads unbuffered raw input during WM_INPUT, updates VKeyStates, pretty slow but allows to differentiate between 2 different keyboard devices
#define KB_READINGMODE_UNBUFFERED_RAW 2
// TODO: buffered raw input

RAWINPUTDEVICE Rid;
BYTE VKeyStates[2][256];
bool KeyboardState = true; // connection status
bool bAllowTwoPlayerKB = false; // allow for 2 players to use their own KB devices (nothing to do with gamepads)
unsigned int KeyboardReadingMode = 0; // 0 = buffered synchronous, 1 = unbuffered asynchronous, 2 = unbuffered raw
HANDLE FirstKB = NULL;
HANDLE SecondKB = NULL;
USHORT SteerLeftVKey = VK_LEFT;
USHORT SteerRightVKey = VK_RIGHT;

// Input ScannerConfig - JoyEvent mapping
struct ScannerConfig
{
	unsigned int JoyConfigType = 0xFF; // 0xFF default
	unsigned int unk1; // not read
	unsigned int JoyEvent;
	unsigned int ScannerFunctionPointer;
	uint16_t BitmaskStuff; // used to define which bits are read from the joypad buffer
	uint16_t BitmaskStuff2; // used to define which bits are read from the joypad buffer
	unsigned int unk5; // if prev value was 0x10000 = button texture hash, if 0x10001 = max value, hard to determine
	unsigned int param;
	unsigned int keycode; // normally unknown, we're reutilizing this value
}ScannerConfigs[MAX_JOY_EVENT]; // we're generating scanners for ALL JoyEvents - not something devs normally do

// we use this to track the state of each button for each event
WORD EventStates[2][MAX_JOY_EVENT];
bool EventStatesKB[2][MAX_JOY_EVENT];

unsigned int GameWndProcAddr = 0;
LRESULT(WINAPI* GameWndProc)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI CustomWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INPUT:
		if (KeyboardReadingMode == KB_READINGMODE_UNBUFFERED_RAW)
		{
			UINT dwSize;

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
			LPBYTE lpb = new BYTE[dwSize];
			if (lpb == NULL)
			{
				KeyboardState = false;
				return 0;
			}

			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
			{
				KeyboardState = false;
				OutputDebugString(TEXT("GetRawInputData does not return correct size !\n"));
			}

			RAWINPUT* raw = (RAWINPUT*)lpb;

			if (raw->header.dwType == RIM_TYPEKEYBOARD)
			{
				KeyboardState = true;
				LastControlledDevice = LASTCONTROLLED_KB;

				// going with first-come first-serve -- first KB is always the first used one
				// this can catch a second virtual keyboard of a multi-device keyboard (the ones that use it for NKRO over USB), hence why it's optional
				if ((FirstKB != NULL) && (FirstKB != raw->header.hDevice) && (SecondKB == NULL) && bAllowTwoPlayerKB)
				{
					SecondKB = raw->header.hDevice;
					*(unsigned char*)JOYSTICKTYPE_P2_ADDR = 0;
				}

				if (FirstKB == NULL)
					FirstKB = raw->header.hDevice;

				//printf(" Kbd %p: make=%04x Flags:%04x Reserved:%04x ExtraInformation:%08x, msg=%04x VK=%04x \n",
				//	raw->header.hDevice,
				//raw->data.keyboard.MakeCode,
				//	raw->data.keyboard.Flags,
				//	raw->data.keyboard.Reserved,
				//	raw->data.keyboard.ExtraInformation,
				//	raw->data.keyboard.Message,
				//	raw->data.keyboard.VKey);

				if (raw->header.hDevice == SecondKB)
				{
					switch (raw->data.keyboard.VKey)
					{
					case VK_CONTROL:
						if (raw->data.keyboard.Flags & RI_KEY_E0)
							VKeyStates[1][VK_RCONTROL] = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
						else
							VKeyStates[1][VK_LCONTROL] = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
						break;
					case VK_MENU:
						if (raw->data.keyboard.Flags & RI_KEY_E0)
							VKeyStates[1][VK_RMENU] = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
						else
							VKeyStates[1][VK_LMENU] = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
						break;
					case VK_SHIFT:
						if (raw->data.keyboard.MakeCode == 0x36)
							VKeyStates[1][VK_RSHIFT] = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
						else
							VKeyStates[1][VK_LSHIFT] = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
						break;
					default:
						VKeyStates[1][raw->data.keyboard.VKey] = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
						break;
					}
					return 0;
				}
				else if (raw->header.hDevice)
				{
					switch (raw->data.keyboard.VKey)
					{
					case VK_CONTROL:
						if (raw->data.keyboard.Flags & RI_KEY_E0)
							VKeyStates[0][VK_RCONTROL] = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
						else
							VKeyStates[0][VK_LCONTROL] = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
						break;
					case VK_MENU:
						if (raw->data.keyboard.Flags & RI_KEY_E0)
							VKeyStates[0][VK_RMENU] = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
						else
							VKeyStates[0][VK_LMENU] = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
						break;
					case VK_SHIFT:
						if (raw->data.keyboard.MakeCode == 0x36)
							VKeyStates[0][VK_RSHIFT] = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
						else
							VKeyStates[0][VK_LSHIFT] = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
						break;
					default:
						VKeyStates[0][raw->data.keyboard.VKey] = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
						break;
					}
				}

				return 0;
			}

			delete[] lpb;
			return 0;
		}
		break;
	case WM_KEYDOWN:
		if (KeyboardReadingMode != KB_READINGMODE_UNBUFFERED_RAW)
			LastControlledDevice = LASTCONTROLLED_KB;
		break;
		// special cases for Win32 mouse on non-UG1
#ifndef GAME_UG
	case WM_LBUTTONDOWN:
		bMousePressedDown = true;
		return 0;
	case WM_LBUTTONUP:
		bMousePressedDown = false;
		return 0;
	case WM_RBUTTONDOWN:
		bMouse3PressedDown = true;
		return 0;
	case WM_RBUTTONUP:
		bMouse3PressedDown = false;
		return 0;
	case WM_MBUTTONDOWN:
		bMouse2PressedDown = true;
		return 0;
	case WM_MBUTTONUP:
		bMouse2PressedDown = false;
		return 0;
	case WM_SETFOCUS:
		// confine mouse within the game window
		if (bConfineMouse)
		{
			GetWindowRect(*(HWND*)GAME_HWND_ADDR, &windowRect);
			ClipCursor(&windowRect);
		}
		break;
#endif
	default: 
		break;
	}

	return GameWndProc(hWnd, msg, wParam, lParam);
}

void(*InitJoystick)() = (void(*)())INITJOY_ADDR;
void DummyFunc()
{
	return;
}

// UG1 uses fastcall and works without this so we won't apply it to UG1
#ifndef GAME_UG
ScannerConfig* FindScannerConfig_Custom(unsigned int inJoyEvent, int confignum, int unk)
{
	for (unsigned int i = 0; i < MAX_JOY_EVENT; i++)
	{
		if (ScannerConfigs[i].JoyEvent == inJoyEvent)
			return &(ScannerConfigs[i]);
	}

	return 0;
}
#endif

#ifdef GAME_UG
struct ActualJoypadData
{
	uint32_t destructor;
	char unkdata1[0x130];
	uint32_t type;
	char name[32];
	char unkdata2[0xE4];
	uint32_t config_index;
	uint32_t secondary;
	char unkdata3[0x2C];
}settings_controllers[3];

int settings_cave1_exit = 0x00413C1D;
int settings_cave1_exit_2 = 0x413AF1;
void __declspec(naked) settings_cave1()
{
	_asm
	{
		cmp edx, 2
		mov [esp+0x18], ebx
		mov [esp+0x1C], eax
		jnz settings_cont
		jmp settings_cave1_exit

		settings_cont:
		jmp settings_cave1_exit_2;
	}
}

uint32_t cur_setting_idx;
uint32_t cur_setting_idx_secondary;

// entry: 0x004141DF
int settings_cave2_exit = 0x00414217;
void __declspec(naked) settings_cave2()
{
	_asm
	{
		mov [esi+0x84], eax
		mov eax, ds:0x736344
		mov byte ptr [esi+0x80], 1
		mov cur_setting_idx, ecx
		mov cur_setting_idx_secondary, edx
		jmp settings_cave2_exit
	}
}

// entry: 0x004144EC
int settings_cave3_exit = 0x00414507;
void __declspec(naked) settings_cave3()
{
	_asm
	{
		//mov eax, ds:0x736344
		mov esi, cur_setting_idx
		jmp settings_cave3_exit
	}
}

#pragma runtime_checks( "", off )
#define JOYCONFIG_EMPTYSTR "Not bindable"
// joy config menu
// entrypoint: 0x00414830

void __stdcall SetConfigButtonText(int JoyMapIndex, int Secondary)
{
	unsigned int ObjHash = 0;
	_asm mov ObjHash, ecx

	char* FEngPkgName = "PC_JoyPad_Config.fng";
	if (*(int*)CONFIG_JOY_INDEX_ADDR == 0)
		FEngPkgName = "PC_Keyboard_Config.fng";

	if (*(int*)CONFIG_JOY_INDEX_ADDR == 0)
	{
		// Keyboard config

		if ((JoyMapIndex == 3) || (JoyMapIndex == 4))
		{
			if (JoyMapIndex == 3)
			{
				if (Secondary == 0)
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ControlsTextsPC[SteerLeftVKey]);
				else
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), JOYCONFIG_EMPTYSTR);
			}
			if (JoyMapIndex == 4)
			{
				if (Secondary == 0)
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ControlsTextsPC[SteerRightVKey]);
				else
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), JOYCONFIG_EMPTYSTR);
			}
		}
		else
		{
			if (Secondary == 0)
				FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ControlsTextsPC[ScannerConfigs[MapKeyboardConfigToEvent(JoyMapIndex)].keycode]);
			else
				FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), JOYCONFIG_EMPTYSTR);
		}
	}
	else
	{
		// JoyPad config
		if ((JoyMapIndex >= 1) && (JoyMapIndex <= 4))
		{
			if (JoyMapIndex == 1)
			{
				if (Secondary == 0)
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, ScannerConfigs[MapJoypadConfigToEvent(JoyMapIndex)].BitmaskStuff));
				else
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, ScannerConfigs[MapSecondaryJoypadConfigToEvent(JoyMapIndex)].BitmaskStuff));
			}
			if (JoyMapIndex == 2)
			{
				if (Secondary == 0)
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, ScannerConfigs[MapJoypadConfigToEvent(JoyMapIndex)].BitmaskStuff));
				else
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, ScannerConfigs[MapSecondaryJoypadConfigToEvent(JoyMapIndex)].BitmaskStuff));
			}
			if (JoyMapIndex == 3)
			{
				if (Secondary == 0)
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, SteerLeftAxis));
				else
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, SteerLeftButton));
			}
			if (JoyMapIndex == 4)
			{
				if (Secondary == 0)
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, SteerRightAxis));
				else
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, SteerRightButton));
			}
		}
		else
		{
			if (Secondary == 0)
				FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, ScannerConfigs[MapJoypadConfigToEvent(JoyMapIndex)].BitmaskStuff));
			else
				FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), JOYCONFIG_EMPTYSTR);
		}
	}
}
#pragma runtime_checks( "", restore )
#endif

int bStringHash(char* a1)
{
	char* v1; // edx@1
	char v2; // cl@1
	int result; // eax@1

	v1 = a1;
	v2 = *a1;
	for (result = -1; v2; ++v1)
	{
		result = v2 + 33 * result;
		v2 = v1[1];
	}
	return result;
}

bool bIsAnyAnalogActive()
{
	return (g_Controllers[0].state.Gamepad.sThumbLX || g_Controllers[0].state.Gamepad.sThumbLY || g_Controllers[0].state.Gamepad.sThumbRX || g_Controllers[0].state.Gamepad.sThumbRY || g_Controllers[0].state.Gamepad.bRightTrigger || g_Controllers[0].state.Gamepad.bLeftTrigger);
}

float GetAnalogStickValue(int index, WORD bind)
{
	float result = 0.0f;
	int rdi = index;
	switch (bind)
	{
	case XINPUT_GAMEPAD_LT_CONFIGDEF:
		result = g_Controllers[rdi].state.Gamepad.bLeftTrigger / 255.0f;
		break;
	case XINPUT_GAMEPAD_RT_CONFIGDEF:
		result = g_Controllers[rdi].state.Gamepad.bRightTrigger / 255.0f;
		break;
	case XINPUT_GAMEPAD_LS_X_CONFIGDEF:
		result = g_Controllers[rdi].state.Gamepad.sThumbLX / (float)(0x7FFF);
		break;
	case XINPUT_GAMEPAD_RS_X_CONFIGDEF:
		result = g_Controllers[rdi].state.Gamepad.sThumbRX / (float)(0x7FFF);
		break;
	case XINPUT_GAMEPAD_LS_Y_CONFIGDEF:
		result = g_Controllers[rdi].state.Gamepad.sThumbLY / (float)(0x7FFF);
		break;
	case XINPUT_GAMEPAD_RS_Y_CONFIGDEF:
		result = g_Controllers[rdi].state.Gamepad.sThumbRY / (float)(0x7FFF);
		break;
	case XINPUT_GAMEPAD_LS_UP_CONFIGDEF:
		if (g_Controllers[rdi].state.Gamepad.sThumbLY > 0)
			result = g_Controllers[rdi].state.Gamepad.sThumbLY / (float)(0x7FFF);
		break;
	case XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF:
		if (g_Controllers[rdi].state.Gamepad.sThumbLY < 0)
			result = -g_Controllers[rdi].state.Gamepad.sThumbLY / (float)(0x7FFF);
		break;
	case XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF:
		if (g_Controllers[rdi].state.Gamepad.sThumbLX < 0)
			result = -g_Controllers[rdi].state.Gamepad.sThumbLX / (float)(0x7FFF);
		break;
	case XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF:
		if (g_Controllers[rdi].state.Gamepad.sThumbLX > 0)
			result = g_Controllers[rdi].state.Gamepad.sThumbLX / (float)(0x7FFF);
		break;
	case XINPUT_GAMEPAD_RS_UP_CONFIGDEF:
		if (g_Controllers[rdi].state.Gamepad.sThumbRY > 0)
			result = g_Controllers[rdi].state.Gamepad.sThumbRY / (float)(0x7FFF);
		break;
	case XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF:
		if (g_Controllers[rdi].state.Gamepad.sThumbRY < 0)
			result = -g_Controllers[rdi].state.Gamepad.sThumbRY / (float)(0x7FFF);
		break;
	case XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF:
		if (g_Controllers[rdi].state.Gamepad.sThumbRX < 0)
			result = -g_Controllers[rdi].state.Gamepad.sThumbRX / (float)(0x7FFF);
		break;
	case XINPUT_GAMEPAD_RS_RIGHT_CONFIGDEF:
		if (g_Controllers[rdi].state.Gamepad.sThumbRX > 0)
			result = g_Controllers[rdi].state.Gamepad.sThumbRX / (float)(0x7FFF);
		break;
	default:
		break;
	}
	return result;
}

uint16_t GetAnalogActivity()
{
	if (GetAnalogStickValue(0, XINPUT_GAMEPAD_LT_CONFIGDEF) >= 1.0f)
		return XINPUT_GAMEPAD_LT_CONFIGDEF;
	if (GetAnalogStickValue(0, XINPUT_GAMEPAD_RT_CONFIGDEF) >= 1.0f)
		return XINPUT_GAMEPAD_RT_CONFIGDEF;
	if (GetAnalogStickValue(0, XINPUT_GAMEPAD_LS_UP_CONFIGDEF) >= 1.0f)
		return XINPUT_GAMEPAD_LS_UP_CONFIGDEF;
	if (GetAnalogStickValue(0, XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF) >= 1.0f)
		return XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF;
	if (GetAnalogStickValue(0, XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF) >= 1.0f)
		return XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF;
	if (GetAnalogStickValue(0, XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF) >= 1.0f)
		return XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF;
	if (GetAnalogStickValue(0, XINPUT_GAMEPAD_RS_UP_CONFIGDEF) >= 1.0f)
		return XINPUT_GAMEPAD_RS_UP_CONFIGDEF;
	if (GetAnalogStickValue(0, XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF) >= 1.0f)
		return XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF;
	if (GetAnalogStickValue(0, XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF) >= 1.0f)
		return XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF;
	if (GetAnalogStickValue(0, XINPUT_GAMEPAD_RS_RIGHT_CONFIGDEF) >= 1.0f)
		return XINPUT_GAMEPAD_RS_RIGHT_CONFIGDEF;
	return 0;
}



//////////////////////////////////////////////////////////////////
// Scanner functions
//////////////////////////////////////////////////////////////////
int Scanner_DigitalUpOrDown_XInput(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	int ci = 0;
	if ((int)Joystick == JOY2_ADDR)
		ci = 1;

	WORD wButtons = g_Controllers[ci].state.Gamepad.wButtons;

	//printf("Scanner: %x %x %x %x %x\n", EventNode, unk1, unk2, inScannerConfig, Joystick);

	// throttle/brake is very time sensitive, so we return all the time
	if ((inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG_ALTERNATE) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG_ALTERNATE))
	{
		// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
		if ((wButtons & inScannerConfig->BitmaskStuff))
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (wButtons & inScannerConfig->BitmaskStuff);
			return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
		}
		// on change to FALSE -- return JoyEvent number
		if (!(wButtons & inScannerConfig->BitmaskStuff))
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (wButtons & inScannerConfig->BitmaskStuff);
			return inScannerConfig->JoyEvent;
		}
	}

	// we're using BitmaskStuff to define which button is actually pressed
	if ((wButtons & inScannerConfig->BitmaskStuff) != EventStates[ci][inScannerConfig->JoyEvent])
	{
		// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
		if ((wButtons & inScannerConfig->BitmaskStuff))
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (wButtons & inScannerConfig->BitmaskStuff);
			return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
		}
		// on change to FALSE -- return JoyEvent number
		if (!(wButtons & inScannerConfig->BitmaskStuff))
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (wButtons & inScannerConfig->BitmaskStuff);
			return inScannerConfig->JoyEvent;
		}
	}

	// otherwise just keep returning zero, we're only returning changes
	return 0;
}

int Scanner_DigitalUpOrDown_RawKB(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	int ci = 0;
	if ((int)Joystick == JOY2_ADDR)
		ci = 1;

	// throttle is very time sensitive, so we return all the time
	if ((inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG_ALTERNATE) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG_ALTERNATE))
	{

		if (inScannerConfig->keycode && VKeyStates[ci][inScannerConfig->keycode])
		{
			EventStatesKB[ci][inScannerConfig->JoyEvent] = VKeyStates[ci][inScannerConfig->keycode];
			return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
		}

		if (inScannerConfig->keycode && !VKeyStates[ci][inScannerConfig->keycode])
		{
			EventStatesKB[ci][inScannerConfig->JoyEvent] = VKeyStates[ci][inScannerConfig->keycode];
			return inScannerConfig->JoyEvent;
		}
	}

	// we're using BitmaskStuff to define which button is actually pressed
	if (inScannerConfig->keycode && ((VKeyStates[ci][inScannerConfig->keycode]) != EventStatesKB[ci][inScannerConfig->JoyEvent]))
	{
		// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
		if (inScannerConfig->keycode && VKeyStates[ci][inScannerConfig->keycode])
		{
			EventStatesKB[ci][inScannerConfig->JoyEvent] = VKeyStates[ci][inScannerConfig->keycode];
			return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
		}
		// on change to FALSE -- return JoyEvent number
		if (inScannerConfig->keycode && !VKeyStates[ci][inScannerConfig->keycode])
		{
			EventStatesKB[ci][inScannerConfig->JoyEvent] = VKeyStates[ci][inScannerConfig->keycode];
			return inScannerConfig->JoyEvent;
		}
	}

	// otherwise just keep returning zero, we're only returning changes
	return 0;
}

int Scanner_DigitalUpOrDown_SyncKB(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	bool bKeyState = false;

	if (inScannerConfig->keycode)
		bKeyState = VKeyStates[0][inScannerConfig->keycode] >> 7;

	// throttle is very time sensitive, so we return all the time
	if ((inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG_ALTERNATE) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG_ALTERNATE))
	{
		if (bKeyState)
		{
			EventStatesKB[0][inScannerConfig->JoyEvent] = bKeyState;
			return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
		}

		if (!bKeyState)
		{
			EventStatesKB[0][inScannerConfig->JoyEvent] = bKeyState;
			return inScannerConfig->JoyEvent;
		}
	}

	// we're using BitmaskStuff to define which button is actually pressed
	if ((bKeyState) != EventStatesKB[0][inScannerConfig->JoyEvent])
	{
		// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
		if (bKeyState)
		{
			EventStatesKB[0][inScannerConfig->JoyEvent] = bKeyState;
			return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
		}
		// on change to FALSE -- return JoyEvent number
		if (!bKeyState)
		{
			EventStatesKB[0][inScannerConfig->JoyEvent] = bKeyState;
			return inScannerConfig->JoyEvent;
		}
	}

	// otherwise just keep returning zero, we're only returning changes
	return 0;
}

int Scanner_DigitalUpOrDown_AsyncKB(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	bool bKeyState = false;

	if (inScannerConfig->keycode)
		bKeyState = GetAsyncKeyState(inScannerConfig->keycode) >> 15;

	// throttle/brake is very time sensitive, so we return all the time
	if ((inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG_ALTERNATE) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG_ALTERNATE))
	{
		if (bKeyState)
		{
			EventStatesKB[0][inScannerConfig->JoyEvent] = bKeyState;
			return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
		}

		if (!bKeyState)
		{
			EventStatesKB[0][inScannerConfig->JoyEvent] = bKeyState;
			return inScannerConfig->JoyEvent;
		}
	}

	// we're using BitmaskStuff to define which button is actually pressed
	if (bKeyState != EventStatesKB[0][inScannerConfig->JoyEvent])
	{
		// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
		if (bKeyState)
		{
			EventStatesKB[0][inScannerConfig->JoyEvent] = bKeyState;
			return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
		}
		// on change to FALSE -- return JoyEvent number
		if (!bKeyState)
		{
			EventStatesKB[0][inScannerConfig->JoyEvent] = bKeyState;
			return inScannerConfig->JoyEvent;
		}
	}

	// otherwise just keep returning zero, we're only returning changes
	return 0;
}

int Scanner_DigitalUpOrDown_KB(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	switch (KeyboardReadingMode)
	{
	case KB_READINGMODE_UNBUFFERED_RAW:
		return Scanner_DigitalUpOrDown_RawKB(EventNode, unk1, unk2, inScannerConfig, Joystick);
	case KB_READINGMODE_BUFFERED:
		return Scanner_DigitalUpOrDown_SyncKB(EventNode, unk1, unk2, inScannerConfig, Joystick);
	case KB_READINGMODE_UNBUFFERED_ASYNC:
	default:
		return Scanner_DigitalUpOrDown_AsyncKB(EventNode, unk1, unk2, inScannerConfig, Joystick);
	}
}

int Scanner_DigitalUpOrDown(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	return Scanner_DigitalUpOrDown_XInput(EventNode, unk1, unk2, inScannerConfig, Joystick) | Scanner_DigitalUpOrDown_KB(EventNode, unk1, unk2, inScannerConfig, Joystick);
}

// same thing as DigitalUpOrDown except no KEY UP detection, only KEY DOWN
int Scanner_DigitalDown_XInput(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	int ci = 0;
	if ((int)Joystick == JOY2_ADDR)
		ci = 1;

	WORD wButtons = g_Controllers[ci].state.Gamepad.wButtons;

	// we're using BitmaskStuff to define which button is actually pressed
	if ((wButtons & inScannerConfig->BitmaskStuff) != EventStates[ci][inScannerConfig->JoyEvent])
	{
		EventStates[ci][inScannerConfig->JoyEvent] = (wButtons & inScannerConfig->BitmaskStuff);
		// on change to TRUE -- return event
		if ((wButtons & inScannerConfig->BitmaskStuff))
		{
			return inScannerConfig->JoyEvent;
		}
	}

	// otherwise just keep returning zero, we're only returning changes
	return 0;
}

// same thing as DigitalUpOrDown except no KEY UP detection, only KEY DOWN
int Scanner_DigitalDown_RawKB(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	int ci = 0;
	if ((int)Joystick == JOY2_ADDR)
		ci = 1;

	// we're using BitmaskStuff to define which button is actually pressed
	if (inScannerConfig->keycode && ((VKeyStates[ci][inScannerConfig->keycode]) != EventStatesKB[ci][inScannerConfig->JoyEvent]))
	{
		EventStatesKB[ci][inScannerConfig->JoyEvent] = (VKeyStates[ci][inScannerConfig->keycode]);
		// on change to TRUE -- return event
		if ((VKeyStates[ci][inScannerConfig->keycode]))
		{
			return inScannerConfig->JoyEvent;
		}
	}

	// otherwise just keep returning zero, we're only returning changes
	return 0;
}

int Scanner_DigitalDown_SyncKB(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	bool bKeyState = false;

	if (inScannerConfig->keycode)
		bKeyState = VKeyStates[0][inScannerConfig->keycode] >> 7;

	// we're using BitmaskStuff to define which button is actually pressed
	if (bKeyState != EventStatesKB[0][inScannerConfig->JoyEvent])
	{
		EventStatesKB[0][inScannerConfig->JoyEvent] = bKeyState;
		// on change to TRUE -- return event
		if (bKeyState)
		{
			return inScannerConfig->JoyEvent;
		}
	}

	// otherwise just keep returning zero, we're only returning changes
	return 0;
}

int Scanner_DigitalDown_AsyncKB(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	bool bKeyState = false;

	if (inScannerConfig->keycode)
		bKeyState = GetAsyncKeyState(inScannerConfig->keycode) >> 15;

	// we're using BitmaskStuff to define which button is actually pressed
	if (bKeyState != EventStatesKB[0][inScannerConfig->JoyEvent])
	{
		EventStatesKB[0][inScannerConfig->JoyEvent] = bKeyState;
		// on change to TRUE -- return event
		if (bKeyState)
		{
			return inScannerConfig->JoyEvent;
		}
	}

	// otherwise just keep returning zero, we're only returning changes
	return 0;
}

int Scanner_DigitalDown_KB(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	switch (KeyboardReadingMode)
	{
	case KB_READINGMODE_UNBUFFERED_RAW:
		return Scanner_DigitalDown_RawKB(EventNode, unk1, unk2, inScannerConfig, Joystick);
	case KB_READINGMODE_BUFFERED:
		return Scanner_DigitalDown_SyncKB(EventNode, unk1, unk2, inScannerConfig, Joystick);
	case KB_READINGMODE_UNBUFFERED_ASYNC:
	default:
		return Scanner_DigitalDown_AsyncKB(EventNode, unk1, unk2, inScannerConfig, Joystick);
	}
}

int Scanner_DigitalDown(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	return Scanner_DigitalDown_XInput(EventNode, unk1, unk2, inScannerConfig, Joystick) | Scanner_DigitalDown_KB(EventNode, unk1, unk2, inScannerConfig, Joystick);
}

// when an analog axis is assigned to a digital-only function
int Scanner_DigitalAnalog_XInput(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	int ci = 0;
	if ((int)Joystick == JOY2_ADDR)
		ci = 1;

	WORD threshold = FEUPDOWN_ANALOG_THRESHOLD;
	if (inScannerConfig->JoyEvent == JOY_EVENT_SHIFTUP || inScannerConfig->JoyEvent == JOY_EVENT_SHIFTDOWN || inScannerConfig->JoyEvent == JOY_EVENT_SHIFTUP_ALTERNATE || inScannerConfig->JoyEvent == JOY_EVENT_SHIFTDOWN_ALTERNATE)
		threshold = SHIFT_ANALOG_THRESHOLD;

	// we're using BitmaskStuff to define which button is actually pressed
	switch (inScannerConfig->BitmaskStuff)
	{
	case XINPUT_GAMEPAD_LT_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.bLeftTrigger > TRIGGER_ACTIVATION_THRESHOLD) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.bLeftTrigger > TRIGGER_ACTIVATION_THRESHOLD)
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.bLeftTrigger > TRIGGER_ACTIVATION_THRESHOLD);
				return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
			}
			// on change to FALSE -- return JoyEvent number
			if (!(g_Controllers[ci].state.Gamepad.bLeftTrigger > TRIGGER_ACTIVATION_THRESHOLD))
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.bLeftTrigger > TRIGGER_ACTIVATION_THRESHOLD);
				return inScannerConfig->JoyEvent;
			}
			return 0;
		}
		break;
	case XINPUT_GAMEPAD_RT_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.bRightTrigger > TRIGGER_ACTIVATION_THRESHOLD) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.bRightTrigger > TRIGGER_ACTIVATION_THRESHOLD)
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.bRightTrigger > TRIGGER_ACTIVATION_THRESHOLD);
				return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
			}
			// on change to FALSE -- return JoyEvent number
			if (!(g_Controllers[ci].state.Gamepad.bRightTrigger > TRIGGER_ACTIVATION_THRESHOLD))
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.bRightTrigger > TRIGGER_ACTIVATION_THRESHOLD);
				return inScannerConfig->JoyEvent;
			}
			return 0;
		}
		break;
	case XINPUT_GAMEPAD_LS_UP_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbLY > threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbLY > threshold)
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbLY > threshold);
				return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
			}
			// on change to FALSE -- return JoyEvent number
			if (!(g_Controllers[ci].state.Gamepad.sThumbLY > threshold))
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbLY > threshold);
				return inScannerConfig->JoyEvent;
			}
			return 0;
		}
		break;
	case XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbLY < -threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbLY < -threshold)
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbLY < -threshold);
				return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
			}
			// on change to FALSE -- return JoyEvent number
			if (!(g_Controllers[ci].state.Gamepad.sThumbLY < -threshold))
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbLY < -threshold);
				return inScannerConfig->JoyEvent;
			}
			return 0;
		}
		break;
	case XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbLX < -threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbLX < -threshold)
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbLX < -threshold);
				return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
			}
			// on change to FALSE -- return JoyEvent number
			if (!(g_Controllers[ci].state.Gamepad.sThumbLX < -threshold))
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbLX < -threshold);
				return inScannerConfig->JoyEvent;
			}
			return 0;
		}
		break;
	case XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbLX > threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbLX > threshold)
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbLX > threshold);
				return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
			}
			// on change to FALSE -- return JoyEvent number
			if (!(g_Controllers[ci].state.Gamepad.sThumbLX > threshold))
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbLX > threshold);
				return inScannerConfig->JoyEvent;
			}
			return 0;
		}
		break;
	case XINPUT_GAMEPAD_RS_UP_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbRY > threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbRY > threshold)
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbRY > threshold);
				return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
			}
			// on change to FALSE -- return JoyEvent number
			if (!(g_Controllers[ci].state.Gamepad.sThumbRY > threshold))
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbRY > threshold);
				return inScannerConfig->JoyEvent;
			}
			return 0;
		}
		break;
	case XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbRY < -threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbRY < -threshold)
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbRY < -threshold);
				return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
			}
			// on change to FALSE -- return JoyEvent number
			if (!(g_Controllers[ci].state.Gamepad.sThumbRY < -threshold))
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbRY < -threshold);
				return inScannerConfig->JoyEvent;
			}
			return 0;
		}
		break;
	case XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbRX < -threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbRX < -threshold)
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbRX < -threshold);
				return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
			}
			// on change to FALSE -- return JoyEvent number
			if (!(g_Controllers[ci].state.Gamepad.sThumbRX < -threshold))
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbRX < -threshold);
				return inScannerConfig->JoyEvent;
			}
			return 0;
		}
		break;
	case XINPUT_GAMEPAD_RS_RIGHT_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbRX > threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbRX > threshold)
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbRX > threshold);
				return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
			}
			// on change to FALSE -- return JoyEvent number
			if (!(g_Controllers[ci].state.Gamepad.sThumbRX > threshold))
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbRX > threshold);
				return inScannerConfig->JoyEvent;
			}
			return 0;
		}
		break;
	}
	// otherwise just keep returning zero, we're only returning changes
	return 0;
}

int Scanner_DigitalDownAnalog_XInput(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	int ci = 0;
	if ((int)Joystick == JOY2_ADDR)
		ci = 1;

	WORD threshold = FEUPDOWN_ANALOG_THRESHOLD;
	if (inScannerConfig->JoyEvent == JOY_EVENT_SHIFTUP || inScannerConfig->JoyEvent == JOY_EVENT_SHIFTDOWN || inScannerConfig->JoyEvent == JOY_EVENT_SHIFTUP_ALTERNATE || inScannerConfig->JoyEvent == JOY_EVENT_SHIFTDOWN_ALTERNATE)
		threshold = SHIFT_ANALOG_THRESHOLD;

	// we're using BitmaskStuff to define which button is actually pressed
	switch (inScannerConfig->BitmaskStuff)
	{
	case XINPUT_GAMEPAD_LT_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.bLeftTrigger > TRIGGER_ACTIVATION_THRESHOLD) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.bLeftTrigger > TRIGGER_ACTIVATION_THRESHOLD)
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.bLeftTrigger > TRIGGER_ACTIVATION_THRESHOLD);
				return inScannerConfig->JoyEvent;
			}
		}
		break;
	case XINPUT_GAMEPAD_RT_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.bRightTrigger > TRIGGER_ACTIVATION_THRESHOLD) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.bRightTrigger > TRIGGER_ACTIVATION_THRESHOLD);
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.bRightTrigger > TRIGGER_ACTIVATION_THRESHOLD)
			{
				return inScannerConfig->JoyEvent;
			}
		}
		break;
	case XINPUT_GAMEPAD_LS_UP_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbLY > threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbLY > threshold);
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbLY > threshold)
			{
				return inScannerConfig->JoyEvent;
			}
		}
		break;
	case XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbLY < -threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbLY < -threshold);
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbLY < -threshold)
			{
				return inScannerConfig->JoyEvent;
			}
		}
		break;
	case XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbLX < -threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbLX < -threshold);
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbLX < -threshold)
			{
				return inScannerConfig->JoyEvent;
			}
		}
		break;
	case XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbLX > threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbLX > threshold);
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbLX > threshold)
			{
				return inScannerConfig->JoyEvent;
			}
		}
		break;
	case XINPUT_GAMEPAD_RS_UP_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbRY > threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbRY > threshold);
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbRY > threshold)
			{
				return inScannerConfig->JoyEvent;
			}
		}
		break;
	case XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbRY < -threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbRY < -threshold);
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbRY < -threshold)
			{
				return inScannerConfig->JoyEvent;
			}
		}
		break;
	case XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbRX < -threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbRX < -threshold);
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbRX < -threshold)
			{
				return inScannerConfig->JoyEvent;
			}
		}
		break;
	case XINPUT_GAMEPAD_RS_RIGHT_CONFIGDEF:
		if ((g_Controllers[ci].state.Gamepad.sThumbRX > threshold) != EventStates[ci][inScannerConfig->JoyEvent])
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (g_Controllers[ci].state.Gamepad.sThumbRX > threshold);
			// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
			if (g_Controllers[ci].state.Gamepad.sThumbRX > threshold)
			{
				return inScannerConfig->JoyEvent;
			}
		}
		break;
	}
	// otherwise just keep returning zero, we're only returning changes
	return 0;
}


int Scanner_DigitalAnalog(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	return Scanner_DigitalAnalog_XInput(EventNode, unk1, unk2, inScannerConfig, Joystick) | Scanner_DigitalUpOrDown_KB(EventNode, unk1, unk2, inScannerConfig, Joystick);
}

int Scanner_DigitalDownAnalog(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	return Scanner_DigitalDownAnalog_XInput(EventNode, unk1, unk2, inScannerConfig, Joystick) | Scanner_DigitalDown_KB(EventNode, unk1, unk2, inScannerConfig, Joystick);
}

int Scanner_DigitalAnyButton(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	int ci = 0;
	if ((int)Joystick == JOY2_ADDR)
		ci = 1;

	WORD wButtons = g_Controllers[ci].state.Gamepad.wButtons;

	// we're using BitmaskStuff to define which button is actually pressed
	if ((wButtons) != EventStates[ci][inScannerConfig->JoyEvent])
	{
		// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
		if ((wButtons))
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (wButtons);
			return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
		}
		// on change to FALSE -- return JoyEvent number
		if (!(wButtons & inScannerConfig->BitmaskStuff))
		{
			EventStates[ci][inScannerConfig->JoyEvent] = (wButtons);
			return inScannerConfig->JoyEvent;
		}
	}
	
	// otherwise just keep returning zero, we're only returning changes
	return 0;

}

int Scanner_Analog(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	int ci = 0;
	if ((int)Joystick == JOY2_ADDR)
		ci = 1;

	BYTE axis = 0;

	switch (inScannerConfig->BitmaskStuff)
	{
	case XINPUT_GAMEPAD_LT_CONFIGDEF:
		axis = g_Controllers[ci].state.Gamepad.bLeftTrigger;
		break;
	case XINPUT_GAMEPAD_RT_CONFIGDEF:
		axis = g_Controllers[ci].state.Gamepad.bRightTrigger;
		break;
	case XINPUT_GAMEPAD_LS_X_CONFIGDEF:
		axis = (char)(((float)(g_Controllers[ci].state.Gamepad.sThumbLX) / (float)(0x7FFF)) * (float)(0x7F)) + 0x80;
		break;
	case XINPUT_GAMEPAD_RS_X_CONFIGDEF:
		axis = (char)(((float)(g_Controllers[ci].state.Gamepad.sThumbRX) / (float)(0x7FFF)) * (float)(0x7F)) + 0x80;
		break;
	case XINPUT_GAMEPAD_LS_Y_CONFIGDEF:
		axis = (char)(((float)(g_Controllers[ci].state.Gamepad.sThumbLY) / (float)(0x7FFF)) * (float)(0x7F)) + 0x80;
		break;
	case XINPUT_GAMEPAD_RS_Y_CONFIGDEF:
		axis = (char)(((float)(g_Controllers[ci].state.Gamepad.sThumbRY) / (float)(0x7FFF)) * (float)(0x7F)) + 0x80;
		break;
	default:
		break;
	}

	// invert some broken axis
	if (inScannerConfig->JoyEvent == JOY_EVENT_DEBUG_CAMERA_INOUT)
		axis = -axis;
	if (inScannerConfig->JoyEvent == JOY_EVENT_CARSEL_ORBIT_UPDOWN)
		axis = -axis;
#ifdef GAME_UG2
	if (inScannerConfig->JoyEvent == JOY_EVENT_HYDRAULIC_PRESSURIZE_ANALOG_UD)
		axis = -axis;
	if (inScannerConfig->JoyEvent == JOY_EVENT_HYDRAULIC_BOUNCE_ANALOG_UD)
		axis = -axis;
#endif

	//printf("%s 0x%hX\n", JoyEventNames[inScannerConfig->JoyEvent], inScannerConfig->JoyEvent + (axis << 8));

	if ((axis != EventStates[ci][inScannerConfig->JoyEvent]) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG_ALTERNATE) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG_ALTERNATE))
	{
		EventStates[ci][inScannerConfig->JoyEvent] = axis;
		return inScannerConfig->JoyEvent + (axis << 8);
	}

	return 0;
}

int Scanner_Analog_DragSteer(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	int ci = 0;
	if ((int)Joystick == JOY2_ADDR)
		ci = 1;

	BYTE axis = 0;
	char signed_axis = 0;

	switch (inScannerConfig->BitmaskStuff)
	{
	case XINPUT_GAMEPAD_LS_X_CONFIGDEF:
		axis = (BYTE)(((float)(g_Controllers[ci].state.Gamepad.sThumbLX) / (float)(0x7FFF)) * (float)(0x7F));
		break;
	case XINPUT_GAMEPAD_RS_X_CONFIGDEF:
		axis = (BYTE)(((float)(g_Controllers[ci].state.Gamepad.sThumbRX) / (float)(0x7FFF)) * (float)(0x7F));
		break;
	case XINPUT_GAMEPAD_LS_Y_CONFIGDEF:
		axis = (BYTE)(((float)(g_Controllers[ci].state.Gamepad.sThumbLY) / (float)(0x7FFF)) * (float)(0x7F));
		break;
	case XINPUT_GAMEPAD_RS_Y_CONFIGDEF:
		axis = (BYTE)(((float)(g_Controllers[ci].state.Gamepad.sThumbRY) / (float)(0x7FFF)) * (float)(0x7F));
		break;
	default:
		return 0;
	}
	signed_axis = axis;

	if (axis != EventStates[ci][inScannerConfig->JoyEvent])
	{
		EventStates[ci][inScannerConfig->JoyEvent] = axis;
		if ((inScannerConfig->JoyEvent == JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT_ANALOG) && signed_axis < 0)
			return inScannerConfig->JoyEvent + (axis << 8);
		if ((inScannerConfig->JoyEvent == JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT_ANALOG) && signed_axis > 0)
			return inScannerConfig->JoyEvent + (axis << 8);
	}
	return 0;
}

// STEERING HANDLER

void RealDriver_JoyHandler_Steer(int JoystickPort, int JoyEvent, char Value, void* RealDriver)
{
	float* SteerVal = (float*)((int)RealDriver + REALDRIVER_STEER_OFFSET);
	float AnalogSteerValue = GetAnalogStickValue(JoystickPort, SteerLeftAxis) - GetAnalogStickValue(JoystickPort, SteerRightAxis);
	float DigitalSteerValue = 0.0f;
	float DigitalSteerValueKB = 0.0f;
	float FinalSteerValue;

	// get gamepad digital steering
	WORD wButtons = g_Controllers[JoystickPort].state.Gamepad.wButtons;
	if (wButtons & SteerLeftButton)
	{
		DigitalSteerValue = DigitalSteerValue + 1.0;
	}
	if (wButtons & SteerRightButton)
	{
		DigitalSteerValue = DigitalSteerValue - 1.0;
	}

	// get KB digital steering
	switch (KeyboardReadingMode)
	{
	case KB_READINGMODE_UNBUFFERED_RAW:
		if (SteerLeftVKey && VKeyStates[JoystickPort][SteerLeftVKey])
			DigitalSteerValueKB = DigitalSteerValueKB + 1.0;
		if (SteerRightVKey && VKeyStates[JoystickPort][SteerRightVKey])
			DigitalSteerValueKB = DigitalSteerValueKB - 1.0;
		break;
	case KB_READINGMODE_BUFFERED:
		if (SteerLeftVKey && VKeyStates[0][SteerLeftVKey] >> 7)
			DigitalSteerValueKB = DigitalSteerValueKB + 1.0;
		if (SteerRightVKey && VKeyStates[0][SteerRightVKey] >> 7)
			DigitalSteerValueKB = DigitalSteerValueKB - 1.0;
		break;
	case KB_READINGMODE_UNBUFFERED_ASYNC:
	default:
		if (SteerLeftVKey && GetAsyncKeyState(SteerLeftVKey) >> 15)
			DigitalSteerValueKB = DigitalSteerValueKB + 1.0;
		if (SteerRightVKey && GetAsyncKeyState(SteerRightVKey) >> 15)
			DigitalSteerValueKB = DigitalSteerValueKB - 1.0;
		break;
	}

	FinalSteerValue = AnalogSteerValue + DigitalSteerValue + DigitalSteerValueKB;

	// cap the min/max
	if (FinalSteerValue > 1.0)
		FinalSteerValue = 1.0;
	if (FinalSteerValue < -1.0)
		FinalSteerValue = -1.0;

	*SteerVal = FinalSteerValue;
}

int Scanner_CombinedSteering(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	// always return a value to trigger polling
	return inScannerConfig->JoyEvent + (1 << 8);
}

int Scanner_TypeChanged(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	return 0;
}

void SetupScannerConfig()
{
	CIniReader inireader("");

	unsigned int inXInputConfigDef = 0;

	for (unsigned int i = 0; i < MAX_JOY_EVENT; i++)
	{
		ScannerConfigs[i].JoyEvent = i;
		ScannerConfigs[i].param = 0xFF;
		// keyboard VK codes
		ScannerConfigs[i].keycode = ConvertVKNameToValue(inireader.ReadString("EventsKB", JoyEventNames[i], ""));
		if (ScannerConfigs[i].keycode == 0)
		{
			// try checking for single-char
			char lettercheck[32];
			strcpy(lettercheck, inireader.ReadString("EventsKB", JoyEventNames[i], ""));
			if (strlen(lettercheck) == 1)
				ScannerConfigs[i].keycode = toupper(lettercheck[0]);
		}

		inXInputConfigDef = ConvertXInputOtherConfigDef(inireader.ReadString("Events", JoyEventNames[i], ""));
		if (!inXInputConfigDef)
		{
			if (bIsEventDigitalDownOnly(i))
				ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalDown;
			else
				ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalUpOrDown;

			ScannerConfigs[i].BitmaskStuff = ConvertXInputNameToBitmask(inireader.ReadString("Events", JoyEventNames[i], ""));
		}
		else
		{
			// triggers or sticks or dpad
			ScannerConfigs[i].BitmaskStuff = inXInputConfigDef;
			//ScannerConfigs[i].BitmaskStuff2 = inXInputConfigDef;

			if (bIsEventAnalog(i))
				ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_Analog;
			else if ((inXInputConfigDef != XINPUT_GAMEPAD_DPAD_CONFIGDEF))
			{
				if (bIsEventDigitalDownOnly(i))
					ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalDownAnalog;
				else
					ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalAnalog;
			}
			// this is horribly broken, drag analog steering goes across all lanes...
			if ((i == JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT_ANALOG) || (i == JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT_ANALOG))
				ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_Analog_DragSteer;
		}

		// JOY_EVENT_STEER
		if ((i == JOY_EVENT_STEER) || (i == JOY_EVENT_STEER_ANALOG))
		{
			ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_CombinedSteering;
		}
	}

	ScannerConfigs[JOY_EVENT_TYPE_CHANGED].ScannerFunctionPointer = (unsigned int)&Scanner_TypeChanged;
#ifdef GAME_UG2
	ScannerConfigs[JOY_EVENT_LIVE_TYPE_CHANGED].ScannerFunctionPointer = (unsigned int)&Scanner_TypeChanged;
#endif
	//ScannerConfigs[JOY_EVENT_ANY].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalAnyButton;

	// read steering axis & buttons
	SteerLeftButton = ConvertXInputNameToBitmask(inireader.ReadString("Events", "SteerLeftButton", ""));
	SteerRightButton = ConvertXInputNameToBitmask(inireader.ReadString("Events", "SteerRightButton", ""));
	SteerLeftAxis = ConvertXInputOtherConfigDef(inireader.ReadString("Events", "SteerLeftAxis", "XINPUT_GAMEPAD_LS_LEFT"));
	SteerRightAxis = ConvertXInputOtherConfigDef(inireader.ReadString("Events", "SteerRightAxis", "XINPUT_GAMEPAD_LS_RIGHT"));

	// read steering VK codes
	SteerLeftVKey = ConvertVKNameToValue(inireader.ReadString("EventsKB", "KeyboardSteerLeft", "VK_LEFT"));
	if (SteerLeftVKey == 0)
	{
		// try checking for single-char
		char lettercheck[32];
		strcpy(lettercheck, inireader.ReadString("EventsKB", "KeyboardSteerLeft", "VK_LEFT"));
			if (lettercheck[1] == '\0')
				SteerLeftVKey = toupper(lettercheck[0]);
	}
	SteerRightVKey = ConvertVKNameToValue(inireader.ReadString("EventsKB", "KeyboardSteerRight", "VK_RIGHT"));
	if (SteerRightVKey == 0)
	{
		// try checking for single-char
		char lettercheck[32];
		strcpy(lettercheck, inireader.ReadString("EventsKB", "KeyboardSteerRight", "VK_RIGHT"));
		if (lettercheck[1] == '\0')
			SteerRightVKey = toupper(lettercheck[0]);
	}
}

//////////////////////////////////////////////////////////////////
// Scanner functions END
//////////////////////////////////////////////////////////////////

void InitCustomKBInput()
{
	InitJoystick();
	*(int*)DEVICE_COUNT_ADDR = 2; // without this, game doesn't want to read anything

	if (KeyboardReadingMode == KB_READINGMODE_UNBUFFERED_RAW)
	{
		Rid.usUsagePage = 0x01;          // HID_USAGE_PAGE_GENERIC
		Rid.usUsage = 0x06;              // HID_USAGE_GENERIC_KEYBOARD
		Rid.dwFlags = 0;    // adds keyboard and also ignores legacy keyboard messages
		Rid.hwndTarget = *(HWND*)GAME_HWND_ADDR;

		RegisterRawInputDevices(&Rid, 1, sizeof(Rid));
	}
}

HRESULT UpdateControllerState()
{
	DWORD dwResult;

	dwResult = XInputGetState(0, &g_Controllers[0].state);

	if (dwResult == ERROR_SUCCESS)
	{
		g_Controllers[0].bConnected = true;
		*(int*)DEVICE_COUNT_ADDR = 2;

		// Zero value if thumbsticks are within the dead zone 
		if ((g_Controllers[0].state.Gamepad.sThumbLX < INPUT_DEADZONE_LS &&
			g_Controllers[0].state.Gamepad.sThumbLX > -INPUT_DEADZONE_LS) &&
			(g_Controllers[0].state.Gamepad.sThumbLY < INPUT_DEADZONE_LS &&
				g_Controllers[0].state.Gamepad.sThumbLY > -INPUT_DEADZONE_LS))
		{
			g_Controllers[0].state.Gamepad.sThumbLX = 0;
			g_Controllers[0].state.Gamepad.sThumbLY = 0;
		}

		if ((g_Controllers[0].state.Gamepad.sThumbRX < INPUT_DEADZONE_RS &&
			g_Controllers[0].state.Gamepad.sThumbRX > -INPUT_DEADZONE_RS) &&
			(g_Controllers[0].state.Gamepad.sThumbRY < INPUT_DEADZONE_RS &&
				g_Controllers[0].state.Gamepad.sThumbRY > -INPUT_DEADZONE_RS))
		{
			g_Controllers[0].state.Gamepad.sThumbRX = 0;
			g_Controllers[0].state.Gamepad.sThumbRY = 0;
		}

		if (g_Controllers[0].state.Gamepad.wButtons || g_Controllers[0].state.Gamepad.sThumbLX || g_Controllers[0].state.Gamepad.sThumbLY || g_Controllers[0].state.Gamepad.sThumbRX || g_Controllers[0].state.Gamepad.sThumbRY || g_Controllers[0].state.Gamepad.bRightTrigger || g_Controllers[0].state.Gamepad.bLeftTrigger)
			LastControlledDevice = LASTCONTROLLED_CONTROLLER;

	}
	else
	{
		g_Controllers[0].bConnected = false;
		*(int*)DEVICE_COUNT_ADDR = 1;
	}

	dwResult = XInputGetState(1, &g_Controllers[1].state);

	if (dwResult == ERROR_SUCCESS)
	{
		g_Controllers[1].bConnected = true;
		* (unsigned char*)JOYSTICKTYPE_P2_ADDR = 0;
		// Zero value if thumbsticks are within the dead zone 
		if ((g_Controllers[1].state.Gamepad.sThumbLX < INPUT_DEADZONE_LS_P2 &&
			g_Controllers[1].state.Gamepad.sThumbLX > -INPUT_DEADZONE_LS_P2) &&
			(g_Controllers[1].state.Gamepad.sThumbLY < INPUT_DEADZONE_LS_P2 &&
				g_Controllers[1].state.Gamepad.sThumbLY > -INPUT_DEADZONE_LS_P2))
		{
			g_Controllers[1].state.Gamepad.sThumbLX = 0;
			g_Controllers[1].state.Gamepad.sThumbLY = 0;
		}

		if ((g_Controllers[1].state.Gamepad.sThumbRX < INPUT_DEADZONE_RS_P2 &&
			g_Controllers[1].state.Gamepad.sThumbRX > -INPUT_DEADZONE_RS_P2) &&
			(g_Controllers[1].state.Gamepad.sThumbRY < INPUT_DEADZONE_RS_P2 &&
				g_Controllers[1].state.Gamepad.sThumbRY > -INPUT_DEADZONE_RS_P2))
		{
			g_Controllers[1].state.Gamepad.sThumbRX = 0;
			g_Controllers[1].state.Gamepad.sThumbRY = 0;
		}
	}
	else
	{
		g_Controllers[1].bConnected = false;
		if (SecondKB == NULL)
			*(unsigned char*)JOYSTICKTYPE_P2_ADDR = 0xFF;
	}

	// check controller & KB state for P1
	if (KeyboardState || g_Controllers[0].bConnected)
		*(unsigned char*)JOYSTICKTYPE_P1_ADDR = 1;
	else
		*(unsigned char*)JOYSTICKTYPE_P1_ADDR = 0xFF;
	

	return S_OK;
}

// old function -- repurposed to send keyboard-exclusive commands to the game
void ReadXInput_Extra()
{
	if (g_Controllers[0].bConnected)
	{
		WORD wButtons = g_Controllers[0].state.Gamepad.wButtons;
#ifdef GAME_UG
		if (*(int*)GAMEFLOWMANAGER_STATUS_ADDR == 3)
		{

			if ((wButtons & XINPUT_GAMEPAD_Y))
				*(char*)FE_WINDOWS_KEY_CODE_ADDR = 'D'; // delete profile

			if ((g_Controllers[0].state.Gamepad.bLeftTrigger > TRIGGER_ACTIVATION_THRESHOLD))
				*(char*)FE_WINDOWS_KEY_CODE_ADDR = 'T'; // tutorial
		}
		if ((wButtons & XINPUT_GAMEPAD_BACK) != bQuitButtonOldState)
		{
			if ((wButtons & XINPUT_GAMEPAD_BACK)) // trigger once only on button down state
				*(char*)FE_WINDOWS_KEY_CODE_ADDR = 'Q';
			bQuitButtonOldState = (wButtons & XINPUT_GAMEPAD_BACK);
		}
#endif
#ifdef GAME_UG2
		if ((wButtons & XINPUT_GAMEPAD_BACK) != bQuitButtonOldState)
		{
			if ((wButtons & XINPUT_GAMEPAD_BACK)) // trigger once only on button down state
				FESendKeystroke('Q');
				//*(char*)FE_WINDOWS_KEY_CODE_ADDR = 'Q';
			bQuitButtonOldState = (wButtons & XINPUT_GAMEPAD_BACK);
		}
#endif


	}
}

#ifdef GAME_UG
#define CONFIG_BINDING_DELAY 500
#define UNBINDER_KEY VK_F5

bool bIgnorableVKey(int k)
{
	switch (k)
	{
	case VK_MENU:
	case VK_CONTROL:
	case VK_SHIFT:
		return true;
	default:
		return false;
	}

	return false;
}

int KB_GetCurrentPressedKey()
{
	switch (KeyboardReadingMode)
	{
	case KB_READINGMODE_UNBUFFERED_RAW:
	case KB_READINGMODE_BUFFERED:

		for (int i = 0; i < 255; i++)
		{
			if (i && VKeyStates[0][i] >> 7)
				if (!bIgnorableVKey(i)) // TODO: separate Left Control and Right Alt
					return i;
		}
		return -1;
	case KB_READINGMODE_UNBUFFERED_ASYNC:
	default:

		for (int i = 0; i < 255; i++)
		{
			if (i && GetAsyncKeyState(i) >> 15)
				if (!bIgnorableVKey(i))
					return i;
		}
		return -1;
	}
}

void SaveBindingToIni(int JoyEvent, uint16_t bind)
{
	CIniReader inireader("");
	inireader.WriteString("Events", JoyEventNames[JoyEvent], ConvertXInputBitmaskToName(bind));
}

void SaveSteerBindingToIni(char* name, uint16_t bind)
{
	CIniReader inireader("");
	inireader.WriteString("Events", name, ConvertXInputBitmaskToName(bind));
}

void SaveBindingToIniKB(int JoyEvent, int bind)
{
	CIniReader inireader("");
	inireader.WriteString("EventsKB", JoyEventNames[JoyEvent], VKeyStrings[bind]);
}

void SaveSteerBindingToIniKB(char* name, int bind)
{
	CIniReader inireader("");
	inireader.WriteString("EventsKB", name, VKeyStrings[bind]);
}

uint32_t BindAcceptTimeBase;
uint32_t BindAllowTime;
uint32_t OldSettingIdx;
void HandleInGameConfigMenu()
{
	if (OldSettingIdx != cur_setting_idx)
	{
		BindAcceptTimeBase = timeGetTime();
		BindAllowTime = BindAcceptTimeBase + CONFIG_BINDING_DELAY;
		OldSettingIdx = cur_setting_idx;
	}

	if (cur_setting_idx)
	{
		WORD wButtons = g_Controllers[0].state.Gamepad.wButtons;
		
		// we want to delay the button reads for just a bit so we don't end up accidentally binding buttons
		if (timeGetTime() > BindAllowTime)
		{
			// keyboard config
			if (*(int*)CONFIG_JOY_INDEX_ADDR == 0)
			{
				int KBkey = KB_GetCurrentPressedKey();
				if (KBkey != -1)
				{
					// REMOVE BINDING BY PRESSING A BUTTON ON KEYBOARD
					if (KBkey == UNBINDER_KEY)
					{
						if ((cur_setting_idx == 3) || (cur_setting_idx == 4))
						{
							if (cur_setting_idx == 3)
							{
								if (cur_setting_idx_secondary == 0)
								{
									SteerLeftVKey = 0;
									ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT].keycode = SteerLeftVKey;
									SaveSteerBindingToIniKB("KeyboardSteerLeft", SteerLeftVKey);
									SaveBindingToIniKB(JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT, SteerLeftVKey);
								}
							}
							if (cur_setting_idx == 4)
							{
								if (cur_setting_idx_secondary == 0)
								{
									SteerRightVKey = 0;
									ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT].keycode = SteerRightVKey;
									SaveSteerBindingToIniKB("KeyboardSteerRight", SteerRightVKey);
									SaveBindingToIniKB(JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT, SteerRightVKey);
								}
							}
						}
						else
						{
							if (cur_setting_idx_secondary == 0)
							{
								ScannerConfigs[MapKeyboardConfigToEvent(cur_setting_idx)].keycode = 0;
								SaveBindingToIniKB(MapKeyboardConfigToEvent(cur_setting_idx), ScannerConfigs[MapKeyboardConfigToEvent(cur_setting_idx)].keycode);
							}
						}

						cur_setting_idx = 0;
						return;
					}

					if ((cur_setting_idx == 3) || (cur_setting_idx == 4))
					{
						if (cur_setting_idx == 3)
						{
							if (cur_setting_idx_secondary == 0)
							{
								SteerLeftVKey = KBkey;
								ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT].keycode = SteerLeftVKey;
								SaveSteerBindingToIniKB("KeyboardSteerLeft", SteerLeftVKey);
								SaveBindingToIniKB(JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT, SteerLeftVKey);
							}
						}
						if (cur_setting_idx == 4)
						{
							if (cur_setting_idx_secondary == 0)
							{
								SteerRightVKey = KBkey;
								ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT].keycode = SteerRightVKey;
								SaveSteerBindingToIniKB("KeyboardSteerRight", SteerRightVKey);
								SaveBindingToIniKB(JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT, SteerRightVKey);
							}
						}
					}
					else
					{
						if (cur_setting_idx_secondary == 0)
						{
							ScannerConfigs[MapKeyboardConfigToEvent(cur_setting_idx)].keycode = KBkey;
							if (bIsEventDigitalDownOnly(MapKeyboardConfigToEvent(cur_setting_idx)))
								ScannerConfigs[MapKeyboardConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalDown;
							else
								ScannerConfigs[MapKeyboardConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalUpOrDown;
							SaveBindingToIniKB(MapKeyboardConfigToEvent(cur_setting_idx), ScannerConfigs[MapKeyboardConfigToEvent(cur_setting_idx)].keycode);
						}
					}
					cur_setting_idx = 0;
					return;
				}
			}
			// JoyPad config
			else
			{
				// REMOVE BINDING BY PRESSING A BUTTON ON KEYBOARD
				if (KB_GetCurrentPressedKey() == UNBINDER_KEY)
				{
					if (((cur_setting_idx >= 1) && (cur_setting_idx <= 4)))
					{
						if ((cur_setting_idx_secondary == 0))
						{
							if ((cur_setting_idx == 1) || (cur_setting_idx == 2))
							{
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = 0;
								SaveBindingToIni(MapJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
							}
							if (cur_setting_idx == 3)
							{
								SteerLeftAxis = 0;
								ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT].BitmaskStuff = SteerLeftAxis;
								SaveSteerBindingToIni("SteerLeftAxis", SteerLeftAxis);
								SaveBindingToIni(JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT, SteerLeftAxis);
							}
							if (cur_setting_idx == 4)
							{
								SteerRightAxis = 0;
								ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT].BitmaskStuff = SteerRightAxis;
								SaveSteerBindingToIni("SteerRightAxis", SteerRightAxis);
								SaveBindingToIni(JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT, SteerRightAxis);
							}
						}
						else
						{
							if ((cur_setting_idx == 1) || (cur_setting_idx == 2))
							{
								ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = 0;
								SaveBindingToIni(MapSecondaryJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
							}
							if (cur_setting_idx == 3)
							{
								SteerLeftButton = 0;
								// TODO: map drag steering to digital steering (or redo its entire handler)
								SaveSteerBindingToIni("SteerLeftButton", SteerLeftButton);
							}
							if (cur_setting_idx == 4)
							{
								SteerRightButton = 0;
								// TODO: map drag steering to digital steering (or redo its entire handler)
								SaveSteerBindingToIni("SteerRightButton", SteerRightButton);
							}
						}
					}
					else
					{
						if (cur_setting_idx_secondary == 0)
						{
							ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = 0;
							SaveBindingToIni(MapJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
						}
					}
					cur_setting_idx = 0;
					return;
				}

				
				if (((cur_setting_idx >= 1) && (cur_setting_idx <= 4)))
				{
					if ((cur_setting_idx_secondary == 0))
					{
						// analog ONLY bindings -- reads only the analog axis and nothing else!
						uint16_t act = GetAnalogActivity();
						if (act)
						{
							if (cur_setting_idx == 1)
							{
								// throttle analog
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = act;
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_Analog;
								SaveBindingToIni(MapJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
							}
							if (cur_setting_idx == 2)
							{
								// brakes analog
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = act;
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_Analog;
								SaveBindingToIni(MapJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
							}
							if (cur_setting_idx == 3)
							{
								// steer left analog
								SteerLeftAxis = act;
								ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT].BitmaskStuff = SteerLeftAxis;
								SaveSteerBindingToIni("SteerLeftAxis", SteerLeftAxis);
								SaveBindingToIni(JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT, SteerLeftAxis);
							}
							if (cur_setting_idx == 4)
							{
								// steer right analog
								SteerRightAxis = act;
								ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT].BitmaskStuff = SteerRightAxis;
								SaveSteerBindingToIni("SteerRightAxis", SteerRightAxis);
								SaveBindingToIni(JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT, SteerRightAxis);
							}
							cur_setting_idx = 0;
							return;
						}
					}
					else
					{
						// digital ONLY bindings -- reads only the buttons (on the secondary bind)
						if (wButtons)
						{
							if (cur_setting_idx == 1)
							{
								// throttle digital
								ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = wButtons;
								ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalUpOrDown;
								SaveBindingToIni(MapSecondaryJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
							}
							if (cur_setting_idx == 2)
							{
								// brakes digital
								ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = wButtons;
								ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalUpOrDown;
								SaveBindingToIni(MapSecondaryJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
							}
							if (cur_setting_idx == 3)
							{
								// steer left digital
								SteerLeftButton = wButtons;
								// TODO: map drag steering to digital steering (or redo its entire handler)
								SaveSteerBindingToIni("SteerLeftButton", SteerLeftButton);
							}
							if (cur_setting_idx == 4)
							{
								// steer right digital
								SteerRightButton = wButtons;
								// TODO: map drag steering to digital steering (or redo its entire handler)
								SaveSteerBindingToIni("SteerRightButton", SteerRightButton);
							}
							cur_setting_idx = 0;
							return;
						}
					}
				}
				// all other bindings - these can be bound to both analogs and digitals
				else
				{
					uint16_t act = GetAnalogActivity();
					if (act)
					{
						if (cur_setting_idx_secondary == 0)
						{
							ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = act;
							if (bIsEventDigitalDownOnly(MapJoypadConfigToEvent(cur_setting_idx)))
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalDownAnalog;
							else
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalAnalog;
							SaveBindingToIni(MapJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
						}
						cur_setting_idx = 0;
						return;
					}
					else if (wButtons)
					{
						// detect which button is pressed
						if (cur_setting_idx_secondary == 0)
						{
							ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = wButtons;
							if (bIsEventDigitalDownOnly(MapJoypadConfigToEvent(cur_setting_idx)))
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalDown;
							else
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalUpOrDown;
							SaveBindingToIni(MapJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
						}
						cur_setting_idx = 0;
						return;
					}
				}
			}
		}
	}
}
#endif

void __stdcall ReadControllerData()
{
	MSG outMSG;
	UpdateControllerState();
	ReadXInput_Extra();
	if (KeyboardReadingMode == KB_READINGMODE_BUFFERED)
		GetKeyboardState(VKeyStates[0]);
#ifdef GAME_UG
	if (*(int*)GAMEFLOWMANAGER_STATUS_ADDR == 3)
		HandleInGameConfigMenu();
#endif
}

void InitConfig()
{
	CIniReader inireader("");

	KeyboardReadingMode = inireader.ReadInteger("Input", "KeyboardReadingMode", 0);
	bAllowTwoPlayerKB = inireader.ReadInteger("Input", "AllowTwoPlayerKB", 0);
#ifdef GAME_UG
	ControllerIconMode = inireader.ReadInteger("Icons", "ControllerIconMode", 0);
	LastControlledDevice = inireader.ReadInteger("Icons", "FirstControlDevice", 0);
#endif
#ifndef GAME_UG
	bConfineMouse = inireader.ReadInteger("Input", "ConfineMouse", 0);
#endif

	INPUT_DEADZONE_LS = (inireader.ReadFloat("Deadzone", "DeadzonePercentLS", 0.24f) * FLOAT(0x7FFF));
	INPUT_DEADZONE_RS = (inireader.ReadFloat("Deadzone", "DeadzonePercentRS", 0.24f) * FLOAT(0x7FFF));
	INPUT_DEADZONE_LS_P2 = (inireader.ReadFloat("Deadzone", "DeadzonePercentLS_P2", 0.24f) * FLOAT(0x7FFF));
	INPUT_DEADZONE_RS_P2 = (inireader.ReadFloat("Deadzone", "DeadzonePercentRS_P2", 0.24f) * FLOAT(0x7FFF));
	SHIFT_ANALOG_THRESHOLD = (inireader.ReadFloat("Deadzone", "DeadzonePercent_Shifting", 0.62f) * FLOAT(0x7FFF));
	FEUPDOWN_ANALOG_THRESHOLD = (inireader.ReadFloat("Deadzone", "DeadzonePercent_AnalogStickDigital", 0.50f) * FLOAT(0x7FFF));
	TRIGGER_ACTIVATION_THRESHOLD = (inireader.ReadFloat("Deadzone", "DeadzonePercent_AnalogTriggerDigital", 0.12f) * FLOAT(0xFF));
	
	SetupScannerConfig();
}

int Init()
{
#ifdef GAME_UG
	// hook FEng globally in FEPkgMgr_SendMessageToPackage
	injector::MakeJMP(FENG_SENDMSG_HOOK_ADDR, FEngGlobalCave, true);

	// snoop last activated FEng package
	injector::MakeCALL(0x004F3BC3, SnoopLastFEPackage, true);

	// reroute ScannerConfig table
	injector::WriteMemory(SCANNERCONFIG_POINTER_ADDR, ScannerConfigs, true);
	*(int*)0x00704140 = MAX_JOY_EVENT;

	// hook for OptionsSelectorMenu::NotificationMessage to disable controller options (for now)
	//injector::WriteMemory<unsigned int>(OPTIONSSELECTOR_VTABLE_FUNC_ADDR, (unsigned int)&OptionsSelectorMenu_NotificationMessage_Hook, true);
	settings_controllers[0].type = 0x13;
	settings_controllers[1].type = 0x15;
	injector::WriteMemory<int>(0x00413ADD, (int)(&settings_controllers[0].type), true);
	injector::MakeJMP(0x00413C0A, settings_cave1, true);

	injector::MakeJMP(0x004141DF, settings_cave2, true);
	injector::MakeJMP(0x004144EC, settings_cave3, true);

	// disable writing to the device counter
	injector::MakeNOP(0x00405BE8, 6, true);
	injector::MakeNOP(0x00405D2F, 6, true);
	injector::MakeNOP(0x00406463, 5, true);
	injector::MakeNOP(0x0040648F, 5, true);
	injector::MakeNOP(0x004064BB, 5, true);

	injector::MakeJMP(0x00414830, SetConfigButtonText, true);

	injector::MakeCALL(ACTUALREADJOYDATA_CALL_ADDR2, ReadControllerData, true);
	injector::MakeCALL(ACTUALREADJOYDATA_CALL_ADDR3, ReadControllerData, true);
#endif
#ifdef GAME_UG2
	// custom config finder (because it returns 0 with the original function)
	injector::MakeJMP(FINDSCANNERCONFIG_ADDR, FindScannerConfig_Custom, true);
	// Win32 mouse injection
	injector::MakeCALL(0x00581470, UpdateFECursorPos, true);
	// disable cursor hiding
	injector::MakeJMP(0x005D268C, 0x5D269C, true);
	// disable PC_CURSOR texture to avoid duplicate cursors
	injector::WriteMemory<unsigned int>(0x0050B4EA, 0, true);
	injector::MakeNOP(DISABLE_WHEEL_ADDR, 5, true);
#endif

	// custom steering handler
	injector::WriteMemory<unsigned int>(STEER_HANDLER_ADDR1, (int)&RealDriver_JoyHandler_Steer, true);
	injector::WriteMemory<unsigned int>(STEER_HANDLER_ADDR2, (int)&DummyFunc, true);

	// kill DInput8 joypad reading & event generation
	injector::MakeJMP(EVENTGEN_JMP_ADDR_ENTRY, EVENTGEN_JMP_ADDR_EXIT, true);
	// this kills DInput enumeration COMPLETELY -- even the keyboard
	injector::MakeJMP(DINPUTENUM_JMP_ADDR_ENTRY, DINPUTENUM_JMP_ADDR_EXIT, true);
	// kill game input reading
	injector::MakeJMP(JOYBUFFER_JMP_ADDR_ENTRY, JOYBUFFER_JMP_ADDR_EXIT, true);

	// Replace ActualReadJoystickData with ReadControllerData
	injector::MakeCALL(ACTUALREADJOYDATA_CALL_ADDR1, ReadControllerData, true);

	// KB input init
	injector::MakeCALL(INITJOY_CALL_ADDR, InitCustomKBInput, true);

	// dereference the current WndProc from the game executable and write to the function pointer (to maximize compatibility)
	GameWndProcAddr = *(unsigned int*)WNDPROC_POINTER_ADDR;
	GameWndProc = (LRESULT(WINAPI*)(HWND, UINT, WPARAM, LPARAM))GameWndProcAddr;
	injector::WriteMemory<unsigned int>(WNDPROC_POINTER_ADDR, (unsigned int)&CustomWndProc, true);

	// Init state
	ZeroMemory(g_Controllers, sizeof(CONTROLLER_STATE) * MAX_CONTROLLERS);

	InitConfig();

	return 0;
}


BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		freopen("CON", "w", stdout);
		freopen("CON", "w", stderr);
		Init();
	}
	return TRUE;
}

