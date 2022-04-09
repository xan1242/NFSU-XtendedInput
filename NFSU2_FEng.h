#pragma once

#include <windows.h>
#include <timeapi.h>
#pragma comment(lib,"winmm.lib")
#define GAME_HWND_ADDR 0x00870990

// Win32 mouse API stuff
RECT windowRect;
bool bMousePressedDown = false;
bool bMousePressedDownOldState = false;

bool bMouse2PressedDown = false;
bool bMouse2PressedDownOldState = false;

bool bMouse3PressedDown = false;
bool bMouse3PressedDownOldState = false;

bool bConfineMouse = false;
int TimeSinceLastMouseMovement = 0;

#define MOUSEHIDE_TIME 5000
#define FEMOUSECURSOR_X_ADDR 0x008763D4
#define FEMOUSECURSOR_Y_ADDR 0x008763D8
#define FEMOUSECURSOR_BUTTONPRESS_ADDR 0x008763F1
#define FEMOUSECURSOR_BUTTONPRESS2_ADDR 0x008763F2
#define FEMOUSECURSOR_BUTTONPRESS3_ADDR 0x008763F3

#define FEMOUSECURSOR_BUTTON2PRESS_ADDR 0x008763F7
#define FEMOUSECURSOR_BUTTON2PRESS2_ADDR 0x008763F8
#define FEMOUSECURSOR_BUTTON2PRESS3_ADDR 0x008763F9

#define FEMOUSECURSOR_BUTTON3PRESS_ADDR 0x008763F4
#define FEMOUSECURSOR_BUTTON3PRESS2_ADDR 0x008763F5
#define FEMOUSECURSOR_BUTTON3PRESS3_ADDR 0x008763F6

#define FEMOUSECURSOR_CARORBIT_X_ADDR 0x008763E4
#define FEMOUSECURSOR_CARORBIT_Y_ADDR 0x008763E8
#define FEMOUSE_MOUSEWHEEL_ADDR 0x008763EC

void UpdateFECursorPos()
{
	bool bMouseInGameWindow = false;
	POINT MousePos;
	GetCursorPos(&MousePos);
	GetWindowRect(*(HWND*)GAME_HWND_ADDR, &windowRect);

	float ratio = 480.0 / (windowRect.bottom - windowRect.top); // scaling it to 480 height since that's what FE wants

	if ((MousePos.x >= windowRect.left) || (MousePos.x <= windowRect.right) && (MousePos.y >= windowRect.top) || (MousePos.y <= windowRect.bottom))
		bMouseInGameWindow = true;

	MousePos.x = MousePos.x - windowRect.left;
	MousePos.y = MousePos.y - windowRect.top;

	MousePos.x = (int)((float)(MousePos.x) * ratio);
	MousePos.y = (int)((float)(MousePos.y) * ratio);


	// car orbiting position calculation - always relative to old
	if (bMouse2PressedDown)
	{
		*(int*)FEMOUSECURSOR_CARORBIT_X_ADDR = MousePos.x - *(int*)FEMOUSECURSOR_X_ADDR;
		*(int*)FEMOUSECURSOR_CARORBIT_Y_ADDR = MousePos.y - *(int*)FEMOUSECURSOR_Y_ADDR;
	}

	if (!bMouseInGameWindow)
		SetCursor(LoadCursor(NULL, IDC_ARROW));

	// get time since last movement and hide it after a while (unless the cursor is within the game window so we don't hide it)
	if ((MousePos.x != *(int*)FEMOUSECURSOR_X_ADDR) || (MousePos.y != *(int*)FEMOUSECURSOR_Y_ADDR))
	{
		TimeSinceLastMouseMovement = timeGetTime();
		SetCursor(LoadCursor(NULL, IDC_ARROW));
	}
	else
	{
		if ((TimeSinceLastMouseMovement + MOUSEHIDE_TIME) < timeGetTime())
			SetCursor(NULL);
	}

	*(int*)FEMOUSECURSOR_X_ADDR = MousePos.x;
	*(int*)FEMOUSECURSOR_Y_ADDR = MousePos.y;

	// track mouse click state - make sure it lasts for exactly 1 tick of a loop, because the game is a little overzelaous with reading this input
	if (bMousePressedDown != bMousePressedDownOldState)
	{
		*(bool*)FEMOUSECURSOR_BUTTONPRESS_ADDR = bMousePressedDown;
		*(bool*)FEMOUSECURSOR_BUTTONPRESS2_ADDR = bMousePressedDown;
		*(bool*)FEMOUSECURSOR_BUTTONPRESS3_ADDR = !bMousePressedDown;
		bMousePressedDownOldState = bMousePressedDown;
	}
	else
	{
		//*(bool*)FEMOUSECURSOR_BUTTONPRESS_ADDR = false; // except this one, it's used for car orbiting
		*(bool*)FEMOUSECURSOR_BUTTONPRESS2_ADDR = false;
		*(bool*)FEMOUSECURSOR_BUTTONPRESS3_ADDR = false;
	}

	// track mouse click state - make sure it lasts for exactly 1 tick of a loop, because the game is a little overzelaous with reading this input
	if (bMouse2PressedDown != bMouse2PressedDownOldState)
	{
		*(bool*)FEMOUSECURSOR_BUTTON2PRESS_ADDR = bMouse2PressedDown;
		*(bool*)FEMOUSECURSOR_BUTTON2PRESS2_ADDR = bMouse2PressedDown;
		*(bool*)FEMOUSECURSOR_BUTTON2PRESS3_ADDR = !bMouse2PressedDown;
		bMouse2PressedDownOldState = bMouse2PressedDown;
	}
	else
	{
		*(bool*)FEMOUSECURSOR_BUTTON2PRESS2_ADDR = false;
		*(bool*)FEMOUSECURSOR_BUTTON2PRESS3_ADDR = false;
	}
	// track mouse click state - make sure it lasts for exactly 1 tick of a loop, because the game is a little overzelaous with reading this input
	if (bMouse3PressedDown != bMouse3PressedDownOldState)
	{
		*(bool*)FEMOUSECURSOR_BUTTON3PRESS_ADDR = bMouse3PressedDown;
		*(bool*)FEMOUSECURSOR_BUTTON3PRESS2_ADDR = bMouse3PressedDown;
		*(bool*)FEMOUSECURSOR_BUTTON3PRESS3_ADDR = !bMouse3PressedDown;
		bMouse3PressedDownOldState = bMouse3PressedDown;
	}
	else
	{		
		*(bool*)FEMOUSECURSOR_BUTTON3PRESS2_ADDR = false;
		*(bool*)FEMOUSECURSOR_BUTTON3PRESS3_ADDR = false;
	}
}