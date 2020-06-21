/***************************************************************************

    xmlparser.h

    Simple XML parser class wrapping expat

***************************************************************************/

#include <expat.h>

#include "xmlparser.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif // _MSC_VER


//**************************************************************************
//  LOCAL TYPES
//**************************************************************************

namespace
{
	template<typename T>
	class scanf_parser
	{
	public:
		scanf_parser(const char *format)
			: m_format(format)
		{
		}

		bool operator()(const std::string &text, T &value) const
		{
			int rc = sscanf(text.c_str(), m_format, &value);
			return rc > 0;
		}

	private:
		const char *m_format;
	};
};


//**************************************************************************
//  LOCAL VARIABLES
//**************************************************************************


static const scanf_parser<int> s_int_parser("%d");
static const scanf_parser<unsigned int> s_uint_parser("%u");
static const scanf_parser<float> s_float_parser("%f");

static const util::enum_parser<bool> s_bool_parser =
{
	{ "0", false },
	{ "off", false },
	{ "false", false },
	{ "no", false },
	{ "1", true },
	{ "on", true },
	{ "true", true },
	{ "yes", true }
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

XmlParser::XmlParser()
	: m_root(std::make_unique<Node>())
{
	m_parser = XML_ParserCreate(nullptr);

	XML_SetUserData(m_parser, (void *) this);
	XML_SetElementHandler(m_parser, StartElementHandler, EndElementHandler);
	XML_SetCharacterDataHandler(m_parser, CharacterDataHandler);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

XmlParser::~XmlParser()
{
	XML_ParserFree(m_parser);
}


//-------------------------------------------------
//  Parse
//-------------------------------------------------

bool XmlParser::Parse(QDataStream &input)
{
	m_current_node = m_root;
	m_skipping_depth = 0;

	bool success = InternalParse(input);

	m_current_node = nullptr;
	m_skipping_depth = 0;
	return success;
}


//-------------------------------------------------
//  Parse
//-------------------------------------------------

bool XmlParser::Parse(const QString &file_name)
{
	QFile file(file_name);
	file.open(QFile::ReadOnly);
	QDataStream file_stream(&file);
	return Parse(file_stream);
}


//-------------------------------------------------
//  ParseBytes
//-------------------------------------------------

bool XmlParser::ParseBytes(const void *ptr, size_t sz)
{
	QByteArray byte_array((const char *) ptr, (int)sz);
	QDataStream input(byte_array);
	return Parse(input);
}


//-------------------------------------------------
//  InternalParse
//-------------------------------------------------

bool XmlParser::InternalParse(QDataStream &input)
{
	bool done = false;
	char buffer[8192];

	while (!done && input.device()->waitForReadyRead(-1))
	{
		// read data
		int last_read = input.readRawData(buffer, sizeof(buffer));

		// figure out how much we actually read, and if we're done
		done = last_read <= 0;

		// and feed this into expat
		if (!XML_Parse(m_parser, buffer, last_read, done))
		{
			// an error happened; bail out
			return false;		
		}
	}

	return true;
}


//-------------------------------------------------
//  ErrorMessage
//-------------------------------------------------

QString XmlParser::ErrorMessage() const
{
	XML_Error code = XML_GetErrorCode(m_parser);
	const char *message = XML_ErrorString(code);
	return message;
}


//-------------------------------------------------
//  GetNode
//-------------------------------------------------

XmlParser::Node::ptr XmlParser::GetNode(const std::initializer_list<const char *> &elements)
{
	Node::ptr node = m_root;

	for (auto iter = elements.begin(); iter != elements.end(); iter++)
	{
		Node::ptr &child(node->m_map[*iter]);

		if (!child)
		{
			child = std::make_unique<Node>();
			child->m_parent = node;
		}

		node = child;
	}
	return node;
}


//-------------------------------------------------
//  StartElement
//-------------------------------------------------

void XmlParser::StartElement(const char *element, const char **attributes)
{
	// only try to find this node in our tables if we are not skipping
	Node::ptr child;
	if (m_skipping_depth == 0)
	{
		auto iter = m_current_node->m_map.find(element);
		if (iter != m_current_node->m_map.end())
			child = iter->second;
	}

	// figure out how to handle this element
	element_result result;
	if (child)
	{
		// we do - traverse down the tree
		m_current_node = child;

		// do we have a callback function for beginning this node?
		if (m_current_node->m_begin_func)
		{
			// we do - call it
			Attributes *attributes_object = reinterpret_cast<Attributes *>(reinterpret_cast<void *>(attributes));
			result = m_current_node->m_begin_func(*attributes_object);
		}
		else
		{
			// we don't; we treat this as a normal node
			result = element_result::OK;
		}
	}
	else
	{
		// we don't - we start skipping
		result = element_result::SKIP;
	}

	// do the dirty work
	switch (result)
	{
	case element_result::OK:
		// do nothing
		break;

	case element_result::SKIP:
		// we're skipping this element; treat it the same as an unknown element
		m_skipping_depth++;
		break;

	default:
		assert(false);
		break;
	}

	// finally clear out content
	m_current_content.clear();
}


//-------------------------------------------------
//  EndElement
//-------------------------------------------------

void XmlParser::EndElement(const char *)
{
	if (m_skipping_depth)
	{
		// coming out of an unknown element type
		m_skipping_depth--;
	}
	else
	{
		// call back the end func, if appropriate
		if (m_current_node->m_end_func)
			m_current_node->m_end_func(std::move(m_current_content));

		// and go up the tree
		m_current_node = m_current_node->m_parent.lock();
	}
}


//-------------------------------------------------
//  CharacterData
//-------------------------------------------------

void XmlParser::CharacterData(const char *s, int len)
{
	QString text = QString::fromUtf8(s, len);
	m_current_content.append(std::move(text));
}


//-------------------------------------------------
//  Escape
//-------------------------------------------------

std::string XmlParser::Escape(const QString &str)
{
	std::string result;
	for (QChar qch : str)
	{
		char16_t ch = qch.unicode();
		switch (ch)
		{
		case '<':
			result += "&lt;";
			break;
		case '>':
			result += "&gt;";
			break;
		case '\"':
			result += "&quot;";
			break;
		default:
			if (ch >= 0 && ch <= 127)
				result += (char)ch;
			else
				result += "&#" + std::to_string((int)ch) + ";";
			break;
		}
	}
	return result;
}


//**************************************************************************
//  TRAMPOLINES
//**************************************************************************

//-------------------------------------------------
//  StartElementHandler
//-------------------------------------------------

void XmlParser::StartElementHandler(void *user_data, const char *name, const char **attributes)
{
	XmlParser *parser = (XmlParser *)user_data;
	parser->StartElement(name, attributes);
}


//-------------------------------------------------
//  EndElementHandler
//-------------------------------------------------

void XmlParser::EndElementHandler(void *user_data, const char *name)
{
	XmlParser *parser = (XmlParser *)user_data;
	parser->EndElement(name);
}


//-------------------------------------------------
//  CharacterDataHandler
//-------------------------------------------------

void XmlParser::CharacterDataHandler(void *user_data, const char *s, int len)
{
	XmlParser *parser = (XmlParser *)user_data;
	parser->CharacterData(s, len);
}


//**************************************************************************
//  UTILITY/SUPPORT
//**************************************************************************

//-------------------------------------------------
//  Attributes::Get
//-------------------------------------------------

bool XmlParser::Attributes::Get(const char *attribute, int &value) const
{
	return Get(attribute, value, s_int_parser);
}


//-------------------------------------------------
//  Attributes::Get
//-------------------------------------------------

bool XmlParser::Attributes::Get(const char *attribute, std::uint32_t &value) const
{
	return Get(attribute, value, s_uint_parser);
}


//-------------------------------------------------
//  Attributes::Get
//-------------------------------------------------

bool XmlParser::Attributes::Get(const char *attribute, bool &value) const
{
	return Get(attribute, value, s_bool_parser);
}


//-------------------------------------------------
//  Attributes::Get
//-------------------------------------------------

bool XmlParser::Attributes::Get(const char *attribute, float &value) const
{
	return Get(attribute, value, s_float_parser);
}


//-------------------------------------------------
//  Attributes::Get
//-------------------------------------------------

bool XmlParser::Attributes::Get(const char *attribute, QString &value) const
{
	const char *s = InternalGet(attribute, true);
	if (s)
		value = QString::fromUtf8(s);
	else
		value.clear();
	return s != nullptr;
}


//-------------------------------------------------
//  Attributes::Get
//-------------------------------------------------

bool XmlParser::Attributes::Get(const char *attribute, std::string &value) const
{
	const char *s = InternalGet(attribute, true);
	if (s)
		value = s;
	else
		value.clear();
	return s != nullptr;
}


//-------------------------------------------------
//  Attributes::InternalGet
//-------------------------------------------------

const char *XmlParser::Attributes::InternalGet(const char *attribute, bool return_null) const
{
	const char **actual_attribute = reinterpret_cast<const char **>(const_cast<Attributes *>(this));

	for (size_t i = 0; actual_attribute[i]; i += 2)
	{
		if (!strcmp(attribute, actual_attribute[i + 0]))
			return actual_attribute[i + 1];
	}

	return return_null ? nullptr : "";
}
