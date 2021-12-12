/***************************************************************************

	audit_test.cpp

	Unit tests for audit.cpp

***************************************************************************/

#include "audit.h"
#include "test.h"

class Audit::Test : public QObject
{
	Q_OBJECT

private slots:
	void addMediaForMachine();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************


//-------------------------------------------------
//  addMediaForMachine
//-------------------------------------------------

void Audit::Test::addMediaForMachine()
{
	// get an info DB and a machine
	info::database db;
	QVERIFY(db.load(buildInfoDatabase(":/resources/listxml_alienar.xml")));
	info::machine machine = db.find_machine("alienar").value();

	// set up an audit
	Preferences prefs;
	Audit audit;
	audit.addMediaForMachine(prefs, machine);

	// validate the results
	QVERIFY(audit.entries().size() == 12);
	QVERIFY(audit.entries()[ 0].type() == Audit::Entry::Type::Rom);
	QVERIFY(audit.entries()[ 0].name() == "aarom10");
	QVERIFY(audit.entries()[ 1].type() == Audit::Entry::Type::Rom);
	QVERIFY(audit.entries()[ 1].name() == "aarom11");
	QVERIFY(audit.entries()[ 2].type() == Audit::Entry::Type::Rom);
	QVERIFY(audit.entries()[ 2].name() == "aarom12");
	QVERIFY(audit.entries()[ 3].type() == Audit::Entry::Type::Rom);
	QVERIFY(audit.entries()[ 3].name() == "aarom01");
	QVERIFY(audit.entries()[ 4].type() == Audit::Entry::Type::Rom);
	QVERIFY(audit.entries()[ 4].name() == "aarom02");
	QVERIFY(audit.entries()[ 5].type() == Audit::Entry::Type::Rom);
	QVERIFY(audit.entries()[ 5].name() == "aarom03");
	QVERIFY(audit.entries()[ 6].type() == Audit::Entry::Type::Rom);
	QVERIFY(audit.entries()[ 6].name() == "aarom04");
	QVERIFY(audit.entries()[ 7].type() == Audit::Entry::Type::Rom);
	QVERIFY(audit.entries()[ 7].name() == "aarom05");
	QVERIFY(audit.entries()[ 8].type() == Audit::Entry::Type::Rom);
	QVERIFY(audit.entries()[ 8].name() == "aarom06");
	QVERIFY(audit.entries()[ 9].type() == Audit::Entry::Type::Rom);
	QVERIFY(audit.entries()[ 9].name() == "aarom07");
	QVERIFY(audit.entries()[10].type() == Audit::Entry::Type::Rom);
	QVERIFY(audit.entries()[10].name() == "aarom08");
	QVERIFY(audit.entries()[11].type() == Audit::Entry::Type::Rom);
	QVERIFY(audit.entries()[11].name() == "aarom09");
}


//-------------------------------------------------

static TestFixture<Audit::Test> fixture;
#include "audit_test.moc"
