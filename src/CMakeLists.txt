cmake_minimum_required(VERSION 3.27)
project(MegaBoy)

set(CMAKE_CXX_STANDARD 20)

add_executable(MegaBoy
        main.cpp
        appConfig.cpp
        appConfig.h
        APU.cpp
        APU.h
        squareWave.cpp
        squareWave.h
        bitOps.h
        Cartridge.cpp
        Cartridge.h
        CPU.cpp
        CPU.h
        CPU_interrupts.cpp
        debugUI.cpp
        debugUI.h
        GBCore.cpp
        GBCore.h
        GBMultiplayer.cpp
        GBMultiplayer.h
        gbSystem.cpp
        gbSystem.h
        glFunctions.cpp
        inputManager.cpp
        inputManager.h
        instructionsEngine.h
        MBCBase.h
        MBC.h
        HuC1.cpp
        HuC1.h      
        RomOnlyMBC.h
        MBC1.cpp
        MBC1.h
        MBC2.cpp
        MBC2.h
        MBC2.cpp
        MBC3.cpp
        MBC3.h
        MBC5.cpp
        MBC5.h
        MMU.cpp
        MMU.h
        pixelOps.h
        PPU.cpp
        PPU.h
        registers.h
        resources.h
        shaders.h
        stringUtils.h
        RTCTimer.cpp
        RTCTimer.h
        serialPort.cpp
        serialPort.h
        Shader.cpp
        Shader.h)

if (EMSCRIPTEN)
    target_link_options(MegaBoy PRIVATE -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
    -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_FUNCTIONS=[_main,_malloc,_free] -sEXPORTED_RUNTIME_METHODS=[ccall])
else ()
    add_subdirectory("Libs/GLFW")
    add_subdirectory("Libs/glad")
    add_subdirectory("Libs/nativefiledialog-extended")
    target_link_libraries(MegaBoy glfw glad nfd imgui)
endif()

add_subdirectory("Libs/ImGUI")
target_link_libraries(MegaBoy imgui)
