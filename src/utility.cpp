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


//-------------------------------------------------
//  InternalProcessXml
//-------------------------------------------------

static void InternalProcessXml(const wxXmlNode &node, std::vector<wxString> &path, const std::function<void(const std::vector<wxString> &path, const wxXmlNode &node)> &callback)
{
    path.push_back(node.GetName());
    callback(path, node);

    for (wxXmlNode *child = node.GetChildren(); child; child = child->GetNext())
    {
        InternalProcessXml(*child, path, callback);
    }

    path.resize(path.size() - 1);
}


//-------------------------------------------------
//  ProcessXml
//-------------------------------------------------

void util::ProcessXml(wxXmlDocument &xml, const std::function<void(const std::vector<wxString> &path, const wxXmlNode &node)> &callback)
{
    wxXmlNode &root(*xml.GetRoot());
    std::vector<wxString> path;
    InternalProcessXml(root, path, callback);
}


//-------------------------------------------------
//  InternalGetXmlAttributeValue
//-------------------------------------------------

template<class TFunc>
static bool InternalGetXmlAttributeValue(const wxXmlNode &node, const wxString &attribute_name, TFunc &&func)
{
	wxString attribute_value;
	bool has_attribute = node.GetAttribute(attribute_name, &attribute_value);
	return func(attribute_value) && has_attribute;
}


//-------------------------------------------------
//  GetXmlAttributeValue
//-------------------------------------------------

bool util::GetXmlAttributeValue(const wxXmlNode &node, const wxString &attribute_name, bool &result)
{
	return InternalGetXmlAttributeValue(node, attribute_name, [&result](const wxString &attribute_value)
	{
		long l;
		result = attribute_value.ToLong(&l)
			? l != 0
			: (attribute_value == "true" || attribute_value == "on");
		return true;
	});
}


//-------------------------------------------------
//  GetXmlAttributeValue
//-------------------------------------------------

bool util::GetXmlAttributeValue(const wxXmlNode &node, const wxString &attribute_name, long &result)
{
	return InternalGetXmlAttributeValue(node, attribute_name, [&result](const wxString &attribute_value)
	{
		return attribute_value.ToLong(&result);
	});
}


//-------------------------------------------------
//  GetXmlAttributeValue
//-------------------------------------------------

bool util::GetXmlAttributeValue(const wxXmlNode &node, const wxString &attribute_name, double &result)
{
	return InternalGetXmlAttributeValue(node, attribute_name, [&result](const wxString &attribute_value)
	{
		return attribute_value.ToDouble(&result);
	});
}


//-------------------------------------------------
//  GetXmlAttributeValue
//-------------------------------------------------

bool util::GetXmlAttributeValue(const wxXmlNode &node, const wxString &attribute_name, int &result)
{
	long l;
	bool success = GetXmlAttributeValue(node, attribute_name, l);
	result = (int)l;
	return success;
}


//-------------------------------------------------
//  GetXmlAttributeValue
//-------------------------------------------------

bool util::GetXmlAttributeValue(const wxXmlNode &node, const wxString &attribute_name, float &result)
{
	double d;
	bool success = GetXmlAttributeValue(node, attribute_name, d);
	result = (float)d;
	return success;
}
