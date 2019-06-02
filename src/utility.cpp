/***************************************************************************

    utility.cpp

    Miscellaneous utility code

***************************************************************************/

#include "utility.h"


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
	bool need_quotes = buffer.IsEmpty() || has_spaces;

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
