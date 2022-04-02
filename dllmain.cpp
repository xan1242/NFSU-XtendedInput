// NFS Underground - Xtended Input plugin
// Bringing native XInput to NFS
// by Xan/Tenjoin

// TODO: hook input scanners instead -- they're a better and more accurate target than the joypad input buffer
// TODO: bring rumble/vibration function
// TODO: remapping?
// TODO: bring back P2 controls if possible
// TODO: kill DInput enough so that it doesn't detect XInput controllers but still detects wheels
// TODO: properly restore console FrontEnd objects (help messages, controller icons) -- partially done, HELP menu still needs to be restored

#include "stdafx.h"
#include "stdio.h"
#include <windows.h>
#include <bitset>
#include "NFSU_JoyBitInfo.h"
#include "NFSU_ConsoleButtonHashes.h"
#include "includes\injector\injector.hpp"
#include "includes\IniReader.h"

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
#include <XInput.h>
#pragma comment(lib,"xinput.lib")
#else
#include <XInput.h>
#pragma comment(lib,"xinput9_1_0.lib")
#endif

#define MAX_CONTROLLERS 4  // XInput handles up to 4 controllers 
#define INPUT_DEADZONE  ( 0.24f * FLOAT(0x7FFF) )  // Default to 24% of the +/- 32767 range.   This is a reasonable default value but can be altered if needed.

#define TRIGGER_ACTIVATION_THRESHOLD 0x20
#define SHIFT_ANALOG_THRESHOLD 0x5000
#define FEUPDOWN_ANALOG_THRESHOLD 0x3FFF

#define JOYBUTTONS1_ADDR 0x00719780
#define JOYBUTTONS2_ADDR 0x00719784
#define THROTTLE_AXIS_ADDR 0x00719788
#define BRAKE_AXIS_ADDR 0x00719789
#define STEER_AXIS_ADDR 0x0071978A
#define FE_WINDOWS_KEY_CODE_ADDR 0x007363B1
#define GAMEFLOWMANAGER_STATUS_ADDR 0x0077A920
#define CURRENT_MENUPKG_ADDR 0x72CDD0
#define FEMOUSECURSOR_BUTTONPRESS_ADDR 0x007064B0
#define FEMOUSECURSOR_CARORBIT_X_ADDR 0x007064A4
#define FEMOUSECURSOR_CARORBIT_Y_ADDR 0x007064A8

bool bCarOrbitState = 0;
bool bCarOrbitOldState = 0;
float CarOrbitDivisor = 0.5;

bool bLoadedConsoleButtonTex = false;
char LastFEngPackage[128];
char CurrentSplashText[64];

#define LASTCONTROLLED_KB 0
#define LASTCONTROLLED_CONTROLLER 1
unsigned int LastControlledDevice = 0; // 0 = keyboard, 1 = controller
unsigned int LastControlledDeviceOldState = 0;

#define CONTROLLERICON_XBOXONE 0
#define CONTROLLERICON_PS4 1
unsigned int ControllerIconMode = 0; // 0 = Xbox (One and later only for now), 1 = PlayStation (4 only now) -- planned to add: Nintendo (Wii/U Classic Controller, Switch), PS3/PS2, Xbox 360

// for triggering the over-zelaous inputs once in a tick...
WORD bQuitButtonOldState = 0;
WORD bZButtonOldState = 0;
WORD bXButtonOldState = 0;

struct CONTROLLER_STATE
{
	XINPUT_STATE state;
	bool bConnected;
}g_Controllers[MAX_CONTROLLERS];

enum ResourceFileType
{
	RESOURCE_FILE_NONE = 0,
	RESOURCE_FILE_GLOBAL = 1,
	RESOURCE_FILE_FRONTEND = 2,
	RESOURCE_FILE_INGAME = 3,
	RESOURCE_FILE_TRACK = 4,
	RESOURCE_FILE_NIS = 5,
	RESOURCE_FILE_CAR = 6,
	RESOURCE_FILE_LANGUAGE = 7,
	RESOURCE_FILE_REPLAY = 8,
};

void*(*CreateResourceFile)(char* filename, int ResType, int unk1, int unk2, int unk3) = (void*(*)(char*, int, int, int, int))0x004482F0;
void(*ServiceResourceLoading)() = (void(*)())0x004483C0;
unsigned int(*GetTextureInfo)(unsigned int hash, int unk, int unk2) = (unsigned int(*)(unsigned int, int, int))0x005461C0;

unsigned int GameWndProcAddr = 0;
LRESULT(WINAPI* GameWndProc)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI CustomWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_KEYDOWN)
		LastControlledDevice = LASTCONTROLLED_KB;

	return GameWndProc(hWnd, msg, wParam, lParam);
}

unsigned int ResourceFile_BeginLoading_Addr = 0x00448110;
void __stdcall ResourceFile_BeginLoading(void* ResourceFile, void* unk1, int unk2)
{
	_asm
	{
		mov edx, ResourceFile
		mov ecx, unk2
		mov eax, unk1
		call ResourceFile_BeginLoading_Addr
	}
}

