/***************************************************************************

    xmlparser.h

    Simple XML parser class wrapping expat

***************************************************************************/

#include <charconv>
#include <expat.h>
#include <inttypes.h>
#include <string>

#include <QBuffer>

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
	class strtoll_parser
	{
	public:
		strtoll_parser(int radix)
			: m_radix(radix)
		{
		}

		bool operator()(std::u8string_view text, T &value) const
		{
			char *endptr;
			long long l = strtoll((const char *) text.data(), &endptr, m_radix);
			if (endptr != (const char *)text.data() + text.size())
				return false;

			value = (T)l;
			return value == l;
		}

	private:
		int m_radix;
	};

	template<typename T>
	class strtoull_parser
	{
	public:
		strtoull_parser(int radix)
			: m_radix(radix)
		{
		}

		bool operator()(std::u8string_view text, T &value) const
		{
			char *endptr;
			unsigned long long l = strtoull((const char *)text.data(), &endptr, m_radix);
			if (endptr != (const char *)text.data() + text.size())
				return false;

			value = (T)l;
			return value == l;
		}

	private:
		int m_radix;
	};


	template<typename T>
	class strtof_parser
	{
	public:
		bool operator()(std::u8string_view text, T &value) const
		{
			// it would be better to use std::from_chars(), but GCC 10 doesn't handle it
			// well; using Qt is a stopgap for now until the GCC 11 upgrade happens
			QString s = util::toQString(text);
			bool ok;
			value = (T) s.toDouble(&ok);
			return ok;
		}
	};
};


//**************************************************************************
//  LOCAL VARIABLES
//**************************************************************************

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
	: m_root(std::make_unique<Node>(nullptr))
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
	m_currentNode = m_root.get();
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
	QByteArray byteArray((const char *) ptr, (int)sz);
	QBuffer input(&byteArray);
	return input.open(QIODevice::ReadOnly)
		&& parse(input);
}


//-------------------------------------------------
//  internalParse
//-------------------------------------------------

bool XmlParser::internalParse(QIODevice &input)
{
	bool done = false;
	char buffer[8192];

	if (LOG_XML)
		qDebug("XmlParser::internalParse(): beginning parse");

	while (!done)
	{
		// this seems to be necssary when reading from a QProcess
		input.waitForReadyRead(-1);

		// read data
		int lastRead = input.read(buffer, sizeof(buffer));
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
			done = true;
		}
	}

	bool success = m_errors.size() == 0;
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

XmlParser::Node *XmlParser::getNode(const std::initializer_list<const char *> &elements)
{
	Node *node = m_root.get();

	for (auto iter = elements.begin(); iter != elements.end(); iter++)
	{
		Node::ptr &child(node->m_map[*iter]);

		// create the child node if it is not present
		if (!child)
			child = std::make_unique<Node>(node);

		node = child.get();
	}
	return node;
}


//-------------------------------------------------
//  startElement
//-------------------------------------------------

void XmlParser::startElement(const char *element, const char **attributes)
{
	// only try to find this node in our tables if we are not skipping
	Node *child = nullptr;
	if (m_skippingDepth == 0)
	{
		auto iter = m_currentNode->m_map.find(element);
		if (iter != m_currentNode->m_map.end())
			child = iter->second.get();
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
			Attributes attributesObject(*this, attributes);
			result = m_currentNode->m_beginFunc(attributesObject);
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

	// set up content, but only if we expect to emit it later
	if (m_skippingDepth == 0 && m_currentNode->m_endFunc)
	{
		m_currentContent.emplace();
		m_currentContent->reserve(1024);
	}
	else
	{
		m_currentContent.reset();
	}
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
		{
			m_currentNode->m_endFunc(std::move(m_currentContent.value_or(u8"")));
			m_currentContent.reset();
		}

		// and go up the tree
		m_currentNode = m_currentNode->m_parent;
	}
}


//-------------------------------------------------
//  characterData
//-------------------------------------------------

