/***************************************************************************

    xmlparser.h

    Simple XML parser class wrapping expat (wxXmlDocument is too heavyweight)

***************************************************************************/

#pragma once

#ifndef XMLPARSER_H
#define XMLPARSER_H

// bletchmame headers
#include "utility.h"

// Qt headers
#include <QIODevice>

// standard headers
#include <array>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stack>
#include <type_traits>

struct XML_ParserStruct;

QT_BEGIN_NAMESPACE
class QDataStream;
QT_END_NAMESPACE

// ======================> XmlParser

class XmlParser
{
public:
	class Test;

	enum class ElementResult
	{
		Ok,
		Skip
	};


	// ======================> XmlParser

	struct Error
	{
		int		m_lineNumber;
		int		m_columnNumber;
		QString	m_message;
		QString	m_context;
	};


	// ======================> Attribute

	class Attribute
	{
	public:
		// ctor
		constexpr Attribute(const char **valuePtr = nullptr)
			: m_valuePtr(valuePtr)
		{
		}

		// operator overloading
		operator bool() const;

		// main attribute getter
		template<class T> std::optional<T> as() const noexcept
		{
			static_assert(sizeof(T) == -1, "Expected use of template specialization");
		}

		// alternate radices
		template<class T> std::optional<T> as(int radix) const noexcept
		{
			static_assert(sizeof(T) == -1, "Expected use of template specialization");
		}

		// generic getter with a parser function
		template<typename T, typename TFunc>
		std::optional<T> as(TFunc func) const noexcept
		{
			std::optional<T> result = { };
			std::optional<std::u8string_view> text = as<std::u8string_view>();
			if (text)
			{
				T funcResult;
				if (func(*text, funcResult))
					result = std::move(funcResult);
				else
					[[unlikely]] reportAttributeParsingError(m_valuePtr[-1], m_valuePtr[0]);

			}
			return result;
		}

	private:
		const char **	m_valuePtr;

		static void reportAttributeParsingError(const char *attrName, const char *attrValue) noexcept;
	};


	// ======================> Attributes

	class Attributes
	{
	public:
		Attributes(const char **attributes);

		// bulk attribute retrieval
		template<typename... TArgs>
		auto get(TArgs... attrs) const noexcept
		{
			// convert attributes to an array
			const std::array<std::string_view, sizeof...(attrs)> attrsArray = { std::forward<TArgs>(attrs)... };

			// prepare an array of results
			std::array<Attribute, attrsArray.size()> results;
			std::ranges::fill(results, Attribute((const char **)&m_zero));

			// loop through all attributes
			for (auto i = 0; m_attributes[i]; i += 2)
			{
				std::string_view attrName = m_attributes[i + 0];

				auto iter = std::ranges::find_if(attrsArray, [attrName](std::string_view x)
				{
					return attrName == x;
				});
				if (iter != attrsArray.end())
					results[iter - attrsArray.begin()] = Attribute(&m_attributes[i + 1]);
			}

			// and return
			return results;
		}

	private:
		const char **	m_attributes;
		const char *	m_zero;
	};

	// ctor/dtor
	XmlParser();
	~XmlParser();

	typedef std::function<ElementResult (const Attributes &node) > OnBeginElementCallback;
	template<typename TFunc>
	void onElementBegin(const std::initializer_list<const char *> &elements, TFunc &&func) noexcept
	{
		// we don't want to force callers to specify a return value in the TFunc (usually a
		// lambda) because most of the time it would just return ElementResult::Ok
		//
		// therefore, we are creating a proxy that will supply ElementResult::Ok as a return
		// value if it is not specified
		Attributes *x = nullptr;
		typedef typename std::conditional<
			std::is_void<decltype(func(*x))>::value,
			util::return_value_substitutor<TFunc, ElementResult, ElementResult::Ok>,
			TFunc>::type proxy_type;
		auto proxy = proxy_type(std::move(func));

		// supply the proxy
		getNode(elements).m_beginFunc = std::move(proxy);
	}

	// onElementBegin
	template<typename TFunc>
	void onElementBegin(const std::initializer_list<const std::initializer_list<const char *>> &elements, const TFunc &func) noexcept
	{
		for (auto iter = elements.begin(); iter != elements.end(); iter++)
		{
			TFunc func_duplicate(func);
			onElementBegin(*iter, std::move(func_duplicate));
		}
	}

