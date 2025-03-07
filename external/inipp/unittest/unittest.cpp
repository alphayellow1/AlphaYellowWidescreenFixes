#include "test.h"

#define TEST_CLASS(T) class T
#define TEST_METHOD(Func) void Func()

namespace Assert {

	void IsTrue(bool result) {
		if (!result) throw std::runtime_error("expected true");
	}

	void IsFalse(bool result) {
		if (result) throw std::runtime_error("expected false");
	}

	template <typename T>
	void AreEqual(const T & a, const T & b) {
		if (a != b) throw std::runtime_error("expected equality");
	}

} // namespace Assert

namespace Logger {

	void WriteMessage(const char *msg) {
		std::wcout << msg;
	}

	void WriteMessage(const wchar_t *msg) {
		std::wcout << msg;
	}

} // namespace Logger

template <class CharT>
void WriteMessage(const Ini<CharT> & ini) {
	std::basic_ostringstream<CharT> os;
	ini.generate(os);
	Logger::WriteMessage(os.str().c_str());
}

class MixedFormat : public Format<char> {
public:
	virtual bool is_comment(char ch) const {
		return ch == ';' || ch == '\'' || ch == '#';
	}
};

namespace unittest
{		
	TEST_CLASS(UnitTest)
	{
	public:
		
		TEST_METHOD(TestParseGenerate1)
		{
			std::basic_ostringstream<char> os;
			Assert::IsTrue(runtest<char>("test1.ini", "test1.output", os));
			Logger::WriteMessage(os.str().c_str());
		}

		TEST_METHOD(TestParseGenerate1W)
		{
			std::basic_ostringstream<wchar_t> os;
			Assert::IsTrue(runtest<wchar_t>("test1.ini", "test1.output", os));
			Logger::WriteMessage(os.str().c_str());
		}

		TEST_METHOD(TestParseGenerate2)
		{
			std::basic_ostringstream<char> os;
			Assert::IsTrue(runtest<char>("test2.ini", "test2.output", os));
			Logger::WriteMessage(os.str().c_str());
		}

		TEST_METHOD(TestParseGenerate2W)
		{
			std::basic_ostringstream<wchar_t> os;
			Assert::IsTrue(runtest<wchar_t>("test2.ini", "test2.output", os));
			Logger::WriteMessage(os.str().c_str());
		}

		TEST_METHOD(TestParseGenerate3)
		{
			std::basic_ostringstream<char> os;
			Assert::IsTrue(runtest<char>("test3.ini", "test3.output", os));
			Logger::WriteMessage(os.str().c_str());
		}

		TEST_METHOD(TestParseGenerate4)
		{
			std::basic_ostringstream<char> os;
			Assert::IsTrue(runtest<char>("test4.ini", "test4.output", os));
			Logger::WriteMessage(os.str().c_str());
		}

		TEST_METHOD(TestInterpolate1)
		{
			Ini<char> ini;
			ini.sections["sec1"]["x"] = "${sec2:z}";
			ini.sections["sec1"]["y"] = "2";
			ini.sections["sec2"]["z"] = "${y}";
			ini.interpolate();
			// we do not want x to be 2
			Assert::AreEqual(ini.sections.at("sec1").at("x"), std::string("${y}"));
			ini.generate(std::cout);
		}

		TEST_METHOD(TestInfiniteRecursion1)
		{
			Ini<char> ini;
			ini.sections["test"]["x"] = "0 ${y}";
			ini.sections["test"]["y"] = "1 ${x}";
			ini.interpolate();
			WriteMessage(ini);
		}

		TEST_METHOD(TestInfiniteRecursion2)
		{
			Ini<char> ini;
			ini.sections["test1"]["x"] = "0 ${test2:y}";
			ini.sections["test2"]["y"] = "1 ${test1:x}";
			ini.interpolate();
			WriteMessage(ini);
		}

		TEST_METHOD(TestInfiniteRecursion3)
		{
			Ini<char> ini;
			ini.sections["test1"]["x"] = "${test2:x}";
			ini.sections["test2"]["x"] = "${test3:x}";
			ini.sections["test3"]["x"] = "${test4:x}";
			ini.sections["test4"]["x"] = "x${test1:x}";
			ini.interpolate();
			WriteMessage(ini);
		}

		TEST_METHOD(TestExtract)
		{
			std::string       str{ "oops" };
			int16_t           i16{ 0 };
			int32_t           i32{ 0 };
			bool              bool_{ true };
			Assert::IsTrue(extract(std::string(), str));
			Assert::AreEqual(std::string{ }, str);
			Assert::IsTrue(extract(std::string{ "hello" }, str));
			Assert::AreEqual(std::string{ "hello" }, str);
			Assert::IsTrue(extract(std::string{ "hello world" }, str));
			Assert::AreEqual(std::string{ "hello world" }, str);
			Assert::IsTrue(extract(std::string{ "-10" }, i16));
			Assert::AreEqual(int16_t{ -10 }, i16);
			Assert::IsTrue(extract(std::string{ "false" }, bool_));
			Assert::AreEqual(false, bool_);
			Assert::IsTrue(extract(std::string{ "-10" }, i16));
			Assert::AreEqual(int16_t{ -10 }, i16);
			Assert::IsFalse(extract(std::string{ "xxx" }, i16));
			Assert::AreEqual(int16_t{ -10 }, i16);
			Assert::IsFalse(extract(std::string{ "1000000" }, i16));
			Assert::AreEqual(int16_t{ -10 }, i16);
			Assert::IsFalse(extract(std::string{ "-20 xxx" }, i16));
			Assert::AreEqual(int16_t{ -10 }, i16);
			Assert::IsTrue(extract(std::string{ "1000000" }, i32));
			Assert::AreEqual(int32_t{ 1000000 }, i32);
		}

		TEST_METHOD(TestDefault)
		{
			Ini<char> ini;
			ini.sections["sec0"]["a"] = "0";
			ini.sections["sec0"]["b"] = "1";
			ini.sections["sec1"]["b"] = "2";
			ini.sections["sec1"]["c"] = "${a} ${b}";
			ini.sections["sec2"]["a"] = "3";
			ini.sections["sec2"]["c"] = "${a} ${b}";
			ini.default_section(ini.sections.at("sec0"));
			ini.interpolate();
			WriteMessage(ini);
			Assert::AreEqual(ini.sections.at("sec1").at("c"), std::string("0 2"));
			Assert::AreEqual(ini.sections.at("sec2").at("c"), std::string("3 1"));
		}

		TEST_METHOD(TestHashComment1)
		{
			Ini<char> ini;
			std::istringstream ss("# hello world\n[default]\na=2");
			Assert::IsFalse(ini.format->is_comment('#'));
			ini.parse(ss);
			WriteMessage(ini);
			Assert::AreEqual(ini.sections.at("default").at("a"), std::string("2"));
			Assert::IsFalse(ini.errors.empty());
		}

		TEST_METHOD(TestHashComment2)
		{
			auto fmt = std::make_shared<Format<char>>('[', ']', '=', '#', '$', '{', ':', '}');
			Ini<char> ini(fmt);
			Assert::IsTrue(ini.format->is_comment('#'));
			std::istringstream ss("# hello world\n[default]\na=2");
			ini.parse(ss);
			WriteMessage(ini);
			Assert::AreEqual(ini.sections.at("default").at("a"), std::string("2"));
			Assert::IsTrue(ini.errors.empty());
		}

		TEST_METHOD(TestMixedComment)
		{
			auto fmt = std::make_shared<MixedFormat>();
			Ini<char> ini(fmt);
			Assert::IsTrue(ini.format->is_comment('#'));
			std::istringstream ss("# hello world\n; hello world\n' hello world\n[default]\na=2");
			ini.parse(ss);
			WriteMessage(ini);
			Assert::AreEqual(ini.sections.at("default").at("a"), std::string("2"));
			Assert::IsTrue(ini.errors.empty());
		}

		TEST_METHOD(TestGetValue)
		{
			Ini<char> ini;
			std::istringstream ss("[sec1]\na=2\nb=bla");
			ini.parse(ss);
			WriteMessage(ini);
			const auto & sec = ini.sections.at("sec1");
			int a = 4;
			int b = 7;
			int c = 10;
			Assert::IsTrue(get_value(sec, "a", a));
			Assert::AreEqual(a, 2);
			Assert::IsFalse(get_value(sec, "b", b));
			Assert::AreEqual(b, 7);
			Assert::IsFalse(get_value(sec, "c", c));
			Assert::AreEqual(c, 10);
		}

		TEST_METHOD(TestTrailingComments)
		{
			Ini<char> ini;
			std::istringstream ss("[sec1]\n; line comment\na=2\nb=3 ; trailing comment\nc=4 ; more ; comments ; here");
			ini.parse(ss);
			WriteMessage(ini);
			Assert::IsTrue(ini.errors.empty());
			const auto& sec = ini.sections.at("sec1");
			Assert::AreEqual(sec.at("a"), std::string("2"));
			Assert::AreEqual(sec.at("b"), std::string("3 ; trailing comment"));
			Assert::AreEqual(sec.at("c"), std::string("4 ; more ; comments ; here"));
			ini.strip_trailing_comments();
			WriteMessage(ini);
			Assert::AreEqual(sec.at("a"), std::string("2"));
			Assert::AreEqual(sec.at("b"), std::string("3"));
			Assert::AreEqual(sec.at("c"), std::string("4"));
		}
	};
} // namespace unittest

int main() {
	unittest::UnitTest test;
	test.TestParseGenerate1();
	test.TestParseGenerate1W();
	test.TestParseGenerate2();
	test.TestParseGenerate2W();
	test.TestParseGenerate3();
	test.TestParseGenerate4();
	test.TestInfiniteRecursion1();
	test.TestInfiniteRecursion2();
	test.TestInfiniteRecursion3();
	test.TestInterpolate1();
	test.TestExtract();
	test.TestDefault();
	test.TestHashComment1();
	test.TestHashComment2();
	test.TestMixedComment();
	test.TestGetValue();
	test.TestTrailingComments();
	return 0;
}
