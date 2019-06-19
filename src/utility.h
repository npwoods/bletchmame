/***************************************************************************

    utility.h

    Miscellaneous utility code

***************************************************************************/

#pragma once

#ifndef UTILITY_H
#define UTILITY_H

#include <wx/event.h>
#include <unordered_map>

namespace util {

//**************************************************************************
//  PARSING UTILITY CLASSES
//**************************************************************************

class string_hash
{
public:
	size_t operator()(const char *s) const;
};


class string_compare
{
public:
	bool operator()(const char *s1, const char *s2) const;
};


template<typename T>
class enum_parser
{
public:
	enum_parser(std::initializer_list<std::pair<const char *, T>> values)
		: m_map(values.begin(), values.end())
	{
	}

	bool operator()(const std::string &text, T &value) const
	{
		auto iter = m_map.find(text.c_str());
		bool success = iter != m_map.end();
		value = success ? iter->second : T();
		return success;
	}

private:
	const std::unordered_map<const char *, T, string_hash, string_compare> m_map;
};


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

template<typename TStr, typename TColl, typename TFunc>
TStr string_join(const TStr &delim, const TColl &collection, TFunc func)
{
	TStr result;
	bool is_first = true;

	for (const TStr &member : collection)
	{
		if (is_first)
			is_first = false;
		else
			result += delim;
		result += func(member);
	}
	return result;
}


template<typename TStr, typename TColl>
TStr string_join(const TStr &delim, const TColl &collection)
{
	return string_join(delim, collection, [](const TStr &s) { return s; });
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
