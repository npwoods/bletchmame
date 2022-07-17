/***************************************************************************

    xmlparser.h

    Simple XML parser class wrapping expat

***************************************************************************/

// bletchmame headers
#include "xmlparser.h"
#include "perfprofiler.h"

// dependency headers
#include <expat.h>

// Qt headers
#include <QBuffer>
#include <QCoreApplication>

// standard headers
#include <charconv>
#include <inttypes.h>
#include <string>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif // _MSC_VER


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define LOG_XML_PARSING	0
#define LOG_XML_DATA	0


//**************************************************************************
//  LOCAL TYPES
//**************************************************************************

namespace
{
	class number_parser
	{
	public:
		number_parser(int radix)
			: m_radix(radix)
		{
		}

	protected:
		int get_radix(std::u8string_view text) const
		{
			// determine the actual radix
			int radix;
			if (m_radix > 0)
			{
				// the caller specified the radix
				radix = m_radix;
			}
			else
			{
				// unspecified radix; the radix is 16 if prefixed with '0x', 10 otherwise
				if (text.size() >= 2 && text[0] == '0' && text[1] == 'x')
				{
					radix = 16;
					text.remove_prefix(2);
				}
				else
				{
					radix = 10;
				}
			}
			return radix;
		}

	private:
		int m_radix;
	};

	template<typename T>
	class strtoll_parser : public number_parser
	{
	public:
		using number_parser::number_parser;

		bool operator()(std::u8string_view text, T &value) const
		{
			int radix = get_radix(text);

			char *endptr;
			long long l = strtoll((const char *) text.data(), &endptr, radix);
			if (endptr != (const char *)text.data() + text.size())
				return false;

			value = (T)l;
			return value == l;
		}
	};

	template<typename T>
	class strtoull_parser : public number_parser
	{
	public:
		using number_parser::number_parser;

