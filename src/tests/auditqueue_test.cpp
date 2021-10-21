/***************************************************************************

	auditqueue_test.cpp

	Unit tests for auditqueue.cpp

***************************************************************************/

#include "auditqueue.h"
#include "test.h"


class AuditQueue::Test : public QObject
{
	Q_OBJECT

private slots:
	void test();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  test
//-------------------------------------------------

void AuditQueue::Test::test()
{
	// dependencies
	Preferences prefs;
	info::database infoDb;
	software_list_collection softwareListCollection;

	// set up the audit queue
	QVERIFY(infoDb.load(buildInfoDatabase(":/resources/listxml_coco.xml")));
	AuditQueue auditQueue(prefs, infoDb, softwareListCollection);

	// push some stuff
	auditQueue.push(MachineAuditIdentifier("coco"));
	auditQueue.push(MachineAuditIdentifier("coco2"));
	auditQueue.push(MachineAuditIdentifier("coco2b"));
	auditQueue.push(MachineAuditIdentifier("coco3"));
	auditQueue.push(MachineAuditIdentifier("coco"));
}


//-------------------------------------------------

static TestFixture<AuditQueue::Test> fixture;
#include "auditqueue_test.moc"
