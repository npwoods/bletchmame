/***************************************************************************

    utility_test.cpp

    Unit tests for utility.cpp

***************************************************************************/

#include "utility.h"
#include "test.h"

namespace
{
    class Test : public QObject
    {
        Q_OBJECT

    private slots:
		void string_split_string() { string_split<std::string>(); }
		void string_split_wstring() { string_split<std::wstring>(); }
		void binaryFromHex();

		void enum_parser();
		void return_value_substitutor();

	private:
		template<typename TStr>
		void string_split()
		{
			TStr str = build_string<TStr>("Alpha,Bravo,Charlie");
			auto result = util::string_split(str, [](auto ch) { return ch == ','; });
			QVERIFY(result[0] == build_string<TStr>("Alpha"));
			QVERIFY(result[1] == build_string<TStr>("Bravo"));
			QVERIFY(result[2] == build_string<TStr>("Charlie"));
		}

		//-------------------------------------------------
		//  build_string
		//-------------------------------------------------
		template<typename TStr>
		static TStr build_string(const char *string)
		{
			// get the string into a TStr
			TStr str;
			for (int i = 0; string[i]; i++)
				str += string[i];
			return str;
		}
	};
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  enum_parser
//-------------------------------------------------

void Test::enum_parser()
{
	static const util::enum_parser<int> parser =
	{
		{ "fourtytwo", 42 },
		{ "twentyone", 21 }
	};

	bool result;
	int value;

	result = parser(u8"fourtytwo", value);
	QVERIFY(result);
	QVERIFY(value == 42);

	result = parser(u8"twentyone", value);
	QVERIFY(result);
	QVERIFY(value == 21);

	result = parser(u8"invalid", value);
	QVERIFY(!result);
	QVERIFY(value == 0);
}


//-------------------------------------------------
//  return_value_substitutor
//-------------------------------------------------

void Test::return_value_substitutor()
{
	// a function that returns void
	bool func_called = false;
	auto func = [&]
	{
		func_called = true;
	};

	// proxying it
	auto proxy = util::return_value_substitutor<decltype(func), int, 42>(std::move(func));

	// and lets make sure the proxy works
	int rc = proxy();
	QVERIFY(rc == 42);
	QVERIFY(func_called);
}


//-------------------------------------------------
//  binaryFromHex
//-------------------------------------------------

void Test::binaryFromHex()
{
	using namespace std::literals;

	std::uint8_t buffer[10];
	std::size_t rc = util::binaryFromHex(buffer, "BaaDF00DDEADBeeF12345678"sv);
	QVERIFY(rc == 10);
	QVERIFY(buffer[0] == 0xBA);
	QVERIFY(buffer[1] == 0xAD);
	QVERIFY(buffer[2] == 0xF0);
	QVERIFY(buffer[3] == 0x0D);
	QVERIFY(buffer[4] == 0xDE);
	QVERIFY(buffer[5] == 0xAD);
	QVERIFY(buffer[6] == 0xBE);
	QVERIFY(buffer[7] == 0xEF);
	QVERIFY(buffer[8] == 0x12);
	QVERIFY(buffer[9] == 0x34);
}


//-------------------------------------------------

static TestFixture<Test> fixture;
#include "utility_test.moc"
