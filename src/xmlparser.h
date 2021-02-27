/***************************************************************************

    xmlparser.h

    Simple XML parser class wrapping expat (wxXmlDocument is too heavyweight)

***************************************************************************/

#pragma once

#ifndef XMLPARSER_H
#define XMLPARSER_H

#include <initializer_list>
#include <memory>
#include <type_traits>
#include <optional>

#include <QIODevice>

#include "utility.h"

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


	// ======================> Attributes

	class Attributes
	{
	public:
		Attributes(XmlParser &parser, const char **attributes);

		// main attribute getters
		template<class T> std::optional<T> get(const char *attribute) const;
		template<> std::optional<int>			get<int>			(const char *attribute) const { return get<int>(attribute, 10); }
		template<> std::optional<bool>			get<bool>			(const char *attribute) const;
		template<> std::optional<float>			get<float>			(const char *attribute) const;
		template<> std::optional<QString>		get<QString>		(const char *attribute) const;
		template<> std::optional<std::string>	get<std::string>	(const char *attribute) const;
		template<> std::optional<std::uint32_t>	get<std::uint32_t>	(const char *attribute) const { return get<std::uint32_t>(attribute, 10); }
		template<> std::optional<std::uint64_t>	get<std::uint64_t>	(const char *attribute) const { return get<std::uint64_t>(attribute, 10); }

		// alternate radices
		template<class T> std::optional<T> get(const char *attribute, int radix) const;
		template<> std::optional<int>			get<int>(const char *attribute, int radix) const;
		template<> std::optional<std::uint32_t>	get<std::uint32_t>(const char *attribute, int radix) const;
		template<> std::optional<std::uint64_t>	get<std::uint64_t>(const char *attribute, int radix) const;

		// generic getter with a parser function
		template<typename T, typename TFunc>
		std::optional<T> get(const char *attribute, TFunc func) const
		{
			std::optional<T> result = { };
			std::optional<std::string> text = get<std::string>(attribute);
			if (text.has_value())
			{
				T funcResult;
				if (func(text.value(), funcResult))
					result = std::move(funcResult);
				else
					reportAttributeParsingError(attribute, text.value());
			}
			return result;
		}

	private:
		XmlParser &		m_parser;
		const char **	m_attributes;

		const char *internalGet(const char *attribute, bool return_null = false) const;
		void reportAttributeParsingError(const char *attribute, const std::string &value) const;
	};

	// ctor/dtor
	XmlParser();
	~XmlParser();

	typedef std::function<ElementResult (const Attributes &node) > OnBeginElementCallback;
	template<typename TFunc>
	void onElementBegin(const std::initializer_list<const char *> &elements, TFunc &&func)
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
		getNode(elements)->m_beginFunc = std::move(proxy);
	}

	template<typename TFunc>
	void onElementBegin(const std::initializer_list<const std::initializer_list<const char *>> &elements, const TFunc &func)
	{
		for (auto iter = elements.begin(); iter != elements.end(); iter++)
		{
			TFunc func_duplicate(func);
			onElementBegin(*iter, std::move(func_duplicate));
		}
	}

	typedef std::function<void(QString &&content)> OnEndElementCallback;
	void onElementEnd(const std::initializer_list<const char *> &elements, OnEndElementCallback &&func)
	{
		getNode(elements)->m_endFunc = std::move(func);
	}
	void onElementEnd(const std::initializer_list<const std::initializer_list<const char *>> &elements, OnEndElementCallback &&func)
	{
		for (auto iter = elements.begin(); iter != elements.end(); iter++)
		{
			OnEndElementCallback func_duplicate(func);
			onElementEnd(*iter, std::move(func_duplicate));
		}
	}

	bool parse(QIODevice &input);
	bool parse(const QString &file_name);
	bool parseBytes(const void *ptr, size_t sz);
	QString errorMessagesSingleString() const;

	static std::string escape(const QString &str);

private:
	struct Node
	{
		typedef std::shared_ptr<Node> ptr;
		typedef std::weak_ptr<Node> weak_ptr;
		typedef std::unordered_map<const char *, Node::ptr> Map;

		OnBeginElementCallback	m_beginFunc;
		OnEndElementCallback	m_endFunc;
		Node::weak_ptr			m_parent;
		Map						m_map;
	};

	struct XML_ParserStruct *	m_parser;
	Node::ptr					m_root;
	Node::ptr					m_currentNode;
	int							m_skippingDepth;
	QString						m_currentContent;
	std::vector<Error>			m_errors;

	bool internalParse(QIODevice &input);
	void startElement(const char *name, const char **attributes);
	void endElement(const char *name);
	void characterData(const char *s, int len);
	void appendError(QString &&message);
	void appendCurrentXmlError();
	QString errorContext() const;
	static QString errorContext(const char *contextString, int contextOffset, int contextSize);
	static bool isLineEnding(char ch);
	static bool isWhitespace(char ch);
	Node::ptr getNode(const std::initializer_list<const char *> &elements);

	static void startElementHandler(void *user_data, const char *name, const char **attributes);
	static void endElementHandler(void *user_data, const char *name);
	static void characterDataHandler(void *user_data, const char *s, int len);
};

#endif // XMLPARSER_H
