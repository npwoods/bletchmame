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
	void handleBadRead();
	void mask_00()	{ mask(false, false, ""); }
	void mask_01()	{ mask(false, true, "SHA1(0123456789abcdef0123456789abcdef01234567)"); }
	void mask_10()	{ mask(true, false, "CRC(0123abcd)"); }
	void mask_11()	{ mask(true, true, "CRC(0123abcd) SHA1(0123456789abcdef0123456789abcdef01234567)"); }

private:
	void mask(bool useCrc32, bool useSha1, const char *expected);
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


//-------------------------------------------------
//  handleBadRead
//-------------------------------------------------

void Hash::Test::handleBadRead()
{
	// set up a byte array; this is necessary so that QBuffer::atEnd() returns false
	const char bytes[] = { 0x11, 0x22, 0x33, 0x44 };
	QByteArray byteArray(bytes, sizeof(bytes));

	// create an buffer pointed at that byte array
	QBuffer emptyBuffer(&byteArray);
	QVERIFY(emptyBuffer.open(QIODevice::WriteOnly));

	// process hashes
	Hash::calculate(emptyBuffer);
}


//-------------------------------------------------
//  mask
//-------------------------------------------------

void Hash::Test::mask(bool useCrc32, bool useSha1, const char *expected)
{
	// create a hash
	std::array<std::uint8_t, 20> sha1({ 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67 });
	Hash hash(0x0123ABCD, sha1);

	// mask it
	Hash result = hash.mask(useCrc32, useSha1);

	// validate
	QVERIFY(result.toString() == expected);
}


//-------------------------------------------------

static TestFixture<Hash::Test> fixture;
#include "hash_test.moc"
