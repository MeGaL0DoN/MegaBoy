# MegaBoy

MegaBoy, a Gameboy/Color emulator written in C++, with GLFW/ImGui frontend.

![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![Supported Platforms](https://img.shields.io/badge/Platforms-Windows%20%7C%20Linux%20%7C%20MacOS%20%7C%20Web-blue.svg)

## Getting Started
1. Open the web version and try online [Here](https://megal0don.github.io/MegaBoy/), or download desktop build from [Releases](https://github.com/MeGaL0DoN/MegaBoy/releases)
2. Drag and Drop ROM file into the window, or select ROM file using File → Load
3. Play!
### Default Controls
<table style="width:100%">
  <tr>
    <td style="width:50%; vertical-align: top;">

| Gameboy | Key |
| --- | --- |
| A | <kbd>X</kbd> |
| B | <kbd>Z</kbd> |
| Up | <kbd>↑</kbd> |
| Down | <kbd>↓</kbd> |
| Left | <kbd>←</kbd> |
| Right | <kbd>→</kbd> |
| Start | <kbd>Enter</kbd> |
| Select | <kbd>Backspace</kbd> |

  </td>
  <td style="width:50%; vertical-align: top;">

| Emulator | Key |
| --- | --- |
| Pause | <kbd>Tab</kbd> |
| Restart | <kbd>R</kbd> |
| Fast Forward | <kbd>Space</kbd> |
| Screenshot | <kbd>T</kbd> |
| Save State | <kbd>Alt</kbd> + <kbd>1-9</kbd> |
| Load State | <kbd>Shift</kbd> + <kbd>1-9</kbd> |
| Quick Save | <kbd>Q</kbd> |
| Quick Load | <kbd>`</kbd> |

  </td>
  </tr>
</table>

#### All controls can be rebinded in the settings!

## Screenshots
| ![Screenshot 2025-01-08 012833](https://github.com/user-attachments/assets/f31cda15-81cb-424b-b828-cc05c75e4106) | ![Screenshot 2025-01-08 012507](https://github.com/user-attachments/assets/5f07df43-75d5-408c-8884-8f394b398f8e) | ![Screenshot 2025-01-08 011831](https://github.com/user-attachments/assets/e464a0ac-79dd-41eb-814b-1f6688c3026b) |
|---------------------------------|---------------------------------|---------------------------------|

![Screenshot 2025-01-09 144741](https://github.com/user-attachments/assets/8e8d9467-0805-494b-b009-98875f278aa4) | ![Screenshot 2025-01-09 144125](https://github.com/user-attachments/assets/fe74018d-4d1f-4743-a989-2b3a2523adc4) | ![Screenshot 2025-01-09 144547](https://github.com/user-attachments/assets/625658cb-649a-405e-96bc-83c1ff13ebea) |
|---------------------------------|---------------------------------|---------------------------------|

| ![Screenshot 2025-01-08 013240](https://github.com/user-attachments/assets/7901d9cd-dff7-48d7-89c8-64dbfead17b1) | ![Screenshot 2025-01-09 145157](https://github.com/user-attachments/assets/55fbd527-2a0d-46f6-b0b1-61fb4a2bc0ca) | ![Screenshot 2025-01-08 155855](https://github.com/user-attachments/assets/5cd47cd3-7b11-4938-b9f7-361f807c777b) | 
|---------------------------------|---------------------------------|---------------------------------|

#### Debugger
![Screenshot 2025-01-08 010444](https://github.com/user-attachments/assets/18830ce0-81b3-46eb-a5ec-385ae245e27f)

## Features
* M cycle accurate Sharp LR35902 CPU emulation.
* Pixel FIFO PPU emulation.
* Experimental Audio emulation (can have issues).
* Gameboy Color support.
* Mappers: MBC1, MBC2, MBC3, MBC5, HuC1.
* MBC3 Real Time Clock.
* Battery saves (.sav) support, compatible with other emulators like SameBoy.
* Save states (.mbs) support, allowing to continue any game from the exact spot.
* Web version also saves battery and save states persistently, so you won't lose your progress.
* GUI with minimal debugger.
* Boot ROM support - drag and drop or open file 'dmg_boot.bin' ('cgb_boot.bin' for gameboy color games).
* Game Genie and Game Shark cheats (Emulation → Enter Cheat).
* Can load zipped ROMs.
* Can take game screenshots (Saved in screenshots folder near emulator executable).
* Fast-forward.
* Shaders (currrently coming with LCD and Upscaling).
* Palette selection for DMG.
  
## Compatibility
| ![Screenshot 2025-01-08 014016](https://github.com/user-attachments/assets/bd0d8932-5f5b-4e38-bc1d-b849f81acf2b) | ![Screenshot 2025-01-09 005545](https://github.com/user-attachments/assets/4173ddd5-518c-48c6-90a3-0615859c3196) | ![Screenshot 2025-01-08 014037](https://github.com/user-attachments/assets/1a5f50c6-d23b-43aa-b479-57820ef6cac5) | 
|---------------------------------|---------------------------------|---------------------------------|

![Screenshot 2025-01-08 021836](https://github.com/user-attachments/assets/be9d8340-4792-4a98-a3f3-93040f72aaf2) | ![Screenshot 2025-01-08 020747](https://github.com/user-attachments/assets/83abbd8f-c890-4e9e-964c-2259a98c9d12) | ![Screenshot 2025-01-08 014125](https://github.com/user-attachments/assets/dfbd8117-151c-40e8-8490-423208cbd3fe) |
|---------------------------------|---------------------------------|---------------------------------|

Although complete accuracy is not a goal for this emulator (it doesn't pass all Mooneye test ROMs), I still tried to make it reasonably accurate by emulating CPU at cycle level, and emulating PPU's internal pipeline. This allows it to play games like Pinball Deluxe and Prehistorik Man, known to have issues in many other emulators.

I tested many games while developing this emulator, and as of now, all games I tried work correctly, aside from some audio glitches in some games, which I hope to fix in future updates. If you find a game that doesn’t work, feel free to open an issue, and I will look into it.

## Building
This project uses CMake as build system, allowing it to be build for different platforms without issues.
1. Use ```git clone --recursive https://github.com/MeGaL0DoN/MegaBoy``` to clone the project. Note: regular clone or GitHub "Download ZIP" option won't work, as project includes submodules.
2. After cloning the repository, make sure that you have CMake installed, and then you can either:
   * Go inside the project directory and run the CMake command line tool, or
   * Open and build the project in an IDE that supports CMake (such as Visual Studio or CLion).
3. Note: to build for the web, you need the [Emscripten](https://emscripten.org/) toolchain installed, and must configure CMake to use it.

## Upcoming Features
#### Planned Features
- [ ] More accurate audio emulation
- [ ] Add MBC6, MBC7, HuC-3
#### Maybe Features
- [ ] Online Multiplayer
- [ ] Mobile-Friendly web interface.

## Resources Used
### It wouldn't be possible to make MegaBoy without these resources:

* Pan Docs: https://gbdev.io/pandocs/
* Gbops opcode table: https://izik1.github.io/gbops/
* RGBDS CPU opcode reference: https://rgbds.gbdev.io/docs/v0.9.0/gbz80.7
* SM83 SingleStepTests by originaldave_: https://github.com/SingleStepTests/sm83
* NightShade's Sound Emulation: https://nightshade256.github.io/2021/03/27/gb-sound-emulation.html
* EmuDev discord server! https://discord.gg/dkmJAes
  
### Libraries
* [GLFW](https://github.com/glfw/glfw) - Window management and input.
* [glad](https://github.com/Dav1dde/glad) - Used to load OpenGL functions.
* [ImGui](https://github.com/ocornut/imgui) - User interface.
* [miniaudio](https://github.com/mackron/miniaudio) - Audio.
* [mINI](https://github.com/metayeti/mINI) - INI file writer.
* [miniz](https://github.com/richgel999/miniz) - Zip compression/decompression.
* [nativefiledialog-extended](https://github.com/richgel999/miniz) - Cross platform (desktop) file dialogs.
* [emscripten_browser_file](https://github.com/Armchair-Software/emscripten-browser-file) - File dialog for the web.
* [emscripten_browser_clipboard](https://github.com/Armchair-Software/emscripten-browser-clipboard) - Clipboard management for the web.

## License
This project is licensed under the MIT License - see the LICENSE.md file for details
