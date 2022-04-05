# NFS Underground - Xtended Input

This is a plugin for NFS Underground which brings XInput support to the game!

Currently a work in progress, but fully usable and playable!

Planned to bring to other NFS games in the future!

## Features

- Automatic texture and text swapping for button icons - dynamically detects when you use either a keyboard or a controller (Disable Widescreen Fix's button icons to see the effect)

- Choice between Xbox and PS4 icons (from WS Fix)

- Player 2 controls restored

- Console control feature parity (except rumble): restored debug camera analog control

- Access to some leftover features from Hot Pursuit 2

- Re-done keyboard input code - not using DInput8 anymore. Now talks directly over Win32 (GetAsyncKeyState, GetKeyboardState or raw input)

## Button mappings

You may remap the buttons in the configuration file (NFSU_XtendedInput.ini), however as of now there is 1 limitation: you may only assign 1 button to an event (and vice versa, 1 event to a button). Refer to the EventReference.txt file to see what's available.

Default controller button mappings are:

### Ingame

- Throttle: Right Trigger

- Brake: Left Trigger

- Steering: Left Stick X axis

- E-Brake: A button

- NOS: B button

- Look back: X button

- Change camera: Y button

- Shift up/down: Right Stick Y axis up/down

- Pause/Unpause: Menu/Start

- Quit game: Windows/Back button

- Cycle HUD: D-Pad Up

- Zone Slowmo (HP2): D-Pad Down

- Zone Freeze (HP2): Left shoulder (L1)

- Zone Preview (HP2): Right shoulder (R1)

- Car honk (missing playback code): Left Stick (L3)

### FrontEnd

Please note that some menus may not advertise their functions properly, but they should be fully functional and controllable like in the console versions.

- Navigation: D-Pad & Left Analog Stick

- Accept: A button

- Back: B button

- Left shoulder & Right shoulder buttons: mapped to their respective equivalents

- L2: Left Trigger

- R2: Right Trigger

- Car orbit: Right Stick

- Button 0: Y

- Button 1: X (used for accessing submenus)

- Help: Right Stick (R3)

- Quit game: Windows/Back button

### Debug camera

You must use a controller on the second port to invoke it and use it

- Activation: Windows/Back

- Move forward: A button

- Move backward: Y button

- Move left: X button

- Move right: B button

- Move up: Left Shoulder

- Move down: Left Trigger

- Look up/down/left/right: D-Pad

- Turbo: Right Shoulder

- Super Turbo: Right Trigger

- Movement (analog): Left Stick X/Y

- Look (analog): Right stick X/Y

## Keyboard controls

They're slightly changed from default.

Also the second player can be controlled with another keyboard (raw input mode only).

The first used keyboard will be assigned to the first player (and subsequently the second one to the second player)

By default it'll use GetAsyncKeyState (unbuffered) to read keys which might cause some issues with other hooks to the WndProc of this game, but will deliver the most optimal performance. A key's status is being read as it's being scanned in this mode.

Second mode is GetKeyboardState (buffered). This will store all keys in a buffer and read them during ActualReadJoystickData (which starts right before eDisplayFrame). Use this if you have some issues.

Third mode is raw input reading. This is read during WM_INPUT. It currently exhibits mediocre performance due to the implementation, but allows for differentiating between different keyboards. Use this if you really need it.

### Ingame

- Throttle: Up

- Brake: Down

- Steering: Left/Right

- E-Brake: Space

- NOS: Left Alt

- Look back: X

- Change camera: C

- Shift up/down: Right Shift / Right Control

- Pause: Escape

- Quit game: Q

- Zone Freeze (HP2): Page Down

- Zone Preview (HP2): Delete

- Car honk (missing playback code): H

### FrontEnd

- Navigation: Navigation keys (U/D/L/R)

- Accept: Enter

- Back: Escape

- Left shoulder & Right shoulder buttons: 9 & 0

- L2: O

- R2: P

- Car orbit: WASD

- Button 0: G

- Button 1: F (used for accessing submenus)

- Help: H

- Quit game: Q

### Debug camera

- Activation: M

- Move forward: W

- Move backward: S

- Move left: A

- Move right: D

- Move up: Space

- Move down: Left Control

- Look up: I

- Look down: K

- Look left: J

- Look right: L

- Turbo: Right Shift

- Super Turbo: F

## TODO list:

- Check the top of the source file

- Currently KILLS Direct Input, beware

# Credits

- ThirteenAG & AeroWidescreen - for the great button icons!

- LINK/2012 - injector
