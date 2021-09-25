/***************************************************************************

    info_builder_test.cpp

    Unit tests for info_builder.cpp

***************************************************************************/

#include <QBuffer>

#include "info_builder.h"
#include "test.h"

namespace
{
    class Test : public QObject
    {
        Q_OBJECT

    private slots:
        void general();
		void compareBinaries_coco()		{ compareBinaries(":/resources/listxml_coco.xml"); }
		void compareBinaries_alienar()	{ compareBinaries(":/resources/listxml_alienar.xml"); }

	private:
		void compareBinaries(const QString &fileName);
    };
}


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
//  test
//-------------------------------------------------

void Test::general()
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

void Test::compareBinaries(const QString &fileName)
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
	for (size_t i = 0; i < byteArray.size(); i++)
	{
		if (byteArray[i] != binByteArray[i])
		{
			char buffer[256];
			snprintf(buffer, std::size(buffer), "Difference at index #%d (0x%02X vs 0x%02X)",
				(int)i,
				(unsigned)(std::uint8_t)byteArray[i],
				(unsigned)(std::uint8_t)binByteArray[i]);
			QWARN(buffer);
			break;
		}
	}
	QVERIFY(byteArray == binByteArray);
}


//-------------------------------------------------

static TestFixture<Test> fixture;
#include "info_builder_test.moc"
