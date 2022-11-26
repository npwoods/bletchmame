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
	void expandEnvironmentVariablesGeneral_1() { expandEnvironmentVariablesGeneral("FooBar", "FooBar"); }
	void expandEnvironmentVariablesGeneral_2() { expandEnvironmentVariablesGeneral("FooBar $VAR1", "FooBar Alpha"); }
	void expandEnvironmentVariablesGeneral_3() { expandEnvironmentVariablesGeneral("FooBar $VAR2", "FooBar Bravo"); }
	void expandEnvironmentVariablesGeneral_4() { expandEnvironmentVariablesGeneral("FooBar $VAR1 $VAR2", "FooBar Alpha Bravo"); }
	void expandEnvironmentVariablesGeneral_5() { expandEnvironmentVariablesGeneral("FooBar $BLAH", "FooBar $BLAH"); }
	void expandEnvironmentVariablesGeneral_6() { expandEnvironmentVariablesGeneral("FooBar $VAR1 $BLAH", "FooBar Alpha $BLAH"); }
	void expandEnvironmentVariablesGeneral_7() { expandEnvironmentVariablesGeneral("FooBar $", "FooBar $"); }
	void expandEnvironmentVariablesGeneral_8() { expandEnvironmentVariablesGeneral("FooBar $ $VAR1", "FooBar $ Alpha"); }
	void expandEnvironmentVariablesGeneral_9() { expandEnvironmentVariablesGeneral("$VAR1", "Alpha"); }
	void expandEnvironmentVariablesGeneral_10() { expandEnvironmentVariablesGeneral("$VAR2", "Bravo"); }

private:
	void expandEnvironmentVariablesGeneral(const QString& input, const QString& expected);
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
//  fakeGetEnv
//-------------------------------------------------

static QByteArray fakeGetEnv(const char* varName)
{
	QByteArray result;
	if (!strcmp(varName, "VAR1"))
		result = QString("Alpha").toLatin1();
	else if (!strcmp(varName, "VAR2"))
		result = QString("Bravo").toLatin1();
	return result;
}


//-------------------------------------------------
//  expandEnvironmentVariablesGeneral
//-------------------------------------------------

void ImportMameIniJob::Test::expandEnvironmentVariablesGeneral(const QString &input, const QString &expected)
{
	QString actual = ImportMameIniJob::expandEnvironmentVariablesGeneral(input, fakeGetEnv);
	QVERIFY(actual == expected);
}


//-------------------------------------------------

static TestFixture<ImportMameIniJob::Test> fixture;
#include "importmameinijob_test.moc"
