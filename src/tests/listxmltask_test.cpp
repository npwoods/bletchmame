/***************************************************************************

	listxmltask_test.cpp

	Unit tests for listxmltask.cpp

***************************************************************************/

// bletchmame headers
#include "listxmltask.h"
#include "test.h"

// Qt headers
#include <QTemporaryDir>


class ListXmlTask::Test : public QObject
{
	Q_OBJECT

private slots:
	void internalRun();
	void pathWithFile();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  internalRun
//-------------------------------------------------

void ListXmlTask::Test::internalRun()
{
	// we want to create an infodb deep within in a temporary directory
	QTemporaryDir tempDir;
	QString outputPath = tempDir.filePath("subdir1/subdir2/foo.infodb");

	// create a task
	auto task = ListXmlTask(QString(outputPath));

	// create a test asset
	QFile testAsset(":/resources/listxml_coco.xml");
	QVERIFY(testAsset.open(QFile::ReadOnly));

	// process!
	task.internalRun(testAsset);

	// verify that the result file is there
	QFileInfo fi(outputPath);
	QVERIFY(fi.isFile());
}


//-------------------------------------------------
//  pathWithFile
//-------------------------------------------------

void ListXmlTask::Test::pathWithFile()
{
	// create a temporary directory with a file
	QTemporaryDir tempDir;
	{
		QFile file(tempDir.filePath("tempFile.bin"));
		QVERIFY(file.open(QFile::WriteOnly));
		file.write("foo", 3);
	}

	// now try to create an infodb on a path "mangled" by theat file
	auto task = ListXmlTask(tempDir.filePath("tempFile.bin/subdir/subdir/foo.infodb"));

	// create a test asset
	QFile testAsset(":/resources/listxml_coco.xml");
	QVERIFY(testAsset.open(QFile::ReadOnly));

	// process - this should return an error
	std::optional<ListXmlError> caughtError = task.internalRun(testAsset);

	// and validate the exception
	QVERIFY(caughtError);
	QVERIFY(caughtError->status() == ListXmlResultEvent::Status::ERROR);
}


static TestFixture<ListXmlTask::Test> fixture;
#include "listxmltask_test.moc"
