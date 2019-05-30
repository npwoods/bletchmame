/***************************************************************************

    utility.h

    Miscellaneous utility code

***************************************************************************/

#pragma once

#ifndef UTILITY_H
#define UTILITY_H

#include <wx/event.h>
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

	auto word_begin = str.cbegin();
	for (auto iter = str.cbegin(); iter < str.cend(); iter++)
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
//  string_join
//-------------------------------------------------

template<typename TStr, typename TColl>
TStr string_join(const TStr &delim, const TColl &collection)
{
	TStr result;
	bool is_first = true;

	for (const TStr &member : collection)
	{
		if (is_first)
			is_first = false;
		else
			result += delim;
		result += member;
	}
	return result;
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
//  COMMAND LINE
//**************************************************************************

wxString build_command_line(const wxString &executable, const std::vector<wxString> &argv);


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
//  EVENTS
//**************************************************************************

template<typename TEvent, typename... TArgs>
void PostEvent(wxEvtHandler &dest, const wxEventTypeTag<TEvent> &event_type, TArgs&&... args)
{
	wxEvent *event = new TEvent(event_type, std::forward<TArgs>(args)...);
	wxPostEvent(&dest, *event);
}


template<typename TEvent, typename... TArgs>
void QueueEvent(wxEvtHandler &dest, const wxEventTypeTag<TEvent> &event_type, TArgs&&... args)
{
	wxEvent *event = new TEvent(event_type, std::forward<TArgs>(args)...);
	dest.QueueEvent(event);
}

//**************************************************************************

}; // namespace util

#endif // UTILITY_H
