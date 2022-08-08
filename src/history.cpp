/***************************************************************************

	history.cpp

	Implements a database for history.xml files

***************************************************************************/

// bletchmame headers
#include "history.h"
#include "xmlparser.h"

// Qt headers
#include <QUrl>


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  load
//-------------------------------------------------

bool HistoryDatabase::load(const QString &fileName)
{
	QFile file(fileName);
	return file.open(QIODevice::ReadOnly) && load(file);
}


//-------------------------------------------------
//  load
//-------------------------------------------------

bool HistoryDatabase::load(QIODevice &stream)
{
	// prepare to parse a big history.xml file...
	clear();
	m_texts.reserve(120000);	// version dated 2022-06-30 has 107956 entries
	m_texts.reserve(170000);	// version dated 2022-06-30 has 160959 entries

	// set up the XML parser
	XmlParser xml;
	xml.onElementBegin({ "history", "entry" }, [this](const XmlParser::Attributes &)
	{
		m_texts.push_back(u8"");
	});
	xml.onElementBegin({ "history", "entry", "systems", "system" }, [this](const XmlParser::Attributes &attributes)
	{
		const auto [nameAttr] = attributes.get("name");
		std::u8string_view name = nameAttr.as<std::u8string_view>().value_or(u8"");
		m_lookup.insert({ MachineIdentifier(name), m_texts.size() - 1 });
	});
	xml.onElementBegin({ "history", "entry", "software", "item" }, [this](const XmlParser::Attributes &attributes)
	{
		const auto [listAttr, nameAttr] = attributes.get("list", "name");
		std::u8string_view list = listAttr.as<std::u8string_view>().value_or(u8"");
		std::u8string_view name = nameAttr.as<std::u8string_view>().value_or(u8"");
		m_lookup.insert({ SoftwareIdentifier(list, name), m_texts.size() - 1 });
	});
	xml.onElementEnd({ "history", "entry", "text" }, [this](std::u8string &&content)
	{
		std::u8string_view trimmedContent = util::trim(content);
		if (trimmedContent.size() != content.size())
			content = trimmedContent;
		util::last(m_texts) = std::move(content);
	});

	// and do the dirty work
	bool success = xml.parse(stream);
	m_texts.shrink_to_fit();
	return success;
}


//-------------------------------------------------
//  clear
//-------------------------------------------------

void HistoryDatabase::clear()
{
	m_texts.clear();
	m_lookup.clear();
}


//-------------------------------------------------
//  get
//-------------------------------------------------

std::u8string_view HistoryDatabase::get(const Identifier &identifier) const
{
	auto iter = m_lookup.find(identifier);
	return iter != m_lookup.end()
		? m_texts[iter->second]
		: std::u8string_view();
}


//-------------------------------------------------
//  getRichText
//-------------------------------------------------

QString HistoryDatabase::getRichText(const Identifier &identifier) const
{
	std::u8string_view text = get(identifier);
	return toRichText(text);
}


//-------------------------------------------------
//  isUrl
//-------------------------------------------------

static qsizetype isUrl(const QString &s, qsizetype i)
{
	qsizetype result = 0;
	QString target = "http";
	if (i + target.size() < s.size() && s.mid(i, target.size()) == target)
	{
		qsizetype len = target.size();
		while (i + len < s.size() && !s[i + len].isSpace())
			len++;

		QString candidateUrlString = s.mid(i, len);
		QUrl candidateUrl(candidateUrlString);
		if (candidateUrl.isValid() && !candidateUrl.isLocalFile() && !candidateUrl.isRelative())
			result = len;
	}
	return result;
}


//-------------------------------------------------
//  toRichText - converts raw history text to Qt's
//	rich text, which itself is a subset of HTML
//-------------------------------------------------

QString HistoryDatabase::toRichText(std::u8string_view text)
{
	// convert to QString
	QString text_qstring = util::toQString(text);

	// reserve space
	QString result;
	result.reserve(text_qstring.size() + 100);

	// iterate through the qstring
	for (qsizetype i = 0; i < text_qstring.size(); i++)
	{
		// is this a URL?
		qsizetype urlLength = isUrl(text_qstring, i);
		if (urlLength > 0)
		{
			result += QString("<a href=\"%1\">%1</a>").arg(
				text_qstring.mid(i, urlLength).toHtmlEscaped());
			i += urlLength - 1;
		}
		else if (text_qstring[i] == '\n')
		{
			result += "<br/>";
		}
		else if (text_qstring[i] == '&' || text_qstring[i] == '<' || text_qstring[i] == '>')
		{
			result += QString(text_qstring[i]).toHtmlEscaped();
		}
		else
		{
			result += text_qstring[i];
		}
	}

	// and we're done
	return result;
}
