/***************************************************************************

	importmameinijob_test.cpp

	Unit tests for importmameinijob.cpp

***************************************************************************/

// bletchmame headers
#include "importmameinijob.h"
#include "test.h"

class ImportMameIniJob::Test : public QObject
{
	Q_OBJECT

private slots:
	void areFileInfosEquivalent();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  areFileInfosEquivalent
//-------------------------------------------------

void ImportMameIniJob::Test::areFileInfosEquivalent()
{
	// create a temporary directory
	QTemporaryDir tempDirObj;
	QVERIFY(tempDirObj.isValid());
	QDir tempDir(tempDirObj.path());

	// create two directories
	tempDir.mkdir("./dir1");
	tempDir.mkdir("./dir2");

	// some QFileInfos...
	QFileInfo dir1(tempDir.filePath("./dir1"));
	QFileInfo dir2(tempDir.filePath("./dir2"));
	QFileInfo dir3(tempDir.filePath("./dir3"));
	QFileInfo dir1x(tempDir.filePath("././dir1"));

	// the following should be equivalent
	QVERIFY(ImportMameIniJob::areFileInfosEquivalent(dir1, dir1));
	QVERIFY(ImportMameIniJob::areFileInfosEquivalent(dir1, dir1x));
	QVERIFY(ImportMameIniJob::areFileInfosEquivalent(dir1x, dir1));
	QVERIFY(ImportMameIniJob::areFileInfosEquivalent(dir1x, dir1x));
	QVERIFY(ImportMameIniJob::areFileInfosEquivalent(dir2, dir2));
	QVERIFY(ImportMameIniJob::areFileInfosEquivalent(dir3, dir3));

	// the following should not be equivalent
	QVERIFY(!ImportMameIniJob::areFileInfosEquivalent(dir1, dir2));
	QVERIFY(!ImportMameIniJob::areFileInfosEquivalent(dir2, dir3));
	QVERIFY(!ImportMameIniJob::areFileInfosEquivalent(dir1x, dir3));
}


//-------------------------------------------------

static TestFixture<ImportMameIniJob::Test> fixture;
#include "importmameinijob_test.moc"
