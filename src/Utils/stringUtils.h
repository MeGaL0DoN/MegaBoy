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

    inline std::wstring ToUTF16(const std::string& utf8Str)
    {
        const auto size = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), utf8Str.length(), nullptr, 0);

        if (size <= 0)
            return L"";

        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), utf8Str.length(), result.data(), size);
        return result;
    }

    inline std::string ToUTF8(const std::wstring& utf16Str)
    {
        const auto size = WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), utf16Str.length(), nullptr, 0, nullptr, nullptr);

        if (size <= 0)
            return "";

        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), utf16Str.length(), result.data(), size, nullptr, nullptr);
        return result;
    }

    #endif

    inline auto nativePath(const std::string& path)
    {
        #ifdef _WIN32
            return ToUTF16(path);
        #else
            return path;
        #endif
    }

    inline std::string pathToUTF8(const std::filesystem::path& filePath)
    {
        #ifdef _WIN32
            return ToUTF8(filePath.wstring());
        #else
            return filePath.string();
        #endif
    }

    inline std::filesystem::path getExecutablePath() 
    {
        #if defined(_WIN32)
            wchar_t pathBuf[MAX_PATH];
            GetModuleFileNameW(NULL, pathBuf, MAX_PATH);
        #else
            char pathBuf[4096];
        #if defined(__linux__) || defined(__unix__)
            ssize_t count = readlink("/proc/self/exe", pathBuf, sizeof(pathBuf));
            if (count != -1) 
                pathBuf[count] = '\0';

        #elif defined(__APPLE__)
            uint32_t size = sizeof(pathBuf);
            if (_NSGetExecutablePath(pathBuf, &size) != 0) 
                pathBuf[0] = '\0'; // buffer too small (!)
            else 
                realpath(pathBuf, pathBuf);
        #endif
        #endif
           
        const std::filesystem::path path{ pathBuf };
        return path.parent_path();
    }

    inline std::filesystem::path executableFolderPath { getExecutablePath() };
}