void __stdcall LoadResourceFile(char* filename, int ResType, int unk1, void* unk2, int unk3, int unk4, int unk5)
{
	ResourceFile_BeginLoading(CreateResourceFile(filename, ResType, unk1, unk4, unk5), unk2, unk3);
}

unsigned int FEHashUpper_Addr = 0x004FD230;
unsigned int __stdcall FEHashUpper(const char* instr)
{
	unsigned int result = 0;
	_asm
	{
		mov edx, instr
		call FEHashUpper_Addr
		mov result, eax
	}
	return result;
}

// entrypoint: 0x004B0CB9
unsigned int FECarOrbitCave_Exit1 = 0x004B0CC1;
unsigned int FECarOrbitCave_Exit2 = 0x4B0D37;
static bool bOrbitingWithRightStick = 0;
void __declspec(naked) FECarOrbitCave()
{
	// using assembly here because MSVC insists on using the ebp register...
	_asm
	{
		mov al, ds:FEMOUSECURSOR_BUTTONPRESS_ADDR
		mov cl, bOrbitingWithRightStick
		or al, cl
		mov bCarOrbitState, al
	}

	if (bCarOrbitState != bCarOrbitOldState)
	{
		// on the change to false, for precisely 1 tick allow the variables to be updated
		_asm
		{
			mov al, bCarOrbitState
			mov bCarOrbitOldState, al
			jmp FECarOrbitCave_Exit1
		}
	}

	if (bCarOrbitState)
		_asm jmp FECarOrbitCave_Exit1
	_asm jmp FECarOrbitCave_Exit2
}

// texture & control stuff
unsigned int FEPkgMgr_FindPackage_Addr = 0x004F65D0;
unsigned int FEPackageManager_FindPackage_Addr = 0x004F3F90;
unsigned int FEngFindObject_Addr = 0x004FFB70;
unsigned int FEngSendMessageToPackage_Addr = 0x004C96C0;
unsigned int FEPrintf_Addr = 0x004F68A0;
#pragma runtime_checks( "", off )
int __stdcall FEPrintf(void* FEString, char* format)
{
	_asm
	{
		mov eax, FEString
		push format
		call FEPrintf_Addr
	}
}
#pragma runtime_checks( "", restore )
void __stdcall FEngSendMessageToPackage(unsigned int msg, char* dest)
{
	_asm
	{
		push msg
		mov eax, dest
		call FEngSendMessageToPackage_Addr
		add esp, 4
	}
}

unsigned int __stdcall FEPkgMgr_FindPackage(const char* pkgname)
{
	unsigned int result = 0;
	_asm
	{
		mov eax, pkgname
		call FEPkgMgr_FindPackage_Addr
		mov result, eax
	}
	return result;
}

unsigned int __stdcall FEPackageManager_FindPackage(const char* pkgname)
{
	unsigned int result = 0;
	_asm
	{
		push 0x00746104
		mov eax, pkgname
		call FEPackageManager_FindPackage_Addr
		mov result, eax
	}
	return result;
}

unsigned int __stdcall _FEngFindObject(const char* pkg, int hash)
{
	unsigned int result = 0;
	_asm
	{
		mov ecx, hash
		mov edx, pkg
		call FEngFindObject_Addr
		mov result, eax
	}
	return result;
}

unsigned int __stdcall FEngFindObject(const char* pkg, int hash)
{
	char* pkgname = (char*)FEPkgMgr_FindPackage(pkg);
	if (!pkgname)
		return 0;
	else
		return _FEngFindObject(pkgname, hash);
}

unsigned int __stdcall FEngSetTextureHash(unsigned int FEImage, int hash)
{
	int v2;

	if (FEImage)
	{
		if (*(int*)(FEImage + 36) != hash)
		{
			v2 = *(int*)(FEImage + 28);
			*(int*)(FEImage + 36) = hash;
			*(int*)(FEImage + 28) = v2 | 0x400000;
		}
	}
	return FEImage;
}

int __stdcall FEngFindString(const char* pkgname, int hash)
{
	int result; // r3

	result = FEngFindObject(pkgname, hash);
	if (!result || *(int*)(result + 24) != 2)
		result = 0;
	return result;
}

int __stdcall FEngFindImage(const char* pkgname, int hash)
{
	int result;

	result = FEngFindObject(pkgname, hash);
	if (!result || *(int*)(result + 24) != 1)
		result = 0;
	return result;
}

void SnoopLastFEPackage(char* format, char* pkg)
{
	strcpy(LastFEngPackage, pkg);
}