	// onElementEnd (std::u8string)
	typedef std::function<void(std::u8string &&content)> OnEndElementCallback;
	void onElementEnd(const std::initializer_list<const char *> &elements, OnEndElementCallback &&func) noexcept;
	void onElementEnd(const std::initializer_list<const std::initializer_list<const char *>> &elements, OnEndElementCallback &&func) noexcept;

	// onElementEnd (void)
	typedef std::function<void()> OnEndElementVoidCallback;
	void onElementEnd(const std::initializer_list<const char *> &elements, OnEndElementVoidCallback &&func) noexcept;
	void onElementEnd(const std::initializer_list<const std::initializer_list<const char *>> &elements, OnEndElementVoidCallback &&func) noexcept;

	bool parse(QIODevice &input) noexcept;
	bool parse(const QString &file_name) noexcept;
	bool parseBytes(const void *ptr, size_t sz) noexcept;
	QString errorMessagesSingleString() const noexcept;

private:
	struct Node
	{
		typedef std::unique_ptr<Node> ptr;
		typedef std::unordered_map<const char *, Node::ptr> Map;

		// ctor/dtor
		Node() = default;
		Node(const Node &) = delete;
		Node(Node &&) = delete;
		~Node() = default;

		// fields
		OnBeginElementCallback		m_beginFunc;
		OnEndElementCallback		m_endFunc;
		Map							m_map;
	};

	typedef std::stack<const Node *> NodeStack;

	thread_local static XmlParser *	s_currentParser;
	struct XML_ParserStruct *		m_parser;
	Node::ptr						m_root;
	NodeStack						m_currentNodeStack;
	std::optional<std::u8string>	m_currentContent;
	std::vector<Error>				m_errors;

	bool internalParse(QIODevice &input) noexcept;
	bool parseSingleBuffer(QIODevice &input, std::optional<QFile> &xmlDataLog, bool &done) noexcept;
	void startElement(const char *name, const char **attributes) noexcept;
	void endElement(const char *name) noexcept;
	void characterData(const char *s, int len) noexcept;
	void appendError(QString &&message) noexcept;
	void appendCurrentXmlError() noexcept;
	QString errorContext() const noexcept;
	static QString errorContext(const char *contextString, int contextOffset, int contextSize) noexcept;
	static bool isLineEnding(char ch) noexcept;
	static bool isWhitespace(char ch) noexcept;
	Node &getNode(const std::initializer_list<const char *> &elements) noexcept;

	static void startElementHandler(void *user_data, const char *name, const char **attributes);
	static void endElementHandler(void *user_data, const char *name);
	static void characterDataHandler(void *user_data, const char *s, int len);
};


//**************************************************************************
//  TEMPLATE SPECIALIZATIONS
//**************************************************************************

template<> std::optional<int>					XmlParser::Attribute::as<int>() const noexcept;
template<> std::optional<bool>					XmlParser::Attribute::as<bool>() const noexcept;
template<> std::optional<float>					XmlParser::Attribute::as<float>() const noexcept;
template<> std::optional<QString>				XmlParser::Attribute::as<QString>() const noexcept;
template<> std::optional<const char8_t *>		XmlParser::Attribute::as<const char8_t *>() const noexcept;
template<> std::optional<std::u8string_view>	XmlParser::Attribute::as<std::u8string_view>() const noexcept;
template<> std::optional<std::uint8_t>			XmlParser::Attribute::as<std::uint8_t>() const noexcept;
template<> std::optional<std::uint32_t>			XmlParser::Attribute::as<std::uint32_t>() const noexcept;
template<> std::optional<std::uint64_t>			XmlParser::Attribute::as<std::uint64_t>() const noexcept;

template<> std::optional<int>					XmlParser::Attribute::as<int>(int radix) const noexcept;
template<> std::optional<std::uint8_t>			XmlParser::Attribute::as<std::uint8_t>(int radix) const noexcept;
template<> std::optional<std::uint32_t>			XmlParser::Attribute::as<std::uint32_t>(int radix) const noexcept;
template<> std::optional<std::uint64_t>			XmlParser::Attribute::as<std::uint64_t>(int radix) const noexcept;


#endif // XMLPARSER_H
