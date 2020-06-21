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
		void string_icontains_string() { string_icontains<std::string>(); }
		void string_icontains_wstring() { string_icontains<std::wstring>(); }

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

		template<typename TStr>
		void string_icontains()
		{
			TStr str = build_string<TStr>("Alpha,Bravo,Charlie");

			bool result1 = util::string_icontains(str, build_string<TStr>("Bravo"));
			QVERIFY(result1);
			(void)result1;

			bool result2 = util::string_icontains(str, build_string<TStr>("brAvo"));
			QVERIFY(result2);
			(void)result2;

			bool result3 = util::string_icontains(str, build_string<TStr>("ZYXZYX"));
			QVERIFY(!result3);
			(void)result3;
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

	result = parser(std::string("fourtytwo"), value);
	QVERIFY(result);
	QVERIFY(value == 42);

	result = parser(std::string("twentyone"), value);
	QVERIFY(result);
	QVERIFY(value == 21);

	result = parser(std::string("invalid"), value);
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

static TestFixture<Test> fixture;
#include "utility_test.moc"
