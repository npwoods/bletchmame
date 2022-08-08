/***************************************************************************

    machinelistitemmodel_test.cpp

    Unit tests for machinelistitemmodel.cpp

***************************************************************************/

// bletchmame headers
#include "machinelistitemmodel.h"
#include "test.h"

// Qt headers
#include <QBuffer>


namespace
{
	class Test : public QObject
	{
		Q_OBJECT

	private slots:
		void general();
		void auditStatusChanged();
		void allAuditStatusesChanged();
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

	// ensure that we can handle this even if we are empty
	model.allAuditStatusesChanged();

	// load an info DB
	{
		QByteArray byteArray = buildInfoDatabase(":/resources/listxml_coco.xml");
		QBuffer buffer(&byteArray);
		QVERIFY(buffer.open(QIODevice::ReadOnly));
		QVERIFY(db.load(buffer));
	}

	// verify that we got rows
	QVERIFY(model.rowCount(QModelIndex()) == 104);

	// try loading a different one
	{
		QByteArray byteArray = buildInfoDatabase(":/resources/listxml_alienar.xml");
		QBuffer buffer(&byteArray);
		QVERIFY(buffer.open(QIODevice::ReadOnly));
		QVERIFY(db.load(buffer));
	}

	// verify that we got rows
	QVERIFY(model.rowCount(QModelIndex()) == 13);
}


//-------------------------------------------------
//  auditStatusChanged
//-------------------------------------------------

void Test::auditStatusChanged()
{
	// create a MachineListItemModel
	info::database db;
	MachineListItemModel model(nullptr, db, nullptr, { });
	QByteArray byteArray = buildInfoDatabase(":/resources/listxml_coco.xml");
	QBuffer buffer(&byteArray);
	QVERIFY(buffer.open(QIODevice::ReadOnly));
	QVERIFY(db.load(buffer));

	// listen to dataChanged signal
	std::optional<QModelIndex> topLeft;
	std::optional<QModelIndex> bottomRight;
	connect(&model, &QAbstractItemModel::dataChanged, &model, [&](const QModelIndex &topLeftParam, const QModelIndex &bottomRightParam, const QVector<int> &roles)
	{
		if (roles.size() != 1 || roles[0] != Qt::DecorationRole)
			throw false;
		topLeft = topLeftParam;
		bottomRight = bottomRightParam;
	});

	// invoke with a bogus item; nothing should happen
	model.auditStatusChanged(MachineIdentifier("BOGUS_MACHINE"));
	QVERIFY(!topLeft);
	QVERIFY(!bottomRight);

	// now try a real machine
	model.auditStatusChanged(MachineIdentifier("coco"));
	QVERIFY(topLeft == model.index(9, 0));
	QVERIFY(bottomRight == model.index(9, 0));

	// now try another real machine
	model.auditStatusChanged(MachineIdentifier("coco2b"));
	QVERIFY(topLeft == model.index(12, 0));
	QVERIFY(bottomRight == model.index(12, 0));
}


//-------------------------------------------------
//  allAuditStatusesChanged
//-------------------------------------------------

void Test::allAuditStatusesChanged()
{
	// create a MachineListItemModel
	info::database db;
	MachineListItemModel model(nullptr, db, nullptr, { });
	QByteArray byteArray = buildInfoDatabase(":/resources/listxml_coco.xml");
	QBuffer buffer(&byteArray);
	QVERIFY(buffer.open(QIODevice::ReadOnly));
	QVERIFY(db.load(buffer));

	// listen to dataChanged signal
	std::optional<QModelIndex> topLeft;
	std::optional<QModelIndex> bottomRight;
	std::optional<QVector<int>> roles;
	connect(&model, &QAbstractItemModel::dataChanged, &model, [&](const QModelIndex &topLeftParam, const QModelIndex &bottomRightParam, const QVector<int> &rolesParam)
	{
		topLeft = topLeftParam;
		bottomRight = bottomRightParam;
		roles = rolesParam;
	});

	// invoke allAuditStatusesChanged(); everything should change
	model.allAuditStatusesChanged();
	QVERIFY(topLeft);
	QVERIFY(topLeft->row() == 0);
	QVERIFY(bottomRight);
	QVERIFY(bottomRight->row() == db.machines().size() - 1);
	QVERIFY(roles);
	QVERIFY(roles->size() == 1);
	QVERIFY((*roles)[0] == Qt::DecorationRole);
}


//-------------------------------------------------

static TestFixture<Test> fixture;
#include "machinelistitemmodel_test.moc"
