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
    AssetFinder assetFinder;
    assetFinder.setPaths({ ":/resources/sample_archive.zip" });

    QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("alpha.txt").value_or(QByteArray())) == "11111");
    QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("bravo.txt").value_or(QByteArray())) == "22222");
    QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("charlie.txt").value_or(QByteArray())) == "33333");
    QVERIFY(!assetFinder.findAssetBytes("unknown.txt"));
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
