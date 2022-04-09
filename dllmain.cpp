// Need for Speed (Black Box) - Xtended Input plugin
// Bringing native XInput to NFS
// by Xan/Tenjoin

// TODO: bring rumble/vibration function
// TODO: remapping? -- partially done, but only 1 event per key and 1 key per event
// TODO: kill DInput enough so that it doesn't detect XInput controllers but still detects wheels
// TODO: proper raw input for keyboard (and maybe non XInput gamepads?)
// TODO (UG): properly restore console FrontEnd objects (help messages, controller icons) -- partially done, HELP menu still needs to be restored
// TODO (UG): reassigned button textures -- so when you set Y button for BUTTON1 in FE, it should draw Y and not keep drawing X
// TODO (UG2): button assignments, maybe button hash assignments and FE stuff

#include "stdafx.h"
#include "stdio.h"
#include <windows.h>
#include <bitset>
#include "includes\injector\injector.hpp"
#include "includes\IniReader.h"

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
#include <XInput.h>
#pragma comment(lib,"xinput.lib")
#else
#include <XInput.h>
#pragma comment(lib,"xinput9_1_0.lib")
#endif

#ifdef GAME_UG
#include "NFSU_ConsoleButtonHashes.h"
#include "NFSU_EventNames.h"
#include "NFSU_XtendedInput_FEng.h"
#include "NFSU_Addresses.h"
#endif

#ifdef GAME_UG2
#include "NFSU2_EventNames.h"
#include "NFSU2_Addresses.h"
#endif

#include "NFSU_XtendedInput_XInputConfig.h"
#include "NFSU_XtendedInput_VKHash.h"

#define MAX_CONTROLLERS 4  // XInput handles up to 4 controllers 
#define INPUT_DEADZONE  ( 0.24f * FLOAT(0x7FFF) )  // Default to 24% of the +/- 32767 range.   This is a reasonable default value but can be altered if needed.

#define TRIGGER_ACTIVATION_THRESHOLD 0x20
#define SHIFT_ANALOG_THRESHOLD 0x5000
#define FEUPDOWN_ANALOG_THRESHOLD 0x3FFF

// for triggering the over-zelaous inputs once in a tick...
WORD bQuitButtonOldState = 0;
WORD bYButtonOldState = 0;
WORD bXButtonOldState = 0;

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
BYTE VKeyStates[2][255];
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
	short int BitmaskStuff; // used to define which bits are read from the joypad buffer
	short int BitmaskStuff2; // used to define which bits are read from the joypad buffer
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
	if ((msg == WM_INPUT) && (KeyboardReadingMode == KB_READINGMODE_UNBUFFERED_RAW))
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
	if ((msg == WM_KEYDOWN) && (KeyboardReadingMode != KB_READINGMODE_UNBUFFERED_RAW))
		LastControlledDevice = LASTCONTROLLED_KB;

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
	if ((inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG_ALTERNATE) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG_ALTERNATE) || (inScannerConfig->JoyEvent == JOY_EVENT_STEER_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_STEER))
	{

		if (VKeyStates[ci][inScannerConfig->keycode])
		{
			EventStatesKB[ci][inScannerConfig->JoyEvent] = VKeyStates[ci][inScannerConfig->keycode];
			return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
		}

		if (!VKeyStates[ci][inScannerConfig->keycode])
		{
			EventStatesKB[ci][inScannerConfig->JoyEvent] = VKeyStates[ci][inScannerConfig->keycode];
			return inScannerConfig->JoyEvent;
		}
	}

	// we're using BitmaskStuff to define which button is actually pressed
	if ((VKeyStates[ci][inScannerConfig->keycode]) != EventStatesKB[ci][inScannerConfig->JoyEvent])
	{
		// on change to TRUE -- return 0xFF + JoyEvent number (normally this is a number defined in scannerconfig)
		if (VKeyStates[ci][inScannerConfig->keycode])
		{
			EventStatesKB[ci][inScannerConfig->JoyEvent] = VKeyStates[ci][inScannerConfig->keycode];
			return inScannerConfig->JoyEvent + (inScannerConfig->param << 8);
		}
		// on change to FALSE -- return JoyEvent number
		if (!VKeyStates[ci][inScannerConfig->keycode])
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
	bool bKeyState = VKeyStates[0][inScannerConfig->keycode] >> 7;

	// throttle is very time sensitive, so we return all the time
	if ((inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG_ALTERNATE) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG_ALTERNATE) || (inScannerConfig->JoyEvent == JOY_EVENT_STEER_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_STEER))
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
	bool bKeyState = GetAsyncKeyState(inScannerConfig->keycode) >> 15;

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

