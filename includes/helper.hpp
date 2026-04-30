#pragma once

#include <windows.h>

#include <algorithm>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <future>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <climits>

#include <spdlog/spdlog.h>

#if defined(_WIN64)
using IMAGE_NT_HEADERS_X = IMAGE_NT_HEADERS64;
#else
using IMAGE_NT_HEADERS_X = IMAGE_NT_HEADERS32;
#endif

template<typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template<typename T>
concept Integral = std::is_integral_v<T>;

namespace Memory
{
	struct RegisterProxy
	{
		uintptr_t value;

		template<typename T>
		operator T() const
		{
			static_assert(sizeof(T) <= sizeof(uintptr_t));
			static_assert(std::is_trivially_copyable_v<T>);
			return std::bit_cast<T>(value);
		}
	};

	inline RegisterProxy ReadRegister(uintptr_t reg)
	{
		return RegisterProxy{ reg };
	}

	struct MemProxy
	{
		uintptr_t address;

		template<typename T>
		operator T& () const
		{
			return *reinterpret_cast<T*>(address);
		}
	};

	inline MemProxy ReadMem(uintptr_t address)
	{
		return MemProxy{ address };
	}

	template<typename A>
	concept AddressLike = std::is_pointer_v<std::remove_reference_t<A>> || std::is_integral_v<std::remove_reference_t<A>>;

