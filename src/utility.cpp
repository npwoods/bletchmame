/***************************************************************************

    utility.cpp

    Miscellaneous utility code

***************************************************************************/

#include <sstream>

#include "utility.h"
#include "validity.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  string_hash::operator()
//-------------------------------------------------

size_t util::string_hash::operator()(const char *s) const
{
	size_t result = 31337;
	for (size_t i = 0; s[i]; i++)
		result = ((result << 5) + result) + s[i];
	return result;
}


//-------------------------------------------------
//  string_compare::operator()
//-------------------------------------------------

bool util::string_compare::operator()(const char *s1, const char *s2) const
{
	return !strcmp(s1, s2);
}


//-------------------------------------------------
//  append_conditionally_quoted
//-------------------------------------------------

static void append_conditionally_quoted(wxString &buffer, const wxString &text)
{
	auto iter = std::find_if(text.begin(), text.end(), [](wchar_t ch)
	{
		return ch == ' ';
	});
	bool has_spaces = iter != text.end();
	bool need_quotes = text.IsEmpty() || has_spaces;

	if (need_quotes)
		buffer += "\"";
	buffer += text;
	if (need_quotes)
		buffer += "\"";
}


//-------------------------------------------------
//  build_command_line
//-------------------------------------------------

wxString util::build_command_line(const wxString &executable, const std::vector<wxString> &argv)
{
	wxString result;
	append_conditionally_quoted(result, executable);

	for (const wxString &arg : argv)
	{
		result += " ";
		append_conditionally_quoted(result, arg);
	}
	return result;
}


//-------------------------------------------------
//  test_string_split
//-------------------------------------------------

template<typename TStr>
static void test_string_split()
{
	// get the string into a TStr
	const char *string = "Alpha,Bravo,Charlie";
	TStr str;
	for (int i = 0; string[i]; i++)
		str += string[i];

	auto result = util::string_split(str, [](auto ch) { return ch == ','; });
	assert(result[0] == "Alpha");
	assert(result[1] == "Bravo");
	assert(result[2] == "Charlie");
}


//-------------------------------------------------
//  test_build_command_line
//-------------------------------------------------

static void test_build_command_line()
{
	wxString result = util::build_command_line("C:\\mame64.exe", { "foobar", "-rompath", "C:\\MyRoms", "-samplepath", "" });
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
}


//-------------------------------------------------
//  validity_checks
//-------------------------------------------------

static validity_check validity_checks[] =
{
	test_string_split<std::string>,
	test_string_split<std::wstring>,
	test_build_command_line,
	test_enum_parser,
	test_return_value_substitutor
};
