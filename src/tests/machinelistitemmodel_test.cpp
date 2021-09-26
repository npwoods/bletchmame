/***************************************************************************

    machinelistitemmodel_test.cpp

    Unit tests for machinelistitemmodel.cpp

***************************************************************************/

#include <QBuffer>

#include "machinelistitemmodel.h"
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
//  general
//-------------------------------------------------

void Test::general()
{
	// create a MachineListItemModel
	info::database db;
	MachineListItemModel model(nullptr, db, nullptr, { });
	QVERIFY(model.rowCount(QModelIndex()) == 0);

	// load an info DB
	{
		QByteArray byteArray = buildInfoDatabase(":/resources/listxml_coco.xml");
		QBuffer buffer(&byteArray);
		QVERIFY(buffer.open(QIODevice::ReadOnly));
		QVERIFY(db.load(buffer));
	}

	// verify that we got rows
	QVERIFY(model.rowCount(QModelIndex()) == 15);

	// try loading a different one
	{
		QByteArray byteArray = buildInfoDatabase(":/resources/listxml_alienar.xml");
		QBuffer buffer(&byteArray);
		QVERIFY(buffer.open(QIODevice::ReadOnly));
		QVERIFY(db.load(buffer));
	}

	// verify that we got rows
	QVERIFY(model.rowCount(QModelIndex()) == 1);
}


static TestFixture<Test> fixture;
#include "machinelistitemmodel_test.moc"