		bool operator()(std::u8string_view text, T &value) const
		{
			int radix = get_radix(text);

			char *endptr;
			unsigned long long l = strtoull((const char *)text.data(), &endptr, radix);
			if (endptr != (const char *)text.data() + text.size())
				return false;

			value = (T)l;
			return value == l;
		}
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
}


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

thread_local XmlParser *XmlParser::s_currentParser;

//-------------------------------------------------
//  ctor
//-------------------------------------------------

XmlParser::XmlParser()
	: m_root(std::make_unique<Node>())
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

bool XmlParser::parse(QIODevice &input) noexcept
{
	// push the initial node onto the stack
	assert(m_currentNodeStack.empty());
	m_currentNodeStack.push(m_root.get());

	// parse all the things!
	s_currentParser = this;
	bool success = internalParse(input);
	s_currentParser = nullptr;

	// clear out the node stack and return
	assert(!success || m_currentNodeStack.size() == 1);
	m_currentNodeStack = decltype(m_currentNodeStack)();
	return success;
}


//-------------------------------------------------
//  parse
//-------------------------------------------------

bool XmlParser::parse(const QString &file_name) noexcept
{
	QFile file(file_name);
	if (!file.open(QFile::ReadOnly))
		return false;
	return parse(file);
}


//-------------------------------------------------
//  parseBytes
//-------------------------------------------------

bool XmlParser::parseBytes(const void *ptr, size_t sz) noexcept
{
	QByteArray byteArray((const char *) ptr, (int)sz);
	QBuffer input(&byteArray);
	return input.open(QIODevice::ReadOnly)
		&& parse(input);
}


//-------------------------------------------------
//  internalParse
//-------------------------------------------------

bool XmlParser::internalParse(QIODevice &input) noexcept
{
	ProfilerScope prof(CURRENT_FUNCTION);

	bool done = false;

	if (LOG_XML_PARSING)
		qDebug("XmlParser::internalParse(): beginning parse");

	// if appropriate, log the XML data
	std::optional<QFile> xmlDataLog;
	if (LOG_XML_DATA)
	{
		QString appDirPath = QCoreApplication::applicationDirPath();
		if (!appDirPath.isEmpty())
		{
			xmlDataLog.emplace(QString("%1/parsedxml.xml").arg(appDirPath));
			if (!xmlDataLog->open(QIODeviceBase::WriteOnly))
				xmlDataLog.reset();
		}
	}

	while (!done)
	{
		// this seems to be necssary when reading from a QProcess
		input.waitForReadyRead(-1);

		// parse one buffer
		if (!parseSingleBuffer(input, xmlDataLog, done))
		{
			// an error happened; append the error and bail out
			appendCurrentXmlError();
			done = true;
		}
	}

	bool success = m_errors.size() == 0;
	if (LOG_XML_PARSING)
		qDebug("XmlParser::internalParse(): ending parse (success=%s)", success ? "true" : "false");
	return success;
}


//-------------------------------------------------
//  parseSingleBuffer
//-------------------------------------------------

bool XmlParser::parseSingleBuffer(QIODevice &input, std::optional<QFile> &xmlDataLog, bool &done) noexcept
{
	const int bufferSize = 131072;

	// get expat's buffer
	void *buffer = XML_GetBuffer(m_parser, bufferSize);
	if (!buffer)
		return false;

	// read data
	int lastRead = input.read((char *) buffer, bufferSize);
	if (LOG_XML_PARSING)
		qDebug("XmlParser::parseSingleBuffer(): input.read() returned %d", lastRead);

	// figure out if we're done (note that with read(), while the documentation states that
	// '0' signifies end of input and a negative number signifies an error condition such
	// as reading past the end of input, QProcess seems to (at least sometimes) return '-1'
	// without returning '0'
	done = lastRead <= 0;

	// log the XML data if appropriate
	if (xmlDataLog && lastRead > 0)
		xmlDataLog->write((const char *) buffer, lastRead);

	// and feed this into expat
	return XML_ParseBuffer(m_parser, done ? 0 : lastRead, done) != XML_STATUS_ERROR;
}


//-------------------------------------------------
//  appendCurrentXmlError
//-------------------------------------------------

void XmlParser::appendCurrentXmlError() noexcept
{
	XML_Error code = XML_GetErrorCode(m_parser);
	const char *errorString = XML_ErrorString(code);
	appendError(errorString);
}


//-------------------------------------------------
//  appendError
//-------------------------------------------------

void XmlParser::appendError(QString &&message) noexcept
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

QString XmlParser::errorContext() const noexcept
{
	int contextOffset = 0, contextSize = 0;
	const char *contextString = XML_GetInputContext(m_parser, &contextOffset, &contextSize);
	return errorContext(contextString, contextOffset, contextSize);
}


//-------------------------------------------------
//  errorContext
//-------------------------------------------------

QString XmlParser::errorContext(const char *contextString, int contextOffset, int contextSize) noexcept
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

QString XmlParser::errorMessagesSingleString() const noexcept
{
	[[unlikely]];
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

bool XmlParser::isLineEnding(char ch) noexcept
{
	return ch == '\r' || ch == '\n';
}


//-------------------------------------------------
//  isWhitespace
//-------------------------------------------------

bool XmlParser::isWhitespace(char ch) noexcept
{
	return ch == ' ' || ch == '\t';
}


//-------------------------------------------------
//  onElementEnd (const char *)
//-------------------------------------------------

void XmlParser::onElementEnd(const std::initializer_list<const char *> &elements, OnEndElementCallback &&func) noexcept
{
	getNode(elements).m_endFunc = std::move(func);
}


//-------------------------------------------------
//  onElementEnd (multiple const char *)
//-------------------------------------------------

void XmlParser::onElementEnd(const std::initializer_list<const std::initializer_list<const char *>> &elements, OnEndElementCallback &&func) noexcept
{
	for (auto iter = elements.begin(); iter != elements.end(); iter++)
	{
		OnEndElementCallback func_duplicate(func);
		onElementEnd(*iter, std::move(func_duplicate));
	}
}


//-------------------------------------------------
//  onElementEnd (void)
//-------------------------------------------------

void XmlParser::onElementEnd(const std::initializer_list<const char *> &elements, OnEndElementVoidCallback &&func) noexcept
{
	getNode(elements).m_endFunc = [func{ std::move(func) }](std::u8string &&)
	{
		func();
	};
}


//-------------------------------------------------
//  onElementEnd (multiple void)
//-------------------------------------------------

void XmlParser::onElementEnd(const std::initializer_list<const std::initializer_list<const char *>> &elements, OnEndElementVoidCallback &&func) noexcept
{
	for (auto iter = elements.begin(); iter != elements.end(); iter++)
	{
		OnEndElementVoidCallback func_duplicate(func);
		onElementEnd(*iter, std::move(func_duplicate));
	}
}


//-------------------------------------------------
//  getNode
//-------------------------------------------------

XmlParser::Node &XmlParser::getNode(const std::initializer_list<const char *> &elements) noexcept
{
	Node *node = m_root.get();

	for (auto iter = elements.begin(); iter != elements.end(); iter++)
	{
		// special case
		bool isElipsis = !strcmp(*iter, "...");

		// find out the element name used to walk
		const char *elementName = iter[isElipsis ? -1 : 0];

		Node::ptr &child(node->m_map[elementName]);

		if (!isElipsis)
		{
			// create the child node if it is not present
			if (!child)
				child = std::make_unique<Node>();

			node = child.get();
		}
	}
	return *node;
}


//-------------------------------------------------
//  startElement
//-------------------------------------------------

void XmlParser::startElement(const char *element, const char **attributes) noexcept
{
	ProfilerScope prof(CURRENT_FUNCTION);

	// only try to find this node in our tables if we are not skipping
	const Node *currentNode = m_currentNodeStack.top();
	const Node *childNode;
	if (currentNode)
	{
		// we're in a node - find the child element
		auto iter = currentNode->m_map.find(element);
		if (iter != currentNode->m_map.end())
		{
			// we've found the child (which could be the current node if we're recursing)
			childNode = iter->second ? iter->second.get() : currentNode;
		}
		else
		{
			// this child is unknown - we have to ignore it
			childNode = nullptr;
		}
	}
	else
	{
		// we're currently in an unknown node and we're ignoring it - so we need to ignore the child too
		childNode = nullptr;
	}

	// do we have a callback function for beginning this node?
	if (childNode && childNode->m_beginFunc)
	{
		// we do - call it
		Attributes attributesObject(attributes);
		ElementResult result = childNode->m_beginFunc(attributesObject);

		// were we instructed to skip?
		if (result == ElementResult::Skip)
			childNode = nullptr;
	}

	// and push this onto the stack
	m_currentNodeStack.push(childNode);

	// set up content, but only if we expect to emit it later
	if (childNode && childNode->m_endFunc)
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

void XmlParser::endElement(const char *) noexcept
{
	ProfilerScope prof(CURRENT_FUNCTION);

	// call back the end func, if appropriate
	const Node *currentNode = m_currentNodeStack.top();
	if (currentNode && currentNode->m_endFunc)
	{
		currentNode->m_endFunc(m_currentContent ? std::move(*m_currentContent) : std::u8string());
		m_currentContent.reset();
	}

	// and go up the tree
	m_currentNodeStack.pop();
}


//-------------------------------------------------
//  characterData
//-------------------------------------------------

void XmlParser::characterData(const char *s, int len) noexcept
{
	if (m_currentContent)
		*m_currentContent += std::u8string_view((const char8_t *) s, len);
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
//  Attribute::operator bool
//-------------------------------------------------

XmlParser::Attribute::operator bool() const
{
	return bool(*m_valuePtr);
}


//-------------------------------------------------
//  Attribute::as<int>
//-------------------------------------------------

template<> std::optional<int> XmlParser::Attribute::as<int>() const noexcept
{
	return as<int>(-1);
}


//-------------------------------------------------
//  Attribute::as<int>
//-------------------------------------------------

template<> std::optional<int> XmlParser::Attribute::as<int>(int radix) const noexcept
{
	return as<int>(strtoll_parser<int>(radix));
}


//-------------------------------------------------
//  Attribute::as<std::uint8_t>
//-------------------------------------------------

template<> std::optional<std::uint8_t> XmlParser::Attribute::as() const noexcept
{
	return as<std::uint8_t>(-1);
}


//-------------------------------------------------
//  Attribute::as<std::uint8_t>
//-------------------------------------------------

template<> std::optional<std::uint8_t> XmlParser::Attribute::as(int radix) const noexcept
{
	return as<std::uint8_t>(strtoull_parser<std::uint8_t>(radix));
}


//-------------------------------------------------
//  Attribute::as<std::uint32_t>
//-------------------------------------------------

template<> std::optional<std::uint32_t> XmlParser::Attribute::as() const noexcept
{
	return as<std::uint32_t>(-1);
}


//-------------------------------------------------
//  Attribute::as<std::uint32_t>
//-------------------------------------------------

template<> std::optional<std::uint32_t> XmlParser::Attribute::as(int radix) const noexcept
{
	return as<std::uint32_t>(strtoull_parser<std::uint32_t>(radix));
}


//-------------------------------------------------
//  Attribute::as<std::uint64_t>
//-------------------------------------------------

template<> std::optional<std::uint64_t> XmlParser::Attribute::as() const noexcept
{
	return as<std::uint64_t>(-1);
}


//-------------------------------------------------
//  Attribute::as<std::uint64_t>
//-------------------------------------------------

template<> std::optional<std::uint64_t> XmlParser::Attribute::as(int radix) const noexcept
{
	return as<std::uint64_t>(strtoull_parser<std::uint64_t>(radix));
}


//-------------------------------------------------
//  Attribute::as<bool>
//-------------------------------------------------

template<> std::optional<bool> XmlParser::Attribute::as<bool>() const noexcept
{
	return as<bool>(s_bool_parser);
}


//-------------------------------------------------
//  Attribute::as<float>
//-------------------------------------------------

template<> std::optional<float> XmlParser::Attribute::as<float>() const noexcept
{
	return as<float>(strtof_parser<float>());
}


//-------------------------------------------------
//  Attribute::as<QString>
//-------------------------------------------------

template<> std::optional<QString> XmlParser::Attribute::as<QString>() const noexcept
{
	return *m_valuePtr
		? QString::fromUtf8(*m_valuePtr)
		: std::optional<QString>();
}


//-------------------------------------------------
//  Attribute::as<const char8_t *>
//-------------------------------------------------

template<> std::optional<const char8_t *> XmlParser::Attribute::as<const char8_t *>() const noexcept
{
	const char8_t *s = reinterpret_cast<const char8_t *>(*m_valuePtr);
	return s
		? s
		: std::optional<const char8_t *>();
}


//-------------------------------------------------
//  Attribute::as<std::u8string_view>
//-------------------------------------------------

template<> std::optional<std::u8string_view> XmlParser::Attribute::as<std::u8string_view>() const noexcept
{
	const char8_t *s = reinterpret_cast<const char8_t *>(*m_valuePtr);
	return s
		? s
		: std::optional<std::u8string_view>();
}


//-------------------------------------------------
//  Attribute::reportAttributeParsingError
//-------------------------------------------------

void XmlParser::Attribute::reportAttributeParsingError(const char *attrName, const char *attrValue) noexcept
{
	QString message = QString("Error parsing attribute \"%1\" (text=\"%2\")").arg(
		QString(attrName),
		QString(attrValue));
	XmlParser::s_currentParser->appendError(std::move(message));
}


//-------------------------------------------------
//  Attributes ctor
//-------------------------------------------------

XmlParser::Attributes::Attributes(const char **attributes)
	: m_attributes(attributes)
	, m_zero(nullptr)
{
}
