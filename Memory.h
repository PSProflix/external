#pragma once
#include <windows.h>
#include <vector>
#include <iostream>
#include <TlHelp32.h>

class Memory {
public:
    DWORD processId = 0;
    HANDLE processHandle = NULL;

    Memory(const char* processName) {
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(PROCESSENTRY32W);

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

        // Convert processName to wide string for comparison
        wchar_t wProcessName[MAX_PATH];
        size_t outSize;
        mbstowcs_s(&outSize, wProcessName, processName, strlen(processName) + 1);

        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (lstrcmpiW(entry.szExeFile, wProcessName) == 0) {
                    processId = entry.th32ProcessID;
                    processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }

        if (snapshot) CloseHandle(snapshot);
    }

    ~Memory() {
        if (processHandle) CloseHandle(processHandle);
    }

    uintptr_t GetModuleAddress(const char* moduleName) {
        MODULEENTRY32W entry;
        entry.dwSize = sizeof(MODULEENTRY32W);

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);

        wchar_t wModuleName[MAX_PATH];
        size_t outSize;
        mbstowcs_s(&outSize, wModuleName, moduleName, strlen(moduleName) + 1);

        uintptr_t moduleBase = 0;
        if (Module32FirstW(snapshot, &entry)) {
            do {
                if (lstrcmpiW(entry.szModule, wModuleName) == 0) {
                    moduleBase = (uintptr_t)entry.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snapshot, &entry));
        }

        if (snapshot) CloseHandle(snapshot);
        return moduleBase;
    }

    template <typename T>
    T Read(uintptr_t address) {
        T buffer;
        ReadProcessMemory(processHandle, (LPCVOID)address, &buffer, sizeof(T), NULL);
        return buffer;
    }
};