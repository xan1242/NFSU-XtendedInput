#pragma once

int bStringHash(char* a1);

//
// Constants for gamepad buttons -- from XInput.h
//
#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000

// other gamepad stuff -- sticks & triggers
#define XINPUT_GAMEPAD_LT_CONFIGDEF			0xABC0
#define XINPUT_GAMEPAD_RT_CONFIGDEF			0xABC1
#define XINPUT_GAMEPAD_LS_X_CONFIGDEF		0xABC2
#define XINPUT_GAMEPAD_LS_Y_CONFIGDEF		0xABC3
#define XINPUT_GAMEPAD_RS_X_CONFIGDEF		0xABC4
#define XINPUT_GAMEPAD_RS_Y_CONFIGDEF		0xABC5
#define XINPUT_GAMEPAD_LS_UP_CONFIGDEF		0xABC7
#define XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF	0xABC8
#define XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF	0xABC9
#define XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF	0xABCA
#define XINPUT_GAMEPAD_RS_UP_CONFIGDEF		0xABCB
#define XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF	0xABCC
#define XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF	0xABCD
#define XINPUT_GAMEPAD_RS_RIGHT_CONFIGDEF	0xABCE
#define XINPUT_GAMEPAD_DPAD_CONFIGDEF		0xDAD0

// comparing strings by hash
#define XINPUT_GAMEPAD_DPAD_UP_HASH         0xE4BBAB51 
#define XINPUT_GAMEPAD_DPAD_DOWN_HASH       0x024A7864 
#define XINPUT_GAMEPAD_DPAD_LEFT_HASH       0x024EAEB7 
#define XINPUT_GAMEPAD_DPAD_RIGHT_HASH      0x4C934D2A 
#define XINPUT_GAMEPAD_START_HASH           0xB60DCFE2 
#define XINPUT_GAMEPAD_BACK_HASH            0xA0A172C5 
#define XINPUT_GAMEPAD_LEFT_THUMB_HASH      0xD89FD05E 
#define XINPUT_GAMEPAD_RIGHT_THUMB_HASH     0x4A112211 
#define XINPUT_GAMEPAD_LEFT_SHOULDER_HASH   0x80D7FD44 
#define XINPUT_GAMEPAD_RIGHT_SHOULDER_HASH  0x7575D617
#define XINPUT_GAMEPAD_LB_HASH				0x74BF0962
#define XINPUT_GAMEPAD_RB_HASH				0x74BF0A28
#define XINPUT_GAMEPAD_A_HASH               0xA6726515 
#define XINPUT_GAMEPAD_B_HASH               0xA6726516 
#define XINPUT_GAMEPAD_X_HASH               0xA672652C 
#define XINPUT_GAMEPAD_Y_HASH               0xA672652D 
#define XINPUT_GAMEPAD_LT_HASH              0x74BF0974
#define XINPUT_GAMEPAD_RT_HASH              0x74BF0A3A
#define XINPUT_GAMEPAD_LS_X_HASH            0xA0A73ECA
#define XINPUT_GAMEPAD_LS_Y_HASH            0xA0A73ECB
#define XINPUT_GAMEPAD_RS_X_HASH            0xA0AA8910
#define XINPUT_GAMEPAD_RS_Y_HASH            0xA0AA8911
#define XINPUT_GAMEPAD_DPAD_HASH            0xA0A2CB0D
#define XINPUT_GAMEPAD_LS_UP_HASH			0xB58F17F7
#define XINPUT_GAMEPAD_LS_DOWN_HASH			0x55ABA68A
#define XINPUT_GAMEPAD_LS_LEFT_HASH			0x55AFDCDD
#define XINPUT_GAMEPAD_LS_RIGHT_HASH		0x0C1A4010
#define XINPUT_GAMEPAD_RS_UP_HASH			0xB5FBAAFD
#define XINPUT_GAMEPAD_RS_DOWN_HASH			0x23891310
#define XINPUT_GAMEPAD_RS_LEFT_HASH			0x238D4963
#define XINPUT_GAMEPAD_RS_RIGHT_HASH		0x95A53D56


