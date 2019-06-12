/***************************************************************************

    xmlparser.h

    Simple XML parser class wrapping expat (wxXmlDocument is too heavyweight)

***************************************************************************/

#pragma once

#ifndef XMLPARSER_H
#define XMLPARSER_H

#include <initializer_list>
#include <memory>
#include <unordered_map>

struct XML_ParserStruct;
class wxInputStream;

class XmlParser
{
public:
	class Attributes
	{
	public:
		Attributes() = delete;
		~Attributes() = delete;

		bool Get(const char *attribute, int &value) const;
		bool Get(const char *attribute, bool &value) const;
		bool Get(const char *attribute, float &value) const;
		bool Get(const char *attribute, wxString &value) const;
		bool Get(const char *attribute, std::string &value) const;

		template<typename T>
		bool Get(const char *attribute, T &value, T &&default_value) const
		{
			bool result = Get(attribute, value);
			if (!result)
				value = std::move(default_value);
			return result;
		}

	private:
		const char *InternalGet(const char *attribute, bool return_null = false) const;
	};

	// ctor/dtor
	XmlParser();
	~XmlParser();

	typedef std::function<void(const Attributes &node)> OnBeginElementCallback;
	void OnElement(const std::initializer_list<const char *> &elements, OnBeginElementCallback &&func)
	{
		GetNode(elements)->m_begin_func = std::move(func);
	}

	typedef std::function<void(wxString &&content)> OnEndElementCallback;
	void OnElement(const std::initializer_list<const char *> &elements, OnEndElementCallback &&func)
	{
		GetNode(elements)->m_end_func = std::move(func);
	}

	bool Parse(wxInputStream &input);
	bool Parse(const wxString &file_name);
	bool ParseXml(const wxString &xml_text);
	wxString ErrorMessage() const;

	static std::string Escape(const wxString &str);

private:
	struct StringHash
	{
		size_t operator()(const char *s) const;
	};

	struct StringCompare
	{
		bool operator()(const char *s1, const char *s2) const;
	};

	struct Node
	{
		typedef std::shared_ptr<Node> ptr;
		typedef std::weak_ptr<Node> weak_ptr;
		typedef std::unordered_map<const char *, Node::ptr, StringHash, StringCompare> Map;

		OnBeginElementCallback	m_begin_func;
		OnEndElementCallback	m_end_func;
		Node::weak_ptr		m_parent;
		Map						m_map;
	};

	struct XML_ParserStruct *	m_parser;
	Node::ptr				m_root;
	Node::ptr				m_current_node;
	int							m_unknown_depth;
	wxString					m_current_content;

	bool InternalParse(wxInputStream &input);
	void StartElement(const char *name, const char **attributes);
	void EndElement(const char *name);
	void CharacterData(const char *s, int len);
	Node::ptr GetNode(const std::initializer_list<const char *> &elements);

	static void StartElementHandler(void *user_data, const char *name, const char **attributes);
	static void EndElementHandler(void *user_data, const char *name);
	static void CharacterDataHandler(void *user_data, const char *s, int len);
};

#endif // XMLPARSER_H
