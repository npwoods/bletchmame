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
	void general();
	void areFileInfosEquivalent();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void ImportMameIniJob::Test::general()
{
	// create a temporary directory
	QTemporaryDir tempDirObj;
	QVERIFY(tempDirObj.isValid());
	QDir tempDir(tempDirObj.path());

	// copy our test MAME.INI to the temporary directory
	QString mameIniPath = tempDir.filePath("mame.ini");
	QVERIFY(QFile::copy(":/resources/testmameini.ini", mameIniPath));

	// set up preferences
	Preferences prefs;
	prefs.setGlobalPath(Preferences::global_path_type::ROMS,	tempDir.filePath("./mamefiles/roms1"));
	prefs.setGlobalPath(Preferences::global_path_type::SAMPLES, tempDir.filePath("./mamefiles/samples"));

	// set up an import job and import our INI
	ImportMameIniJob importJob(prefs);
	QVERIFY(importJob.loadMameIni(mameIniPath));

	// and validate the results
	QVERIFY(importJob.entries().size() == 10);
	QVERIFY(importJob.entries()[0]->labelDisplayText()	== "ROMs");
	QVERIFY(importJob.entries()[0]->importAction()		== ImportMameIniJob::ImportAction::AlreadyPresent);
	QVERIFY(importJob.entries()[1]->labelDisplayText()	== "ROMs");
	QVERIFY(importJob.entries()[1]->importAction()		== ImportMameIniJob::ImportAction::Supplement);
	QVERIFY(importJob.entries()[2]->labelDisplayText()	== "Samples");
	QVERIFY(importJob.entries()[2]->importAction()		== ImportMameIniJob::ImportAction::AlreadyPresent);
	QVERIFY(importJob.entries()[3]->labelDisplayText()	== "Config Files");
	QVERIFY(importJob.entries()[3]->importAction()		== ImportMameIniJob::ImportAction::Replace);
	QVERIFY(importJob.entries()[4]->labelDisplayText()	== "NVRAM Files");
	QVERIFY(importJob.entries()[4]->importAction()		== ImportMameIniJob::ImportAction::Replace);
	QVERIFY(importJob.entries()[5]->labelDisplayText()	== "CHD Diff Files");
	QVERIFY(importJob.entries()[5]->importAction()		== ImportMameIniJob::ImportAction::Replace);
	QVERIFY(importJob.entries()[6]->labelDisplayText()	== "Hash Files");
	QVERIFY(importJob.entries()[6]->importAction()		== ImportMameIniJob::ImportAction::Supplement);
	QVERIFY(importJob.entries()[7]->labelDisplayText()	== "Artwork Files");
	QVERIFY(importJob.entries()[7]->importAction()		== ImportMameIniJob::ImportAction::Supplement);
	QVERIFY(importJob.entries()[8]->labelDisplayText()	== "Plugins");
	QVERIFY(importJob.entries()[8]->importAction()		== ImportMameIniJob::ImportAction::Supplement);
	QVERIFY(importJob.entries()[9]->labelDisplayText()	== "Cheats");
	QVERIFY(importJob.entries()[9]->importAction()		== ImportMameIniJob::ImportAction::Supplement);
}


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
