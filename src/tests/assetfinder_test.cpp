/***************************************************************************

	assetfinder_test.cpp

	Unit tests for assetfinder.cpp

***************************************************************************/

#include "assetfinder.h"
#include "test.h"

namespace
{
	class Test : public QObject
	{
		Q_OBJECT

	private slots:
		void empty();
		void zip();
		void isValidArchive_zip()       { isValidArchive(":/resources/sample_archive.zip", true); }
		void isValidArchive_garbage()   { isValidArchive(":/resources/garbage.bin", false); }

	private:
		void isValidArchive(const char *path, bool expectedResult);
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
//  zip
//-------------------------------------------------

void Test::zip()
{
	// the subtleties with these tests is that we need to specifically emulate MAME's behavior
	AssetFinder assetFinder;
	assetFinder.setPaths({ ":/resources/sample_archive.zip" });

	// basic lookups by filename
	QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("alpha.txt").value_or(QByteArray())) == "11111");
	QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("bravo.txt").value_or(QByteArray())) == "22222");
	QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("charlie.txt").value_or(QByteArray())) == "33333");

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


static TestFixture<Test> fixture;
#include "assetfinder_test.moc"
