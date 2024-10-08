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
        main.cpp
        appConfig.cpp
        appConfig.h
        MMU.cpp
        MMU.h
        PPU.h
        PPUCore.cpp
        PPUCore.h
        resources.h
        defines.h
        serialPort.cpp
        serialPort.h
        Cartridge.cpp
        Cartridge.h
        GBCore.cpp
        GBCore.h
        gbSystem.cpp
        gbSystem.h
        inputManager.cpp
        inputManager.h
        debugUI.cpp
        debugUI.h
        "APU/APU.cpp"
        "APU/APU.h"
        "APU/squareWave.h" 
        "APU/sweepWave.h"               
        "CPU/CPU.cpp"
        "CPU/CPU.h"
        "CPU/CPU_interrupts.cpp"
        "CPU/instructionsEngine.h"
        "CPU/registers.h"
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
        "Utils/glFunctions.cpp"
        "Utils/bitOps.h"
        "Utils/pixelOps.h"
        "Utils/fileUtils.h"
        "Utils/Shader.cpp"
        "Utils/Shader.h" )

if (EMSCRIPTEN)
    target_link_options(MegaBoy PRIVATE -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
    -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_FUNCTIONS=[_main,_malloc,_free] -sEXPORTED_RUNTIME_METHODS=[ccall])
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
    target_link_libraries(MegaBoy glfw glad nfd)
endif()

add_subdirectory("Libs/ImGUI")
target_link_libraries(MegaBoy imgui)