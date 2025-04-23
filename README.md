# MegaBoy

MegaBoy, a cross-platform accurate Gameboy/Color emulator made in C++

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

#### All controls are remappable in the settings!

## Screenshots
| ![Screenshot 2025-04-03 175455](https://github.com/user-attachments/assets/d307bfd5-ebf2-49d9-a945-5f67a69f233c) | ![Screenshot 2025-04-04 003156](https://github.com/user-attachments/assets/a3dd0f43-f0e7-4260-9330-09cda38da140) | ![Screenshot 2025-04-03 183113](https://github.com/user-attachments/assets/80563fd5-c9fa-438d-973f-aa450e62c84b) |
|---------------------------------|---------------------------------|---------------------------------|

![Screenshot 2025-04-03 183545](https://github.com/user-attachments/assets/9b415629-56f8-419a-bb72-ddc6984fe823) | ![Screenshot 2025-04-03 183706](https://github.com/user-attachments/assets/d966dc12-cbd1-4f2d-82d1-264b941d5fa9) | ![Screenshot 2025-04-03 183856](https://github.com/user-attachments/assets/6f050bc9-5df0-4e43-b46b-0d1eeda2ad31) |
|---------------------------------|---------------------------------|---------------------------------|

| ![Screenshot 2025-04-03 184218](https://github.com/user-attachments/assets/b6f5e64b-a41a-411d-99b0-198a93dba4d8) | ![Screenshot 2025-04-03 184315](https://github.com/user-attachments/assets/eb502521-24d2-4132-997e-848d7e291c0b) | ![Screenshot 2025-04-03 184827](https://github.com/user-attachments/assets/c9acbb5f-ecd2-4f1c-86e9-11eaf0bf8b8d) | 
|---------------------------------|---------------------------------|---------------------------------|

#### Debugger
![Screenshot 2025-04-04 005603](https://github.com/user-attachments/assets/a5a117ef-ea74-4860-b7b5-e74eabf24cd3)

## Features
* Cycle-accurate SM83 CPU emulation
* Cycle-accurate PPU emulation
* Experimental audio emulation
* Built-in GUI with debugger
* Gameboy Color support
* Boot ROM support - simply drag and drop or open file `dmg_boot.bin` (`cgb_boot.bin` for gameboy color)
* Supports running DMG games in CGB mode (with unique palettes using `cgb_boot.bin`) and CGB games in DMG mode
* Mappers: MBC1, MBC2, MBC3, MBC5, MBC6, HuC1, HuC3
* MBC3 and HuC3 include Real-Time Clock
* Battery save support (`.sav` files), compatible with other emulators like SameBoy
* Save state support (`.mbs` files), allowing you to resume any game from the exact point you left off
* Persistent battery and save states even in the web version, so you won't lose your progress
* Game Genie and Game Shark cheats (Emulation → Enter Cheat)
* Can load zipped ROMs
* Can take game screenshots (Saved in `screenshots` folder near the emulator executable)
* Fast-forwarding
* Shader support (LCD, upscaling, GBC color correction); Configurable DMG palette
  
## Compatibility
| ![Screenshot 2025-04-04 003712](https://github.com/user-attachments/assets/43fa966d-8def-4a48-855b-51efbe87f061) | ![Screenshot 2025-04-04 003757](https://github.com/user-attachments/assets/82b58f71-9fea-4e0a-b39b-fb839538e9de) | ![Screenshot 2025-04-04 003901](https://github.com/user-attachments/assets/f5eed9aa-653a-4098-b7cc-27320583b977) | 
|---------------------------------|---------------------------------|---------------------------------|

![Screenshot 2025-04-04 004116](https://github.com/user-attachments/assets/8eceb952-b742-4f93-a5cd-1c5fbb582290) | ![Screenshot 2025-04-04 004548](https://github.com/user-attachments/assets/18b999f7-ea37-4775-80f7-e423e28f96d2) | ![Screenshot 2025-04-04 004931](https://github.com/user-attachments/assets/92bbecd1-bf45-4943-9989-d87139b05651) |
|---------------------------------|---------------------------------|---------------------------------|

Although complete accuracy was not a goal for this emulator, it's still very accurate (passing all mooneye-gb DMG/CGB tests except `intr_2_mode0_timing_sprites.gb`). This allows it to play games like Pinball Deluxe and Prehistorik Man, known to have issues in many other emulators.

I tested many games while developing this emulator, and as of now, all games I tried work correctly, aside from some audio glitches in a few games, which will be fixed in the future updates. If you find a game that doesn’t work, feel free to open an issue, and I will look into it.

## Building
This project uses CMake as build system, allowing it to be build for different platforms without issues.
1. Use ```git clone --recursive https://github.com/MeGaL0DoN/MegaBoy``` to clone the project. Note: regular clone or GitHub "Download ZIP" option won't work, as project includes submodules.
2. After cloning the repository, make sure that you have CMake installed, and then you can either:
   * Go inside the project directory and run the CMake command line tool, or
   * Open and build the project in an IDE that supports CMake (such as Visual Studio or CLion).
3. Note: to build for the web, you need the [Emscripten](https://emscripten.org/) toolchain installed, and must configure CMake to use it.

## Upcoming Features
#### Planned Features
- [ ] Improve accuracy of the audio emulation
- [ ] Add ~~MBC6~~, MBC7, ~~HuC-3~~
- [ ] Add gamepad support
#### Maybe Features
- [ ] Super Game Boy
- [ ] Online Multiplayer
- [ ] Mobile-Friendly web interface

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
