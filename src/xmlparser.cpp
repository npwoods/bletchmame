/***************************************************************************

    xmlparser.h

    Simple XML parser class wrapping expat

***************************************************************************/

#include <expat.h>

#include "xmlparser.h"
#include "validity.h"


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

	while (!done)
	{
		// read data
		int last_read = input.readRawData(buffer, sizeof(buffer));

		// figure out how much we actually read, and if we're done
		done = last_read == 0;

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


//**************************************************************************
//  VALIDITY CHECKS
//**************************************************************************

//-------------------------------------------------
//  test
//-------------------------------------------------

static void test()
{
	const bool INVALID_BOOL_VALUE = (bool)42;

	XmlParser xml;
	QString charlie_value;
	bool charlie_value_parsed	= INVALID_BOOL_VALUE;
	int foxtrot_value			= 0;
	bool foxtrot_value_parsed	= INVALID_BOOL_VALUE;
	bool golf_value				= INVALID_BOOL_VALUE;
	bool golf_value_parsed		= INVALID_BOOL_VALUE;
	bool hotel_value			= INVALID_BOOL_VALUE;
	bool hotel_value_parsed		= INVALID_BOOL_VALUE;
	bool india_value			= INVALID_BOOL_VALUE;
	bool india_value_parsed		= INVALID_BOOL_VALUE;
	std::uint32_t julliet_value	= INVALID_BOOL_VALUE;
	bool julliet_value_parsed	= INVALID_BOOL_VALUE;
	float kilo_value			= INVALID_BOOL_VALUE;
	bool kilo_value_parsed		= INVALID_BOOL_VALUE;
	xml.OnElementBegin({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		charlie_value_parsed	= attributes.Get("charlie", charlie_value);
	});
	xml.OnElementBegin({ "alpha", "echo" }, [&](const XmlParser::Attributes &attributes)
	{
		foxtrot_value_parsed	= attributes.Get("foxtrot", foxtrot_value);
		golf_value_parsed		= attributes.Get("golf", golf_value);
		hotel_value_parsed		= attributes.Get("hotel", hotel_value);
		india_value_parsed		= attributes.Get("india", india_value);
		julliet_value_parsed	= attributes.Get("julliet", julliet_value);
		kilo_value_parsed		= attributes.Get("kilo", kilo_value);
	});

	const char *xml_text =
		"<alpha>"
		"<bravo charlie=\"delta\"/>"
		"<echo foxtrot=\"42\" hotel=\"on\" india=\"off\" julliet=\"2500000000\" kilo=\"3.14159\"/>/>"
		"</alpha>";
	bool result = xml.ParseBytes(xml_text, strlen(xml_text));

	assert(result);
	assert(charlie_value == "delta");
	assert(charlie_value_parsed);
	assert(foxtrot_value == 42);
	assert(foxtrot_value_parsed);
	assert(!golf_value);
	assert(!golf_value_parsed);
	assert(hotel_value);
	assert(hotel_value_parsed);
	assert(!india_value);
	assert(india_value_parsed);
	assert(julliet_value == 2500000000);
	assert(julliet_value_parsed);
	assert(abs(kilo_value - 3.14159f) < 0.000000001);
	assert(kilo_value_parsed);
	(void)result;
}


//-------------------------------------------------
//  unicode
//-------------------------------------------------

static void unicode()
{
	XmlParser xml;
	QString bravo_value;
	QString charlie_value;
	xml.OnElementBegin({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		bool result = attributes.Get("charlie", charlie_value);
		assert(result);
		(void)result;
	});
	xml.OnElementEnd({ "alpha", "bravo" }, [&](QString &&value)
	{
		bravo_value = std::move(value);
	});

	const char *xml_text = "<alpha><bravo charlie=\"&#x6B7B;\">&#x60AA;</bravo></alpha>";
	bool result = xml.ParseBytes(xml_text, strlen(xml_text));
	assert(result);
	assert(bravo_value.toStdWString() == L"\u60AA");
	assert(charlie_value.toStdWString() == L"\u6B7B");

	(void)result;
	(void)bravo_value;
	(void)charlie_value;
}


//-------------------------------------------------
//  skipping
//-------------------------------------------------

static void skipping()
{
	XmlParser xml;
	int expected_invocations = 0;
	int unexpected_invocations = 0;
	xml.OnElementBegin({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		bool skip_value;
		attributes.Get("skip", skip_value);
		return skip_value ? XmlParser::element_result::SKIP : XmlParser::element_result::OK;
	});
	xml.OnElementBegin({ "alpha", "bravo", "expected" }, [&](const XmlParser::Attributes &)
	{
		expected_invocations++;
	});
	xml.OnElementBegin({ "alpha", "bravo", "unexpected" }, [&](const XmlParser::Attributes &)
	{
		unexpected_invocations++;
	});

	const char *xml_text =
		"<alpha>"
		"<bravo skip=\"no\"><expected/></bravo>"
		"<bravo skip=\"yes\"><unexpected/></bravo>"
		"</alpha>";
	bool result = xml.ParseBytes(xml_text, strlen(xml_text));
	assert(result);
	assert(expected_invocations == 1);
	assert(unexpected_invocations == 0);
	(void)result;
}


//-------------------------------------------------
//  multiple
//-------------------------------------------------

static void multiple()
{
	XmlParser xml;
	int total = 0;
	xml.OnElementBegin({ { "alpha", "bravo" },
						 { "alpha", "charlie" },
						 { "alpha", "delta" }}, [&](const XmlParser::Attributes &attributes)
	{
		int value;
		attributes.Get("value", value);
		total += value;
	});

	const char *xml_text =
		"<alpha>"
		"<bravo value=\"2\" />"
		"<charlie value=\"3\" />"
		"<delta value=\"5\" />"
		"<echo value=\"-666\" />"
		"</alpha>";
	bool result = xml.ParseBytes(xml_text, strlen(xml_text));
	assert(result);
	assert(total == 10);
	(void)result;
	(void)total;
}


//-------------------------------------------------
//  validity_checks
//-------------------------------------------------

static validity_check validity_checks[] =
{
	test,
	unicode,
	skipping,
	multiple
};

