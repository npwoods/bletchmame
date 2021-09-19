/***************************************************************************

	audit_test.cpp

	Unit tests for audit.cpp

***************************************************************************/

#include <QBuffer>

#include "audit.h"
#include "test.h"


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

class Audit::Test : public QObject
{
	Q_OBJECT

private slots:
	void calculateHashes();
	void calculateHashesForEmptyFile();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  calculateHashes
//-------------------------------------------------

void Audit::Test::calculateHashes()
{
	// get a resource
	QFile file(":/resources/garbage.bin");
	QVERIFY(file.open(QIODevice::ReadOnly));

	// process hashes
	std::uint32_t crc32;
	QByteArray sha1;
	Audit::calculateHashes(file, &crc32, &sha1);

	// and verify
	QVERIFY(crc32 == 0x0FAF9FDB);
	std::uint8_t expectedSha1[] = { 0xC2, 0x79, 0x09, 0x18, 0x4E, 0xE9, 0x17, 0x07, 0x07, 0xC1, 0xBE, 0x9A, 0x4C, 0xFB, 0xE8, 0x3B, 0x35, 0x96, 0x72, 0xE1 };
	QVERIFY(sha1 == QByteArray((const char *)expectedSha1, std::size(expectedSha1)));
}


//-------------------------------------------------
//  calculateHashesForEmptyFile
//-------------------------------------------------

void Audit::Test::calculateHashesForEmptyFile()
{
	// create an empty buffer
	QBuffer emptyBuffer;
	QVERIFY(emptyBuffer.open(QIODevice::ReadOnly));

	// process hashes
	std::uint32_t crc32;
	QByteArray sha1;
	Audit::calculateHashes(emptyBuffer, &crc32, &sha1);
}


static TestFixture<Audit::Test> fixture;
#include "audit_test.moc"
