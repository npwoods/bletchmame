/***************************************************************************

    xmlparser.h

    Simple XML parser class wrapping expat (wxXmlDocument is too heavyweight)

***************************************************************************/

#include <expat.h>
#include <wx/stream.h>
#include <wx/sstream.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>

#include "xmlparser.h"
#include "validity.h"


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

bool XmlParser::Parse(wxInputStream &input)
{
	m_current_node = m_root;
	m_unknown_depth = 0;

	bool success = InternalParse(input);

	m_current_node = nullptr;
	m_unknown_depth = 0;
	return success;
}


//-------------------------------------------------
//  Parse
//-------------------------------------------------

bool XmlParser::Parse(const wxString &file_name)
{
	wxFileInputStream input(file_name);
	return Parse(input);
}


//-------------------------------------------------
//  ParseXml
//-------------------------------------------------

bool XmlParser::ParseXml(const wxString &xml_text)
{
	wxStringInputStream string_stream(xml_text);
	return Parse(string_stream);
}


//-------------------------------------------------
//  InternalParse
//-------------------------------------------------

bool XmlParser::InternalParse(wxInputStream &input)
{
	bool done = false;
	char buffer[8192];

	while (!done)
	{
		// read data
		input.Read(buffer, sizeof(buffer));

		// figure out how much we actually read, and if we're done
		int last_read = static_cast<int>(input.LastRead());
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

wxString XmlParser::ErrorMessage() const
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
	Node::ptr child;

	if (m_unknown_depth == 0)
	{
		auto iter = m_current_node->m_map.find(element);
		if (iter != m_current_node->m_map.end())
			child = iter->second;
	}

	// do we have a child?
	if (child)
	{
		// we do - traverse down the tree
		m_current_node = child;

		// call back the begin func, if appropriate
		if (m_current_node->m_begin_func)
		{
			Attributes *xxx = reinterpret_cast<Attributes *>(reinterpret_cast<void *>(attributes));
			m_current_node->m_begin_func(*xxx);
		}
	}
	else
	{
		// we don't - go into the unknown
		m_unknown_depth++;
	}

	m_current_content.clear();
}


//-------------------------------------------------
//  EndElement
//-------------------------------------------------

void XmlParser::EndElement(const char *)
{
	if (m_unknown_depth)
	{
		// coming out of an unknown element type
		m_unknown_depth--;
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
	m_current_content.append(s, len);
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
	const char *string_value = InternalGet(attribute);
	int rc = sscanf(string_value, "%d", &value);
	return rc > 0;
}


//-------------------------------------------------
//  Attributes::Get
//-------------------------------------------------

bool XmlParser::Attributes::Get(const char *attribute, bool &value) const
{
	int int_value;
	if (Get(attribute, int_value))
	{
		value = int_value != 0;
		return true;
	}

	const char *string_value = InternalGet(attribute);
	if (!strcmp(string_value, "false") || !strcmp(string_value, "off"))
	{
		value = false;
		return true;
	}
	if (!strcmp(string_value, "true") || !strcmp(string_value, "on"))
	{
		value = true;
		return true;
	}

	return false;
}


//-------------------------------------------------
//  Attributes::Get
//-------------------------------------------------

bool XmlParser::Attributes::Get(const char *attribute, float &value) const
{
	const char *string_value = InternalGet(attribute);
	int rc = sscanf(string_value, "%f", &value);
	return rc > 0;
}


//-------------------------------------------------
//  Attributes::Get
//-------------------------------------------------

bool XmlParser::Attributes::Get(const char *attribute, wxString &value) const
{
	const char *s = InternalGet(attribute, true);
	if (s)
		value = s;
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


//-------------------------------------------------
//  StringHash::operator()
//-------------------------------------------------

size_t XmlParser::StringHash::operator()(const char *s) const
{
	size_t result = 31337;
	for (size_t i = 0; s[i]; i++)
		result = ((result << 5) + result) + s[i];
	return result;
}


//-------------------------------------------------
//  StringCompare::operator()
//-------------------------------------------------

bool XmlParser::StringCompare::operator()(const char *s1, const char *s2) const
{
	return !strcmp(s1, s2);
}


//**************************************************************************
//  VALIDITY CHECKS
//**************************************************************************

//-------------------------------------------------
//  test
//-------------------------------------------------

static void test()
{
	XmlParser xml;
	wxString charlie_value;
	int foxtrot_value = 0;
	xml.OnElement({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		(void)attributes;
		assert(attributes.Get("charlie", charlie_value));
	});
	xml.OnElement({ "alpha", "echo" }, [&](const XmlParser::Attributes &attributes)
	{
		(void)attributes;
		assert(attributes.Get("foxtrot", foxtrot_value));
	});

	bool result = xml.ParseXml(
		"<alpha>"
		"<bravo charlie=\"delta\"/>"
		"<echo foxtrot=\"42\"/>"
		"</alpha>");
	assert(result);
	assert(charlie_value == "delta");
	assert(foxtrot_value == 42);

	(void)result;
	(void)foxtrot_value;
}


//-------------------------------------------------
//  validity_checks
//-------------------------------------------------

static validity_check validity_checks[] =
{
	test
};
