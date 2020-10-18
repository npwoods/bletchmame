/***************************************************************************

    info_builder_test.cpp

    Unit tests for info_builder.cpp

***************************************************************************/

#include <QBuffer>

#include "info_builder.h"
#include "test.h"

namespace
{
    class Test : public QObject
    {
        Q_OBJECT

    private slots:
        void general();
    };
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  buildInfoDatabase
//-------------------------------------------------

static QByteArray buildInfoDatabase(QIODevice &stream, QString &errorMessage)
{
	// process the results
	info::database_builder builder;
	bool success = builder.process_xml(stream, errorMessage);

	// get the stream
	QByteArray result;
	if (success)
	{
		QBuffer buffer(&result);
		if (!buffer.open(QIODevice::WriteOnly))
			throw false;
		builder.emit_info(buffer);
	}
	return result;
}


//-------------------------------------------------
//  buildInfoDatabase
//-------------------------------------------------

QByteArray buildInfoDatabase(const QString &fileName)
{
	// open the file
	QFile file(fileName);
	if (!file.open(QFile::ReadOnly))
		return QByteArray();

	// process the file
	QString errorMessage;
	QByteArray result = buildInfoDatabase(file, errorMessage);
	return errorMessage.isEmpty()
		? result
		: QByteArray();
}


//-------------------------------------------------
//  test
//-------------------------------------------------

void Test::general()
{
	QByteArray byteArray = buildInfoDatabase();
	QVERIFY(byteArray.size() > 0);
}


static TestFixture<Test> fixture;
#include "info_builder_test.moc"
