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
    QVERIFY(assetFinder.findAssetBytes("unknown.txt").isEmpty());
}


//-------------------------------------------------
//  zip
//-------------------------------------------------

void Test::zip()
{
    AssetFinder assetFinder;
    assetFinder.setPaths({ ":/resources/sample_archive.zip" });

    QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("alpha.txt")) == "11111");
    QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("bravo.txt")) == "22222");
    QVERIFY(QString::fromUtf8(assetFinder.findAssetBytes("charlie.txt")) == "33333");
    QVERIFY(assetFinder.findAssetBytes("unknown.txt").isEmpty());
}


static TestFixture<Test> fixture;
#include "assetfinder_test.moc"