	template<AddressLike A>
	[[nodiscard]] inline std::uint8_t* ToBytePtr(A address) noexcept
	{
		using Decayed = std::remove_cvref_t<A>;

		if constexpr (std::is_pointer_v<Decayed>)
		{
			return reinterpret_cast<std::uint8_t*>(const_cast<std::remove_const_t<std::remove_pointer_t<Decayed>>*>(address));
		}
		else
		{
			return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(address));
		}
	}

	template<typename Address, typename T> requires AddressLike<Address>&& std::is_trivially_copyable_v<T>
	inline void Write(Address writeAddress, const T& value)
	{
		std::uint8_t* dst = ToBytePtr(writeAddress);

		DWORD oldProtect = 0;
		if (!VirtualProtect(dst, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			spdlog::error("VirtualProtect failed in Memory::Write");
			return;
		}

		std::memcpy(dst, &value, sizeof(T));
		FlushInstructionCache(GetCurrentProcess(), dst, sizeof(T));

		DWORD dummy = 0;
		VirtualProtect(dst, sizeof(T), oldProtect, &dummy);
	}

	template<typename Address, typename Range> requires AddressLike<Address>&&std::ranges::contiguous_range<Range>&&std::is_trivial_v<std::ranges::range_value_t<Range>>
	inline void Write(Address writeAddress, const Range& r)
	{
		using CharT = std::ranges::range_value_t<Range>;

		std::uint8_t* dst = ToBytePtr(writeAddress);

		const std::size_t charSize = sizeof(CharT);
		const std::size_t dataSize = std::ranges::size(r) * charSize;
		const std::size_t totalSize = dataSize + charSize;

		DWORD oldProtect = 0;
		if (!VirtualProtect(dst, totalSize, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			spdlog::error("VirtualProtect failed in Memory::Write (strings)");
			return;
		}

		std::memcpy(dst, std::ranges::data(r), dataSize);
		std::memset(dst + dataSize, 0, charSize);

		DWORD dummy = 0;
		VirtualProtect(dst, totalSize, oldProtect, &dummy);
	}

	template<typename Address, typename T> requires AddressLike<Address>&& std::is_trivially_copyable_v<T>
	inline void Write(const std::vector<Address>& addresses, std::size_t startIndex, std::size_t endIndex, std::ptrdiff_t offset, const T& value)
	{
		if (addresses.empty())
		{
			return;
		}

		if (startIndex > endIndex || endIndex >= addresses.size())
		{
			spdlog::error("Memory::Write(range): invalid range [{}..{}] (size={})", startIndex, endIndex, addresses.size());
			return;
		}

		for (std::size_t i = startIndex; i <= endIndex; ++i)
		{
			auto* address = ToBytePtr(addresses[i]);

			if (!address)
			{
				continue;
			}

			Write(address + offset, value);
		}
	}

	template <std::size_t N>
	inline void PatchBytes(std::uint8_t* address, const char(&bytes)[N])
	{
		static_assert(N > 1, "PatchBytes: empty byte string");

		constexpr std::size_t kSize = N - 1;

		DWORD oldProtect{};

		if (!VirtualProtect(address, kSize, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			spdlog::error("VirtualProtect failed in Memory::PatchBytes");

			return;
		}

		std::memcpy(address, bytes, kSize);

		FlushInstructionCache(GetCurrentProcess(), address, kSize);

		VirtualProtect(address, kSize, oldProtect, &oldProtect);
	}

	template <typename T> requires std::is_trivially_copyable_v<T>
	inline void PatchBytes(std::uint8_t* address, const T& data)
	{
		DWORD oldProtect{};

		if (!VirtualProtect(address, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			spdlog::error("VirtualProtect failed in Memory::PatchBytes<T>");
			return;
		}

		std::memcpy(address, &data, sizeof(T));

		FlushInstructionCache(GetCurrentProcess(), address, sizeof(T));

		VirtualProtect(address, sizeof(T), oldProtect, &oldProtect);
	}

	inline void PatchBytes(std::uint8_t* address, const std::uint8_t* data, std::size_t size)
	{
		DWORD oldProtect{};

		if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			spdlog::error("VirtualProtect failed in Memory::PatchBytes(raw)");
			return;
		}

		std::memcpy(address, data, size);

		FlushInstructionCache(GetCurrentProcess(), address, size);

		VirtualProtect(address, size, oldProtect, &oldProtect);
	}

	template <typename T> requires std::is_trivially_copyable_v<T>
	inline void PatchBytes(const std::vector<std::uint8_t*>& addresses,	std::size_t startIndex, std::size_t endIndex, std::ptrdiff_t offset, const T& data)
	{
		if (addresses.empty())
		{
			spdlog::error("Memory::PatchBytes(range): empty address list");

			return;
		}

		if (startIndex > endIndex || endIndex >= addresses.size())
		{
			spdlog::error("Memory::PatchBytes(range): invalid range [{}..{}] (size={})", startIndex, endIndex, addresses.size());

			return;
		}

		for (std::size_t i = startIndex; i <= endIndex; ++i)
		{
			std::uint8_t* addr = addresses[i];

			if (!addr)
			{
				continue;
			}

			Memory::PatchBytes(addr + offset, data);
		}
	}

	inline void WriteNOPs(std::uint8_t* address, std::size_t numBytes)
	{
		if (!address || numBytes == 0)
		{
			return;
		}

		DWORD oldProtect{};

		if (!VirtualProtect(address, numBytes, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			spdlog::error("VirtualProtect failed in Memory::WriteNOPs");

			return;
		}

		std::memset(address, 0x90, numBytes);

		FlushInstructionCache(GetCurrentProcess(), address, numBytes);

		VirtualProtect(address, numBytes, oldProtect, &oldProtect);
	}

	inline void WriteNOPs(const std::vector<std::uint8_t*>& addresses, std::size_t startIndex, std::size_t endIndex, std::ptrdiff_t offset, std::size_t numBytes)
	{
		if (addresses.empty())
		{
			spdlog::error("Memory::WriteNOPs(range): empty address list");
			return;
		}

		if (startIndex > endIndex || endIndex >= addresses.size())
		{
			spdlog::error("Memory::WriteNOPs(range): invalid range [{}..{}] (size={})", startIndex, endIndex, addresses.size());
			return;
		}

		if (numBytes == 0)
		{
			spdlog::warn("Memory::WriteNOPs(range): numBytes == 0 (nothing to do)");
			return;
		}

		for (std::size_t i = startIndex; i <= endIndex; ++i)
		{
			std::uint8_t* addr = addresses[i];
			if (!addr) continue;

			WriteNOPs(addr + offset, numBytes);
		}
	}

	enum class CallType
    {
        Relative,
        Absolute
    };

    inline std::optional<std::vector<uint8_t>>
    PatchCALL(uint8_t* callSite, uint8_t* target, CallType type = CallType::Relative)
    {
        if (!callSite || !target)
        {
            spdlog::error("PatchCALL failed: null pointer (callSite={}, target={})", fmt::ptr(callSite), fmt::ptr(target));

            return std::nullopt;
        }

        DWORD oldProt{};

        if (type == CallType::Relative)
        {
            intptr_t nextInstr = reinterpret_cast<intptr_t>(callSite) + 5;
            intptr_t rel64 = reinterpret_cast<intptr_t>(target) - nextInstr;

            if (rel64 < INT32_MIN || rel64 > INT32_MAX)
            {
                spdlog::error("PatchCALL (Relative) failed: rel32 overflow.");
                return std::nullopt;
            }

            std::vector<uint8_t> original(5);

            std::memcpy(original.data(), callSite, 5);

            int32_t rel32 = static_cast<int32_t>(rel64);

            uint8_t patch[5];

            patch[0] = 0xE8;

            std::memcpy(&patch[1], &rel32, sizeof(rel32));

            if (!VirtualProtect(callSite, 5, PAGE_EXECUTE_READWRITE, &oldProt))
            {
                spdlog::error("PatchCALL failed: VirtualProtect.");
                return std::nullopt;
            }

            std::memcpy(callSite, patch, 5);

            FlushInstructionCache(GetCurrentProcess(), callSite, 5);

            VirtualProtect(callSite, 5, oldProt, &oldProt);

            spdlog::info("PatchCALL (Relative): {:#x} -> {:#x}", reinterpret_cast<uintptr_t>(callSite), reinterpret_cast<uintptr_t>(target));

            return original;
        }
        else
        {
		#if defined(_WIN64)
            constexpr size_t patchSize = 12;

            std::vector<uint8_t> original(patchSize);
            std::memcpy(original.data(), callSite, patchSize);

            uint8_t patch[patchSize] = { 0 };

            patch[0] = 0x48;
            patch[1] = 0xB8; // mov rax, imm64
            std::memcpy(&patch[2], &target, sizeof(uint64_t));

            patch[10] = 0xFF;
            patch[11] = 0xD0; // call rax

            if (!VirtualProtect(callSite, patchSize, PAGE_EXECUTE_READWRITE, &oldProt))
            {
                spdlog::error("PatchCALL (Absolute) failed: VirtualProtect.");
                return std::nullopt;
            }

            std::memcpy(callSite, patch, patchSize);
            FlushInstructionCache(GetCurrentProcess(), callSite, patchSize);
            VirtualProtect(callSite, patchSize, oldProt, &oldProt);

            spdlog::info("PatchCALL (Absolute x64): {:#x} -> {:#x}",
                reinterpret_cast<uintptr_t>(callSite),
                reinterpret_cast<uintptr_t>(target));

            return original;

		#else
            constexpr size_t patchSize = 6;

            std::vector<uint8_t> original(patchSize);
            std::memcpy(original.data(), callSite, patchSize);

            uint8_t patch[patchSize];

            patch[0] = 0x68;
            std::memcpy(&patch[1], &target, sizeof(uint32_t));
            patch[5] = 0xC3;

            if (!VirtualProtect(callSite, patchSize, PAGE_EXECUTE_READWRITE, &oldProt))
            {
                spdlog::error("PatchCALL (Absolute) failed: VirtualProtect.");
                return std::nullopt;
            }

            std::memcpy(callSite, patch, patchSize);
            FlushInstructionCache(GetCurrentProcess(), callSite, patchSize);
            VirtualProtect(callSite, patchSize, oldProt, &oldProt);

            spdlog::info("PatchCALL (Absolute x86): {:#x} -> {:#x}", reinterpret_cast<uintptr_t>(callSite), reinterpret_cast<uintptr_t>(target));

            return original;
		#endif
        }
    }

	static inline int hexval(char c) noexcept
	{
		if (c >= '0' && c <= '9')
		{
			return c - '0';
		}

		if (c >= 'a' && c <= 'f')
		{
			return 10 + (c - 'a');
		}

		if (c >= 'A' && c <= 'F')
		{
			return 10 + (c - 'A');
		}

		return -1;
	}

	std::vector<int> pattern_to_byte(const char* pattern)
	{
		std::vector<int> bytes;
		if (!pattern) return bytes;

		const char* p = pattern;
		while (*p)
		{
			while (*p && (std::isspace(static_cast<unsigned char>(*p)) || *p == '-'))
			{
				++p;
			}

			if (!*p)
			{
				break;
			}

			if (*p == '?')
			{
				++p;
				if (*p == '?') ++p;
				bytes.push_back(-1);
				continue;
			}

			int hi = hexval(*p);

			if (hi < 0)
			{
				++p;
				continue;
			}

			++p;

			int lo = -1;

			if (*p && std::isxdigit(static_cast<unsigned char>(*p)))
			{
				lo = hexval(*p);
				++p;
			}

			int val = (lo >= 0) ? ((hi << 4) | lo) : hi;

			bytes.push_back(val & 0xFF);
		}

		if (bytes.size() > 1000000)
		{
			return {};
		}

		return bytes;
	}

	thread_local std::vector<std::string> g_last_scan_signatures;

	inline std::unordered_map<std::string, std::vector<int>> s_pattern_cache;

	inline std::mutex s_pattern_cache_mutex;

	static std::vector<int> get_pattern_bytes_cached(const char* pattern)
	{
		if (!pattern)
		{
			return {};
		}

		std::string key(pattern);
		{
			std::lock_guard<std::mutex> lk(s_pattern_cache_mutex);
			auto it = s_pattern_cache.find(key);
			if (it != s_pattern_cache.end()) return it->second;
		}

		std::vector<int> bytes = pattern_to_byte(pattern);
		{
			std::lock_guard<std::mutex> lk(s_pattern_cache_mutex);
			s_pattern_cache.try_emplace(key, bytes);
		}

		return bytes;
	}

	std::uint8_t* PatternScan(void* module, const char* signature)
	{
		auto patternBytes = pattern_to_byte(signature);
		size_t s = patternBytes.size();

		auto dos = (PIMAGE_DOS_HEADER)module;
		auto nt = (PIMAGE_NT_HEADERS)((uint8_t*)module + dos->e_lfanew);

		size_t size = nt->OptionalHeader.SizeOfImage;

		uint8_t* base = (uint8_t*)module;
		uint8_t* end = base + size;

		uint8_t* current = base;

		while (current < end)
		{
			MEMORY_BASIC_INFORMATION mbi{};
			if (!VirtualQuery(current, &mbi, sizeof(mbi)))
				break;

			uint8_t* regionStart = (uint8_t*)mbi.BaseAddress;
			uint8_t* regionEnd = regionStart + mbi.RegionSize;

			uint8_t* scanStart = (uint8_t*)std::max((uintptr_t)regionStart, (uintptr_t)base);
			uint8_t* scanEnd = (uint8_t*)std::min((uintptr_t)regionEnd, (uintptr_t)end);

			if (mbi.State == MEM_COMMIT &&
				!(mbi.Protect & PAGE_GUARD) &&
				!(mbi.Protect & PAGE_NOACCESS))
			{
				for (uint8_t* i = scanStart; i + s <= scanEnd; i++)
				{
					bool found = true;

					for (size_t j = 0; j < s; j++)
					{
						if (patternBytes[j] != -1 && i[j] != patternBytes[j])
						{
							found = false;
							break;
						}
					}

					if (found)
						return i;
				}
			}

			current = regionEnd;
		}

		return nullptr;
	}

	template<typename... Sigs> requires ((std::convertible_to<Sigs, const char*> && ...))
	std::vector<std::uint8_t*> PatternScan(void* module, const char* firstSignature, Sigs... otherSignatures)
	{
		g_last_scan_signatures.clear();

		g_last_scan_signatures.reserve(1 + sizeof...(Sigs));

		g_last_scan_signatures.emplace_back(firstSignature);

		(g_last_scan_signatures.emplace_back(otherSignatures), ...);

		std::vector<std::uint8_t*> results;

		results.reserve(g_last_scan_signatures.size());

		for (auto const& s : g_last_scan_signatures)
		{
			results.push_back(PatternScan(module, s.c_str()));
		}

		return results;
	}

	template<typename... Args, typename = std::enable_if_t<!std::conjunction_v<std::is_convertible<std::decay_t<Args>, const char*>...>, int>>std::vector<std::uint8_t*> PatternScan(void* firstModule, Args... rest)
	{
		g_last_scan_signatures.clear();

		using Item = std::variant<void*, const char*>;

		std::vector<Item> items;

		items.reserve(1 + sizeof...(rest));

		auto make_variant = [](auto&& v) -> Item
		{
			using T = std::decay_t<decltype(v)>;

			if constexpr (std::is_convertible_v<T, void*>)
			{
				return reinterpret_cast<void*>(v);
			}
			else if constexpr (std::is_convertible_v<T, const char*>)
			{
				return static_cast<const char*>(v);
			}
			else
			{
				static_assert(std::is_convertible_v<T, void*> || std::is_convertible_v<T, const char*>, "PatternScan(...) supports only module pointers and const char* signatures");
			}
		};

		items.emplace_back(reinterpret_cast<void*>(firstModule));

		(items.emplace_back(make_variant(rest)), ...);

		struct Task
		{
			void* module;
			const char* sig;
		};

		std::vector<Task> tasks;

		tasks.reserve(items.size());

		void* currentModule = nullptr;

		for (size_t i = 0; i < items.size(); ++i)
		{
			if (std::holds_alternative<void*>(items[i]))
			{
				currentModule = std::get<void*>(items[i]);
			}
			else
			{
				const char* sig = std::get<const char*>(items[i]);

				g_last_scan_signatures.emplace_back(sig ? sig : "");

				if (!currentModule)
				{
					tasks.push_back({ nullptr, sig });
					continue;
				}

				tasks.push_back({ currentModule, sig });
			}
		}

		if (tasks.size() <= 1)
		{
			std::vector<std::uint8_t*> results;

			results.reserve(tasks.size());

			for (const auto& t : tasks)
			{
				if (!t.module)
				{
					spdlog::error("PatternScan: Signature provided without a preceding valid module pointer (sig index {})", results.size());

					results.push_back(nullptr);

					continue;
				}

				results.push_back(PatternScan(t.module, t.sig));
			}

			return results;
		}

		std::vector<std::future<std::uint8_t*>> futures;

		futures.reserve(tasks.size());

		for (const auto& t : tasks)
		{
			if (!t.module)
			{
				futures.emplace_back(std::async(std::launch::deferred, []() -> std::uint8_t* { return nullptr; }));
				continue;
			}

			futures.emplace_back(std::async(std::launch::async, [m = t.module, s = t.sig]() -> std::uint8_t*
			{
				return PatternScan(m, s);
			}));
		}

		std::vector<std::uint8_t*> results;

		results.reserve(futures.size());

		for (size_t i = 0; i < futures.size(); ++i)
		{
			if (!tasks[i].module)
			{
				results.push_back(nullptr);
				continue;
			}

			results.push_back(futures[i].get());
		}

		return results;
	}

	inline bool AreAllSignaturesValid(const std::vector<std::uint8_t*>& addrs, bool bLoggingEnabled = true) noexcept
	{
		if (addrs.empty())
		{
			if (bLoggingEnabled == true)
			{
				spdlog::error("PatternScan: The address vector is empty.");
			}
				
			return false;
		}

		std::vector<std::size_t> missing;

		missing.reserve(addrs.size());

		for (std::size_t i = 0; i < addrs.size(); ++i)
		{
			if (addrs[i] == nullptr)
			{
				missing.push_back(i);
			}
		}

		if (missing.empty())
		{
			return true;
		}

		if (bLoggingEnabled == true)
		{
			std::string msg = fmt::format("PatternScan: {} signature(s) not found. Missing indices: ", missing.size());

			for (std::size_t k = 0; k < missing.size(); ++k)
			{
				if (k > 0)
				{
					msg += ", ";
				}

				msg += fmt::format("{}", missing[k]);

				if (g_last_scan_signatures.size() == addrs.size())
				{
					msg += fmt::format(" (\"{}\")", g_last_scan_signatures[missing[k]]);
				}
			}

			spdlog::error("{}", msg);
		}
		
		return false;
	}

	std::uint32_t ModuleTimestamp(void* module)
	{
		if (!module)
		{
			return 0;
		}

		auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(module);

		if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		{
			return 0;
		}

		auto ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS_X*>(reinterpret_cast<std::uint8_t*>(module) + dosHeader->e_lfanew);

		if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
		{
			return 0;
		}

		return ntHeaders->FileHeader.TimeDateStamp;
	}

	std::uint8_t* GetAbsolute(std::uint8_t* address) noexcept
	{
		if (!address)
		{
			return nullptr;
		}
		
		std::int32_t offset;

		std::memcpy(&offset, address, sizeof(offset));

		return address + 4 + offset;
	}
	
	static HMODULE GetHandle(const std::initializer_list<std::string>& moduleNames, unsigned int retryDelayMs = 200, int maxAttempts = 0, bool loggingEnabled = true)
	{
		int attempts = 0;

		while (true)
		{
			for (const auto &name : moduleNames)
			{
				HMODULE h = GetModuleHandleA(name.c_str());
				
				if (h != nullptr)
				{
					if (loggingEnabled)
					{
						spdlog::info("Obtained handle for {}: 0x{:x}", name, reinterpret_cast<uintptr_t>(h));
					}
					
					return h;
				}
			}

			++attempts;
			
			if (maxAttempts > 0 && attempts >= maxAttempts)
			{
				if (loggingEnabled)
				{
					std::string joined;
					for (auto it = moduleNames.begin(); it != moduleNames.end(); ++it)
					{
						if (it != moduleNames.begin()) joined += ", ";
						joined += *it;
					}
					
					spdlog::error("Failed to get handle of {}.", joined);
				}
				
				return nullptr;
			}
			
			std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
		}
	}

	static HMODULE GetHandle(const std::string& moduleName, unsigned int retryDelayMs = 200, int maxAttempts = 0, bool loggingEnabled = true)
	{
		return GetHandle(std::initializer_list<std::string>{moduleName}, retryDelayMs, maxAttempts, loggingEnabled);
	}

	static std::string GetModuleFilePathUtf8(HMODULE hModule)
	{
		if (hModule == nullptr)
		{
			return {};
		}

		std::wstring wbuf;

		DWORD size = MAX_PATH;

		for (;;)
		{
			wbuf.resize(size);

			DWORD len = GetModuleFileNameW(hModule, &wbuf[0], static_cast<DWORD>(wbuf.size()));

			if (len == 0)
			{
				return {};
			}

			if (len < wbuf.size())
			{
				wbuf.resize(len);

				break;
			}

			size *= 2;

			if (size > 1 << 20)
			{
				return {};
			}
		}

		int needed = WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), -1, nullptr, 0, nullptr, nullptr);

		if (needed <= 0)
		{
			return {};
		}

		std::string out;

		out.resize(needed - 1);

		WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), -1, out.data(), needed, nullptr, nullptr);

		return out;
	}

	static std::string GetModuleName(HMODULE hModule)
	{
		std::string full = GetModuleFilePathUtf8(hModule);
		
		if (full.empty())
		{
			return {};
		}
		
		try
		{
			return std::filesystem::path(full).filename().string();
		}
		catch (...)
		{
			auto pos = full.find_last_of("\\/");
			
			if (pos == std::string::npos)
			{
				return full;
			}
			
			return full.substr(pos + 1);
		}
	}

	enum class PointerMode
	{
		Absolute,
		Relative
	};

	struct PointerResult
	{
		std::uintptr_t value{};

		template<typename T>
			requires (std::is_pointer_v<T> || std::is_integral_v<T>)
		operator T() const noexcept
		{
			if constexpr (std::is_pointer_v<T>)
			{
				return reinterpret_cast<T>(value);
			}
			else
			{
				return static_cast<T>(value);
			}
		}
	};

	template<typename AddressType>
	PointerResult GetPointerFromAddress(AddressType address, PointerMode mode) noexcept
	{
		std::uintptr_t addr{};

		if constexpr (std::is_pointer_v<AddressType>)
		{
			addr = reinterpret_cast<std::uintptr_t>(address);
		}
		else
		{
			addr = static_cast<std::uintptr_t>(address);
		}

		if (!addr)
		{
			return {};
		}

		if (mode == PointerMode::Absolute)
		{
			std::uintptr_t raw{};

			std::memcpy(&raw, reinterpret_cast<void*>(addr), sizeof(raw));

			return { raw };
		}
		else
		{
			std::intptr_t displacement{};

			std::memcpy(&displacement, reinterpret_cast<void*>(addr), sizeof(displacement));

			std::uintptr_t result = addr + sizeof(displacement) + displacement;

			return { result };
		}
	}

	template<Arithmetic T, typename OutputChar = char8_t>
	inline void WriteNumberAsChar8Digits(std::uint8_t* baseAddress, T value)
	{
		std::string str;

		if constexpr (Integral<T>)
		{
			char buf[std::numeric_limits<T>::digits10 + 3];
			auto [p, ec] = std::to_chars(buf, buf + sizeof(buf), value);
			str.assign(buf, p);
		}
		else
		{
			str = std::to_string(value);
		}

		for (char ch : str)
		{
			OutputChar out = static_cast<OutputChar>(ch);
			Memory::Write(baseAddress, out);
			baseAddress += sizeof(OutputChar);
		}
	}

	template<Arithmetic T, typename OutputChar = char16_t>
	inline void WriteNumberAsChar16Digits(std::uint8_t* baseAddress, T value)
	{
		std::string str;

		if constexpr (Integral<T>)
		{
			char buf[std::numeric_limits<T>::digits10 + 3];
			auto [p, ec] = std::to_chars(buf, buf + sizeof(buf), value);
			str.assign(buf, p);
		}
		else
		{
			str = std::to_string(value);
		}

		for (char ch : str)
		{
			OutputChar out = static_cast<OutputChar>(static_cast<unsigned char>(ch));
			Memory::Write(baseAddress, out);
			baseAddress += sizeof(OutputChar);
		}
	}
}

