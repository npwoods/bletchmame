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

#include <QDataStream>

#include "utility.h"

struct XML_ParserStruct;

QT_BEGIN_NAMESPACE
class QDataStream;
QT_END_NAMESPACE

class XmlParser
{
public:
	class Test;

	enum class ElementResult
	{
		Ok,
		Skip
	};

	class Attributes
	{
	public:
		Attributes() = delete;
		~Attributes() = delete;

		bool get(const char *attribute, int &value) const;
		bool get(const char *attribute, std::uint32_t &value) const;
		bool get(const char *attribute, std::uint64_t &value) const;
		bool get(const char *attribute, bool &value) const;
		bool get(const char *attribute, float &value) const;
		bool get(const char *attribute, QString &value) const;
		bool get(const char *attribute, std::string &value) const;

		template<typename T>
		bool get(const char *attribute, T &value, T &&default_value) const
		{
			bool result = get(attribute, value);
			if (!result)
				value = std::move(default_value);
			return result;
		}

		template<typename T, typename TFunc>
		bool get(const char *attribute, T &value, TFunc func) const
		{
			std::string text;
			bool result = get(attribute, text) && func(text, value);
			if (!result)
				value = T();
			return result;
		}

		template<typename T>
		void get(const char *attribute, std::optional<T> &value) const
		{
			T temp_value;
			bool result = get(attribute, temp_value);
			value = result
				? std::move(temp_value)
				: std::optional<T>();
		}

	private:
		const char *internalGet(const char *attribute, bool return_null = false) const;
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
		getNode(elements)->m_begin_func = std::move(proxy);
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
		getNode(elements)->m_end_func = std::move(func);
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
	bool parse(QDataStream &input);
	bool parse(const QString &file_name);
	bool parseBytes(const void *ptr, size_t sz);
	QString errorMessage() const;

	static std::string escape(const QString &str);

private:
	struct Node
	{
		typedef std::shared_ptr<Node> ptr;
		typedef std::weak_ptr<Node> weak_ptr;
		typedef std::unordered_map<const char *, Node::ptr> Map;

		OnBeginElementCallback	m_begin_func;
		OnEndElementCallback	m_end_func;
		Node::weak_ptr		m_parent;
		Map						m_map;
	};

	struct XML_ParserStruct *	m_parser;
	Node::ptr					m_root;
	Node::ptr					m_current_node;
	int							m_skipping_depth;
	QString						m_current_content;

	bool internalParse(QDataStream &input);
	void startElement(const char *name, const char **attributes);
	void endElement(const char *name);
	void characterData(const char *s, int len);
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
