/***************************************************************************

    utility.cpp

    Miscellaneous utility code

***************************************************************************/

#include <sstream>
#include <QDir>

#include "utility.h"
#include "validity.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

const QString util::g_empty_string;


//-------------------------------------------------
//  append_conditionally_quoted
//-------------------------------------------------

static void append_conditionally_quoted(QString &buffer, const QString &text)
{
	auto iter = std::find_if(text.begin(), text.end(), [](auto ch)
	{
		return ch == ' ';
	});
	bool has_spaces = iter != text.end();
	bool need_quotes = text.isEmpty() || has_spaces;

	if (need_quotes)
		buffer += "\"";
	buffer += text;
	if (need_quotes)
		buffer += "\"";
}


//-------------------------------------------------
//  build_command_line
//-------------------------------------------------

QString util::build_command_line(const QString &executable, const std::vector<QString> &argv)
{
	QString result;
	append_conditionally_quoted(result, executable);

	for (const QString &arg : argv)
	{
		result += " ";
		append_conditionally_quoted(result, arg);
	}
	return result;
}


//-------------------------------------------------
//  wxFileName::IsPathSeparator
//-------------------------------------------------

bool wxFileName::IsPathSeparator(QChar ch)
{
	return ch == '/' || ch == QDir::separator();
}


//-------------------------------------------------
//  wxFileName::SplitPath
//-------------------------------------------------

void wxFileName::SplitPath(const QString &fullpath, QString *path, QString *name, QString *ext)
{
	QFileInfo fi(fullpath);
	if (path)
		*path = fi.dir().absolutePath();
	if (name)
		*name = fi.baseName();
	if (ext)
		*ext = fi.suffix();
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


//-------------------------------------------------
//  test_string_split
//-------------------------------------------------

template<typename TStr>
static void test_string_split()
{
	TStr str = build_string<TStr>("Alpha,Bravo,Charlie");
	auto result = util::string_split(str, [](auto ch) { return ch == ','; });
#if 0
	assert(result[0] == "Alpha");
	assert(result[1] == "Bravo");
	assert(result[2] == "Charlie");
#endif
}


//-------------------------------------------------
//  test_string_split
//-------------------------------------------------

template<typename TStr>
static void test_string_icontains()
{
	TStr str = build_string<TStr>("Alpha,Bravo,Charlie");

	bool result1 = util::string_icontains(str, build_string<TStr>("Bravo"));
	assert(result1);
	(void)result1;

	bool result2 = util::string_icontains(str, build_string<TStr>("brAvo"));
	assert(result2);
	(void)result2;

	bool result3 = util::string_icontains(str, build_string<TStr>("ZYXZYX"));
	assert(!result3);
	(void)result3;
}


//-------------------------------------------------
//  test_build_command_line
//-------------------------------------------------

static void test_build_command_line()
{
	QString result = util::build_command_line("C:\\mame64.exe", { "foobar", "-rompath", "C:\\MyRoms", "-samplepath", "" });
	assert(result == "C:\\mame64.exe foobar -rompath C:\\MyRoms -samplepath \"\"");
}


//-------------------------------------------------
//  test_enum_parser
//-------------------------------------------------

static void test_enum_parser()
{
	static const util::enum_parser<int> parser =
	{
		{ "fourtytwo", 42 },
		{ "twentyone", 21 }
	};

	bool result;
	int value;

	result = parser(std::string("fourtytwo"), value);
	assert(result);
	assert(value == 42);

	result = parser(std::string("twentyone"), value);
	assert(result);
	assert(value == 21);

	result = parser(std::string("invalid"), value);
	assert(!result);
	assert(value == 0);
}


//-------------------------------------------------
//  validity_checks
//-------------------------------------------------

static void test_return_value_substitutor()
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
	assert(rc == 42);
	assert(func_called);
	(void)rc;
}


//-------------------------------------------------
//  validity_checks
//-------------------------------------------------

static validity_check validity_checks[] =
{
	test_string_split<std::string>,
	test_string_split<std::wstring>,
	test_string_icontains<std::string>,
	test_string_icontains<std::wstring>,
	test_build_command_line,
	test_enum_parser,
	test_return_value_substitutor
};