namespace Util
{
	std::pair<int, int> GetPhysicalDesktopDimensions()
	{
		DEVMODE devMode{};

		devMode.dmSize = sizeof(DEVMODE);

		if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode))
		{
			return { devMode.dmPelsWidth, devMode.dmPelsHeight };
		}			

		return {};
	}

	std::string ToLowerAscii(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) 
		{
			return std::tolower(c);
		});

		return s;
	}

	std::wstring WStringLowercase(const std::wstring& s)
	{
		std::wstring out(s);

		for (auto& c : out)
		{
			c = towlower(c);
		}

		return out;
	}

	std::string wstring_to_string(const wchar_t* wstr)
	{
		if (!wstr)
		{
			return {};
		}

		int needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);

		if (needed <= 0)
		{
			return {};
		}
		
		std::string out(needed - 1, '\0');

		WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), needed, nullptr, nullptr);

		return out;
	}

	inline std::string wstring_to_string(const std::wstring& wstr)
	{
		if (wstr.empty())
			return {};

		return wstring_to_string(wstr.c_str());
	}

	bool stringcmp_caseless(const std::string& str1, const std::string& str2)
	{
		if (str1.size() != str2.size())
		{
			return false;
		}

		return std::equal(str1.begin(), str1.end(), str2.begin(), [](char a, char b)
		{
			return std::tolower(a) == std::tolower(b);
		});
	}
}