int Scanner_DigitalSteer(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	int ci = 0;
	if ((int)Joystick == JOY2_ADDR)
		ci = 1;

	WORD wButtons = g_Controllers[ci].state.Gamepad.wButtons;

	// D-Pad steering
	// we only accept dpad in this case (and read left/right), if it's any other button, ignore
	if (inScannerConfig->BitmaskStuff2 != XINPUT_GAMEPAD_DPAD_CONFIGDEF)
		return inScannerConfig->JoyEvent + ((inScannerConfig->param / 2) << 8);
	// if a direction is pressed
	if (((wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)) != EventStates[ci][inScannerConfig->JoyEvent])
	{
		// on change to TRUE
		if ((wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT))
		{
			if ((wButtons & XINPUT_GAMEPAD_DPAD_LEFT))
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
				return inScannerConfig->JoyEvent; // steer left = 0
			}
			if ((wButtons & XINPUT_GAMEPAD_DPAD_RIGHT))
			{
				EventStates[ci][inScannerConfig->JoyEvent] = (wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
				return inScannerConfig->JoyEvent + (inScannerConfig->param << 8); // steer right = max param = 0xFF
			}
		}
		// on change to FALSE -- return JoyEvent number + parameter / 2 = center
		if (!((wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)))
		{
			EventStates[ci][inScannerConfig->JoyEvent] = ((wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT));
			return inScannerConfig->JoyEvent + ((inScannerConfig->param / 2) << 8); // center steer = param / 2 = 0x7F
		}
	}

	return 0;
}

int Scanner_DigitalSteer_RawKB(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	int ci = 0;
	if ((int)Joystick == 0x73B3EC)
		ci = 1;

	unsigned int outparam = inScannerConfig->param / 2;

	// if a direction is pressed
	if ((VKeyStates[ci][SteerLeftVKey] || VKeyStates[ci][SteerRightVKey]))
	{
		if (VKeyStates[ci][SteerLeftVKey])
			outparam = outparam - inScannerConfig->param / 2;
		if (VKeyStates[ci][SteerRightVKey])
			outparam = outparam + inScannerConfig->param / 2;
	}
		
	return inScannerConfig->JoyEvent + (outparam << 8);
}

int Scanner_DigitalSteer_SyncKB(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	bool bKeyStateLeft = VKeyStates[0][SteerLeftVKey] >> 7;
	bool bKeyStateRight = VKeyStates[0][SteerRightVKey] >> 7;
	unsigned int outparam = inScannerConfig->param / 2;

	// if a direction is pressed
	if ((bKeyStateLeft || bKeyStateRight))
	{
		if (bKeyStateLeft)
			outparam = outparam - inScannerConfig->param / 2;
		if (bKeyStateRight)
			outparam = outparam + inScannerConfig->param / 2;
	}

	return inScannerConfig->JoyEvent + (outparam << 8);
}

int Scanner_DigitalSteer_AsyncKB(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	bool bKeyStateLeft = GetAsyncKeyState(SteerLeftVKey) >> 15;
	bool bKeyStateRight = GetAsyncKeyState(SteerRightVKey) >> 15;
	unsigned int outparam = inScannerConfig->param / 2;

	// if a direction is pressed
	if ((bKeyStateLeft || bKeyStateRight))
	{
		if (bKeyStateLeft)
			outparam = outparam - inScannerConfig->param / 2;
		if (bKeyStateRight)
			outparam = outparam + inScannerConfig->param / 2;
	}

	return inScannerConfig->JoyEvent + (outparam << 8);
}