short int ConvertXInputNameToBitmask(char* InName)
{
	switch (bStringHash(InName))
	{
	case XINPUT_GAMEPAD_DPAD_UP_HASH:
		return XINPUT_GAMEPAD_DPAD_UP;
	case XINPUT_GAMEPAD_DPAD_DOWN_HASH:
		return XINPUT_GAMEPAD_DPAD_DOWN;
	case XINPUT_GAMEPAD_DPAD_LEFT_HASH:
		return XINPUT_GAMEPAD_DPAD_LEFT;
	case XINPUT_GAMEPAD_DPAD_RIGHT_HASH:
		return XINPUT_GAMEPAD_DPAD_RIGHT;
	case XINPUT_GAMEPAD_START_HASH:
		return XINPUT_GAMEPAD_START;
	case XINPUT_GAMEPAD_BACK_HASH:
		return XINPUT_GAMEPAD_BACK;
	case XINPUT_GAMEPAD_LEFT_THUMB_HASH:
		return XINPUT_GAMEPAD_LEFT_THUMB;
	case XINPUT_GAMEPAD_RIGHT_THUMB_HASH:
		return XINPUT_GAMEPAD_RIGHT_THUMB;
	case XINPUT_GAMEPAD_LB_HASH:
	case XINPUT_GAMEPAD_LEFT_SHOULDER_HASH:
		return XINPUT_GAMEPAD_LEFT_SHOULDER;
	case XINPUT_GAMEPAD_RB_HASH:
	case XINPUT_GAMEPAD_RIGHT_SHOULDER_HASH:
		return XINPUT_GAMEPAD_RIGHT_SHOULDER;
	case XINPUT_GAMEPAD_A_HASH:
		return XINPUT_GAMEPAD_A;
	case XINPUT_GAMEPAD_B_HASH:
		return XINPUT_GAMEPAD_B;
	case XINPUT_GAMEPAD_X_HASH:
		return XINPUT_GAMEPAD_X;
	case XINPUT_GAMEPAD_Y_HASH:
		return XINPUT_GAMEPAD_Y;

	default:
		break;
	}
	return 0;
}

int ConvertXInputOtherConfigDef(char* InName)
{
	switch (bStringHash(InName))
	{
		case XINPUT_GAMEPAD_LT_HASH:
			return XINPUT_GAMEPAD_LT_CONFIGDEF;
		case XINPUT_GAMEPAD_RT_HASH:
			return XINPUT_GAMEPAD_RT_CONFIGDEF;
		case XINPUT_GAMEPAD_LS_X_HASH:
			return XINPUT_GAMEPAD_LS_X_CONFIGDEF;
		case XINPUT_GAMEPAD_LS_Y_HASH:
			return XINPUT_GAMEPAD_LS_Y_CONFIGDEF;
		case XINPUT_GAMEPAD_RS_X_HASH:
			return XINPUT_GAMEPAD_RS_X_CONFIGDEF;
		case XINPUT_GAMEPAD_RS_Y_HASH:
			return XINPUT_GAMEPAD_RS_Y_CONFIGDEF;
		case XINPUT_GAMEPAD_DPAD_HASH:
			return XINPUT_GAMEPAD_DPAD_CONFIGDEF;
		case XINPUT_GAMEPAD_LS_UP_HASH:
			return XINPUT_GAMEPAD_LS_UP_CONFIGDEF;
		case XINPUT_GAMEPAD_LS_DOWN_HASH:
			return XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF;
		case XINPUT_GAMEPAD_LS_LEFT_HASH:
			return XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF;
		case XINPUT_GAMEPAD_LS_RIGHT_HASH:
			return XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF;
		case XINPUT_GAMEPAD_RS_UP_HASH:
			return XINPUT_GAMEPAD_RS_UP_CONFIGDEF;
		case XINPUT_GAMEPAD_RS_DOWN_HASH:
			return XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF;
		case XINPUT_GAMEPAD_RS_LEFT_HASH:
			return XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF;
		case XINPUT_GAMEPAD_RS_RIGHT_HASH:
			return XINPUT_GAMEPAD_RS_RIGHT_CONFIGDEF;
	}
	return 0;
}