extern "C"
{
	void FILD16_from_ptr(const void* p);
	void FILD32_from_ptr(const void* p);
	void FILD64_from_ptr(const void* p);

	void FIADD16_from_ptr(const void* p);
	void FIADD32_from_ptr(const void* p);
	void FIADD64_from_ptr(const void* p);

	void FISUB16_from_ptr(const void* p);
	void FISUB32_from_ptr(const void* p);
	void FISUB64_from_ptr(const void* p);

	void FISUBR16_from_ptr(const void* p);
	void FISUBR32_from_ptr(const void* p);
	void FISUBR64_from_ptr(const void* p);

	void FIMUL16_from_ptr(const void* p);
	void FIMUL32_from_ptr(const void* p);
	void FIMUL64_from_ptr(const void* p);

	void FIDIV16_from_ptr(const void* p);
	void FIDIV32_from_ptr(const void* p);
	void FIDIV64_from_ptr(const void* p);

	void FIDIVR16_from_ptr(const void* p);
	void FIDIVR32_from_ptr(const void* p);
	void FIDIVR64_from_ptr(const void* p);

	void FICOMP16_from_ptr(const void* p);
	void FICOMP32_from_ptr(const void* p);
	void FICOMP64_from_ptr(const void* p);

	void FSIN_from_ptr(const void* p);
	void FCOS_from_ptr(const void* p);
	void FSINCOS_from_ptr(const void* p);

	void FPTAN_from_ptr(const void* p);
	void FPATAN_from_ptr(const void* p);

	void FPREM_from_ptr(const void* p);
	void FPREM1_from_ptr(const void* p);

	void FYL2X_from_ptr(const void* p);
	void FYL2XP1_from_ptr(const void* p);

	void FSCALE_from_ptr(const void* p);
	void FSQRT_from_ptr(const void* p);

	void FLD_f32_from_ptr(const void* p);
	void FADD_f32_from_ptr(const void* p);
	void FSUB_f32_from_ptr(const void* p);
	void FSUBR_f32_from_ptr(const void* p);
	void FMUL_f32_from_ptr(const void* p);
	void FDIV_f32_from_ptr(const void* p);
	void FDIVR_f32_from_ptr(const void* p);
	void FCOMP_f32_from_ptr(const void* p);

	void FLD_f64_from_ptr(const void* p);
	void FADD_f64_from_ptr(const void* p);
	void FSUB_f64_from_ptr(const void* p);
	void FSUBR_f64_from_ptr(const void* p);
	void FMUL_f64_from_ptr(const void* p);
	void FDIV_f64_from_ptr(const void* p);
	void FDIVR_f64_from_ptr(const void* p);
	void FCOMP_f64_from_ptr(const void* p);

	void preserve_FIADD32_from_ptr(const void* p);
	void preserve_FISUB32_from_ptr(const void* p);
	void preserve_FIMUL32_from_ptr(const void* p);
	void preserve_FIDIV32_from_ptr(const void* p);
	void preserve_FIDIVR32_from_ptr(const void* p);

	void preserve_FLD_f32_from_ptr(const void* p);
	void preserve_FADD_f32_from_ptr(const void* p);
	void preserve_FMUL_f32_from_ptr(const void* p);
	void preserve_FDIV_f32_from_ptr(const void* p);
}

