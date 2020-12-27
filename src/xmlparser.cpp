/***************************************************************************

    xmlparser.h

    Simple XML parser class wrapping expat

***************************************************************************/

#include <expat.h>
#include <inttypes.h>

#include "xmlparser.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif // _MSC_VER


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define LOG_XML		0


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
static const scanf_parser<std::uint64_t> s_ulong_parser("%" SCNu64);
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
	, m_skippingDepth(0)
{
	m_parser = XML_ParserCreate(nullptr);

	XML_SetUserData(m_parser, (void *) this);
	XML_SetElementHandler(m_parser, startElementHandler, endElementHandler);
	XML_SetCharacterDataHandler(m_parser, characterDataHandler);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

XmlParser::~XmlParser()
{
	XML_ParserFree(m_parser);
}


//-------------------------------------------------
//  parse
//-------------------------------------------------

bool XmlParser::parse(QIODevice &input)
{
	QDataStream dataStream(&input);
	return parse(dataStream);
}


//-------------------------------------------------
//  parse
//-------------------------------------------------

bool XmlParser::parse(QDataStream &input)
{
	m_currentNode = m_root;
	m_skippingDepth = 0;

	bool success = internalParse(input);

	m_currentNode = nullptr;
	m_skippingDepth = 0;
	return success;
}


//-------------------------------------------------
//  parse
//-------------------------------------------------

bool XmlParser::parse(const QString &file_name)
{
	QFile file(file_name);
	if (!file.open(QFile::ReadOnly))
		return false;
	return parse(file);
}


//-------------------------------------------------
//  parseBytes
//-------------------------------------------------

bool XmlParser::parseBytes(const void *ptr, size_t sz)
{
	QByteArray byte_array((const char *) ptr, (int)sz);
	QDataStream input(byte_array);
	return parse(input);
}


//-------------------------------------------------
//  internalParse
//-------------------------------------------------

bool XmlParser::internalParse(QDataStream &input)
{
	bool done = false;
	bool success = true;
	char buffer[8192];

	if (LOG_XML)
		qDebug("XmlParser::internalParse(): beginning parse");

	while (!done)
	{
		// this seems to be necssary when reading from a QProcess
		input.device()->waitForReadyRead(-1);

		// read data
		int lastRead = input.readRawData(buffer, sizeof(buffer));
		if (LOG_XML)
			qDebug("XmlParser::internalParse(): input.readRawData() returned %d", lastRead);

		// figure out if we're done (note that with readRawData(), while the documentation states
		// that '0' signifies end of input and a negative number signifies an error condition such
		// as reading past the end of input, QProcess seems to (at least sometimes) return '-1'
		// without returning '0'
		done = lastRead <= 0;

		// and feed this into expat
		if (!XML_Parse(m_parser, buffer, done ? 0 : lastRead, done))
		{
			// an error happened; append the error and bail out
			appendCurrentXmlError();
			success = false;		
			done = true;
		}
	}

	if (LOG_XML)
		qDebug("XmlParser::internalParse(): ending parse (success=%s)", success ? "true" : "false");
	return success;
}


//-------------------------------------------------
//  appendCurrentXmlError
//-------------------------------------------------

void XmlParser::appendCurrentXmlError()
{
	XML_Error code = XML_GetErrorCode(m_parser);
	const char *errorString = XML_ErrorString(code);
	appendError(errorString);
}


//-------------------------------------------------
//  appendError
//-------------------------------------------------

void XmlParser::appendError(QString &&message)
{
	Error &error = m_errors.emplace_back();
	error.m_lineNumber = XML_GetCurrentLineNumber(m_parser);
	error.m_columnNumber = XML_GetCurrentColumnNumber(m_parser);
	error.m_message = std::move(message);
	error.m_context = errorContext();
}


//-------------------------------------------------
//  errorContext
//-------------------------------------------------

QString XmlParser::errorContext() const
{
	int contextOffset = 0, contextSize = 0;
	const char *contextString = XML_GetInputContext(m_parser, &contextOffset, &contextSize);
	return errorContext(contextString, contextOffset, contextSize);
}


//-------------------------------------------------
//  errorContext
//-------------------------------------------------

QString XmlParser::errorContext(const char *contextString, int contextOffset, int contextSize)
{
	QString result;
	if (contextString)
	{
		// find the beginning of the line
		int contextLineBegin = contextOffset;
		while (contextLineBegin > 0 && !isLineEnding(contextString[contextLineBegin - 1]))
			contextLineBegin--;

		// and advance the beginning of the line to cover whitespace
		while (contextLineBegin < contextSize - 1 && isWhitespace(contextString[contextLineBegin]))
			contextLineBegin++;

		// find the end of the line
		int contextLineEnd = contextOffset;
		while (contextLineEnd < contextSize - 1 && !isLineEnding(contextString[contextLineEnd + 1]))
			contextLineEnd++;

		// now that we have all of that, we can format the error message
		result = QString::fromUtf8(&contextString[contextLineBegin], contextLineEnd - contextLineBegin)
			+ "\n"
			+ QString(contextOffset - contextLineBegin, QChar(' '))
			+ "^";
	}
	return result;
}


//-------------------------------------------------
//  errorMessagesSingleString
//-------------------------------------------------

QString XmlParser::errorMessagesSingleString() const
{
	QString result;
	for (const Error &error : m_errors)
	{
		if (!result.isEmpty())
			result += "\n";
		result += QString("%1:%2: %3").arg(
			QString::number(error.m_lineNumber),
			QString::number(error.m_columnNumber),
			error.m_message);
	}
	return result;
}


//-------------------------------------------------
//  isLineEnding
//-------------------------------------------------

bool XmlParser::isLineEnding(char ch)
{
	return ch == '\r' || ch == '\n';
}


//-------------------------------------------------
//  isWhitespace
//-------------------------------------------------

bool XmlParser::isWhitespace(char ch)
{
	return ch == ' ' || ch == '\t';
}


//-------------------------------------------------
//  getNode
//-------------------------------------------------

XmlParser::Node::ptr XmlParser::getNode(const std::initializer_list<const char *> &elements)
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
//  startElement
//-------------------------------------------------

void XmlParser::startElement(const char *element, const char **attributes)
{
	// only try to find this node in our tables if we are not skipping
	Node::ptr child;
	if (m_skippingDepth == 0)
	{
		auto iter = m_currentNode->m_map.find(element);
		if (iter != m_currentNode->m_map.end())
			child = iter->second;
	}

	// figure out how to handle this element
	ElementResult result;
	if (child)
	{
		// we do - traverse down the tree
		m_currentNode = child;

		// do we have a callback function for beginning this node?
		if (m_currentNode->m_beginFunc)
		{
			// we do - call it
			Attributes *attributes_object = reinterpret_cast<Attributes *>(reinterpret_cast<void *>(attributes));
			result = m_currentNode->m_beginFunc(*attributes_object);
		}
		else
		{
			// we don't; we treat this as a normal node
			result = ElementResult::Ok;
		}
	}
	else
	{
		// we don't - we start skipping
		result = ElementResult::Skip;
	}

	// do the dirty work
	switch (result)
	{
	case ElementResult::Ok:
		// do nothing
		break;

	case ElementResult::Skip:
		// we're skipping this element; treat it the same as an unknown element
		m_skippingDepth++;
		break;

	default:
		assert(false);
		break;
	}

	// finally clear out content
	m_currentContent.clear();
}


//-------------------------------------------------
//  endElement
//-------------------------------------------------

void XmlParser::endElement(const char *)
{
	if (m_skippingDepth)
	{
		// coming out of an unknown element type
		m_skippingDepth--;
	}
	else
	{
		// call back the end func, if appropriate
		if (m_currentNode->m_endFunc)
			m_currentNode->m_endFunc(std::move(m_currentContent));

		// and go up the tree
		m_currentNode = m_currentNode->m_parent.lock();
	}
}


//-------------------------------------------------
//  characterData
//-------------------------------------------------

void XmlParser::characterData(const char *s, int len)
{
	QString text = QString::fromUtf8(s, len);
	m_currentContent.append(std::move(text));
}


//-------------------------------------------------
//  escape
//-------------------------------------------------

std::string XmlParser::escape(const QString &str)
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
//  startElementHandler
//-------------------------------------------------

void XmlParser::startElementHandler(void *user_data, const char *name, const char **attributes)
{
	XmlParser *parser = (XmlParser *)user_data;
	parser->startElement(name, attributes);
}


//-------------------------------------------------
//  endElementHandler
//-------------------------------------------------

void XmlParser::endElementHandler(void *user_data, const char *name)
{
	XmlParser *parser = (XmlParser *)user_data;
	parser->endElement(name);
}


//-------------------------------------------------
//  characterDataHandler
//-------------------------------------------------

void XmlParser::characterDataHandler(void *user_data, const char *s, int len)
{
	XmlParser *parser = (XmlParser *)user_data;
	parser->characterData(s, len);
}


//**************************************************************************
//  UTILITY/SUPPORT
//**************************************************************************

//-------------------------------------------------
//  Attributes::get
//-------------------------------------------------

bool XmlParser::Attributes::get(const char *attribute, int &value) const
{
	return get(attribute, value, s_int_parser);
}


//-------------------------------------------------
//  Attributes::get
//-------------------------------------------------

bool XmlParser::Attributes::get(const char *attribute, std::uint32_t &value) const
{
	return get(attribute, value, s_uint_parser);
}


//-------------------------------------------------
//  Attributes::get
//-------------------------------------------------

bool XmlParser::Attributes::get(const char *attribute, std::uint64_t &value) const
{
	return get(attribute, value, s_ulong_parser);
}


//-------------------------------------------------
//  Attributes::get
//-------------------------------------------------

bool XmlParser::Attributes::get(const char *attribute, bool &value) const
{
	return get(attribute, value, s_bool_parser);
}


//-------------------------------------------------
//  Attributes::get
//-------------------------------------------------

bool XmlParser::Attributes::get(const char *attribute, float &value) const
{
	return get(attribute, value, s_float_parser);
}


//-------------------------------------------------
//  Attributes::get
//-------------------------------------------------

bool XmlParser::Attributes::get(const char *attribute, QString &value) const
{
	const char *s = internalGet(attribute, true);
	if (s)
		value = QString::fromUtf8(s);
	else
		value.clear();
	return s != nullptr;
}


//-------------------------------------------------
//  Attributes::get
//-------------------------------------------------

bool XmlParser::Attributes::get(const char *attribute, std::string &value) const
{
	const char *s = internalGet(attribute, true);
	if (s)
		value = s;
	else
		value.clear();
	return s != nullptr;
}


//-------------------------------------------------
//  Attributes::internalGet
//-------------------------------------------------

const char *XmlParser::Attributes::internalGet(const char *attribute, bool return_null) const
{
	const char **actual_attribute = reinterpret_cast<const char **>(const_cast<Attributes *>(this));

	for (size_t i = 0; actual_attribute[i]; i += 2)
	{
		if (!strcmp(attribute, actual_attribute[i + 0]))
			return actual_attribute[i + 1];
	}

	return return_null ? nullptr : "";
}