void XmlParser::characterData(const char *s, int len)
{
	if (m_currentContent)
		*m_currentContent += std::u8string_view((const char8_t *) s, len);
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


//-------------------------------------------------
//  escape
//-------------------------------------------------

std::string XmlParser::escape(const std::u8string_view &str)
{
	return escape(util::toQString(str));
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
//  Attributes ctor
//-------------------------------------------------

XmlParser::Attributes::Attributes(XmlParser &parser, const char **attributes)
	: m_parser(parser)
	, m_attributes(attributes)
{
}


//-------------------------------------------------
//  Attributes::get<int>
//-------------------------------------------------

template<> std::optional<int> XmlParser::Attributes::get<int>(const char *attribute) const
{
	return get<int>(attribute, 10);
}


//-------------------------------------------------
//  Attributes::get<int>
//-------------------------------------------------

template<> std::optional<int> XmlParser::Attributes::get<int>(const char *attribute, int radix) const
{
	return get<int>(attribute, strtoll_parser<int>(radix));
}


//-------------------------------------------------
//  Attributes::get<std::uint8_t>
//-------------------------------------------------

template<> std::optional<std::uint8_t> XmlParser::Attributes::get(const char *attribute) const
{
	return get<std::uint8_t>(attribute, 10);
}


//-------------------------------------------------
//  Attributes::get<std::uint8_t>
//-------------------------------------------------

template<> std::optional<std::uint8_t> XmlParser::Attributes::get(const char *attribute, int radix) const
{
	return get<std::uint8_t>(attribute, strtoull_parser<std::uint8_t>(radix));
}


//-------------------------------------------------
//  Attributes::get<std::uint32_t>
//-------------------------------------------------

template<> std::optional<std::uint32_t> XmlParser::Attributes::get(const char *attribute) const
{
	return get<std::uint32_t>(attribute, 10);
}


//-------------------------------------------------
//  Attributes::get<std::uint32_t>
//-------------------------------------------------

template<> std::optional<std::uint32_t> XmlParser::Attributes::get(const char *attribute, int radix) const
{
	return get<std::uint32_t>(attribute, strtoull_parser<std::uint32_t>(radix));
}


//-------------------------------------------------
//  Attributes::get<std::uint64_t>
//-------------------------------------------------

template<> std::optional<std::uint64_t> XmlParser::Attributes::get(const char *attribute) const
{
	return get<std::uint64_t>(attribute, 10);
}


//-------------------------------------------------
//  Attributes::get<std::uint64_t>
//-------------------------------------------------

template<> std::optional<std::uint64_t> XmlParser::Attributes::get(const char *attribute, int radix) const
{
	return get<std::uint64_t>(attribute, strtoull_parser<std::uint64_t>(radix));
}


//-------------------------------------------------
//  Attributes::get<bool>
//-------------------------------------------------

template<> std::optional<bool> XmlParser::Attributes::get<bool>(const char *attribute) const
{
	return get<bool>(attribute, s_bool_parser);
}


//-------------------------------------------------
//  Attributes::get<float>
//-------------------------------------------------

template<> std::optional<float> XmlParser::Attributes::get<float>(const char *attribute) const
{
	return get<float>(attribute, strtof_parser<float>());
}


//-------------------------------------------------
//  Attributes::get<QString>
//-------------------------------------------------

template<> std::optional<QString> XmlParser::Attributes::get<QString>(const char *attribute) const
{
	const char *s = internalGet(attribute, true);
	return s
		? QString::fromUtf8(s)
		: std::optional<QString>();
}


//-------------------------------------------------
//  Attributes::get<std::u8string_view>
//-------------------------------------------------

template<> std::optional<std::u8string_view> XmlParser::Attributes::get<std::u8string_view>(const char *attribute) const
{
	const char8_t *s = (const char8_t *) internalGet(attribute, true);
	return s
		? s
		: std::optional<std::u8string_view>();
}


//-------------------------------------------------
//  Attributes::internalGet
//-------------------------------------------------

const char *XmlParser::Attributes::internalGet(const char *attribute, bool return_null) const
{
	for (size_t i = 0; m_attributes[i]; i += 2)
	{
		if (!strcmp(attribute, m_attributes[i + 0]))
			return m_attributes[i + 1];
	}

	return return_null ? nullptr : "";
}


//-------------------------------------------------
//  Attributes::reportAttributeParsingError
//-------------------------------------------------

void XmlParser::Attributes::reportAttributeParsingError(const char *attribute, std::u8string_view value) const
{
	QString message = QString("Error parsing attribute \"%1\" (text=\"%2\")").arg(
		QString(attribute),
		util::toQString(value));
	m_parser.appendError(std::move(message));
}


//-------------------------------------------------
//  Node ctor
//-------------------------------------------------

XmlParser::Node::Node(Node *parent)
	: m_parent(parent)
{

}


//-------------------------------------------------
//  Node dtor
//-------------------------------------------------

XmlParser::Node::~Node()
{
}
