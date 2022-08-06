/***************************************************************************

    utility.h

    Miscellaneous utility code

***************************************************************************/

#pragma once

#ifndef UTILITY_H
#define UTILITY_H

// Qt headers
#include <QFileInfo>
#include <QWidget>

// standard headers
#include <unordered_map>
#include <optional>
#include <functional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <wctype.h>


//**************************************************************************
//  QT VERSION CHECK
//**************************************************************************

// we use QTreeView::expandRecursively(), which was introduced in Qt 5.13
#if QT_VERSION < QT_VERSION_CHECK(5, 13, 0)
#error BletchMAME requires Qt version 5.13
#endif


//**************************************************************************
//  ATTRIBUTES
//**************************************************************************

#if defined(__GNUC__) || defined(__clang__)
#define ATTR_COLD __attribute__((cold))
#else
#define ATTR_COLD
#endif


//**************************************************************************
//  HASHES AND EQUIVALENCY
//**************************************************************************

namespace util
{
	template<class T>
	constexpr std::size_t array_hash(const T *ptr, std::size_t length) noexcept
	{
		const std::size_t p1 = 7;
		const std::size_t p2 = 31;
		const std::hash<T> hash;

		std::size_t result = p1;
		for (std::size_t i = 0; i < length; i++)
			result = (result * p2) + hash(ptr[i]);
		return result;
	}

	template<class T, std::size_t N>
	constexpr std::size_t array_hash(const std::array<T, N> &arr) noexcept
	{
		return array_hash(&arr[0], arr.size());
	}
}


namespace std
{
	template<> class hash<const char *>
	{
	public:
		std::size_t operator()(const char *s) const
		{
			return util::array_hash(s, strlen(s));
		}
	};

	template<> class equal_to<const char *>
	{
	public:
		bool operator()(const char *s1, const char *s2) const
		{
			return !strcmp(s1, s2);
		}
	};

	template<> class hash<std::reference_wrapper<const QString>>
	{
	public:
		std::size_t operator()(const std::reference_wrapper<const QString> &s) const
		{
			return std::hash<QString>()(s);
		}
	};
}



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


template<typename TIterator, typename TPredicate>
auto find_if_ptr(TIterator first, TIterator last, TPredicate predicate)
{
	auto iter = std::find_if(first, last, predicate);
	return iter != last
		? &*iter
		: nullptr;
}


template<typename TContainer, typename TPredicate>
auto find_if_ptr(TContainer &container, TPredicate predicate)
{
	return find_if_ptr(container.begin(), container.end(), predicate);
}


template<typename TContainer>
bool contains(typename TContainer::const_iterator begin, typename TContainer::const_iterator end, const typename TContainer::value_type &value)
{
	return std::find(begin, end, value) != end;
}


template<typename TContainer>
bool contains(const TContainer &container, const typename TContainer::value_type &value)
{
	return contains<TContainer>(container.cbegin(), container.cend(), value);
}


//**************************************************************************
//  ENUM UTILITY CLASSES
//**************************************************************************

// ======================> enum_parser
template<typename T>
class enum_parser
{
public:
	enum_parser(std::initializer_list<std::pair<const char *, T>> values)
		: m_map(values.begin(), values.end())
	{
	}

	bool operator()(std::u8string_view text, T &value) const
	{
		auto iter = m_map.find((const char *) text.data());
		bool success = iter != m_map.end();
		value = success ? iter->second : T();
		return success;
	}

	bool operator()(std::u8string_view text, std::optional<T> &value) const
	{
		T inner_value;
		bool success = (*this)(text, inner_value);
		value = success ? inner_value : std::optional<T>();
		return success;
	}

private:
	const std::unordered_map<const char *, T> m_map;
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
		auto operator<=>(const iterator &that) const = default;

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
	iterator end()			const { auto iter = iterator(T::Max); return ++iter; }
	const_iterator cbegin()	const { return begin(); }
	const_iterator cend()	const { return end(); }
};


// ======================> enum_count

template<class T>
constexpr int enum_count()
{
	return static_cast<int>(T::Max) + 1;
}


//**************************************************************************
//  STRING & CONTAINER UTILITIES
//**************************************************************************

extern const QString g_empty_string;


// string functions
QString toQString(std::u8string_view s);
std::u8string toU8String(const QString &s);
std::u8string_view trim(std::u8string_view s);

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
				TStr word(&*word_begin, iter - word_begin);
				results.emplace_back(std::move(word));
			}

			word_begin = iter;
			word_begin++;
		}
	}

	// squeeze off that final word, if necessary
	if (word_begin < str.cend())
	{
		TStr word(&*word_begin, str.cend() - word_begin);
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

	for (const auto &member : collection)
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


//-------------------------------------------------
//  safe_static_cast
//-------------------------------------------------

template<class T> T safe_static_cast(size_t sz)
{
	auto result = static_cast<T>(sz);
	if (sz != result)
		throw std::overflow_error("Overflow");
	return result;
}


//-------------------------------------------------
//  bytesFromHex
//-------------------------------------------------

std::size_t bytesFromHex(std::span<uint8_t> &dest, std::u8string_view hex);


//-------------------------------------------------
//  fixedByteArrayFromHex
//-------------------------------------------------

template<std::size_t N>
std::optional<std::array<std::uint8_t, N>> fixedByteArrayFromHex(std::optional<std::u8string_view> hex)
{
	std::array<std::uint8_t, N> result;
	std::span<std::uint8_t> span(result);
	return hex && bytesFromHex(span, *hex) == N
		? result
		: std::optional<std::array<std::uint8_t, N>>();
}


//-------------------------------------------------
//  overloaded
//-------------------------------------------------

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;


//**************************************************************************
//  COMMAND LINE
//**************************************************************************

QString build_command_line(const QString &executable, const std::vector<QString> &argv);


//**************************************************************************

} // namespace util

//**************************************************************************
//  MISCELLANEOUS QT HELPERS
//**************************************************************************

// useful for popup menus
QPoint globalPositionBelowWidget(const QWidget &widget);

// remove extra items in a grid layout
class QGridLayout;
void truncateGridLayout(QGridLayout &gridLayout, int rows);

// use these to avoid scaling
class QPixmap;
void setPixmapDevicePixelRatioToFit(QPixmap &pixmap, QSize size);
void setPixmapDevicePixelRatioToFit(QPixmap &pixmap, int dimension);

// used to get a unique filename
QFileInfo getUniqueFileName(const QString &directory, const QString &baseName, const QString &suffix);

// used to determine if these are the same file
bool areFileInfosEquivalent(const QFileInfo &fi1, const QFileInfo &fi2);

// utility functions to join/split paths
QStringList splitPathList(const QString &s);
QString joinPathList(const QStringList &list);


#endif // UTILITY_H
