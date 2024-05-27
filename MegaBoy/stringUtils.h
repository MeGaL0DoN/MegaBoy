#pragma once

#ifdef _WIN32
#include "Windows.h"
#elif defined(__linux__) || defined(__unix__)
#include <unistd.h>
#include <limits.h>

#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace StringUtils
{
    #ifdef _WIN32

    inline std::wstring ToUTF16(const char* utf8Str)
    {
        const int len { static_cast<int>(strlen(utf8Str)) };
        const auto size = MultiByteToWideChar(CP_UTF8, 0, utf8Str, len, nullptr, 0);

        if (size <= 0)
            return L"";

        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8Str, len, result.data(), size);
        return result;
    }

    inline std::string ToUTF8(const wchar_t* utf16Str)
    {
        const int len { static_cast<int>(wcslen(utf16Str)) };
        const auto size = WideCharToMultiByte(CP_UTF8, 0, utf16Str, len, nullptr, 0, nullptr, nullptr);

        if (size <= 0)
            return "";

        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, utf16Str, len, result.data(), size, nullptr, nullptr);
        return result;
    }

    #endif

    inline auto nativePath(const std::string& path)
    {
        #ifdef _WIN32
            return ToUTF16(path.c_str());
        #else
            return path;
        #endif
    }

    inline std::string getExecutablePath() 
    {
        #if defined(_WIN32)
            wchar_t pathBuf[MAX_PATH];
            GetModuleFileNameW(NULL, pathBuf, MAX_PATH);
            const auto path { ToUTF8(pathBuf) };
        #else
            char pathBuf[MAX_PATH];
        #if defined(__linux__) || defined(__unix__)
            ssize_t count = readlink("/proc/self/exe", pathBuf, sizeof(pathBuf));
            if (count != -1) 
                pathBuf[count] = '\0';

            const auto path(pathBuf);
        #elif defined(__APPLE__)
            uint32_t size = sizeof(pathBuf);
            if (_NSGetExecutablePath(pathBuf, &size) != 0) 
                pathBuf[0] = '\0'; // buffer too small (!)
            else 
                realpath(pathBuf, pathBuf);

            const auto path { pathBuf };
        #endif
        #endif

        return path.substr(0, path.find_last_of("\\/"));
    }

    inline std::string executablePath{ getExecutablePath() };
}