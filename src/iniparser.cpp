/***************************************************************************

	iniparser.cpp

	Code for parsing INI files

***************************************************************************/

#include "iniparser.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

IniFileParser::IniFileParser(QIODevice &stream)
	: m_textStream(&stream)
{
}


//-------------------------------------------------
//  next
//-------------------------------------------------

bool IniFileParser::next(QString &name, QString &value)
{
	name = "";
	value = "";

	enum class State
	{
		BeforeName,
		Name,
		BeforeValue,
		Value,
		ValueQuoted,
		End
	};

	QString line;
	bool success = false;
	while (!success && m_textStream.readLineInto(&line))
	{
		State state = State::BeforeName;

		size_t i = 0;
		size_t nameStart = 0;
		size_t nameLength = 0;
		size_t valueStart = 0;
		size_t valueLength = 0;
		while (state != State::End)
		{
			std::optional<QChar> ch = i < (size_t)line.size()
				? line[i]
				: std::optional<QChar>();

			switch (state)
			{
			case State::BeforeName:
				if (!ch || ch == '#')
					state = State::End;
				else if (!ch->isSpace())
				{
					state = State::Name;
					nameStart = i;
				}
				break;

			case State::Name:
				if (!ch || ch == '#')
					state = State::End;
				else if (ch->isSpace())
				{
					state = State::BeforeValue;
					nameLength = i - nameStart;
				}
				break;

			case State::BeforeValue:
				if (!ch || ch == '#')
					state = State::End;
				else if (ch == '\"')
				{
					valueStart = i + 1;
					state = State::ValueQuoted;
				}
				else if (!ch->isSpace())
				{
					valueStart = i;
					state = State::Value;
				}
				break;

			case State::Value:
				if (!ch || ch == '#' || ch->isSpace())
				{
					state = State::End;
					valueLength = i - valueStart;
				}
				break;

			case State::ValueQuoted:
				if (!ch || ch == '#' || ch == '\"')
				{
					state = State::End;
					valueLength = i - valueStart;
				}
				break;

			default:
				throw false;
			}

			if (ch)
				i++;
		}

		// we're through with the line - did we find something?
		if (nameLength > 0 && valueLength > 0)
		{
			name = line.mid(nameStart, nameLength);
			value = line.mid(valueStart, valueLength);
			success = true;
		}
	}
	return success;
}