namespace FPU
{
	template<typename T>
	inline void FILD(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FILD requires integer type");

		if constexpr (sizeof(T) == 2)
		{
			FILD16_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else if constexpr (sizeof(T) == 4)
		{
			FILD32_from_ptr(static_cast<const void*>(std::addressof(v)));
		}			
		else if constexpr (sizeof(T) == 8)
		{
			FILD64_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else
		{
			static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Unsupported operand size");
		}
	}

	template<typename T>
	inline void FIADD(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FIADD requires integer type");

		if constexpr (sizeof(T) == 2)
		{
			FIADD16_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else if constexpr (sizeof(T) == 4)
		{
			FIADD32_from_ptr(static_cast<const void*>(std::addressof(v)));
		}			
		else if constexpr (sizeof(T) == 8)
		{
			FIADD64_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else
		{
			static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Unsupported operand size");
		}
	}

	template<typename T>
	inline void FISUB(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FISUB requires integer type");

		if constexpr (sizeof(T) == 2)
		{
			FISUB16_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else if constexpr (sizeof(T) == 4)
		{
			FISUB32_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else if constexpr (sizeof(T) == 8)
		{
			FISUB64_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else
		{
			static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Unsupported operand size");
		}
	}

	template<typename T>
	inline void FISUBR(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FISUBR requires integer type");

		if constexpr (sizeof(T) == 2)
		{
			FISUBR16_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else if constexpr (sizeof(T) == 4)
		{
			FISUBR32_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else if constexpr (sizeof(T) == 8)
		{
			FISUBR64_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else
		{
			static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Unsupported operand size");
		}
	}

	template<typename T>
	inline void FIMUL(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FIMUL requires integer type");

		if constexpr (sizeof(T) == 2)
		{
			FIMUL16_from_ptr(static_cast<const void*>(std::addressof(v)));
		}			
		else if constexpr (sizeof(T) == 4)
		{
			FIMUL32_from_ptr(static_cast<const void*>(std::addressof(v)));
		} 
		else if constexpr (sizeof(T) == 8)
		{
			FIMUL64_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else
		{
			static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Unsupported operand size");
		}
	}

	template<typename T>
	inline void FIDIV(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FIDIV requires integer type");

		if constexpr (sizeof(T) == 2)
		{
			FIDIV16_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else if constexpr (sizeof(T) == 4)
		{
			FIDIV32_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else if constexpr (sizeof(T) == 8)
		{
			FIDIV64_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else
		{
			static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Unsupported operand size");
		}
	}

	template<typename T>
	inline void FIDIVR(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FIDIVR requires integer type");

		if constexpr (sizeof(T) == 2)
		{
			FIDIVR16_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else if constexpr (sizeof(T) == 4)
		{
			FIDIVR32_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else if constexpr (sizeof(T) == 8)
		{
			FIDIVR64_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else
		{
			static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Unsupported operand size");
		}
	}

	template<typename T>
	inline void FICOMP(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FICOMP requires integer type");

		if constexpr (sizeof(T) == 2)
		{
			FICOMP16_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else if constexpr (sizeof(T) == 4)
		{
			FICOMP32_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else if constexpr (sizeof(T) == 8)
		{
			FICOMP64_from_ptr(static_cast<const void*>(std::addressof(v)));
		}
		else
		{
			static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Unsupported integer size for FICOMP");
		}
	}

	inline void FLD(const float& v)
	{
		FLD_f32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FADD(const float& v)
	{
		FADD_f32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FSUB(const float& v)
	{
		FSUB_f32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FSUBR(const float& v)
	{
		FSUBR_f32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FMUL(const float& v)
	{
		FMUL_f32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FDIV(const float& v)
	{
		FDIV_f32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FDIVR(const float& v)
	{
		FDIVR_f32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FCOMP(const float& v)
	{
		FCOMP_f32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FLD(const double& v)
	{
		FLD_f64_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FADD(const double& v)
	{
		FADD_f64_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FSUB(const double& v)
	{
		FSUB_f64_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FSUBR(const double& v)
	{
		FSUBR_f64_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FMUL(const double& v)
	{
		FMUL_f64_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FDIV(const double& v)
	{
		FDIV_f64_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FDIVR(const double& v)
	{
		FDIVR_f64_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FCOMP(const double& v)
	{
		FCOMP_f64_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void FSIN() 
	{
		FSIN_from_ptr(static_cast<const void*>(nullptr));
	}

	inline void FCOS()
	{
		FCOS_from_ptr(static_cast<const void*>(nullptr));
	}

	inline void FSINCOS()
	{
		FSINCOS_from_ptr(static_cast<const void*>(nullptr));
	}

	inline void FPTAN()
	{
		FPTAN_from_ptr(static_cast<const void*>(nullptr));
	}

	inline void FPATAN()
	{
		FPATAN_from_ptr(static_cast<const void*>(nullptr));
	}

	inline void FPREM()
	{
		FPREM_from_ptr(static_cast<const void*>(nullptr));
	}

	inline void FPREM1()
	{
		FPREM1_from_ptr(static_cast<const void*>(nullptr));
	}

	inline void FYL2X()
	{
		FYL2X_from_ptr(static_cast<const void*>(nullptr));
	}

	inline void FYL2XP1()
	{
		FYL2XP1_from_ptr(static_cast<const void*>(nullptr));
	}

	inline void FSCALE()
	{
		FSCALE_from_ptr(static_cast<const void*>(nullptr));
	}

	inline void FSQRT()
	{ 
		FSQRT_from_ptr(static_cast<const void*>(nullptr));
	}

	inline void P_FIADD32(const int32_t& v)
	{
		preserve_FIADD32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void P_FISUB32(const int32_t& v)
	{
		preserve_FISUB32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void P_FIMUL32(const int32_t& v)
	{
		preserve_FIMUL32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void P_FIDIV32(const int32_t& v)
	{
		preserve_FIDIV32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void P_FIDIVR32(const int32_t& v)
	{
		preserve_FIDIVR32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void P_FLD_f32(const float& v)
	{
		preserve_FLD_f32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void P_FADD_f32(const float& v)
	{
		preserve_FADD_f32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void P_FMUL_f32(const float& v)
	{
		preserve_FMUL_f32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}

	inline void P_FDIV_f32(const float& v)
	{
		preserve_FDIV_f32_from_ptr(static_cast<const void*>(std::addressof(v)));
	}
}

namespace Maths
{
	template<typename T>
	constexpr T Pi = T(3.141592653589793238462643383279502884);

	template<typename T>
	inline T DegToRad(T degrees)
	{
		return degrees * (Pi<T> / T(180));
	}

	template<typename T>
	inline T RadToDeg(T radians)
	{
		return radians * (T(180) / Pi<T>);
	}

	enum class AngleMode
	{
		FullAngle,
		HalfAngle,
	};

	template<typename T, typename U>
	inline T CalculateNewFOV_DegBased(T currentFOVDegrees, U aspectRatioScale, AngleMode rep = AngleMode::FullAngle)
	{
		T arScale = static_cast<T>(aspectRatioScale);

		if (rep == AngleMode::FullAngle)
		{
			T halfRad = std::tan(DegToRad(currentFOVDegrees * T(0.5))) * arScale;

			return T(2) * RadToDeg(std::atan(halfRad));
		}
		else if (rep == AngleMode::HalfAngle)
		{
			T currentHalfRad = DegToRad(currentFOVDegrees);

			T newHalfRad = std::atan(std::tan(currentHalfRad) * arScale);

			return RadToDeg(newHalfRad);
		}
	}

	template<typename T, typename U>
	inline T CalculateNewFOV_RadBased(T currentFOVRadians, U aspectRatioScale, AngleMode rep = AngleMode::FullAngle)
	{
		T arScale = static_cast<T>(aspectRatioScale);

		if (rep == AngleMode::FullAngle)
		{
			return T(2) * std::atan(std::tan(currentFOVRadians * T(0.5)) * arScale);
		}
		else if (rep == AngleMode::HalfAngle)
		{
			return std::atan(std::tan(currentFOVRadians) * arScale);
		}
	}

	template<typename T, typename U>
	inline T CalculateNewFOV_MultiplierBased(T currentFOVMultiplier, U aspectRatioScale)
	{
		T arScale = static_cast<T>(aspectRatioScale);

		T newMultiplierFOV = currentFOVMultiplier * arScale;

		return newMultiplierFOV;
	}

	template<typename T, typename U, typename V = U>
	inline T CalculateNewHFOV_RadBased(T currentHFOVRadians, U aspectRatioScale, V fovFactor = V(1), AngleMode rep = AngleMode::FullAngle)
	{
		T arScale = static_cast<T>(aspectRatioScale);

		T factor = static_cast<T>(fovFactor);

		if (rep == AngleMode::FullAngle)
		{
			return T(2) * std::atan(factor * std::tan(currentHFOVRadians * T(0.5)) * arScale);
		}
		else if (rep == AngleMode::HalfAngle)
		{
			return std::atan(factor * std::tan(currentHFOVRadians) * arScale);
		}
	}

	template<typename T, typename V = T>
	inline T CalculateNewVFOV_RadBased(T currentVFOVRadians, V fovFactor = V(1), AngleMode rep = AngleMode::FullAngle)
	{
		T factor = static_cast<T>(fovFactor);

		if (rep == AngleMode::FullAngle)
		{
			return T(2) * std::atan(factor * std::tan(currentVFOVRadians * T(0.5)));
		}
		else if (rep == AngleMode::HalfAngle)
		{
			return std::atan(factor * std::tan(currentVFOVRadians));
		}
	}

	template<typename T, typename U, typename V = U>
	inline T CalculateNewHFOV_DegBased(T currentHFOVDegrees, U aspectRatioScale, V fovFactor = V(1), AngleMode rep = AngleMode::FullAngle)
	{
		if (rep == AngleMode::FullAngle)
		{
			T halfRad = std::tan(DegToRad(currentHFOVDegrees * T(0.5)));

			return RadToDeg(T(2) * std::atan(static_cast<T>(fovFactor) * halfRad * static_cast<T>(aspectRatioScale)));
		}
		else if (rep == AngleMode::HalfAngle)
		{
			T fullRad = std::tan(DegToRad(currentHFOVDegrees));

			return RadToDeg(std::atan(static_cast<T>(fovFactor) * fullRad * static_cast<T>(aspectRatioScale)));
		}
	}

	template<typename T, typename V = T>
	inline T CalculateNewVFOV_DegBased(T currentVFOVDegrees, V fovFactor = V(1), AngleMode rep = AngleMode::FullAngle)
	{
		if (rep == AngleMode::FullAngle)
		{
			T halfRad = std::tan(DegToRad(currentVFOVDegrees * T(0.5)));

			return RadToDeg(T(2) * std::atan(static_cast<T>(fovFactor) * halfRad));
		}
		else if (rep == AngleMode::HalfAngle)
		{
			T fullRad = std::tan(DegToRad(currentVFOVDegrees));

			return RadToDeg(std::atan(static_cast<T>(fovFactor) * fullRad));
		}
	}

	template<Integral Int>
	inline int digitCount(Int number)
	{
		using Unsigned = std::make_unsigned_t<Int>;

		Unsigned u = static_cast<Unsigned>(number < 0 ? -static_cast<std::intmax_t>(number) : number);

		if (u == 0)
		{
			return 1;
		}

		int count = 0;

		while (u != 0)
		{
			u /= 10;
			++count;
		}

		return count;
	}

	inline constexpr float defaultTolerance = 1e-5f;

	enum class ComparisonOperator
	{
		LessThan,
		GreaterThan
	};

	inline bool isCloseRel(float a, float b, float rel = 0.002f)
	{
		return std::fabs(a - b) <= rel * std::max(std::fabs(a), std::fabs(b));
	}

	template<Arithmetic T, Arithmetic U = T>
	inline bool isClose(T originalValue, T comparedValue, U toleranceCheck = static_cast<U>(Maths::defaultTolerance), ComparisonOperator op = ComparisonOperator::LessThan, bool inclusive = false)
	{
		using Common = std::common_type_t<T, U>;

		Common diff = std::abs(static_cast<Common>(originalValue) - static_cast<Common>(comparedValue));

		Common tol = static_cast<Common>(toleranceCheck);

		if (op == ComparisonOperator::LessThan)
		{
			return inclusive ? (diff <= tol) : (diff < tol);
		}
		else
		{
			return inclusive ? (diff >= tol) : (diff > tol);
		}			
	}

	template<Arithmetic T>
	inline bool isClose(T a, T b, ComparisonOperator op, bool inclusive = false)
	{
		using DefaultTolType = std::common_type_t<T, float>;

		return isClose<T, DefaultTolType>(a, b, static_cast<DefaultTolType>(Maths::defaultTolerance), op, inclusive);
	}

	struct AspectRatio
	{
		int w;
		int h;
	};

	static std::vector<AspectRatio> KnownRatios = {
		{1,1},
		{4,3},
		{5,4},
		{3,2},
		{16,10},
		{16,9},
		{21,9},
		{32,9},
		{48,9},
		{2,1}
	};

	std::string GetSimpliedAspectRatio(int width, int height, double tolerance = 0.01)
	{
		if (width <= 0 || height <= 0)
		{
			throw std::invalid_argument("Width and height must be positive");
		}

		int g = std::gcd(width, height);
		int reducedW = width / g;
		int reducedH = height / g;

		double actual = static_cast<double>(width) / height;

		for (const auto& ratio : KnownRatios)
		{
			double known = static_cast<double>(ratio.w) / ratio.h;
			double relDiff = std::abs(known - actual) / known;

			if (relDiff <= tolerance)
			{
				std::ostringstream oss;
				oss << ratio.w << "/" << ratio.h;
				return oss.str();
			}
		}

		std::ostringstream oss;
		oss << reducedW << "/" << reducedH;
		return oss.str();
	}
}