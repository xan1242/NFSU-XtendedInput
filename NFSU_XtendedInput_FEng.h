#pragma once
#define GAMEFLOWMANAGER_STATUS_ADDR 0x0077A920
#define FEMOUSECURSOR_BUTTONPRESS_ADDR 0x007064B0
#define FEMOUSECURSOR_CARORBIT_X_ADDR 0x007064A4
#define FEMOUSECURSOR_CARORBIT_Y_ADDR 0x007064A8

#define CFENG_PINSTANCE_ADDR 0x0073578C

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

#define LASTCONTROLLED_KB 0
#define LASTCONTROLLED_CONTROLLER 1
unsigned int LastControlledDevice = 0; // 0 = keyboard, 1 = controller
unsigned int LastControlledDeviceOldState = 0;

#define CONTROLLERICON_XBOXONE 0
#define CONTROLLERICON_PS4 1
unsigned int ControllerIconMode = 0; // 0 = Xbox (One and later only for now), 1 = PlayStation (4 only now) -- planned to add: Nintendo (Wii/U Classic Controller, Switch), PS3/PS2, Xbox 360

bool bLoadedConsoleButtonTex = false;
char LastFEngPackage[128];
char CurrentSplashText[64];

void* (*CreateResourceFile)(char* filename, int ResType, int unk1, int unk2, int unk3) = (void* (*)(char*, int, int, int, int))0x004482F0;
void(*ServiceResourceLoading)() = (void(*)())0x004483C0;
unsigned int(*GetTextureInfo)(unsigned int hash, int unk, int unk2) = (unsigned int(*)(unsigned int, int, int))0x005461C0;
void(__thiscall* _FEngSetColor)(void* FEObject, unsigned int color) = (void(__thiscall*)(void*, unsigned int))0x004F75B0;
void(__thiscall* OptionsSelectorMenu_NotificationMessage)(void* UndergroundBriefScreen, unsigned int msg, void* FEObject, unsigned int unk2, unsigned int unk3) = (void(__thiscall*)(void*, unsigned int, void*, unsigned int, unsigned int))0x004D4610;


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

unsigned int FEngSetButtonState_Addr = 0x004F5F80;
void __stdcall FEngSetButtonState(const char* pkgname, unsigned int hash, unsigned int state)
{
	int cfeng_instance = *(int*)CFENG_PINSTANCE_ADDR;
	_asm
	{
		push state
		mov edi, hash
		mov eax, pkgname
		push cfeng_instance
		call FEngSetButtonState_Addr
		//add esp, 4
	}
}

#pragma runtime_checks( "", off )
void __stdcall FEngSetColor(void* FEObject, unsigned int color)
{
	_FEngSetColor(FEObject, color);
	_asm add esp, 4
}
#pragma runtime_checks( "", restore )

// we need to disable controller options menu because they crash the game 
// (and currently are unusable anyways -- this will require extensive research to hook into the joypad config properly, for now we just stick to an ini config file)
void DisableControllerOptions()
{
	FEngSetButtonState("MU_OptionsSelector_PC.fng", FEHashUpper("Controller"), 0);
	FEngSetColor((void*)FEngFindObject("MU_OptionsSelector_PC.fng", FEHashUpper("Controller")), 0xFF404040);
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
		FEngSetTextureHash(FEImage, XBOXX_HASH);
	//if (FEImage = FEngFindImage(pkgname, 0xAD7303C0)) // CreateGame_hotkey
	//	FEngSetTextureHash(FEImage, XBOXY_HASH);
	if (FEImage = FEngFindImage(pkgname, 0xC8754FA4)) // Performance_hotkey
		FEngSetTextureHash(FEImage, XBOXX_HASH);
	if (FEImage = FEngFindImage(pkgname, 0xA16A1877)) // Changecolor_hotkey -- during car color changer
		FEngSetTextureHash(FEImage, XBOXA_HASH);
	if (FEImage = FEngFindImage(pkgname, 0xBDD6AEEA)) // DecalColor_hotkey
		FEngSetTextureHash(FEImage, XBOXX_HASH);
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
		if (FEHashUpper(CurrentSplashText) != 0xD7027E29) // "Press A button"
			FEPrintf((void*)FEngFindString("LS_Splash_PC.fng", 0x13CF446D), "Press A button"); // mouse_click text object
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
		FEngSetTextureHash(FEImage, PSSQUARE_HASH);
	if (FEImage = FEngFindImage(pkgname, 0x83A65176)) // Stock_hotkey
		FEngSetTextureHash(FEImage, PSSQUARE_HASH);
	//if (FEImage = FEngFindImage(pkgname, 0xAD7303C0)) // CreateGame_hotkey
	//	FEngSetTextureHash(FEImage, PSTRIANGLE_HASH);
	if (FEImage = FEngFindImage(pkgname, 0xC8754FA4)) // Performance_hotkey
		FEngSetTextureHash(FEImage, PSSQUARE_HASH);
	if (FEImage = FEngFindImage(pkgname, 0xA16A1877)) // Changecolor_hotkey -- during car color changer
		FEngSetTextureHash(FEImage, PSCROSS_HASH);
	if (FEImage = FEngFindImage(pkgname, 0xBDD6AEEA)) // DecalColor_hotkey
		FEngSetTextureHash(FEImage, PSSQUARE_HASH);
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
		if (FEHashUpper(CurrentSplashText) != 0x10EC9E60) // "Press X button"
			FEPrintf((void*)FEngFindString("LS_Splash_PC.fng", 0x13CF446D), "Press X button"); // mouse_click text object
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
#pragma runtime_checks( "", off )
void __stdcall OptionsSelectorMenu_NotificationMessage_Hook(unsigned int unk1, void* FEObject, unsigned int unk2, unsigned int unk3)
{
	unsigned int thethis = 0;
	_asm mov thethis, ecx

	DisableControllerOptions();

	return OptionsSelectorMenu_NotificationMessage((void*)thethis, unk1, FEObject, unk2, unk3);
}
#pragma runtime_checks( "", restore )
