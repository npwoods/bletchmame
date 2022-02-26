/***************************************************************************

    utility_test.cpp

    Unit tests for utility.cpp

***************************************************************************/

// bletchmame headers
#include "utility.h"
#include "test.h"


namespace
{
    class Test : public QObject
    {
        Q_OBJECT

    private slots:
		void string_split_string()					{ string_split<std::string>(); }
		void string_split_wstring()					{ string_split<std::wstring>(); }
		void fixedByteArrayFromHex();
		void fixedByteArrayFromHex_parseError1()	{ fixedByteArrayFromHex_parseError(u8""); }
		void fixedByteArrayFromHex_parseError2()	{ fixedByteArrayFromHex_parseError(u8"01234"); }
		void fixedByteArrayFromHex_parseError3()	{ fixedByteArrayFromHex_parseError(u8"0123456X"); }

		void enum_parser();
		void return_value_substitutor();

		void toQString();
		void toU8String();

		void getUniqueFileName();
		void areFileInfosEquivalent();
		void splitPathList1();
		void splitPathList2();
		void joinPathList();

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

		void fixedByteArrayFromHex_parseError(std::u8string_view s);
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
//  fixedByteArrayFromHex
//-------------------------------------------------

void Test::fixedByteArrayFromHex()
{
	using namespace std::literals;

	auto buffer = util::fixedByteArrayFromHex<10>(u8"BaaDF00DDEADBeeF12345678"sv);
	QVERIFY(buffer.has_value());
	QVERIFY((*buffer)[0] == 0xBA);
	QVERIFY((*buffer)[1] == 0xAD);
	QVERIFY((*buffer)[2] == 0xF0);
	QVERIFY((*buffer)[3] == 0x0D);
	QVERIFY((*buffer)[4] == 0xDE);
	QVERIFY((*buffer)[5] == 0xAD);
	QVERIFY((*buffer)[6] == 0xBE);
	QVERIFY((*buffer)[7] == 0xEF);
	QVERIFY((*buffer)[8] == 0x12);
	QVERIFY((*buffer)[9] == 0x34);
}


//-------------------------------------------------
//  fixedByteArrayFromHex_parseError
//-------------------------------------------------

void Test::fixedByteArrayFromHex_parseError(std::u8string_view s)
{
	auto buffer = util::fixedByteArrayFromHex<4>(s);
	QVERIFY(!buffer.has_value());
}


//-------------------------------------------------
//  toQString
//-------------------------------------------------

void Test::toQString()
{
	std::u8string str = u8"Hey \u5582";
	QString qstr = util::toQString(str);
	QVERIFY(qstr == QString::fromWCharArray(L"Hey \u5582"));
}


//-------------------------------------------------
//  toU8String
//-------------------------------------------------

void Test::toU8String()
{
	QString qstr = QString::fromWCharArray(L"Hey \u5582");
	std::u8string str = util::toU8String(qstr);
	QVERIFY(str == u8"Hey \u5582");
}


//-------------------------------------------------
//  getUniqueFileName
//-------------------------------------------------

void Test::getUniqueFileName()
{
	auto writeStubFile = [](const QFileInfo &fi)
	{
		QFile file(fi.absoluteFilePath());
		file.open(QIODevice::WriteOnly);
	};

	QTemporaryDir tempDir;

	QFileInfo fi1 = ::getUniqueFileName(tempDir.path(), "foo", "txt");
	QVERIFY(fi1.fileName() == "foo.txt");
	writeStubFile(fi1);

	QFileInfo fi2 = ::getUniqueFileName(tempDir.path(), "foo", "txt");
	QVERIFY(fi2.fileName() == "foo (2).txt");
	writeStubFile(fi2);

	QFileInfo fi3 = ::getUniqueFileName(tempDir.path(), "foo", "txt");
	QVERIFY(fi3.fileName() == "foo (3).txt");
	writeStubFile(fi3);

	QFileInfo fi4 = ::getUniqueFileName(tempDir.path(), "bar", "txt");
	QVERIFY(fi4.fileName() == "bar.txt");
	writeStubFile(fi4);

	QFileInfo fi5 = ::getUniqueFileName(tempDir.path(), "bar", "txt");
	QVERIFY(fi5.fileName() == "bar (2).txt");
	writeStubFile(fi5);
}


//-------------------------------------------------
//  areFileInfosEquivalent
//-------------------------------------------------

void Test::areFileInfosEquivalent()
{
	// create a temporary directory
	QTemporaryDir tempDirObj;
	QVERIFY(tempDirObj.isValid());
	QDir tempDir(tempDirObj.path());

	// create two directories
	tempDir.mkdir("./dir1");
	tempDir.mkdir("./dir2");

	// some QFileInfos...
	QFileInfo dir1(tempDir.filePath("./dir1"));
	QFileInfo dir2(tempDir.filePath("./dir2"));
	QFileInfo dir3(tempDir.filePath("./dir3"));
	QFileInfo dir1x(tempDir.filePath("././dir1"));

	// the following should be equivalent
	QVERIFY(::areFileInfosEquivalent(dir1, dir1));
	QVERIFY(::areFileInfosEquivalent(dir1, dir1x));
	QVERIFY(::areFileInfosEquivalent(dir1x, dir1));
	QVERIFY(::areFileInfosEquivalent(dir1x, dir1x));
	QVERIFY(::areFileInfosEquivalent(dir2, dir2));
	QVERIFY(::areFileInfosEquivalent(dir3, dir3));

	// the following should not be equivalent
	QVERIFY(!::areFileInfosEquivalent(dir1, dir2));
	QVERIFY(!::areFileInfosEquivalent(dir2, dir3));
	QVERIFY(!::areFileInfosEquivalent(dir1x, dir3));
}


//-------------------------------------------------
//  splitPathList1
//-------------------------------------------------

void Test::splitPathList1()
{
	QString s = "/alpha;/bravo:/charlie";

#ifdef Q_OS_WINDOWS
	// ':' is not a path list separator on Windows
	s = s.replace(':', ';');
#endif

	QStringList list = splitPathList(s);

	QVERIFY(list.size() == 3);
	QVERIFY(list[0] == "/alpha");
	QVERIFY(list[1] == "/bravo");
	QVERIFY(list[2] == "/charlie");
}


//-------------------------------------------------
//  splitPathList2
//-------------------------------------------------

void Test::splitPathList2()
{
	QStringList list = splitPathList("");
	QVERIFY(list.size() == 0);
}


//-------------------------------------------------
//  joinPathList
//-------------------------------------------------

void Test::joinPathList()
{
	QStringList list;
	list << "/alpha";
	list << "/bravo";
	list << "/charlie";

	QString s = ::joinPathList(list);
	QVERIFY(s == "/alpha;/bravo;/charlie");
}


//-------------------------------------------------

static TestFixture<Test> fixture;
#include "utility_test.moc"