char* ControlsTextsXBOX[] = { "A", "B", "X", "Y", "LB", "RB", "View (Select)", "Menu (Start)", "Left stick", "Right stick", "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right", "Right stick X", "Right stick Y", "Right stick Left", "Right stick Right" ,"Right stick Up", "Right stick Down", "Left stick X", "Left stick Y", "Left stick Left", "Left stick Right", "Left stick Up", "Left stick Down", "LT", "RT"};
//const char* ControlsTextsPS[] = { "Cross", "Circle", "Square", "Triangle", "L1", "R1", "Select", "Start", "L3", "R3", "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right", "Right stick Left/Right", "Right stick Up/Down", "Left stick Left/Right", "Left stick Up/Down", "L2 / R2", "D-pad" };

char* unkstr = "Unknown";

char* __stdcall ConvertBitmaskToString_XBOX(uint16_t in)
{
	switch (in)
	{
	case XINPUT_GAMEPAD_DPAD_UP:
		return ControlsTextsXBOX[10];
	case XINPUT_GAMEPAD_DPAD_DOWN:
		return ControlsTextsXBOX[11];
	case XINPUT_GAMEPAD_DPAD_LEFT:
		return ControlsTextsXBOX[12];
	case XINPUT_GAMEPAD_DPAD_RIGHT:
		return ControlsTextsXBOX[13];
	case XINPUT_GAMEPAD_START:
		return ControlsTextsXBOX[7];
	case XINPUT_GAMEPAD_BACK:
		return ControlsTextsXBOX[6];
	case XINPUT_GAMEPAD_LEFT_THUMB:
		return ControlsTextsXBOX[8];
	case XINPUT_GAMEPAD_RIGHT_THUMB:
		return ControlsTextsXBOX[9];
	case XINPUT_GAMEPAD_LEFT_SHOULDER:
		return ControlsTextsXBOX[4];
	case XINPUT_GAMEPAD_RIGHT_SHOULDER:
		return ControlsTextsXBOX[5];
	case XINPUT_GAMEPAD_A:
		return ControlsTextsXBOX[0];
	case XINPUT_GAMEPAD_B:
		return ControlsTextsXBOX[1];
	case XINPUT_GAMEPAD_X:
		return ControlsTextsXBOX[2];
	case XINPUT_GAMEPAD_Y:
		return ControlsTextsXBOX[3];

	case XINPUT_GAMEPAD_LT_CONFIGDEF:
		return ControlsTextsXBOX[27];
	case XINPUT_GAMEPAD_RT_CONFIGDEF:
		return ControlsTextsXBOX[26];
	case XINPUT_GAMEPAD_LS_X_CONFIGDEF:
		return ControlsTextsXBOX[20];
	case XINPUT_GAMEPAD_LS_Y_CONFIGDEF:
		return ControlsTextsXBOX[21];
	case XINPUT_GAMEPAD_RS_X_CONFIGDEF:
		return ControlsTextsXBOX[14];
	case XINPUT_GAMEPAD_RS_Y_CONFIGDEF:
		return ControlsTextsXBOX[15];
	case XINPUT_GAMEPAD_LS_UP_CONFIGDEF:
		return ControlsTextsXBOX[24];
	case XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF:
		return ControlsTextsXBOX[25];
	case XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF:
		return ControlsTextsXBOX[22];
	case XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF:
		return ControlsTextsXBOX[23];
	case XINPUT_GAMEPAD_RS_UP_CONFIGDEF:
		return ControlsTextsXBOX[18];
	case XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF:
		return ControlsTextsXBOX[19];
	case XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF:
		return ControlsTextsXBOX[16];
	case XINPUT_GAMEPAD_RS_RIGHT_CONFIGDEF:
		return ControlsTextsXBOX[17];

	default:
		break;
	}
	return unkstr;
}