void SetPCFEButtons(char* pkgname)
{
	int FEImage = 0;
	if (FEImage = FEngFindImage(pkgname, 0x06749436)) // DeleteProfile_hotkey
		FEngSetTextureHash(FEImage, PC_DELETE);
	if (FEImage = FEngFindImage(pkgname, 0xA7615A5D)) // DeletePersona_hotkey
		FEngSetTextureHash(FEImage, PC_DELETE);
	if (FEImage = FEngFindImage(pkgname, 0x6E1D75B7)) // Default_hotkey
		FEngSetTextureHash(FEImage, PC_CREATEROOM);
	if (FEImage = FEngFindImage(pkgname, 0x83A65176)) // Stock_hotkey
		FEngSetTextureHash(FEImage, PC_CREATEROOM);
	if (FEImage = FEngFindImage(pkgname, 0xC8754FA4)) // Performance_hotkey
		FEngSetTextureHash(FEImage, PC_PERFORMANCE);
	if (FEImage = FEngFindImage(pkgname, 0xA16A1877)) // Changecolor_hotkey -- during car color changer
		FEngSetTextureHash(FEImage, PC_X);
	if (FEImage = FEngFindImage(pkgname, 0xBDD6AEEA)) // DecalColor_hotkey
		FEngSetTextureHash(FEImage, PC_CREATEROOM);
	if (FEImage = FEngFindImage(pkgname, 0x548F2015)) // Quit_hotkey
		FEngSetTextureHash(FEImage, PC_QUIT);
	if (FEImage = FEngFindImage(pkgname, 0x55DAFA35)) // customize_hotkey
		FEngSetTextureHash(FEImage, PC_CUSTOM);
	if (FEImage = FEngFindImage(pkgname, 0x571A8D51)) // Next_hotkey -- the "accept" button
		FEngSetTextureHash(FEImage, PC_X);
	if (FEImage = FEngFindImage(pkgname, 0x2A1208C3)) // Back_hotkey
		FEngSetTextureHash(FEImage, PC_BACK);
	if (FEImage = FEngFindImage(pkgname, 0x6340B325)) // PC_tutorial_01 (tutorial key)
		FEngSetTextureHash(FEImage, PC_TUTORIAL);
	if (FEImage = FEngFindImage(pkgname, 0x6340B326)) // PC_tutorial_02 (tutorial key)
		FEngSetTextureHash(FEImage, PC_TUTORIAL);

	// set splash screen text
	if (FEHashUpper(pkgname) == 0x2729D8C3) // if we're in LS_Splash_PC.fng
	{
		wcstombs(CurrentSplashText, *(wchar_t**)(FEngFindString("LS_Splash_PC.fng", 0x13CF446D) + 0x60), 0x800);
		if (FEHashUpper(CurrentSplashText) != 0xE7126192) // "Press enter to continue" -- TODO: multi-lingual support...
			FEPrintf((void*)FEngFindString("LS_Splash_PC.fng", 0x13CF446D), "Press enter to continue"); // mouse_click text object
	}
}

void SetPCIGButtons(char* pkgname)
{
	int FEImage = 0;
	if (FEImage = FEngFindImage(pkgname, 0x571A8D51)) // Next_hotkey -- the "accept" button
		FEngSetTextureHash(FEImage, PC_X);
	if (FEImage = FEngFindImage(pkgname, 0x2A1208C3)) // Back_hotkey
		FEngSetTextureHash(FEImage, PC_BACK);
	if (FEImage = FEngFindImage(pkgname, 0x548F2015)) // Quit_hotkey
		FEngSetTextureHash(FEImage, PC_QUIT);
}

void SetXboxFEButtons(char* pkgname)
{
	int FEImage = 0;
	if (FEImage = FEngFindImage(pkgname, 0x06749436)) // DeleteProfile_hotkey
		FEngSetTextureHash(FEImage, XBOXY_HASH);
	if (FEImage = FEngFindImage(pkgname, 0xA7615A5D)) // DeletePersona_hotkey
		FEngSetTextureHash(FEImage, XBOXY_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x6E1D75B7)) // Default_hotkey
		FEngSetTextureHash(FEImage, XBOXY_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x83A65176)) // Stock_hotkey
		FEngSetTextureHash(FEImage, XBOXY_HASH);
	//if (FEImage = FEngFindImage(pkgname, 0xAD7303C0)) // CreateGame_hotkey
	//	FEngSetTextureHash(FEImage, XBOXY_HASH);
	if (FEImage = FEngFindImage(pkgname, 0xC8754FA4)) // Performance_hotkey
		FEngSetTextureHash(FEImage, XBOXX_HASH);
	if (FEImage = FEngFindImage(pkgname, 0xA16A1877)) // Changecolor_hotkey -- during car color changer
		FEngSetTextureHash(FEImage, XBOXA_HASH);
	if (FEImage = FEngFindImage(pkgname, 0xBDD6AEEA)) // DecalColor_hotkey
		FEngSetTextureHash(FEImage, XBOXY_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x548F2015)) // Quit_hotkey
		FEngSetTextureHash(FEImage, XBOXBACK_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x55DAFA35)) // customize_hotkey
		FEngSetTextureHash(FEImage, XBOXX_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x571A8D51)) // Next_hotkey -- the "accept" button
		FEngSetTextureHash(FEImage, XBOXA_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x2A1208C3)) // Back_hotkey
		FEngSetTextureHash(FEImage, XBOXB_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x6340B325)) // PC_tutorial_01 (tutorial key)
		FEngSetTextureHash(FEImage, XBOXLT_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x6340B326)) // PC_tutorial_02 (tutorial key)
		FEngSetTextureHash(FEImage, XBOXLT_HASH);


	
	// set splash screen text
	if (FEHashUpper(pkgname) == 0x2729D8C3) // if we're in LS_Splash_PC.fng
	{
		wcstombs(CurrentSplashText, *(wchar_t**)(FEngFindString("LS_Splash_PC.fng", 0x13CF446D) + 0x60), 0x800);
		if (FEHashUpper(CurrentSplashText) != 0x544518FD) // "Press MENU button"
			FEPrintf((void*)FEngFindString("LS_Splash_PC.fng", 0x13CF446D), "Press MENU/A button"); // mouse_click text object
	}
}

