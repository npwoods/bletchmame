/***************************************************************************

	iniparser_test.cpp

	Unit tests for iniparser.cpp

***************************************************************************/

#include "iniparser.h"
#include "test.h"

namespace
{
	class Test : public QObject
	{
		Q_OBJECT

	private slots:
		void general();
	};
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void Test::general()
{
	// open the file
	QFile file(":/resources/testini.ini");
	QVERIFY(file.open(QIODevice::ReadOnly));

	// set up the parser
	IniFileParser iniParser(file);

	// first value
	QString name, value;
	QVERIFY(iniParser.next(name, value));
	QVERIFY(name == "alpha");
	QVERIFY(value == "alpha_value");

	// next value
	QVERIFY(iniParser.next(name, value));
	QVERIFY(name == "bravo");
	QVERIFY(value == "bravo value");

	// at the end
	QVERIFY(!iniParser.next(name, value));
	QVERIFY(name == "");
	QVERIFY(value == "");
}


//-------------------------------------------------

static TestFixture<Test> fixture;
#include "iniparser_test.moc"
