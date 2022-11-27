/***************************************************************************

	iniparser.h

	Code for parsing INI files

***************************************************************************/

#ifndef INIPARSER_H
#define INIPARSER_H

// Qt headers
#include <QTextStream>


// ======================> IniFileParser

class IniFileParser
{
public:
	IniFileParser(QIODevice &stream);

	// methods
	bool next(QString &name, QString &value);

private:
	QTextStream	m_textStream;
};


#endif // INIPARSER_H
