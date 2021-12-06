/***************************************************************************

	auditcursor_test.cpp

	Unit tests for auditcursor.cpp

***************************************************************************/

#include <QBuffer>

#include "auditcursor.h"
#include "machinelistitemmodel.h"
#include "test.h"

namespace
{
	class Test : public QObject
	{
		Q_OBJECT

	private slots:
		void general();
		void filterChange1();
		void filterChange2();

	private:
		static void createInfoDb(info::database &db);
		static void validateAuditIdentifier(const std::optional<AuditIdentifier> &identifier, const char *expected);
	};
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  createInfoDb
//-------------------------------------------------

void Test::createInfoDb(info::database &db)
{
	QByteArray byteArray = buildInfoDatabase(":/resources/listxml_coco.xml");
	QBuffer buffer(&byteArray);
	QVERIFY(buffer.open(QIODevice::ReadOnly));
	QVERIFY(db.load(buffer));
}


//-------------------------------------------------
//  validateAuditIdentifier
//-------------------------------------------------

void Test::validateAuditIdentifier(const std::optional<AuditIdentifier> &identifier, const char *expected)
{
	QVERIFY(identifier.has_value());
	const MachineAuditIdentifier *machineAuditIdentifier = std::get_if<MachineAuditIdentifier>(&identifier.value());
	QVERIFY(machineAuditIdentifier);
	QVERIFY(machineAuditIdentifier->machineName() == expected);
}


//-------------------------------------------------
//  general
//-------------------------------------------------

void Test::general()
{
	// create a MachineListItemModel
	info::database db;
	createInfoDb(db);
	MachineListItemModel model(nullptr, db, nullptr, { });
	model.setMachineFilter([](const info::machine &machine)
	{
		return machine.name() == "coco" || machine.name() == "coco2"
			|| machine.name() == "coco2b" || machine.name() == "coco3";
	});
	QVERIFY(model.rowCount(QModelIndex()) == 4);

	// set up preferences
	Preferences prefs;

	// test an AuditCursor
	{
		AuditCursor cursor(prefs);
		cursor.setListItemModel(&model);
		validateAuditIdentifier(cursor.next(0), "coco");
		validateAuditIdentifier(cursor.next(0), "coco2");
		validateAuditIdentifier(cursor.next(0), "coco2b");
		validateAuditIdentifier(cursor.next(0), "coco3");
		QVERIFY(!cursor.next(0).has_value());
	}

	// now repeat this process after marking some of these as audited
	prefs.setMachineAuditStatus("coco2", AuditStatus::Found);
	prefs.setMachineAuditStatus("coco2b", AuditStatus::Found);
	{
		AuditCursor cursor(prefs);
		cursor.setListItemModel(&model);
		validateAuditIdentifier(cursor.next(0), "coco");
		validateAuditIdentifier(cursor.next(0), "coco3");
		QVERIFY(!cursor.next(0).has_value());
	}
}


//-------------------------------------------------
//  filterChange1
//-------------------------------------------------

void Test::filterChange1()
{
	// create a MachineListItemModel
	info::database db;
	createInfoDb(db);
	MachineListItemModel model(nullptr, db, nullptr, { });
	model.setMachineFilter([](const info::machine &machine)
	{
		return machine.name() == "coco" || machine.name() == "coco2"
			|| machine.name() == "coco2b" || machine.name() == "coco3";
	});
	QVERIFY(model.rowCount(QModelIndex()) == 4);

	// start going through an audit cursor
	Preferences prefs;
	AuditCursor cursor(prefs);
	cursor.setListItemModel(&model);
	validateAuditIdentifier(cursor.next(0), "coco");
	validateAuditIdentifier(cursor.next(0), "coco2");

	// now change the filter
	model.setMachineFilter([](const info::machine &machine)
	{
		return false;
	});
	QVERIFY(model.rowCount(QModelIndex()) == 0);

	// and ensure the cursor is still functional
	QVERIFY(!cursor.next(0).has_value());
}


//-------------------------------------------------
//  filterChange2
//-------------------------------------------------

void Test::filterChange2()
{
	// create a MachineListItemModel
	info::database db;
	createInfoDb(db);
	MachineListItemModel model(nullptr, db, nullptr, { });
	model.setMachineFilter([](const info::machine &machine)
		{
			return machine.name() == "coco" || machine.name() == "coco2"
				|| machine.name() == "coco2b" || machine.name() == "coco3";
		});
	QVERIFY(model.rowCount(QModelIndex()) == 4);

	// start going through an audit cursor
	Preferences prefs;
	AuditCursor cursor(prefs);
	cursor.setListItemModel(&model);
	validateAuditIdentifier(cursor.next(0), "coco");
	validateAuditIdentifier(cursor.next(0), "coco2");

	// now change the filter
	model.setMachineFilter([](const info::machine &machine)
	{
		return machine.name() == "coco";
	});
	QVERIFY(model.rowCount(QModelIndex()) == 1);

	// and ensure the cursor is still functional
	validateAuditIdentifier(cursor.next(0), "coco");
	QVERIFY(!cursor.next(0).has_value());
}


//**************************************************************************

static TestFixture<Test> fixture;
#include "auditcursor_test.moc"
