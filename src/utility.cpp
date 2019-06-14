/***************************************************************************

    utility.cpp

    Miscellaneous utility code

***************************************************************************/

#include <sstream>

#include "utility.h"
#include "validity.h"

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
//  validity_checks
//-------------------------------------------------

static validity_check validity_checks[] =
{
	test_string_split<std::string>,
	test_string_split<std::wstring>,
	test_build_command_line
};