void SetXboxIGButtons(char* pkgname)
{
	int FEImage = 0;
	if (FEImage = FEngFindImage(pkgname, 0x571A8D51)) // Next_hotkey -- the "accept" button
		FEngSetTextureHash(FEImage, XBOXA_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x2A1208C3)) // Back_hotkey
		FEngSetTextureHash(FEImage, XBOXB_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x548F2015)) // Quit_hotkey
		FEngSetTextureHash(FEImage, XBOXBACK_HASH);
}

void SetPlayStationFEButtons(char* pkgname)
{
	int FEImage = 0;
	if (FEImage = FEngFindImage(pkgname, 0x06749436)) // DeleteProfile_hotkey
		FEngSetTextureHash(FEImage, PSTRIANGLE_HASH);
	if (FEImage = FEngFindImage(pkgname, 0xA7615A5D)) // DeletePersona_hotkey
		FEngSetTextureHash(FEImage, PSTRIANGLE_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x6E1D75B7)) // Default_hotkey
		FEngSetTextureHash(FEImage, PSTRIANGLE_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x83A65176)) // Stock_hotkey
		FEngSetTextureHash(FEImage, PSTRIANGLE_HASH);
	//if (FEImage = FEngFindImage(pkgname, 0xAD7303C0)) // CreateGame_hotkey
	//	FEngSetTextureHash(FEImage, PSTRIANGLE_HASH);
	if (FEImage = FEngFindImage(pkgname, 0xC8754FA4)) // Performance_hotkey
		FEngSetTextureHash(FEImage, PSSQUARE_HASH);
	if (FEImage = FEngFindImage(pkgname, 0xA16A1877)) // Changecolor_hotkey -- during car color changer
		FEngSetTextureHash(FEImage, PSCROSS_HASH);
	if (FEImage = FEngFindImage(pkgname, 0xBDD6AEEA)) // DecalColor_hotkey
		FEngSetTextureHash(FEImage, PSTRIANGLE_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x548F2015)) // Quit_hotkey
		FEngSetTextureHash(FEImage, PSSHARE_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x55DAFA35)) // customize_hotkey
		FEngSetTextureHash(FEImage, PSSQUARE_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x571A8D51)) // Next_hotkey -- the "accept" button
		FEngSetTextureHash(FEImage, PSCROSS_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x2A1208C3)) // Back_hotkey
		FEngSetTextureHash(FEImage, PSCIRCLE_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x6340B325)) // PC_tutorial_01 (tutorial key)
		FEngSetTextureHash(FEImage, PSL2_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x6340B326)) // PC_tutorial_02 (tutorial key)
		FEngSetTextureHash(FEImage, PSL2_HASH);

	// set splash screen text
	if (FEHashUpper(pkgname) == 0x2729D8C3) // if we're in LS_Splash_PC.fng
	{
		wcstombs(CurrentSplashText, *(wchar_t**)(FEngFindString("LS_Splash_PC.fng", 0x13CF446D) + 0x60), 0x800);
		if (FEHashUpper(CurrentSplashText) != 0x0BACC2D4) // "Press OPTIONS button"
			FEPrintf((void*)FEngFindString("LS_Splash_PC.fng", 0x13CF446D), "Press OPTIONS/X button"); // mouse_click text object
	}
}

void SetPlayStationIGButtons(char* pkgname)
{
	int FEImage = 0;
	if (FEImage = FEngFindImage(pkgname, 0x571A8D51)) // Next_hotkey -- the "accept" button
		FEngSetTextureHash(FEImage, PSCROSS_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x2A1208C3)) // Back_hotkey
		FEngSetTextureHash(FEImage, PSCIRCLE_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x548F2015)) // Quit_hotkey
		FEngSetTextureHash(FEImage, PSSHARE_HASH);
}

