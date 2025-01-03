#include "stdafx.h"

namespace Memory
{
    template<typename T>
    void Write(std::uint8_t* writeAddress, T value)
    {
        DWORD oldProtect;
        VirtualProtect((LPVOID)(writeAddress), sizeof(T), PAGE_EXECUTE_WRITECOPY, &oldProtect);
        *(reinterpret_cast<T*>(writeAddress)) = value;
        VirtualProtect((LPVOID)(writeAddress), sizeof(T), oldProtect, &oldProtect);
    }

    void PatchBytes(std::uint8_t* address, const char* pattern, unsigned int numBytes)
    {
        DWORD oldProtect;
        VirtualProtect((LPVOID)address, numBytes, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy((LPVOID)address, pattern, numBytes);
        VirtualProtect((LPVOID)address, numBytes, oldProtect, &oldProtect);
    }

    std::vector<int> pattern_to_byte(const char* pattern)
    {
        auto bytes = std::vector<int>{};
        auto start = const_cast<char*>(pattern);
        auto end = const_cast<char*>(pattern) + strlen(pattern);

        for (auto current = start; current < end; ++current) {
            if (*current == '?') {
                ++current;
                if (*current == '?')
                    ++current;
                bytes.push_back(-1);
            }
            else {
                bytes.push_back(strtoul(current, &current, 16));
            }
        }
        return bytes;
    }

    std::uint8_t* PatternScan(void* module, const char* signature) {
        auto dosHeader = (PIMAGE_DOS_HEADER)module;
        auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)module + dosHeader->e_lfanew);

        auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
        auto patternBytes = pattern_to_byte(signature);
        auto scanBytes = reinterpret_cast<std::uint8_t*>(module);

        auto s = patternBytes.size();
        auto d = patternBytes.data();

        for (auto i = 0ul; i < sizeOfImage - s; ++i) {
            bool found = true;
            for (auto j = 0ul; j < s; ++j) {
                if (scanBytes[i + j] != d[j] && d[j] != -1) {
                    found = false;
                    break;
                }
            }
            if (found) {
                return &scanBytes[i];
            }
        }

        return nullptr;
    }

    std::uint8_t* MultiPatternScan(void* module, const std::vector<const char*>& signatures) {
        for (const auto& signature : signatures) {
            if (std::uint8_t* result = PatternScan(module, signature)) {
                return result;
            }
        }
        return nullptr;
    }

    static HMODULE GetThisDllHandle()
    {
        MEMORY_BASIC_INFORMATION info;
        size_t len = VirtualQueryEx(GetCurrentProcess(), (void*)GetThisDllHandle, &info, sizeof(info));
        assert(len == sizeof(info));
        return len ? (HMODULE)info.AllocationBase : NULL;
    }

    std::uint32_t ModuleTimestamp(void* module)
    {
        auto dosHeader = (PIMAGE_DOS_HEADER)module;
        auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)module + dosHeader->e_lfanew);
        return ntHeaders->FileHeader.TimeDateStamp;
    }

    std::uint8_t* GetAbsolute(std::uint8_t* address) noexcept
    {
        if (address == nullptr)
            return nullptr;

        std::int32_t offset = *reinterpret_cast<std::int32_t*>(address);
        std::uint8_t* absoluteAddress = address + 4 + offset;

        return absoluteAddress;
    }
}

namespace Util
{
    std::pair<int, int> GetPhysicalDesktopDimensions() {
        if (DEVMODE devMode{ .dmSize = sizeof(DEVMODE) }; EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode))
            return { devMode.dmPelsWidth, devMode.dmPelsHeight };

        return {};
    }

    std::string wstring_to_string(const wchar_t* wstr)
    {
        size_t len = std::wcslen(wstr);
        std::string str(len, '\0');
        size_t converted = 0;
        wcstombs_s(&converted, &str[0], str.size() + 1, wstr, str.size());
        return str;
    }

    bool stringcmp_caseless(const std::string& str1, const std::string& str2) {
        if (str1.size() != str2.size()) {
            return false;
        }
        return std::equal(str1.begin(), str1.end(), str2.begin(),
            [](char a, char b) {
                return std::tolower(a) == std::tolower(b);
            });
    }
}
