#include "stdafx.h"
#include <cmath>
#include <string>
#include <cstdint>
#include <charconv>
#include <type_traits>
#include <windows.h>
#include <cstring>
#include <array>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <spdlog/spdlog.h>
#include <variant>
#include <bit>
#include <chrono>
#include <cctype>
#include <future>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <initializer_list>
#include <thread>
#include <filesystem>
#include <stdexcept>
#include <cassert>
#include <limits>
#include <algorithm>

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
	template<typename T>
	void Write(std::uint8_t* writeAddress, T value)
	{
		DWORD oldProtect = 0;

		if (VirtualProtect(writeAddress, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect) == false)
		{
			spdlog::error("VirtualProtect failed in Memory::Write");
			return;
		}

		std::memcpy(writeAddress, &value, sizeof(T));

		FlushInstructionCache(GetCurrentProcess(), writeAddress, sizeof(T));

		DWORD dummy = 0;

		VirtualProtect(writeAddress, sizeof(T), oldProtect, &dummy);
	}

	void PatchBytes(std::uint8_t* address, const char* pattern, unsigned int numBytes)
	{
		DWORD oldProtect;

		if (VirtualProtect(address, numBytes, PAGE_EXECUTE_READWRITE, &oldProtect) == false)
		{
			spdlog::error("VirtualProtect failed in Memory::PatchBytes");
			return;
		}

		std::memcpy(address, pattern, numBytes);

		FlushInstructionCache(GetCurrentProcess(), address, numBytes);

		VirtualProtect(address, numBytes, oldProtect, &oldProtect);
	}

	inline void WriteNOPs(std::uint8_t* address, std::size_t numBytes)
	{
		if (!address || numBytes == 0)
		{
			return;
		}
		
		constexpr std::size_t MaxStackNops = 16;

		if (numBytes <= MaxStackNops)
		{
			std::uint8_t buf[MaxStackNops];

			std::memset(buf, 0x90, numBytes);

			PatchBytes(address, reinterpret_cast<const char*>(buf), static_cast<unsigned int>(numBytes));
		}
		else
		{
			std::vector<std::uint8_t> nops(numBytes, 0x90);

			PatchBytes(address, reinterpret_cast<const char*>(nops.data()), static_cast<unsigned int>(numBytes));
		}
	}

	bool PatchCallRel32(uint8_t* callSite, uint8_t* target, std::array<uint8_t, 5>* out_orig)
	{
		if (!callSite || !target)
		{
			return false;
		}

		if (callSite[0] != 0xE8)
		{
			return false;
		}

		if (out_orig) std::memcpy(out_orig->data(), callSite, 5);

		intptr_t nextInstr = reinterpret_cast<intptr_t>(callSite) + 5;

		intptr_t rel64 = reinterpret_cast<intptr_t>(target) - nextInstr;

		if (rel64 < INT32_MIN || rel64 > INT32_MAX)
		{
			return false;
		}

		int32_t rel32 = static_cast<int32_t>(rel64);

		uint8_t patch[5];

		patch[0] = 0xE8;

		std::memcpy(&patch[1], &rel32, sizeof(rel32));

		DWORD oldProt;

		if (!VirtualProtect(callSite, 5, PAGE_EXECUTE_READWRITE, &oldProt))
		{
			return false;
		}

		std::memcpy(callSite, patch, 5);

		FlushInstructionCache(GetCurrentProcess(), callSite, 5);

		VirtualProtect(callSite, 5, oldProt, &oldProt);

		return true;
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
		if (!module || !signature) 
		{
			return nullptr;
		}

		auto patternBytes = get_pattern_bytes_cached(signature);

		const std::size_t s = patternBytes.size();

		if (s == 0)
		{
			return nullptr;
		}

		std::size_t anchor_index = SIZE_MAX;

		int anchor_value = -1;

		for (std::size_t i = 0; i < s; ++i)
		{
			if (patternBytes[i] != -1)
			{
				anchor_index = i; anchor_value = patternBytes[i];
				break;
			}
		}

		bool all_wildcards = (anchor_index == SIZE_MAX);

		auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(module);

		if (!dosHeader || dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		{
			return nullptr;
		}

		auto ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS_X*>(reinterpret_cast<std::uint8_t*>(module) + dosHeader->e_lfanew);

		if (!ntHeaders || ntHeaders->Signature != IMAGE_NT_SIGNATURE)
		{
			return nullptr;
		}

		PIMAGE_SECTION_HEADER firstSection = IMAGE_FIRST_SECTION(ntHeaders);

		unsigned numSections = ntHeaders->FileHeader.NumberOfSections;

		std::vector<unsigned> order;

		order.reserve(numSections);

		for (unsigned i = 0; i < numSections; ++i)
		{
			PIMAGE_SECTION_HEADER sh = &firstSection[i];

			if (std::strncmp(reinterpret_cast<const char*>(sh->Name), ".text", 5) == 0 || (sh->Characteristics & IMAGE_SCN_CNT_CODE))
			{
				order.push_back(i);
			}
		}

		for (unsigned i = 0; i < numSections; ++i)
		{
			if (std::find(order.begin(), order.end(), i) == order.end())
			{
				order.push_back(i);
			}
		}

		for (unsigned idx = 0; idx < order.size(); ++idx)
		{
			PIMAGE_SECTION_HEADER sh = &firstSection[order[idx]];

			std::uint8_t* secBase = reinterpret_cast<std::uint8_t*>(module) + sh->VirtualAddress;

			std::size_t secSize = static_cast<std::size_t>(sh->Misc.VirtualSize);

			if (secSize == 0)
			{
				secSize = static_cast<std::size_t>(sh->SizeOfRawData);
			}
			if (secSize == 0)
			{
				continue;
			}
			if (secSize > 0x80000000u)
			{
				continue;
			}
			if (s > secSize)
			{
				continue;
			}

			std::uintptr_t secStart = reinterpret_cast<std::uintptr_t>(secBase);

			std::uintptr_t secEnd = secStart + secSize;

			std::uintptr_t queryPtr = secStart;

			while (queryPtr < secEnd)
			{
				MEMORY_BASIC_INFORMATION mbi{};

				if (VirtualQuery(reinterpret_cast<LPCVOID>(queryPtr), &mbi, sizeof(mbi)) == 0)
				{
					break;
				}

				std::uintptr_t regionBase = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);

				std::uintptr_t regionTop = regionBase + mbi.RegionSize;

				std::uintptr_t scanStart = (std::max)(regionBase, secStart);

				std::uintptr_t scanEnd = (std::min)(regionTop, secEnd);

				if ((mbi.State & MEM_COMMIT) && scanEnd > scanStart)
				{
					std::uint8_t* rStart = reinterpret_cast<std::uint8_t*>(scanStart);

					std::uint8_t* rEnd = reinterpret_cast<std::uint8_t*>(scanEnd);

					if (all_wildcards)
					{
						return rStart;
					}

					std::uint8_t* lastPossible = rEnd - s;

					if (lastPossible < rStart)
					{

					}
					else 
					{
						std::uint8_t* memchr_search_start = rStart + anchor_index;

						std::uint8_t* memchr_search_end = lastPossible + anchor_index;

						std::uint8_t* p = memchr_search_start;

						while (p <= memchr_search_end)
						{
							void* found = std::memchr(p, static_cast<int>(anchor_value), static_cast<std::size_t>(memchr_search_end - p + 1));

							if (!found)
							{
								break;
							}

							std::uint8_t* foundByte = reinterpret_cast<std::uint8_t*>(found);

							std::uint8_t* candidate = foundByte - anchor_index;

							bool ok = true;

							for (std::size_t j = 0; j < s; ++j)
							{
								int pb = patternBytes[j];

								if (pb != -1 && candidate[j] != static_cast<std::uint8_t>(pb))
								{
									ok = false;
									break;
								}
							}

							if (ok)
							{
								return candidate;
							}

							p = foundByte + 1;
						}
					}
				}

				if (regionTop <= queryPtr)
				{
					break;
				}

				queryPtr = regionTop;
			}
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

	template<typename PartT, typename FullT = uintptr_t>
	inline PartT ReadRegister(FullT full, unsigned start_bit, unsigned end_bit) noexcept
	{
		const unsigned full_bits = sizeof(FullT) * 8;

		const unsigned width = end_bit - start_bit + 1u;

		assert(start_bit <= end_bit && end_bit < full_bits);

		assert(width <= sizeof(PartT) * 8u);

		FullT mask = (width >= full_bits) ? ~FullT(0) : ((FullT(1) << width) - FullT(1));

		FullT sliced = (full >> start_bit) & mask;

		if constexpr (std::is_signed_v<PartT>)
		{
			using UnsignedPart = std::make_unsigned_t<PartT>;

			UnsignedPart u = static_cast<UnsignedPart>(sliced);

			if (width > 0)
			{
				UnsignedPart sign_bit = UnsignedPart(1) << (width - 1);

				if (u & sign_bit)
				{
					UnsignedPart sign_mask = ~((UnsignedPart(1) << width) - UnsignedPart(1));
					u |= sign_mask;
				}
			}

			PartT out;

			std::memcpy(&out, &u, sizeof(out));

			return out;
		}
		else
		{
			return static_cast<PartT>(sliced);
		}
	}

	template<typename PartT, typename FullT = uintptr_t>
	inline PartT ReadRegister(FullT full) noexcept
	{
		constexpr unsigned full_bits = sizeof(FullT) * 8u;

		static_assert(sizeof(PartT) * 8u >= full_bits, "PartT too small to hold full register");

		if constexpr (std::is_signed_v<PartT>)
		{
			return static_cast<PartT>(full);
		}
		else
		{
			return static_cast<PartT>(full);
		}
	}

	template<typename PartT, typename FullT = uintptr_t>
	inline FullT WriteRegister(FullT full, PartT value, unsigned start_bit, unsigned end_bit) noexcept
	{
		const unsigned full_bits = sizeof(FullT) * 8u;

		const unsigned width = end_bit - start_bit + 1u;

		assert(start_bit <= end_bit && end_bit < full_bits);

		assert(width <= sizeof(PartT) * 8u);

		FullT field_mask = (width >= full_bits) ? ~FullT(0) : ((FullT(1) << width) - FullT(1));

		field_mask <<= start_bit;

		full &= ~field_mask;

		using UnsignedPart = std::make_unsigned_t<PartT>;

		UnsignedPart u;

		std::memcpy(&u, &value, sizeof(u));

		FullT shifted = (static_cast<FullT>(u) & ((FullT(1) << width) - 1u)) << start_bit;

		return full | shifted;
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

	template<typename T>
	std::uint8_t* GetPointerFromAddress(std::uint8_t* address, PointerMode mode) noexcept
	{
		if (!address)
		{
			return nullptr;
		}
		
		T raw;

		std::memcpy(&raw, address, sizeof(raw));

		if (mode == PointerMode::Absolute)
		{
			return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(raw));
		}			
		else
		{
			using signed_t = std::make_signed_t<T>;

			signed_t disp;

			std::memcpy(&disp, address, sizeof(disp));

			return address + sizeof(T) + static_cast<std::intptr_t>(disp);
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
}