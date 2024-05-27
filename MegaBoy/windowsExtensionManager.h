#pragma once
//
//#ifdef _WIN32
//
//#include <windows.h>
//#include <iostream>
//#include <string>
//
//std::string getExecutablePath()
//{
//    char path[MAX_PATH];
//    GetModuleFileNameA(NULL, path, MAX_PATH);
//    return std::string(path);
//}
//
//constexpr std::string_view programName = "MegaBoy";
//
//// Function to set the registry values for file association
//void associateFileExtension(const std::string& extension)
//{
//    HKEY hKey;
//    LONG lResult = 0;
//    bool bSuccess = true;
//    std::string keyName = "Software\\Classes\\" + extension;
//    std::string keyValue = "";
//    std::string command = "\"" + getExecutablePath() + "\" \"%1\"";
//
//    lResult = RegCreateKeyExA(HKEY_CURRENT_USER, keyName.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
//    if (lResult == ERROR_SUCCESS)
//    {
//        RegSetValueExA(hKey, "", 0, REG_SZ, (const BYTE*)keyValue.c_str(), (DWORD)keyValue.size() + 1);
//        RegCloseKey(hKey);
//    }
//    else
//    {
//        bSuccess = false;
//    }
//
//    const std::string programId = "MegaBoy";
//
//    keyName = "Software\\Classes\\" + programId + "\\shell\\open\\command";
//    lResult = RegCreateKeyExA(HKEY_CURRENT_USER, keyName.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
//
//    if (lResult == ERROR_SUCCESS)
//    {
//        RegSetValueExA(hKey, "", 0, REG_SZ, (const BYTE*)command.c_str(), (DWORD)command.size() + 1);
//        RegCloseKey(hKey);
//    }
//    else
//    {
//        bSuccess = false;
//    }
//
//    if (bSuccess)
//        std::cout << "File association created successfully!" << std::endl;
//    else
//        std::cout << "Failed to create file association." << std::endl;
//}
//#endif