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

	// validate that the audit queue's collections are of the expected sizes
	QVERIFY(auditQueue.m_auditTaskMap.size() == 4);
	QVERIFY(auditQueue.m_undispatchedAudits.size() == 4);

	// validate the state of the queue
	auto iter = auditQueue.m_undispatchedAudits.cbegin();
	QVERIFY(*iter++ == AuditIdentifier(MachineAuditIdentifier("coco")));
	QVERIFY(*iter++ == AuditIdentifier(MachineAuditIdentifier("coco3")));
	QVERIFY(*iter++ == AuditIdentifier(MachineAuditIdentifier("coco2b")));
	QVERIFY(*iter++ == AuditIdentifier(MachineAuditIdentifier("coco2")));
	QVERIFY(iter == auditQueue.m_undispatchedAudits.cend());

	// create a task
	AuditTask::ptr auditTask = auditQueue.tryCreateAuditTask();
	QVERIFY(auditTask);

	// validate that the audit task has what we expect
	auto auditTaskIdentifiers = auditTask->getIdentifiers();
	QVERIFY(auditTaskIdentifiers.size() == 3);
	QVERIFY(auditTaskIdentifiers[0] == AuditIdentifier(MachineAuditIdentifier("coco")));
	QVERIFY(auditTaskIdentifiers[1] == AuditIdentifier(MachineAuditIdentifier("coco3")));
	QVERIFY(auditTaskIdentifiers[2] == AuditIdentifier(MachineAuditIdentifier("coco2b")));

	// validate that the audit queue's collections are in the expected state
	QVERIFY(auditQueue.m_auditTaskMap.size() == 4);
	QVERIFY(auditQueue.m_undispatchedAudits.size() == 1);
	QVERIFY(auditQueue.m_undispatchedAudits.front() == AuditIdentifier(MachineAuditIdentifier("coco2")));

	// create another task
	AuditTask::ptr auditTask2 = auditQueue.tryCreateAuditTask();
	QVERIFY(auditTask2);
	QVERIFY(auditQueue.m_auditTaskMap.size() == 4);
	QVERIFY(auditQueue.m_undispatchedAudits.empty());
	QVERIFY(auditTask2->getIdentifiers().size() == 1);

	// try to create another task, because the queue is emptied out we should not get one
	AuditTask::ptr auditTask3 = auditQueue.tryCreateAuditTask();
	QVERIFY(!auditTask3);
}


//-------------------------------------------------

static TestFixture<AuditQueue::Test> fixture;
#include "auditqueue_test.moc"
