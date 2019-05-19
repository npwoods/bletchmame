/***************************************************************************

    utility.h

    Miscellaneous utility code

***************************************************************************/

#pragma once

#ifndef UTILITY_H
#define UTILITY_H

#include <wx/xml/xml.h>

namespace util {

//**************************************************************************
//  STRING UTILITIES
//**************************************************************************

//-------------------------------------------------
//  string_split
//-------------------------------------------------

template<typename TStr, typename TFunc>
std::vector<TStr> string_split(const TStr &str, TFunc &&is_delim)
{
	std::vector<TStr> results;

	TStr::const_iterator word_begin = str.cbegin();
	for (TStr::const_iterator iter = str.cbegin(); iter < str.cend(); iter++)
	{
		if (is_delim(*iter))
		{
			if (word_begin < iter)
			{
				TStr word(word_begin, iter);
				results.emplace_back(std::move(word));
			}

			word_begin = iter;
			word_begin++;
		}
	}

	// squeeze off that final word, if necessary
	if (word_begin < str.cend())
	{
		TStr word(word_begin, str.cend());
		results.emplace_back(std::move(word));
	}

	return results;
}


//-------------------------------------------------
//  string_truncate
//-------------------------------------------------

template<typename TStr, typename TChar>
void string_truncate(TStr &str, TChar ch)
{
    size_t index = str.find(ch);
    if (index != std::string::npos)
        str.resize(index);
}


//**************************************************************************
//  XML UTILITIES
//**************************************************************************

void ProcessXml(wxXmlDocument &xml, const std::function<void(const std::vector<wxString> &path, const wxXmlNode &node)> &callback);
inline bool GetXmlAttributeValue(const wxXmlNode &node, const wxString &attribute_name, wxString &result) { return node.GetAttribute(attribute_name, &result); }
bool GetXmlAttributeValue(const wxXmlNode &node, const wxString &attribute_name, bool &result);
bool GetXmlAttributeValue(const wxXmlNode &node, const wxString &attribute_name, long &result);
bool GetXmlAttributeValue(const wxXmlNode &node, const wxString &attribute_name, int &result);
bool GetXmlAttributeValue(const wxXmlNode &node, const wxString &attribute_name, double &result);
bool GetXmlAttributeValue(const wxXmlNode &node, const wxString &attribute_name, float &result);


//**************************************************************************

}; // namespace util

#endif // UTILITY_H