void __stdcall SetControllerFEng(char* pkgname)
{
	if (!bLoadedConsoleButtonTex)
	{
		LoadResourceFile("GLOBAL\\UG_ConsoleButtons.tpk", RESOURCE_FILE_NONE, 0, NULL, 0, 0, 0);
		ServiceResourceLoading();
		bLoadedConsoleButtonTex = true;
	}

	if (*(int*)GAMEFLOWMANAGER_STATUS_ADDR == 3)
	{
		if (LastControlledDevice == LASTCONTROLLED_CONTROLLER)
		{
			switch (ControllerIconMode)
			{
			case CONTROLLERICON_PS4:
				SetPlayStationFEButtons(pkgname);
				break;
			case CONTROLLERICON_XBOXONE:
			default:
				SetXboxFEButtons(pkgname);
				break;
			}
		}
		if (LastControlledDevice == LASTCONTROLLED_KB)
			SetPCFEButtons(pkgname);

	}
	if (*(int*)GAMEFLOWMANAGER_STATUS_ADDR == 6)
	{
		if (LastControlledDevice == LASTCONTROLLED_CONTROLLER)
		{
			switch (ControllerIconMode)
			{
			case CONTROLLERICON_PS4:
				SetPlayStationIGButtons(pkgname);
				break;
			case CONTROLLERICON_XBOXONE:
			default:
				SetXboxIGButtons(pkgname);
				break;
			}
		}
		if (LastControlledDevice == LASTCONTROLLED_KB)
			SetPCIGButtons(pkgname);
	}
}

// entrypoint: 0x004F7C08
unsigned int FEngGlobalCaveExit = 0x004F7C0E;
unsigned int FEngGlobalCaveEBP = 0;
unsigned int FEngGlobalCaveESI = 0;
char* CurrentFEngPackage = NULL;
void __declspec(naked) FEngGlobalCave()
{
	_asm mov FEngGlobalCaveEBP, ebp
	_asm mov FEngGlobalCaveESI, esi
	CurrentFEngPackage = *(char**)(FEngGlobalCaveESI + 0xC);
	SetControllerFEng(CurrentFEngPackage);
	_asm
	{
		mov esi, FEngGlobalCaveESI
		mov ebp, FEngGlobalCaveEBP
		cmp ebp, 0xC519BFC3
		jmp FEngGlobalCaveExit
	}
}

void DummyFunc()
{
	return;
}

HRESULT UpdateControllerState()
{
	DWORD dwResult;

	dwResult = XInputGetState(0, &g_Controllers[0].state);

	if (dwResult == ERROR_SUCCESS)
		g_Controllers[0].bConnected = true;
	else
		g_Controllers[0].bConnected = false;

	dwResult = XInputGetState(1, &g_Controllers[1].state);

	if (dwResult == ERROR_SUCCESS)
		g_Controllers[1].bConnected = true;
	else
		g_Controllers[1].bConnected = false;

	return S_OK;
}

