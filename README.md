# Need for Speed (Black Box) - Xtended Input

This is a plugin for NFS Underground & Underground 2 which brings XInput support to the games!

Currently a work in progress, but fully usable and playable!

Planned to bring to other NFS games in the future!

## Features

- Automatic texture and text swapping for button icons - dynamically detects when you use either a keyboard or a controller (Disable Widescreen Fix's button icons to see the effect)

- Choice between Xbox and PS4 icons (from WS Fix)

- Player 2 controls restored

- Console control feature parity (except rumble): restored debug camera analog control

- Access to some leftover features from Hot Pursuit 2

- Re-done keyboard input code - not using DInput8 anymore. Now talks directly over Win32 (GetAsyncKeyState, GetKeyboardState or raw input)

- (UG2 only) Re-done mouse input - also using Win32 for mouse input (with auto hiding after 5 seconds)

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

- Car honk (missing playback code): Left Stick (L3)

Underground only:

- Cycle HUD: D-Pad Up

- Zone Slowmo (HP2): D-Pad Down

- Zone Freeze (HP2): Left shoulder (L1)

- Zone Preview (HP2): Right shoulder (R1)

Underground 2 only:

- SMS/Map/Status/Engage Event: D-Pad

- Car Hydraulics activate: Left Stick click (L3)

- Car Hydraulics control: Right Stick X/Y

- Car Bounce activate: Right Stick click (R3)

- Car Bounce control: Left Stick X/Y

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

- Drop car: Start

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

- Car honk (missing playback code): H

Underground only:

- Zone Freeze (HP2): Page Down

- Zone Preview (HP2): Delete

Underground 2:

- SMS: Tab

- Map: M

- Career progress: 1

- Engage event: Enter

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

- Activation (UG1): M

- Activation (UG2): Backspace

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

- Drop car: 5

## TODO list:

- Check the top of the source file

- Currently KILLS Direct Input, beware

# Credits

- ThirteenAG & AeroWidescreen - for the great button icons!

- LINK/2012 - injector
