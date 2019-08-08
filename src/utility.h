/***************************************************************************

    utility.h

    Miscellaneous utility code

***************************************************************************/

#pragma once

#ifndef UTILITY_H
#define UTILITY_H

#include <wx/event.h>
#include <unordered_map>
#include <optional>

namespace util {


//**************************************************************************
//  PARSING UTILITY CLASSES
//**************************************************************************

template<typename TFunc, typename TValue, TValue Value>
class return_value_substitutor
{
public:
	return_value_substitutor(TFunc &&func)
		: m_func(std::move(func))
	{
	}

	template<typename... TArgs>
	TValue operator()(TArgs&&... args)
	{
		m_func(args...);
		return Value;
	}

private:
	TFunc m_func;
};


//**************************************************************************
//  ENUM UTILITY CLASSES
//**************************************************************************

// ======================> string_hash
class string_hash
{
public:
	size_t operator()(const char *s) const;
};


// ======================> string_compare
class string_compare
{
public:
	bool operator()(const char *s1, const char *s2) const;
};


// ======================> enum_parser
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

	bool operator()(const std::string &text, std::optional<T> &value) const
	{
		T inner_value;
		bool success = (*this)(text, inner_value);
		value = success ? inner_value : std::optional<T>();
		return success;
	}

private:
	const std::unordered_map<const char *, T, string_hash, string_compare> m_map;
};


// ======================> enum_parser_bidirectional
template<typename T>
class enum_parser_bidirectional : public enum_parser<T>
{
public:
	enum_parser_bidirectional(std::initializer_list<std::pair<const char *, T>> values)
		: enum_parser<T>(values)
	{
		for (const auto &[str, value] : values)
			m_reverse_map.emplace(value, str);
	}

	const char *operator[](T val) const
	{
		auto iter = m_reverse_map.find(val);
		return iter->second;
	}


private:
	std::unordered_map<T, const char *> m_reverse_map;
};


// ======================> all_enums
template<typename T>
class all_enums
{
public:
	class iterator
	{
	public:
		iterator(T value) : m_value(value) { }
		T operator*() const { return m_value; }
		iterator &operator++() { bump(+1); return *this; }
		iterator &operator--() { bump(-1); return *this; }
		iterator operator++(int) { iterator result = *this; bump(+1); return result; }
		iterator operator--(int) { iterator result = *this; bump(-1); return result; }
		bool operator==(const iterator &that) const { return m_value == that.m_value; }
		bool operator!=(const iterator &that) const { return m_value != that.m_value; }
		bool operator<(const iterator &that) const { return m_value < that.m_value; }

	private:
		T m_value;

		void bump(std::int64_t offset)
		{
			auto i = static_cast<std::int64_t>(m_value);
			i += offset;
			m_value = static_cast<T>(i);
		}
	};

	typedef iterator const_iterator;

	iterator begin()		const { return iterator((T)0); }
	iterator end()			const { return iterator(T::count); }
	const_iterator cbegin()	const { return begin(); }
	const_iterator cend()	const { return end(); }
};


//**************************************************************************
//  STRING & CONTAINER UTILITIES
//**************************************************************************

extern const wxString g_empty_string;

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


//-------------------------------------------------
//  string_icontains
//-------------------------------------------------

template<typename TStr>
inline bool string_icontains(const TStr &str, const TStr &target)
{
	auto iter = std::search(str.begin(), str.end(), target.begin(), target.end(), [](auto ch1, auto ch2)
	{
		// TODO - this does not handle UTF-8
		return sizeof(ch1) > 0
			? towlower(static_cast<wchar_t>(ch1)) == towlower(static_cast<wchar_t>(ch2))
			: tolower(static_cast<char>(ch1)) == tolower(static_cast<char>(ch2));
	});
	return iter < str.end();
}

	
//-------------------------------------------------
//  last
//-------------------------------------------------

template<typename T>
auto &last(T &container)
{
	assert(container.end() > container.begin());
	return *(container.end() - 1);
}


//-------------------------------------------------
//  salt
//-------------------------------------------------

inline void salt(void *destination, const void *original, size_t original_size, const void *salt, size_t salt_size)
{
	char *destination_ptr = (char *)destination;
	const char *original_ptr = (char *)original;
	const char *salt_ptr = (char *)salt;

	for (size_t i = 0; i < original_size; i++)
		destination_ptr[i] = original_ptr[i] ^ salt_ptr[i % salt_size];
}


template<typename TStruct, typename TSalt>
TStruct salt(const TStruct &original, const TSalt &salt_value)
{
	TStruct result;
	salt((void *)&result, (const void *)&original, sizeof(original), (const void *)&salt_value, sizeof(salt_value));
	return result;
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