void ReadXInput()
{
	std::bitset<32> b1(*(int*)JOYBUTTONS1_ADDR);
	std::bitset<32> b2(*(int*)JOYBUTTONS2_ADDR);
	bool bLastWasKeyboard = false;

	if (g_Controllers[0].bConnected)
	{
		WORD wButtons = g_Controllers[0].state.Gamepad.wButtons;

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

		if (wButtons || g_Controllers[0].state.Gamepad.sThumbLX || g_Controllers[0].state.Gamepad.sThumbLY || g_Controllers[0].state.Gamepad.sThumbRX || g_Controllers[0].state.Gamepad.sThumbRY || g_Controllers[0].state.Gamepad.bRightTrigger || g_Controllers[0].state.Gamepad.bLeftTrigger)
			LastControlledDevice = LASTCONTROLLED_CONTROLLER;

		*(unsigned char*)THROTTLE_AXIS_ADDR = g_Controllers[0].state.Gamepad.bRightTrigger;
		*(unsigned char*)BRAKE_AXIS_ADDR = g_Controllers[0].state.Gamepad.bLeftTrigger;
		*(char*)STEER_AXIS_ADDR = (char)(((float)(g_Controllers[0].state.Gamepad.sThumbLX) / (float)(0x7FFF)) * (float)(0x7F)) + 0x80;
	
		b1[NFSUJOY_DIGITAL_LEFT] = b1[NFSUJOY_DIGITAL_LEFT] ? !(wButtons & XINPUT_GAMEPAD_DPAD_LEFT) : b1[NFSUJOY_DIGITAL_LEFT];
		b1[NFSUJOY_DIGITAL_RIGHT] = b1[NFSUJOY_DIGITAL_RIGHT] ? !(wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) : b1[NFSUJOY_DIGITAL_RIGHT];

		b1[NFSUJOY_PAUSE] = b1[NFSUJOY_PAUSE] ? !(wButtons & XINPUT_GAMEPAD_START) : b1[NFSUJOY_PAUSE];
		b1[NFSUJOY_NOS] = b1[NFSUJOY_NOS] ? !(wButtons & XINPUT_GAMEPAD_B) : b1[NFSUJOY_NOS];
		b1[NFSUJOY_EBRAKE] = b1[NFSUJOY_EBRAKE] ? !(wButtons & XINPUT_GAMEPAD_A) : b1[NFSUJOY_EBRAKE];
		b1[NFSUJOY_RESETCAR] = b1[NFSUJOY_RESETCAR] ? !(wButtons & XINPUT_GAMEPAD_BACK) : b1[NFSUJOY_RESETCAR];
		b1[NFSUJOY_LOOKBACK] = b1[NFSUJOY_LOOKBACK] ? !(wButtons & XINPUT_GAMEPAD_X) : b1[NFSUJOY_LOOKBACK];
		b1[NFSUJOY_CHANGECAM] = b1[NFSUJOY_CHANGECAM] ? !(wButtons & XINPUT_GAMEPAD_Y) : b1[NFSUJOY_CHANGECAM];
		b1[NFSUJOY_SHIFTUP] = b1[NFSUJOY_SHIFTUP] ? !(g_Controllers[0].state.Gamepad.sThumbRY > SHIFT_ANALOG_THRESHOLD) : b1[NFSUJOY_SHIFTUP];
		b1[NFSUJOY_SHIFTDOWN] = b1[NFSUJOY_SHIFTDOWN] ? !(g_Controllers[0].state.Gamepad.sThumbRY < -SHIFT_ANALOG_THRESHOLD) : b1[NFSUJOY_SHIFTDOWN];

		b1[NFSUJOY_FE_UP] = b1[NFSUJOY_FE_UP] ? !(wButtons & XINPUT_GAMEPAD_DPAD_UP) : b1[NFSUJOY_FE_UP];
		b1[NFSUJOY_FE_DOWN] = b1[NFSUJOY_FE_DOWN] ? !(wButtons & XINPUT_GAMEPAD_DPAD_DOWN) : b1[NFSUJOY_FE_DOWN];
		b1[NFSUJOY_FE_UP] = b1[NFSUJOY_FE_UP] ? !(g_Controllers[0].state.Gamepad.sThumbLY > FEUPDOWN_ANALOG_THRESHOLD) : b1[NFSUJOY_FE_UP];
		b1[NFSUJOY_FE_DOWN] = b1[NFSUJOY_FE_DOWN] ? !(g_Controllers[0].state.Gamepad.sThumbLY < -FEUPDOWN_ANALOG_THRESHOLD) : b1[NFSUJOY_FE_DOWN];
		
		b1[NFSUJOY_FE_LEFT] = b1[NFSUJOY_FE_LEFT] ? !(wButtons & XINPUT_GAMEPAD_DPAD_LEFT) : b1[NFSUJOY_FE_LEFT];
		b1[NFSUJOY_FE_RIGHT] = b1[NFSUJOY_FE_RIGHT] ? !(wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) : b1[NFSUJOY_FE_RIGHT];
		b1[NFSUJOY_FE_ACCEPT] = b1[NFSUJOY_FE_ACCEPT] ? !(wButtons & XINPUT_GAMEPAD_A) : b1[NFSUJOY_FE_ACCEPT];
		b1[NFSUJOY_FE_BACK] = b1[NFSUJOY_FE_BACK] ? !(wButtons & XINPUT_GAMEPAD_B) : b1[NFSUJOY_FE_BACK];
		b1[NFSUJOY_FE_BIT24] = b1[NFSUJOY_FE_BIT24] ? !(wButtons & XINPUT_GAMEPAD_X) : b1[NFSUJOY_FE_BIT24];
		b1[NFSUJOY_FE_BIT22] = b1[NFSUJOY_FE_BIT22] ? !(wButtons & XINPUT_GAMEPAD_Y) : b1[NFSUJOY_FE_BIT22];
		b1[NFSUJOY_FE_L1] = b1[NFSUJOY_FE_L1] ? !(wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) : b1[NFSUJOY_FE_L1];
		b1[NFSUJOY_FE_R1] = b1[NFSUJOY_FE_R1] ? !(wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) : b1[NFSUJOY_FE_R1];
		b1[NFSUJOY_FE_L2] = b1[NFSUJOY_FE_L2] ? !(g_Controllers[0].state.Gamepad.bLeftTrigger > TRIGGER_ACTIVATION_THRESHOLD) : b1[NFSUJOY_FE_L2];
		b1[NFSUJOY_FE_R2] = b1[NFSUJOY_FE_R2] ? !(g_Controllers[0].state.Gamepad.bRightTrigger > TRIGGER_ACTIVATION_THRESHOLD) : b1[NFSUJOY_FE_R2];
		// splash screen start button
		if (FEHashUpper(LastFEngPackage) == 0x2729D8C3)
			b1[NFSUJOY_FE_ACCEPT] = b1[NFSUJOY_FE_ACCEPT] ? !(wButtons & XINPUT_GAMEPAD_START) : b1[NFSUJOY_FE_ACCEPT];

		// car orbiting
		if ((g_Controllers[0].state.Gamepad.sThumbRX != 0) || (g_Controllers[0].state.Gamepad.sThumbRY != 0))
		{
			bOrbitingWithRightStick = true;
			if (g_Controllers[0].state.Gamepad.sThumbRX != 0)
				*(int*)FEMOUSECURSOR_CARORBIT_X_ADDR = -g_Controllers[0].state.Gamepad.sThumbRX;
			if (g_Controllers[0].state.Gamepad.sThumbRY != 0)
				*(int*)FEMOUSECURSOR_CARORBIT_Y_ADDR = g_Controllers[0].state.Gamepad.sThumbRY;
			CarOrbitDivisor = 0.000031f;
		}
		else
		{
			bOrbitingWithRightStick = false;
			CarOrbitDivisor = 0.5;
			if (!*(bool*)FEMOUSECURSOR_BUTTONPRESS_ADDR)
			{
				*(int*)FEMOUSECURSOR_CARORBIT_X_ADDR = 0;
				*(int*)FEMOUSECURSOR_CARORBIT_Y_ADDR = 0;
			}
		}
		if (*(bool*)FEMOUSECURSOR_BUTTONPRESS_ADDR)
			CarOrbitDivisor = 0.5;

		// tester
		//b2[NFSUJOY_BIT33] = !(wButtons & XINPUT_GAMEPAD_DPAD_UP);

		// TODO: these are a lil' buggy
		if (*(int*)GAMEFLOWMANAGER_STATUS_ADDR == 3)
		{
			if ((wButtons & XINPUT_GAMEPAD_X) != bXButtonOldState)
			{
				if (wButtons & XINPUT_GAMEPAD_X) // trigger once only on button down state
				{
					if (FEHashUpper((char*)CURRENT_MENUPKG_ADDR) == 0x1F549740) // MU_GaragePerformanceCategory.fng
						*(char*)FE_WINDOWS_KEY_CODE_ADDR = 'P'; // performance stats
					else if (FEHashUpper((char*)CURRENT_MENUPKG_ADDR) == 0xF53C6787) // MU_GarageVinylLayerV2.fng -- call the color picker when X is pressed!!!
						FEngSendMessageToPackage(0xC519BFC0, "MU_GarageVinylLayerV2.fng");
					else
						*(char*)FE_WINDOWS_KEY_CODE_ADDR = 'C'; // customize car
				}
				bXButtonOldState = wButtons & XINPUT_GAMEPAD_X;
			}

			if ((wButtons & XINPUT_GAMEPAD_Y))
				*(char*)FE_WINDOWS_KEY_CODE_ADDR = 'D'; // delete profile

			if (((wButtons & XINPUT_GAMEPAD_Y) != bZButtonOldState) && (FEHashUpper((char*)CURRENT_MENUPKG_ADDR) != 0xFD6DFFB3)) // except in MU_UG_NewOrLoad_PC2.fng 
			{
				if ((wButtons & XINPUT_GAMEPAD_Y)) // trigger once only on button down state
					*(char*)FE_WINDOWS_KEY_CODE_ADDR = 'Z'; // reset to default, decal color, etc...
				bZButtonOldState = (wButtons & XINPUT_GAMEPAD_Y);
			}

			//if ((wButtons & XINPUT_GAMEPAD_B))
			//	*(char*)FE_WINDOWS_KEY_CODE_ADDR = 0x1B; // escape

			if ((g_Controllers[0].state.Gamepad.bLeftTrigger > TRIGGER_ACTIVATION_THRESHOLD))
				*(char*)FE_WINDOWS_KEY_CODE_ADDR = 'T'; // tutorial
		}
		if ((wButtons & XINPUT_GAMEPAD_BACK) != bQuitButtonOldState)
		{
			if ((wButtons & XINPUT_GAMEPAD_BACK)) // trigger once only on button down state
				*(char*)FE_WINDOWS_KEY_CODE_ADDR = 'Q';
			bQuitButtonOldState = (wButtons & XINPUT_GAMEPAD_BACK);
		}
	}
	// controller 2 - debug camera and other stuff
	if (g_Controllers[1].bConnected)
	{
		WORD wButtons = g_Controllers[1].state.Gamepad.wButtons;
		b2[NFSUJOY_DEBUGCAM_ACTIVATE] = b2[NFSUJOY_DEBUGCAM_ACTIVATE] ? !(wButtons & XINPUT_GAMEPAD_BACK) : b2[NFSUJOY_DEBUGCAM_ACTIVATE];
		b2[NFSUJOY_DEBUGCAM_MOVEFORWARD] = b2[NFSUJOY_DEBUGCAM_MOVEFORWARD] ? !(wButtons & XINPUT_GAMEPAD_A) : b2[NFSUJOY_DEBUGCAM_MOVEFORWARD];
		b2[NFSUJOY_DEBUGCAM_MOVEBACKWARD] = b2[NFSUJOY_DEBUGCAM_MOVEBACKWARD] ? !(wButtons & XINPUT_GAMEPAD_Y) : b2[NFSUJOY_DEBUGCAM_MOVEBACKWARD];
		b2[NFSUJOY_DEBUGCAM_MOVELEFT] = b2[NFSUJOY_DEBUGCAM_MOVELEFT] ? !(wButtons & XINPUT_GAMEPAD_X) : b2[NFSUJOY_DEBUGCAM_MOVELEFT];
		b2[NFSUJOY_DEBUGCAM_MOVERIGHT] = b2[NFSUJOY_DEBUGCAM_MOVERIGHT] ? !(wButtons & XINPUT_GAMEPAD_B) : b2[NFSUJOY_DEBUGCAM_MOVERIGHT];
		b2[NFSUJOY_DEBUGCAM_MOVEUP] = b2[NFSUJOY_DEBUGCAM_MOVEUP] ? !(wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) : b2[NFSUJOY_DEBUGCAM_MOVEUP];
		b2[NFSUJOY_DEBUGCAM_MOVEDOWN] = b2[NFSUJOY_DEBUGCAM_MOVEDOWN] ? !(g_Controllers[1].state.Gamepad.bLeftTrigger > TRIGGER_ACTIVATION_THRESHOLD) : b2[NFSUJOY_DEBUGCAM_MOVEDOWN];
		b2[NFSUJOY_DEBUGCAM_LOOKUP] = b2[NFSUJOY_DEBUGCAM_LOOKUP] ? !(wButtons & XINPUT_GAMEPAD_DPAD_UP) : b2[NFSUJOY_DEBUGCAM_LOOKUP];
		b2[NFSUJOY_DEBUGCAM_LOOKDOWN] = b2[NFSUJOY_DEBUGCAM_LOOKDOWN] ? !(wButtons & XINPUT_GAMEPAD_DPAD_DOWN) : b2[NFSUJOY_DEBUGCAM_LOOKDOWN];
		b2[NFSUJOY_DEBUGCAM_LOOKLEFT] = b2[NFSUJOY_DEBUGCAM_LOOKLEFT] ? !(wButtons & XINPUT_GAMEPAD_DPAD_LEFT) : b2[NFSUJOY_DEBUGCAM_LOOKLEFT];
		b2[NFSUJOY_DEBUGCAM_LOOKRIGHT] = b2[NFSUJOY_DEBUGCAM_LOOKRIGHT] ? !(wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) : b2[NFSUJOY_DEBUGCAM_LOOKRIGHT];
		b2[NFSUJOY_DEBUGCAM_TURBO] = b2[NFSUJOY_DEBUGCAM_TURBO] ? !(wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) : b2[NFSUJOY_DEBUGCAM_TURBO];
		b2[NFSUJOY_DEBUGCAM_SUPERTURBO] = b2[NFSUJOY_DEBUGCAM_SUPERTURBO] ? !(g_Controllers[1].state.Gamepad.bRightTrigger > TRIGGER_ACTIVATION_THRESHOLD) : b2[NFSUJOY_DEBUGCAM_SUPERTURBO];
	}

	// write bits back
	*(int*)JOYBUTTONS1_ADDR = b1.to_ulong();
	*(int*)JOYBUTTONS2_ADDR = b2.to_ulong();
}

