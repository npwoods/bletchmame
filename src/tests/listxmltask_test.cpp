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
	QFile testAsset(":/resources/listxml.xml");
	QVERIFY(testAsset.open(QFile::ReadOnly));

	// process!
	task.internalProcess(testAsset);

	// verify that the result file is there
	QFileInfo fi(outputPath);
	QVERIFY(fi.isFile());
}


static TestFixture<ListXmlTask::Test> fixture;
#include "listxmltask_test.moc"
