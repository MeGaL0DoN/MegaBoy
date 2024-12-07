#pragma once

#include <filesystem>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Windows.h"
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <libproc.h>
#endif

namespace FileUtils
{
    #ifdef _WIN32
    inline std::wstring toUTF16(std::string_view utf8Str)
    {
        const auto size = MultiByteToWideChar(CP_UTF8, 0, utf8Str.data(), utf8Str.length(), nullptr, 0);

        if (size <= 0)
            return L"";

        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8Str.data(), utf8Str.length(), result.data(), size);
        return result;
    }
    inline std::string toUTF8(std::wstring_view utf16Str)
    {
        const auto size = WideCharToMultiByte(CP_UTF8, 0, utf16Str.data(), utf16Str.length(), nullptr, 0, nullptr, nullptr);

        if (size <= 0)
            return "";

        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, utf16Str.data(), utf16Str.length(), result.data(), size, nullptr, nullptr);
        return result;
    }
    #endif

    inline std::filesystem::path nativePathFromUTF8(std::string_view utf8Str)
    {
#ifdef _WIN32
        return toUTF16(utf8Str);
#else
        return std::filesystem::path(utf8Str);
#endif
    }
    inline std::string pathToUTF8(const std::filesystem::path& path)
    {
        #ifdef _WIN32
            return toUTF8(path.wstring());
        #else
            return path.string();
        #endif
    }
    inline auto pathNativeStr(const std::filesystem::path& path)
	{
		#ifdef _WIN32
			return path.wstring();
		#else
			return path.string();
		#endif
	}

    inline std::filesystem::path replaceExtension(std::filesystem::path path, const char* newExt)
    {
        path.replace_extension(newExt);
        return path;
    }

    inline uint32_t remainingBytes(std::istream& st)
	{
		const auto pos = st.tellg();
		st.seekg(0, std::ios::end);
		const auto availableBytes = st.tellg() - pos;
		st.seekg(pos, std::ios::beg);
		return static_cast<uint32_t>(availableBytes);
	}

#ifndef EMSCRIPTEN
    inline std::filesystem::path getExecutablePath() 
    {
        #ifdef _WIN32
            wchar_t pathBuf[MAX_PATH];
            GetModuleFileNameW(NULL, pathBuf, MAX_PATH);
        #else
            char pathBuf[4096];
        #ifdef __APPLE__
            pid_t pid = getpid();
            proc_pidpath(pid, pathBuf, sizeof(pathBuf));
        #elif defined(__linux__) || defined(__unix__)
            ssize_t count = readlink("/proc/self/exe", pathBuf, sizeof(pathBuf));
            if (count != -1) pathBuf[count] = '\0';
        #endif
        #endif
           
        const std::filesystem::path path{ pathBuf };
        return path.parent_path();
    }

    inline const std::filesystem::path executableFolderPath { getExecutablePath() };
#else
    inline const std::filesystem::path executableFolderPath { };
#endif
}