void __stdcall ReadControllerData()
{
	UpdateControllerState();
	ReadXInput();
}

// entrypoint: 0x004065AD
unsigned int JoyEventCaveExit = 0x004065B6;
unsigned int SavedObj = 0;
void __declspec(naked) JoyEventCave()
{
	_asm mov SavedObj, esi

	ReadControllerData();

	_asm
	{
		mov esi, SavedObj
		mov eax, [esi+8]
		mov edi, ds:JOYBUTTONS1_ADDR
		jmp JoyEventCaveExit
	}
}

void InitConfig()
{
	CIniReader inireader("");

	ControllerIconMode = inireader.ReadInteger("Icons", "ControllerIconMode", 0);
	LastControlledDevice = inireader.ReadInteger("Icons", "FirstControlDevice", 0);
}

int Init()
{
	// kill DInput8 joypad reading & event generation -- TODO: try to kill dinput from ever being created if possible
	injector::MakeCALL(0x0040A7B5, DummyFunc, true);
	injector::MakeCALL(0x00401921, DummyFunc, true);
	injector::MakeCALL(0x0040164C, DummyFunc, true);
	injector::MakeJMP(0x004071B8, 0x004075D1, true);
	// hook the reading for XInput
	injector::MakeJMP(0x004065AD, JoyEventCave, true);
	// hook for car orbiting with right stick (hooking mouse inputs) + bugfix (adding a slight delay to give time for the actual vars to be read and updated)
	injector::MakeJMP(0x004B0CB9, FECarOrbitCave, true);
	injector::WriteMemory<int>(0x004B0CE7, (int)&CarOrbitDivisor, true);
	injector::WriteMemory<int>(0x004B0D17, (int)&CarOrbitDivisor, true);
	// hook FEng globally in FEPkgMgr_SendMessageToPackage
	injector::MakeJMP(0x004F7C08, FEngGlobalCave, true);
	// snoop last activated FEng package
	injector::MakeCALL(0x004F3BC3, SnoopLastFEPackage, true);

	// this kills DInput enumeration COMPLETELY -- even the keyboard
	//injector::MakeJMP(0x00405695, 0x004056AA, true);
	// kill DInput8 enumeration for joypads only
	injector::MakeJMP(0x00418E4B, 0x00418E68, true);

	// dereference the current WndProc from the game executable and write to the function pointer (to maximize compatibility)
	GameWndProcAddr = *(unsigned int*)0x4088FC;
	GameWndProc = (LRESULT(WINAPI*)(HWND, UINT, WPARAM, LPARAM))GameWndProcAddr;
	injector::WriteMemory<unsigned int>(0x4088FC, (unsigned int)&CustomWndProc, true);

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

