/***************************************************************************

	hash_test.cpp

	Unit tests for hash.cpp

***************************************************************************/

#include <QBuffer>
#include <QTest>

#include "hash.h"
#include "test.h"


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

class Hash::Test : public QObject
{
	Q_OBJECT

private slots:
	void calculate();
	void calculateForEmptyFile();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  calculate
//-------------------------------------------------

void Hash::Test::calculate()
{
	// get a resource
	QFile file(":/resources/garbage.bin");
	QVERIFY(file.open(QIODevice::ReadOnly));

	// process hashes
	Hash hash = Hash::calculate(file);

	// and verify
	QVERIFY(hash.crc32() == 0x0FAF9FDB);
	std::array<std::uint8_t, 20> expectedSha1({ 0xC2, 0x79, 0x09, 0x18, 0x4E, 0xE9, 0x17, 0x07, 0x07, 0xC1, 0xBE, 0x9A, 0x4C, 0xFB, 0xE8, 0x3B, 0x35, 0x96, 0x72, 0xE1 });
	QVERIFY(hash.sha1() == expectedSha1);

	// verify toString too
	QVERIFY(hash.toString() == "CRC(0faf9fdb) SHA1(c27909184ee9170707c1be9a4cfbe83b359672e1)");
}


//-------------------------------------------------
//  calculateForEmptyFile
//-------------------------------------------------

void Hash::Test::calculateForEmptyFile()
{
	// create an empty buffer
	QBuffer emptyBuffer;
	QVERIFY(emptyBuffer.open(QIODevice::ReadOnly));

	// process hashes
	Hash::calculate(emptyBuffer);
}


static TestFixture<Hash::Test> fixture;
#include "hash_test.moc"
