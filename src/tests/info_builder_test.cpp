/***************************************************************************

    info_builder_test.cpp

    Unit tests for info_builder.cpp

***************************************************************************/

// bletchmame headers
#include "info_builder.h"
#include "test.h"

// Qt headers
#include <QBuffer>

using namespace std::literals;


// ======================> Test

class info::database_builder::Test : public QObject
{
    Q_OBJECT

private slots:
    void general();
	void compareBinaries_alienar()	{ compareBinaries(":/resources/listxml_alienar.xml"); }
	void compareBinaries_coco()		{ compareBinaries(":/resources/listxml_coco.xml"); }
	void compareBinaries_fake()		{ compareBinaries(":/resources/listxml_fake.xml"); }
	void stringTable();
	void singleString1()			{ singleString(u8""sv); }
	void singleString2()			{ singleString(u8"A"sv); }
	void singleString3()			{ singleString(u8"BC"sv); }
	void singleString4()			{ singleString(u8"CDE"sv); }
	void singleString5()			{ singleString(u8"FGHI"sv); }
	void singleString6()			{ singleString(u8"JKLMN"sv); }
	void singleString7()			{ singleString(u8"OPQRS"sv); }
	void singleString8()			{ singleString(u8"TUVWX"sv); }
	void singleString9()			{ singleString(u8"YZabc"sv); }
	void singleString10()			{ singleString(u8"defgh"sv); }
	void singleString11()			{ singleString(u8"ijklm"sv); }
	void singleString12()			{ singleString(u8"nopqr"sv); }
	void singleString13()			{ singleString(u8"stuvw"sv); }
	void singleString14()			{ singleString(u8"xyz0"sv); }
	void singleString15()			{ singleString(u8"12345"sv); }
	void singleString16()			{ singleString(u8"6789 "sv); }
	void singleString17()			{ singleString(u8"123!!"sv); }
	void singleString18()			{ singleString(u8"a_very_big_STRING!!!!"sv); }
	void singleString19()			{ singleString(u8"***another very_big_STRING!!!!"sv); }

private:
	void compareBinaries(const QString &fileName);
	void singleString(std::u8string_view s);
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  buildInfoDatabase
//-------------------------------------------------

static QByteArray buildInfoDatabase(QIODevice &stream, QString &errorMessage)
{
	// process the results
	info::database_builder builder;
	bool success = builder.process_xml(stream, errorMessage);

	// get the stream
	QByteArray result;
	if (success)
	{
		QBuffer buffer(&result);
		if (!buffer.open(QIODevice::WriteOnly))
			throw false;
		builder.emit_info(buffer);
	}
	return result;
}


//-------------------------------------------------
//  buildInfoDatabase
//-------------------------------------------------

QByteArray buildInfoDatabase(const QString &fileName, bool skipDtd)
{
	// open the file
	QFile file(fileName);
	if (!file.open(QFile::ReadOnly))
		return QByteArray();

	// if we're asked to, skip the DTD
	if (skipDtd)
	{
		char lastChars[2] = { 0, };
		while (lastChars[0] != ']' || lastChars[1] != '>')
		{
			QByteArray byteArray = file.read(1);
			if (byteArray.size() != 1)
				throw false;
			lastChars[0] = lastChars[1];
			lastChars[1] = byteArray[0];
		}
	}

	// process the file
	QString errorMessage;
	QByteArray result = buildInfoDatabase(file, errorMessage);
	return errorMessage.isEmpty()
		? result
		: QByteArray();
}


//-------------------------------------------------
//  general
//-------------------------------------------------

void info::database_builder::Test::general()
{
	QByteArray byteArray = buildInfoDatabase();
	QVERIFY(byteArray.size() > 0);
}


//-------------------------------------------------
//  compareBinaries - perform a binary comparison
//	to validate that byte for byte, the info DB we
//	build is identical to whats there
// 
//	a caveat - info DB is (for now) endian sensitive,
//  but in practice all of the platforms we care
//	about are little endian
//-------------------------------------------------

void info::database_builder::Test::compareBinaries(const QString &fileName)
{
	const bool dumpBinaries = false;

	// get the binaries
	QByteArray byteArray = buildInfoDatabase(fileName);
	QVERIFY(byteArray.size() > 0);

	// get the binary file name
	QString binFileName = fileName;
	binFileName.replace(".xml", ".bin");
	QVERIFY(binFileName != fileName);

	// dump the binaries if appropriate
	if (dumpBinaries)
	{
		int lastSlashPos = binFileName.lastIndexOf('/');
		QVERIFY(lastSlashPos >= 0);
		QString dumpBinFileName = binFileName.right(binFileName.size() - lastSlashPos - 1);

		QFile dumpFile(dumpBinFileName);
		QVERIFY(dumpFile.open(QIODevice::WriteOnly));
		dumpFile.write(byteArray);
	}

	// read the binary file
	QFile binFile(binFileName);
	QVERIFY(binFile.open(QIODevice::ReadOnly));
	QByteArray binByteArray = binFile.readAll();
	binFile.close();

	// the moment of truth - is the binary that we built byte-for-byte identical to what is there?
	QVERIFY(byteArray.size() == binByteArray.size());
	for (int i = 0; i < byteArray.size(); i++)
	{
		if (byteArray[i] != binByteArray[i])
		{
			qWarning("Difference at index #%d (0x%02X vs 0x%02X)",
				(int)i,
				(unsigned)(std::uint8_t)byteArray[i],
				(unsigned)(std::uint8_t)binByteArray[i]);
			break;
		}
	}
	QVERIFY(byteArray == binByteArray);
}


//-------------------------------------------------
//  stringTable
//-------------------------------------------------

void info::database_builder::Test::stringTable()
{
	using namespace std::literals;

	// create a string table (shink it to fit so we exercise growing)
	string_table stringTable;
	stringTable.shrinkToFit();

	// add some strings
	std::uint32_t empty1 = stringTable.get(u8"");
	std::uint32_t alpha1 = stringTable.get(u8"Alpha");
	std::uint32_t bravo1 = stringTable.get(u8"BravoBravo");
	std::uint32_t charlie1 = stringTable.get(u8"Charlie");
	std::uint32_t delta1 = stringTable.get(u8"DeltaDelta");
	std::uint32_t alpha2 = stringTable.get(u8"Alpha");
	std::uint32_t empty2 = stringTable.get(u8"");
	std::uint32_t bravo2 = stringTable.get(u8"BravoBravo");
	std::uint32_t bravo3 = stringTable.get(u8"BravoBravo");
	std::uint32_t delta2 = stringTable.get(u8"DeltaDelta");
	std::uint32_t charlie2 = stringTable.get(u8"Charlie");
	std::uint32_t delta3 = stringTable.get(u8"DeltaDelta");

	// verify that a bunch of them are what we expect
	QVERIFY(empty1 == empty2);
	QVERIFY(alpha1 == alpha2);
	QVERIFY(bravo1 == bravo2);
	QVERIFY(bravo1 == bravo3);
	QVERIFY(charlie1 == charlie2);
	QVERIFY(delta1 == delta2);
	QVERIFY(delta1 == delta3);

	// and validate that the lookups work
	string_table::SsoBuffer sso;
	QVERIFY(std::u8string_view(stringTable.lookup(empty1, sso)) == u8""sv);
	QVERIFY(std::u8string_view(stringTable.lookup(alpha1, sso)) == u8"Alpha"sv);
	QVERIFY(std::u8string_view(stringTable.lookup(bravo1, sso)) == u8"BravoBravo"sv);
	QVERIFY(std::u8string_view(stringTable.lookup(charlie1, sso)) == u8"Charlie"sv);
	QVERIFY(std::u8string_view(stringTable.lookup(delta1, sso)) == u8"DeltaDelta"sv);
}


//-------------------------------------------------
//  singleString
//-------------------------------------------------

void info::database_builder::Test::singleString(std::u8string_view s)
{
	// create a string table (shink it to fit so we exercise growing)
	string_table stringTable;
	stringTable.shrinkToFit();

	// verify that the string is added sensically
	std::uint32_t s1 = stringTable.get(s);
	std::uint32_t s2 = stringTable.get(s);
	QVERIFY(s1 == s2);

	// and verify that a lookup does the right thing
	string_table::SsoBuffer sso;
	QVERIFY(stringTable.lookup(s1, sso) == s);
}


//-------------------------------------------------

static TestFixture<info::database_builder::Test> fixture;
#include "info_builder_test.moc"
