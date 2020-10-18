/***************************************************************************

	listxmltask_test.cpp

	Unit tests for listxmltask.cpp

***************************************************************************/

#include <QTemporaryDir>

#include "listxmltask.h"
#include "test.h"


class ListXmlTask::Test : public QObject
{
	Q_OBJECT

private slots:
	void internalProcess();
	void pathWithFile();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  internalProcess
//-------------------------------------------------

void ListXmlTask::Test::internalProcess()
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
	task.internalProcess(testAsset);

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

	// process - this should throw an exception
	std::unique_ptr<list_xml_exception> caughtException;
	try
	{
		task.internalProcess(testAsset);
	}
	catch (list_xml_exception &exception)
	{
		caughtException = std::make_unique<list_xml_exception>(std::move(exception));
	}

	// and validate the exception
	QVERIFY(caughtException);
	QVERIFY(caughtException->m_status == ListXmlResultEvent::Status::ERROR);
}


static TestFixture<ListXmlTask::Test> fixture;
#include "listxmltask_test.moc"
