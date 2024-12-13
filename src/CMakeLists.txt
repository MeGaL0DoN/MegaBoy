cmake_minimum_required(VERSION 3.27)
project(MegaBoy)

set(CMAKE_CXX_STANDARD 20)

if(MSVC)
    add_compile_options(
            $<$<CONFIG:>:/MT> #---------|
            $<$<CONFIG:Release>:/MT> #--|
    )
elseif(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
endif()

add_executable(MegaBoy
        MegaBoy.cpp
        appConfig.cpp
        appConfig.h
        GBCore.cpp
        GBCore.h
        gbSystem.cpp
        gbSystem.h
        MMU.cpp
        MMU.h
        SerialPort.cpp
        SerialPort.h
        Cartridge.cpp
        Cartridge.h
        Joypad.cpp
        Joypad.h
        keyBindManager.h
        resources.h
        defines.h
        debugUI.cpp
        debugUI.h
        "PPU/PPU.h"
        "PPU/PPUCore.cpp"
        "PPU/PPUCore.h"
        "APU/APU.cpp"
        "APU/APU.h"
        "APU/squareWave.h" 
        "APU/sweepWave.h"               
        "CPU/CPU.cpp"
        "CPU/CPU.h"
        "CPU/registers.h"
        "CPU/regDefines.h"
        "CPU/CPUInstructions.h"
        "CPU/CPUInterrupts.cpp"
        "CPU/CPUDisassembly.cpp"
        "Mappers/MBCBase.h"
        "Mappers/MBC.h"
        "Mappers/HuC1.cpp"
        "Mappers/HuC1.h"      
        "Mappers/RomOnlyMBC.h"
        "Mappers/MBC1.cpp"
        "Mappers/MBC1.h"
        "Mappers/MBC2.cpp"
        "Mappers/MBC2.h"
        "Mappers/MBC2.cpp"
        "Mappers/MBC3.cpp"
        "Mappers/MBC3.h"
        "Mappers/MBC5.cpp"
        "Mappers/MBC5.h"
        "Mappers/RTCTimer.cpp"
        "Mappers/RTCTimer.h"
        "Utils/memstream.h"
        "Utils/glFunctions.cpp"
        "Utils/bitOps.h"
        "Utils/pixelOps.h"
        "Utils/fileUtils.h"
        "Utils/Shader.cpp"
        "Utils/Shader.h" )

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)

if (supported)
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET MegaBoy PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()

## set_property(TARGET MegaBoy PROPERTY COMPILE_WARNING_AS_ERROR ON)

if (EMSCRIPTEN)
    target_link_options(MegaBoy PRIVATE -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
    -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_FUNCTIONS=[_main,_malloc,_free] -sEXPORTED_RUNTIME_METHODS=[ccall])

    target_link_libraries(MegaBoy idbfs.js)
else ()
    if (MSVC)
        set_target_properties(
                MegaBoy PROPERTIES
                LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
                LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
        )
    elseif(MINGW)
        set_target_properties(
                MegaBoy PROPERTIES
                LINK_FLAGS_RELEASE "-Wl,-subsystem,windows -s"
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set_target_properties(
                MegaBoy PROPERTIES
                LINK_FLAGS_RELEASE "-s"
        )
    endif()

    add_subdirectory("Libs/GLFW")
    add_subdirectory("Libs/glad")
    add_subdirectory("Libs/nativefiledialog-extended")

    if(WIN32)
        target_link_libraries(MegaBoy winmm) # for timeBeginPeriod(1)
    endif()

    target_link_libraries(MegaBoy glfw glad nfd)
endif()

add_subdirectory("Libs/miniz")
add_subdirectory("Libs/ImGUI")
target_link_libraries(MegaBoy imgui miniz)