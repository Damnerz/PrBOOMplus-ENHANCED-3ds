# PrBoom+ Nintendo 3DS Port

## What is this?
This is more or less a straight port of PrBoom+ 2.6.66 to the Nintendo 3DS, with some extra added enhancements exclusive to the system.  
Just about everything you'd expect desktop PrBoom+ to do, this port should also be able to do.  
The only things it can't do are a few unsupported renderer features (which have been stripped out anyway) and networking features.  
In addition, all video modes apart from 8-bit are supported. By default, the GPU-accelerated OpenGL mode is enabled, though the other software-rendering modes are also available should you wish to fall back on them (so far, all modes produce roughly similar performance, with OpenGL being moderately faster)  

## Features
- PrBoom+ 2.6.66
- Features both GPU-accelerated OpenGL mode, and software-renderer modes
- Great performance on New 3DS, decent-ish performance on Old 3DS
- Good quality stereoscopic 3D
- Interchangeable touchscreen mouse and keyboard

## Setup
Before doing anything, [dump the DSP firm](https://github.com/zoogie/DSP1/releases) so that the sound will work.  
Download `prboom-plus-3DS.zip` and `prboom-plus.3dsx/cia` from the [latest release](https://github.com/Voxel9/PrBoom-Plus-3DS/releases/latest) page.  
Extract the ZIP to the root of the SD card. If you're planning to run the .3dsx version, copy `prboom-plus.3dsx` to `/3ds/PrBoom-Plus`.  
If you're planning to run the .cia version, copy `prboom-plus.cia` to anywhere on the SD card, then install it using [FBI](https://github.com/Steveice10/FBI).  
Obtain the WAD for the game you want to run and also copy it to `/3ds/PrBoom-Plus/`. **Only have one game WAD in this folder at a time.**  
Open the Homebrew Launcher (.3dsx) or HOME Menu (.cia) on your system and select PrBoom+ to launch the game.  

### PWADs/Dehacked files
If you have any PWADs/DEHs that you'd like to apply to the game, run the game at least once to create `prboom-plus.cfg`, then quit the game.  
Back on PC, copy your PWADs/DEHs over to `/3ds/PrBoom-Plus`, then open the newly-created config file in a text editor.  
Fill out the exact filenames of the PWADs/DEHs in the `wadfile_X` and `dehfile_X` config entries, including the file extension.  
Save the config and run the game again. The mods should now take effect, assuming PrBoom+ supports them.  

### A note about MIDI music playback
Thanks to SDL Mixer, the music is played back on the incredibly old Timidity backend, using instruments from the eawpats package (which is bundled with the release for convenience sake).  
Currently, I can't get the vanilla instruments we're all familiar with to play out-of-the-box, and without resorting to external MIDI instruments, so we're stuck with this for now until a different backend is implemented.  

### Configuring input
You can remap any of the joystick inputs via the `joyb_XXX` config entries, as follows:  

| 3DS button | ID |
| ---------- | -- |
| A          | 0  |
| B          | 1  |
| X          | 2  |
| Y          | 3  |
| L          | 4  |
| R          | 5  |
| START      | 6  |
| SELECT     | 7  |
| DPad up    | 8  |
| DPad down  | 9  |
| DPad left  | 10 |
| DPad right | 11 |
