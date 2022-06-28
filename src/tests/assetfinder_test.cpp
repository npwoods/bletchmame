/***************************************************************************

	assetfinder_test.cpp

	Unit tests for assetfinder.cpp

***************************************************************************/

// bletchmame headers
#include "assetfinder.h"
#include "hash.h"
#include "test.h"

// Qt headers
#include <QBuffer>


namespace
{
	class Test : public QObject
	{
		Q_OBJECT

	private slots:
		void empty();
		void archive_zip()				{ archive(":/resources/sample_archive.zip"); }
		void archive_7zip()				{ archive(":/resources/sample_archive.7z"); }
		void isValidArchive_zip()       { isValidArchive(":/resources/sample_archive.zip", true); }
		void isValidArchive_7zip()      { isValidArchive(":/resources/sample_archive.7z", true); }
		void isValidArchive_garbage()   { isValidArchive(":/resources/garbage.bin", false); }
		void loadByCrc_zip_1()			{ loadByCrc(":/resources/sample_archive.zip", "verybig/big1.bin"); }
		void loadByCrc_zip_2()			{ loadByCrc(":/resources/sample_archive.zip", "verybig/big2.bin"); }
		void loadByCrc_zip_3()			{ loadByCrc(":/resources/sample_archive.zip", "verybig/big3.bin"); }

	private:
		void isValidArchive(const char *path, bool expectedResult);
		void archive(const QString &fileName);
		void loadByCrc(const QString &fileName, const QString &member);
	};
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  empty
//-------------------------------------------------

void Test::empty()
{
	AssetFinder assetFinder;
	QVERIFY(!assetFinder.findAssetBytes("unknown.txt"));
}


//-------------------------------------------------
//  archive
//-------------------------------------------------

void Test::archive(const QString &fileName)
{
	// the subtleties with these tests is that we need to specifically emulate MAME's behavior
	AssetFinder assetFinder;
	assetFinder.setPaths({ fileName });

	// basic lookups by filename
	QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("alpha.txt").value_or(QByteArray())) == "11111");
	QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("bravo.txt").value_or(QByteArray())) == "22222");
	QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("charlie.txt").value_or(QByteArray())) == "33333");
	QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("subdir/delta.txt").value_or(QByteArray())) == "4444444444");
	QVERIFY(assetFinder.findAssetBytes("verybig/big1.bin").value_or(QByteArray()).size() == 100000);
	QVERIFY(assetFinder.findAssetBytes("verybig/big2.bin").value_or(QByteArray()).size() == 110000);
	QVERIFY(assetFinder.findAssetBytes("verybig/big3.bin").value_or(QByteArray()).size() == 120000);

	// MAME's zip support is case insensitive
	QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("CHaRLie.TXT").value_or(QByteArray())) == "33333");

	// and MAME's zip support has searching by CRC32 in addition to name
	QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("FIND_CHARLIE_BY_CRC", 0xAFAB3DEB).value_or(QByteArray())) == "33333");
	QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("bravo.txt", 0xBAADF00D).value_or(QByteArray())) == "22222");

	// given a clash between "correct filename wrong CRC32" and "wrong filename correct CRC32", the latter wins
	QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("alpha.txt", 0xAFAB3DEB).value_or(QByteArray())) == "33333");

	// unknown file lookups
	QVERIFY(!assetFinder.findAssetBytes("unknown.txt"));
	QVERIFY(!assetFinder.findAssetBytes("UNKNOWN_CRC", 0xBAADF00D));
}


//-------------------------------------------------
//  isValidArchive
//-------------------------------------------------

void Test::isValidArchive(const char *path, bool expectedResult)
{
	bool result = AssetFinder::isValidArchive(path);
	QVERIFY(result == expectedResult);
}


//-------------------------------------------------
//  loadByCrc
//-------------------------------------------------

void Test::loadByCrc(const QString &fileName, const QString &member)
{
	AssetFinder assetFinder;
	assetFinder.setPaths({ fileName });

	// look up the member by file name
	std::optional<QByteArray> byteArray = assetFinder.findAssetBytes(member);
	QVERIFY(byteArray);

	// prepare a QBuffer with the same bytes
	QBuffer buffer;
	buffer.setData(*byteArray);
	QVERIFY(buffer.open(QIODevice::ReadOnly));

	// calculate the CRC32
	std::optional<Hash> hash = Hash::calculate(buffer, [](std::uint64_t) { return false; });
	QVERIFY(hash);
	QVERIFY(hash->crc32());
	std::uint32_t crc32 = *hash->crc32();

	// open up the file again by CRC-32
	std::optional<QByteArray> byteArray2 = assetFinder.findAssetBytes("NONSENSE", crc32);
	QVERIFY(byteArray2 == byteArray);
}


//-------------------------------------------------

static TestFixture<Test> fixture;
#include "assetfinder_test.moc"