int Scanner_DigitalSteer_KB(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	switch (KeyboardReadingMode)
	{
	case KB_READINGMODE_UNBUFFERED_RAW:
		return Scanner_DigitalSteer_RawKB(EventNode, unk1, unk2, inScannerConfig, Joystick);
	case KB_READINGMODE_BUFFERED:
		return Scanner_DigitalSteer_SyncKB(EventNode, unk1, unk2, inScannerConfig, Joystick);
	case KB_READINGMODE_UNBUFFERED_ASYNC:
	default:
		return Scanner_DigitalSteer_AsyncKB(EventNode, unk1, unk2, inScannerConfig, Joystick);
	}
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
	if ((VKeyStates[ci][inScannerConfig->keycode]) != EventStatesKB[ci][inScannerConfig->JoyEvent])
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
	bool bKeyState = VKeyStates[0][inScannerConfig->keycode] >> 7;

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
	bool bKeyState = GetAsyncKeyState(inScannerConfig->keycode) >> 15;

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
	switch (inScannerConfig->BitmaskStuff2)
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

int Scanner_DigitalAnalog(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	return Scanner_DigitalAnalog_XInput(EventNode, unk1, unk2, inScannerConfig, Joystick) | Scanner_DigitalUpOrDown_KB(EventNode, unk1, unk2, inScannerConfig, Joystick);
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

	switch (inScannerConfig->BitmaskStuff2)
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

	switch (inScannerConfig->BitmaskStuff2)
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

int Scanner_CombinedSteering(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	int result = Scanner_Analog(EventNode, unk1, unk2, inScannerConfig, Joystick);

	if (LastControlledDevice == LASTCONTROLLED_KB)
		return Scanner_DigitalSteer_KB(EventNode, unk1, unk2, inScannerConfig, Joystick);

	return result;
}

int Scanner_CombinedDigitalSteering(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	if (LastControlledDevice == LASTCONTROLLED_KB)
		return Scanner_DigitalSteer_KB(EventNode, unk1, unk2, inScannerConfig, Joystick);

	return Scanner_DigitalSteer(EventNode, unk1, unk2, inScannerConfig, Joystick);
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
			ScannerConfigs[i].BitmaskStuff2 = inXInputConfigDef;

			if (bIsEventAnalog(i))
				ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_Analog;
			else if ((inXInputConfigDef != XINPUT_GAMEPAD_DPAD_CONFIGDEF))
				ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalAnalog;
			// this is horribly broken, drag analog steering goes across all lanes...
			if ((i == JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT_ANALOG) || (i == JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT_ANALOG))
				ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_Analog_DragSteer;
		}

		// JOY_EVENT_STEER
		if (i == JOY_EVENT_STEER)
		{
			if (inXInputConfigDef == XINPUT_GAMEPAD_DPAD_CONFIGDEF)
				ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_CombinedDigitalSteering;
			else
				ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_CombinedSteering;
		}
	}
	ScannerConfigs[JOY_EVENT_TYPE_CHANGED].ScannerFunctionPointer = (unsigned int)&Scanner_TypeChanged;
#ifdef GAME_UG2
	ScannerConfigs[JOY_EVENT_LIVE_TYPE_CHANGED].ScannerFunctionPointer = (unsigned int)&Scanner_TypeChanged;
#endif
	//ScannerConfigs[JOY_EVENT_ANY].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalAnyButton;

	// read steering VK codes
	SteerLeftVKey = ConvertVKNameToValue(inireader.ReadString("EventsKB", "KeyboardSteerLeft", "VK_LEFT"));
	if (SteerLeftVKey == 0)
	{
		// try checking for single-char
		char lettercheck[32];
		strcpy(lettercheck, inireader.ReadString("EventsKB", "KeyboardSteerLeft", "VK_LEFT"));
			if (strlen(lettercheck) == 1)
				SteerLeftVKey = toupper(lettercheck[0]);
	}
	SteerRightVKey = ConvertVKNameToValue(inireader.ReadString("EventsKB", "KeyboardSteerRight", "VK_RIGHT"));
	if (SteerRightVKey == 0)
	{
		// try checking for single-char
		char lettercheck[32];
		strcpy(lettercheck, inireader.ReadString("EventsKB", "KeyboardSteerRight", "VK_RIGHT"));
		if (strlen(lettercheck) == 1)
			SteerRightVKey = toupper(lettercheck[0]);
	}
}

//////////////////////////////////////////////////////////////////
// Scanner functions END
//////////////////////////////////////////////////////////////////

