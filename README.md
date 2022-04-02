# NFS Underground - Xtended Input

This is an plugin for NFS Underground which brings XInput support to the game.

Currently work in progress, but fully usable and playable!

Planned to bring to other NFS games in the future!

## Features

- Automatic texture and text swapping for button icons - dynamically detects when you use either a keyboard or a controller (Disable Widescreen Fix's button icons to see the effect)

- Choice between Xbox and PS4 icons (from WS Fix)

## Button mappings

They are currently fixed and can't be changed without recompiling the code, but it should suit most, if not all players.

### Ingame

- Throttle: Right Trigger

- Brake: Left Trigger

- Steering: Left Stick X axis

- Digital steering: D-Pad Left & Right

- E-Brake: A button

- NOS: B button

- Look back: X button

- Change camera: Y button

- Shift up/down: Right Stick Y axis up/down

- Pause: Menu/Start

- Quit game: Windows/Back button

### FrontEnd

- Navigation: D-Pad & Left Analog Stick

- Accept: A button

- Back: B button

- Left shoulder & Right shoulder buttons: mapped to their respective equivalents

- L2: Left Trigger

- R2: Right Trigger

- Quit game: Windows/Back button

Some extra notes about FrontEnd: PC version has had some changes to console navigation and complicated some things

- Vinyl color changing: X button

This button is NOT marked anywhere on the screen because the menu has to normally be invoked by the mouse.

### Debug camera

Currently does NOT support analog inputs. You can only use digital inputs. You must use a controller on the second port to invoke it and use it.

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

## TODO list:

- Check the top of the source file, but to sum it up - lower level and more direct access to the game would be nice!

- Currently KILLS Direct Input, beware

# Credits

- ThirteenAG & AeroWidescreen - for the great button icons!

- LINK/2012 - injector
