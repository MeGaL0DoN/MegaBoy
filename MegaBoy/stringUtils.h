#pragma once

#ifdef _WIN32

#include "Windows.h"

namespace StringUtils
{
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
}

#endif