void InitCustomKBInput()
{
	InitJoystick();
	*(int*)JOYSTICK_P1_CONNECTION_STATUS_ADDR = 1; // without this, game doesn't want to read anything

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

		// Zero value if thumbsticks are within the dead zone 
		if ((g_Controllers[0].state.Gamepad.sThumbLX < INPUT_DEADZONE &&
			g_Controllers[0].state.Gamepad.sThumbLX > -INPUT_DEADZONE) &&
			(g_Controllers[0].state.Gamepad.sThumbLY < INPUT_DEADZONE &&
				g_Controllers[0].state.Gamepad.sThumbLY > -INPUT_DEADZONE))
		{
			g_Controllers[0].state.Gamepad.sThumbLX = 0;
			g_Controllers[0].state.Gamepad.sThumbLY = 0;
		}

		if ((g_Controllers[0].state.Gamepad.sThumbRX < INPUT_DEADZONE &&
			g_Controllers[0].state.Gamepad.sThumbRX > -INPUT_DEADZONE) &&
			(g_Controllers[0].state.Gamepad.sThumbRY < INPUT_DEADZONE &&
				g_Controllers[0].state.Gamepad.sThumbRY > -INPUT_DEADZONE))
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
	}

	dwResult = XInputGetState(1, &g_Controllers[1].state);

	if (dwResult == ERROR_SUCCESS)
	{
		g_Controllers[1].bConnected = true;
		* (unsigned char*)JOYSTICKTYPE_P2_ADDR = 0;
		// Zero value if thumbsticks are within the dead zone 
		if ((g_Controllers[1].state.Gamepad.sThumbLX < INPUT_DEADZONE &&
			g_Controllers[1].state.Gamepad.sThumbLX > -INPUT_DEADZONE) &&
			(g_Controllers[1].state.Gamepad.sThumbLY < INPUT_DEADZONE &&
				g_Controllers[1].state.Gamepad.sThumbLY > -INPUT_DEADZONE))
		{
			g_Controllers[1].state.Gamepad.sThumbLX = 0;
			g_Controllers[1].state.Gamepad.sThumbLY = 0;
		}

		if ((g_Controllers[1].state.Gamepad.sThumbRX < INPUT_DEADZONE &&
			g_Controllers[1].state.Gamepad.sThumbRX > -INPUT_DEADZONE) &&
			(g_Controllers[1].state.Gamepad.sThumbRY < INPUT_DEADZONE &&
				g_Controllers[1].state.Gamepad.sThumbRY > -INPUT_DEADZONE))
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

void __stdcall ReadControllerData()
{
	MSG outMSG;
	UpdateControllerState();
	ReadXInput_Extra();
	if (KeyboardReadingMode == KB_READINGMODE_BUFFERED)
		GetKeyboardState(VKeyStates[0]);
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
	SetupScannerConfig();
}

int Init()
{
	// kill DInput8 joypad reading & event generation
	injector::MakeJMP(EVENTGEN_JMP_ADDR_ENTRY, EVENTGEN_JMP_ADDR_EXIT, true);
#ifdef GAME_UG
	// hook FEng globally in FEPkgMgr_SendMessageToPackage
	injector::MakeJMP(FENG_SENDMSG_HOOK_ADDR, FEngGlobalCave, true);

	// snoop last activated FEng package
	injector::MakeCALL(0x004F3BC3, SnoopLastFEPackage, true);
#endif
#ifndef GAME_UG
	// custom config finder (because it returns 0 with the original function)
	injector::MakeJMP(FINDSCANNERCONFIG_ADDR, FindScannerConfig_Custom, true);
#endif
	// this kills DInput enumeration COMPLETELY -- even the keyboard
	injector::MakeJMP(DINPUTENUM_JMP_ADDR_ENTRY, DINPUTENUM_JMP_ADDR_EXIT, true);
	// kill game input reading
	injector::MakeJMP(JOYBUFFER_JMP_ADDR_ENTRY, JOYBUFFER_JMP_ADDR_EXIT, true);

	// Replace ActualReadJoystickData with ReadControllerData
	injector::MakeCALL(ACTUALREADJOYDATA_CALL_ADDR1, ReadControllerData, true);
#ifdef GAME_UG
	injector::MakeCALL(ACTUALREADJOYDATA_CALL_ADDR2, ReadControllerData, true);
	injector::MakeCALL(ACTUALREADJOYDATA_CALL_ADDR3, ReadControllerData, true);
#endif
	// reroute ScannerConfig table
	injector::WriteMemory(SCANNERCONFIG_POINTER_ADDR, ScannerConfigs, true);
	*(int*)0x00704140 = MAX_JOY_EVENT;

	// KB input init
	injector::MakeCALL(INITJOY_CALL_ADDR, InitCustomKBInput, true);
#ifdef GAME_UG
	// hook for OptionsSelectorMenu::NotificationMessage to disable controller options (for now)
	injector::WriteMemory<unsigned int>(OPTIONSSELECTOR_VTABLE_FUNC_ADDR, (unsigned int)&OptionsSelectorMenu_NotificationMessage_Hook, true);
#endif
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

