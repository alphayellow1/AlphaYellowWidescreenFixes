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

template<typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template<typename T>
concept Integral = std::is_integral_v<T>;

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

	std::vector<int> pattern_to_byte(const char* pattern)
	{
		auto bytes = std::vector<int>{};

		auto start = const_cast<char*>(pattern);

		auto end = const_cast<char*>(pattern) + strlen(pattern);

		for (auto current = start; current < end; ++current)
		{
			if (*current == '?')
			{
				++current;

				if (*current == '?')
				{
					++current;
				}

				bytes.push_back(-1);
			}
			else
			{
				bytes.push_back(strtoul(current, &current, 16));
			}
		}

		return bytes;
	}

	thread_local std::vector<std::string> g_last_scan_signatures;

	std::uint8_t* PatternScan(void* module, const char* signature)
	{
		auto dosHeader = (PIMAGE_DOS_HEADER)module;

		auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)module + dosHeader->e_lfanew);

		auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;

		auto patternBytes = pattern_to_byte(signature);

		auto scanBytes = reinterpret_cast<std::uint8_t*>(module);

		auto s = patternBytes.size();

		auto d = patternBytes.data();

		for (auto i = 0ul; i < sizeOfImage - s; ++i)
		{
			bool found = true;

			for (auto j = 0ul; j < s; ++j)
			{
				if (scanBytes[i + j] != d[j] && d[j] != -1)
				{
					found = false;

					break;
				}
			}
			if (found == true)
			{
				return &scanBytes[i];
			}
		}

		return nullptr;
	}

	template<typename... Sigs, typename = std::enable_if_t<(std::is_convertible_v<Sigs, const char*> && ...)>>
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

	template<typename... Args, typename = std::enable_if_t<!std::conjunction_v<std::is_convertible<std::decay_t<Args>, const char*>...>, int>>
	std::vector<std::uint8_t*> PatternScan(void* firstModule, Args... rest)
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

		std::vector<std::uint8_t*> results;
		results.reserve(items.size());

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

				g_last_scan_signatures.emplace_back(sig);

				if (!currentModule)
				{
					spdlog::error("PatternScan: Signature provided without a preceding module pointer (sig index {})", g_last_scan_signatures.size() - 1);
					results.push_back(nullptr);
					continue;
				}

				results.push_back(PatternScan(currentModule, sig));
			}
		}

		return results;
	}

	inline bool AreAllSignaturesValid(const std::vector<std::uint8_t*>& addrs) noexcept
	{
		if (addrs.empty())
		{
			spdlog::error("PatternScan: The address vector is empty.");
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

		std::string msg = fmt::format("PatternScan: {} signature(s) not found. Missing indices: ", missing.size());

		for (std::size_t k = 0; k < missing.size(); ++k)
		{
			if (k > 0) msg += ", ";

			msg += fmt::format("{}", missing[k]);

			if (g_last_scan_signatures.size() == addrs.size())
			{
				msg += fmt::format(" (\"{}\")", g_last_scan_signatures[missing[k]]);
			}
		}

		spdlog::error("{}", msg);
		return false;
	}

	template<typename PartT, typename FullT = uintptr_t>
	inline PartT ReadRegister(FullT full, unsigned start_bit, unsigned end_bit) noexcept
	{
		const unsigned full_bits = sizeof(FullT) * 8;

		const unsigned width = end_bit - start_bit + 1u;

		assert(width <= sizeof(PartT) * 8u);

		FullT mask;

		if (width >= full_bits) mask = ~FullT(0);

		else mask = ((FullT(1) << (width - 1)) << 1) - FullT(1);

		FullT sliced = (full >> start_bit) & mask;

		if constexpr (std::is_signed_v<PartT>)
		{
			using UnsignedPart = std::make_unsigned_t<PartT>;
			UnsignedPart u = static_cast<UnsignedPart>(sliced);
			PartT out;
			std::memcpy(&out, &u, sizeof(out)); // preserve bit pattern
			return out;
		}
		else
		{
			return static_cast<PartT>(sliced);
		}
	}

	template<typename PartT, typename FullT = uintptr_t>
	inline void WriteRegister(FullT& target, unsigned start_bit, unsigned end_bit, PartT value) noexcept
	{
		static_assert(std::is_integral_v<PartT>, "PartT must be integral");
		static_assert(std::is_unsigned_v<FullT>, "FullT must be unsigned");

		const unsigned full_bits = sizeof(FullT) * 8;
		assert(start_bit <= end_bit && end_bit < full_bits);

		const unsigned width = end_bit - start_bit + 1u;
		assert(width <= sizeof(PartT) * 8u);

		// Build mask for bit range
		FullT mask;
		if (width >= full_bits) mask = ~FullT(0);
		else mask = ((FullT(1) << (width - 1)) << 1) - FullT(1);

		FullT shifted_mask = (width >= full_bits) ? ~FullT(0) : (mask << start_bit);

		// Convert value to unsigned type (preserve bits if signed)
		using UnsignedPart = std::make_unsigned_t<PartT>;
		UnsignedPart u;
		if constexpr (std::is_signed_v<PartT>)
			std::memcpy(&u, &value, sizeof(u)); // bit-preserve signed pattern
		else
			u = static_cast<UnsignedPart>(value);

		FullT value_masked = (static_cast<FullT>(u) & mask) << start_bit;

		target = (target & ~shifted_mask) | value_masked;
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
		{
			return nullptr;
		}

		std::int32_t offset = *reinterpret_cast<std::int32_t*>(address);

		std::uint8_t* absoluteAddress = address + 4 + offset;

		return absoluteAddress;
	}

	static HMODULE GetHandle(const std::string& moduleName, unsigned int retryDelayMs = 200, int maxAttempts = 0)
	{
		// attempt counter
		int attempts = 0;

		while (true)
		{
			HMODULE h = GetModuleHandleA(moduleName.c_str());
			if (h != nullptr)
			{
				spdlog::info("Obtained handle for {}: 0x{:x}", moduleName, reinterpret_cast<uintptr_t>(h));
				return h;
			}

			++attempts;

			// If maxAttempts > 0 and we've exhausted them, log and return nullptr
			if (maxAttempts > 0 && attempts >= maxAttempts)
			{
				spdlog::error("Failed to get handle of {}.", moduleName);
				return nullptr;
			}

			// Sleep before trying again
			std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
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
		if (!address) return nullptr;

		if (mode == PointerMode::Absolute)
		{
			T raw = *reinterpret_cast<T*>(address);
			return reinterpret_cast<std::uint8_t*>(raw);
		}
		else // Relative
		{
			T disp = *reinterpret_cast<T*>(address);
			return address + sizeof(T) + disp;
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

extern "C" {
	void FILD16_from_ptr(void* p);
	void FILD32_from_ptr(void* p);
	void FILD64_from_ptr(void* p);

	void FIADD16_from_ptr(void* p);
	void FIADD32_from_ptr(void* p);
	void FIADD64_from_ptr(void* p);

	void FISUB16_from_ptr(void* p);
	void FISUB32_from_ptr(void* p);
	void FISUB64_from_ptr(void* p);

	void FISUBR16_from_ptr(void* p);
	void FISUBR32_from_ptr(void* p);
	void FISUBR64_from_ptr(void* p);

	void FIMUL16_from_ptr(void* p);
	void FIMUL32_from_ptr(void* p);
	void FIMUL64_from_ptr(void* p);

	void FIDIV16_from_ptr(void* p);
	void FIDIV32_from_ptr(void* p);
	void FIDIV64_from_ptr(void* p);

	void FIDIVR16_from_ptr(void* p);
	void FIDIVR32_from_ptr(void* p);
	void FIDIVR64_from_ptr(void* p);

	void FICOMP16_from_ptr(void* p);
	void FICOMP32_from_ptr(void* p);
	void FICOMP64_from_ptr(void* p);

	void FSIN_from_ptr(void* p);
	void FCOS_from_ptr(void* p);
	void FSINCOS_from_ptr(void* p);

	void FPTAN_from_ptr(void* p);
	void FPATAN_from_ptr(void* p);

	void FPREM_from_ptr(void* p);
	void FPREM1_from_ptr(void* p);

	void FYL2X_from_ptr(void* p);
	void FYL2XP1_from_ptr(void* p);

	void FSCALE_from_ptr(void* p);
	void FSQRT_from_ptr(void* p);

	void FLD_f32_from_ptr(void* p);
	void FADD_f32_from_ptr(void* p);
	void FSUB_f32_from_ptr(void* p);
	void FSUBR_f32_from_ptr(void* p);
	void FMUL_f32_from_ptr(void* p);
	void FDIV_f32_from_ptr(void* p);
	void FDIVR_f32_from_ptr(void* p);
	void FCOMP_f32_from_ptr(void* p);

	void FLD_f64_from_ptr(void* p);
	void FADD_f64_from_ptr(void* p);
	void FSUB_f64_from_ptr(void* p);
	void FSUBR_f64_from_ptr(void* p);
	void FMUL_f64_from_ptr(void* p);
	void FDIV_f64_from_ptr(void* p);
	void FDIVR_f64_from_ptr(void* p);
	void FCOMP_f64_from_ptr(void* p);

	void preserve_FIADD32_from_ptr(void* p);
	void preserve_FISUB32_from_ptr(void* p);
	void preserve_FIMUL32_from_ptr(void* p);
	void preserve_FIDIV32_from_ptr(void* p);
	void preserve_FIDIVR32_from_ptr(void* p);

	void preserve_FLD_f32_from_ptr(void* p);
	void preserve_FADD_f32_from_ptr(void* p);
	void preserve_FMUL_f32_from_ptr(void* p);
	void preserve_FDIV_f32_from_ptr(void* p);
}

namespace FPU
{
	template<typename T>
	inline void FILD(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FILD requires integer type");
		if constexpr (sizeof(T) == 2) FILD16_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 4) FILD32_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 8) FILD64_from_ptr((void*)std::addressof(v));
	}

	template<typename T>
	inline void FIADD(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FIADD requires integer type");
		if constexpr (sizeof(T) == 2) FIADD16_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 4) FIADD32_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 8) FIADD64_from_ptr((void*)std::addressof(v));
	}

	template<typename T>
	inline void FISUB(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FISUB requires integer type");
		if constexpr (sizeof(T) == 2) FISUB16_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 4) FISUB32_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 8) FISUB64_from_ptr((void*)std::addressof(v));
	}

	template<typename T>
	inline void FISUBR(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FISUBR requires integer type");
		if constexpr (sizeof(T) == 2) FISUBR16_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 4) FISUBR32_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 8) FISUBR64_from_ptr((void*)std::addressof(v));
	}

	template<typename T>
	inline void FIMUL(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FIMUL requires integer type");
		if constexpr (sizeof(T) == 2) FIMUL16_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 4) FIMUL32_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 8) FIMUL64_from_ptr((void*)std::addressof(v));
	}

	template<typename T>
	inline void FIDIV(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FIDIV requires integer type");
		if constexpr (sizeof(T) == 2) FIDIV16_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 4) FIDIV32_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 8) FIDIV64_from_ptr((void*)std::addressof(v));
	}

	template<typename T>
	inline void FIDIVR(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FIDIVR requires integer type");
		if constexpr (sizeof(T) == 2) FIDIVR16_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 4) FIDIVR32_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 8) FIDIVR64_from_ptr((void*)std::addressof(v));
	}

	template<typename T>
	inline void FICOMP(const T& v)
	{
		static_assert(std::is_integral_v<T>, "FICOMP requires integer type");
		if constexpr (sizeof(T) == 2) FICOMP16_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 4) FICOMP32_from_ptr((void*)std::addressof(v));
		else if constexpr (sizeof(T) == 8) FICOMP64_from_ptr((void*)std::addressof(v));
		else static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Unsupported integer size for FICOMP");
	}

	inline void FLD(const float& v) { FLD_f32_from_ptr((void*)std::addressof(v)); }
	inline void FADD(const float& v) { FADD_f32_from_ptr((void*)std::addressof(v)); }
	inline void FSUB(const float& v) { FSUB_f32_from_ptr((void*)std::addressof(v)); }
	inline void FSUBR(const float& v) { FSUBR_f32_from_ptr((void*)std::addressof(v)); }
	inline void FMUL(const float& v) { FMUL_f32_from_ptr((void*)std::addressof(v)); }
	inline void FDIV(const float& v) { FDIV_f32_from_ptr((void*)std::addressof(v)); }
	inline void FDIVR(const float& v) { FDIVR_f32_from_ptr((void*)std::addressof(v)); }
	inline void FCOMP(const float& v) { FCOMP_f32_from_ptr((void*)std::addressof(v)); }

	inline void FLD(const double& v) { FLD_f64_from_ptr((void*)std::addressof(v)); }
	inline void FADD(const double& v) { FADD_f64_from_ptr((void*)std::addressof(v)); }
	inline void FSUB(const double& v) { FSUB_f64_from_ptr((void*)std::addressof(v)); }
	inline void FSUBR(const double& v) { FSUBR_f64_from_ptr((void*)std::addressof(v)); }
	inline void FMUL(const double& v) { FMUL_f64_from_ptr((void*)std::addressof(v)); }
	inline void FDIV(const double& v) { FDIV_f64_from_ptr((void*)std::addressof(v)); }
	inline void FDIVR(const double& v) { FDIVR_f64_from_ptr((void*)std::addressof(v)); }
	inline void FCOMP(const double& v) { FCOMP_f64_from_ptr((void*)std::addressof(v)); }

	inline void FSIN() { FSIN_from_ptr((void*)nullptr); }
	inline void FCOS() { FCOS_from_ptr((void*)nullptr); }
	inline void FSINCOS() { FSINCOS_from_ptr((void*)nullptr); }

	inline void FPTAN() { FPTAN_from_ptr((void*)nullptr); }
	inline void FPATAN() { FPATAN_from_ptr((void*)nullptr); }

	inline void FPREM() { FPREM_from_ptr((void*)nullptr); }
	inline void FPREM1() { FPREM1_from_ptr((void*)nullptr); }

	inline void FYL2X() { FYL2X_from_ptr((void*)nullptr); }
	inline void FYL2XP1() { FYL2XP1_from_ptr((void*)nullptr); }

	inline void FSCALE() { FSCALE_from_ptr((void*)nullptr); }
	inline void FSQRT() { FSQRT_from_ptr((void*)nullptr); }

	inline void P_FIADD32(const int32_t& v) { preserve_FIADD32_from_ptr((void*)std::addressof(v)); }
	inline void P_FISUB32(const int32_t& v) { preserve_FISUB32_from_ptr((void*)std::addressof(v)); }
	inline void P_FIMUL32(const int32_t& v) { preserve_FIMUL32_from_ptr((void*)std::addressof(v)); }
	inline void P_FIDIV32(const int32_t& v) { preserve_FIDIV32_from_ptr((void*)std::addressof(v)); }
	inline void P_FIDIVR32(const int32_t& v) { preserve_FIDIVR32_from_ptr((void*)std::addressof(v)); }

	inline void P_FLD_f32(const float& v) { preserve_FLD_f32_from_ptr((void*)std::addressof(v)); }
	inline void P_FADD_f32(const float& v) { preserve_FADD_f32_from_ptr((void*)std::addressof(v)); }
	inline void P_FMUL_f32(const float& v) { preserve_FMUL_f32_from_ptr((void*)std::addressof(v)); }
	inline void P_FDIV_f32(const float& v) { preserve_FDIV_f32_from_ptr((void*)std::addressof(v)); }
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
		int count = 0;

		if (number == 0)
		{
			return 1;
		}

		if constexpr (std::is_signed_v<Int>)
		{
			if (number < 0) number = -number;
		}

		while (number != 0)
		{
			number /= 10;
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
	inline bool isClose(T originalValue,
		T comparedValue,
		U toleranceCheck = static_cast<U>(Maths::defaultTolerance),
		ComparisonOperator op = ComparisonOperator::LessThan,
		bool inclusive = false)
	{
		using Common = std::common_type_t<T, U>;
		Common diff = std::abs(static_cast<Common>(originalValue) - static_cast<Common>(comparedValue));
		Common tol = static_cast<Common>(toleranceCheck);

		if (op == ComparisonOperator::LessThan)
			return inclusive ? (diff <= tol) : (diff < tol);
		else
			return inclusive ? (diff >= tol) : (diff > tol);
	}

	template<Arithmetic T>
	inline bool isClose(T a, T b, ComparisonOperator op, bool inclusive = false)
	{
		using DefaultTolType = std::common_type_t<T, float>;
		return isClose<T, DefaultTolType>(a, b, static_cast<DefaultTolType>(Maths::defaultTolerance), op, inclusive);
	}
}
