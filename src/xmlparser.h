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
class wxInputStream;

class XmlParser
{
public:
	enum class element_result
	{
		OK,
		SKIP
	};

	class Attributes
	{
	public:
		Attributes() = delete;
		~Attributes() = delete;

		bool Get(const char *attribute, int &value) const;
		bool Get(const char *attribute, std::uint32_t &value) const;
		bool Get(const char *attribute, bool &value) const;
		bool Get(const char *attribute, float &value) const;
		bool Get(const char *attribute, QString &value) const;
		bool Get(const char *attribute, std::string &value) const;

		template<typename T>
		bool Get(const char *attribute, T &value, T &&default_value) const
		{
			bool result = Get(attribute, value);
			if (!result)
				value = std::move(default_value);
			return result;
		}

		template<typename T, typename TFunc>
		bool Get(const char *attribute, T &value, TFunc func) const
		{
			std::string text;
			bool result = Get(attribute, text) && func(text, value);
			if (!result)
				value = T();
			return result;
		}

		template<typename T>
		void Get(const char *attribute, std::optional<T> &value) const
		{
			T temp_value;
			bool result = Get(attribute, temp_value);
			value = result
				? std::move(temp_value)
				: std::optional<T>();
		}

	private:
		const char *InternalGet(const char *attribute, bool return_null = false) const;
	};

	// ctor/dtor
	XmlParser();
	~XmlParser();

	typedef std::function<element_result (const Attributes &node) > OnBeginElementCallback;
	template<typename TFunc>
	void OnElementBegin(const std::initializer_list<const char *> &elements, TFunc &&func)
	{
		// we don't want to force callers to specify a return value in the TFunc (usually a
		// lambda) because most of the time it would just return element_result::OK
		//
		// therefore, we are creating a proxy that will supply element_result::OK as a return
		// value if it is not specified
		Attributes *x = nullptr;
		typedef typename std::conditional<
			std::is_void<decltype(func(*x))>::value,
			util::return_value_substitutor<TFunc, element_result, element_result::OK>,
			TFunc>::type proxy_type;
		auto proxy = proxy_type(std::move(func));

		// supply the proxy
		GetNode(elements)->m_begin_func = std::move(proxy);
	}

	template<typename TFunc>
	void OnElementBegin(const std::initializer_list<const std::initializer_list<const char *>> &elements, const TFunc &func)
	{
		for (auto iter = elements.begin(); iter != elements.end(); iter++)
		{
			TFunc func_duplicate(func);
			OnElementBegin(*iter, std::move(func_duplicate));
		}
	}

	typedef std::function<void(QString &&content)> OnEndElementCallback;
	void OnElementEnd(const std::initializer_list<const char *> &elements, OnEndElementCallback &&func)
	{
		GetNode(elements)->m_end_func = std::move(func);
	}
	void OnElementEnd(const std::initializer_list<const std::initializer_list<const char *>> &elements, OnEndElementCallback &&func)
	{
		for (auto iter = elements.begin(); iter != elements.end(); iter++)
		{
			OnEndElementCallback func_duplicate(func);
			OnElementEnd(*iter, std::move(func_duplicate));
		}
	}

	bool Parse(QDataStream &input);
	bool Parse(const QString &file_name);
	bool ParseBytes(const void *ptr, size_t sz);
	QString ErrorMessage() const;

	static std::string Escape(const QString &str);

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

	bool InternalParse(QDataStream &input);
	void StartElement(const char *name, const char **attributes);
	void EndElement(const char *name);
	void CharacterData(const char *s, int len);
	Node::ptr GetNode(const std::initializer_list<const char *> &elements);

	static void StartElementHandler(void *user_data, const char *name, const char **attributes);
	static void EndElementHandler(void *user_data, const char *name);
	static void CharacterDataHandler(void *user_data, const char *s, int len);
};

#endif // XMLPARSER_H
