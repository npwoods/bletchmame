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
	void test1();
	void test2();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  test1
//-------------------------------------------------

void AuditQueue::Test::test1()
{
	// dependencies
	Preferences prefs;
	info::database infoDb;
	software_list_collection softwareListCollection;

	// set up the audit queue
	QVERIFY(infoDb.load(buildInfoDatabase(":/resources/listxml_coco.xml")));
	AuditQueue auditQueue(prefs, infoDb, softwareListCollection);

	// push some stuff
	auditQueue.push(MachineAuditIdentifier("coco"), true);
	auditQueue.push(MachineAuditIdentifier("coco2"), true);
	auditQueue.push(MachineAuditIdentifier("coco2b"), true);
	auditQueue.push(MachineAuditIdentifier("coco3"), true);
	auditQueue.push(MachineAuditIdentifier("coco"), true);

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
//  test2
//-------------------------------------------------

void AuditQueue::Test::test2()
{
	// dependencies
	Preferences prefs;
	info::database infoDb;
	software_list_collection softwareListCollection;

	// set up the audit queue
	QVERIFY(infoDb.load(buildInfoDatabase(":/resources/listxml_coco.xml")));
	AuditQueue auditQueue(prefs, infoDb, softwareListCollection);

	// push some stuff
	auditQueue.push(MachineAuditIdentifier("coco"), false);
	auditQueue.push(MachineAuditIdentifier("coco2"), false);
	auditQueue.push(MachineAuditIdentifier("coco2b"), false);
	auditQueue.push(MachineAuditIdentifier("coco2"), false);

	// validate that the audit queue's collections are of the expected sizes
	QVERIFY(auditQueue.m_auditTaskMap.size() == 3);
	QVERIFY(auditQueue.m_undispatchedAudits.size() == 3);

	// validate the state of the queue
	auto iter = auditQueue.m_undispatchedAudits.cbegin();
	QVERIFY(*iter++ == AuditIdentifier(MachineAuditIdentifier("coco")));
	QVERIFY(*iter++ == AuditIdentifier(MachineAuditIdentifier("coco2")));
	QVERIFY(*iter++ == AuditIdentifier(MachineAuditIdentifier("coco2b")));
	QVERIFY(iter == auditQueue.m_undispatchedAudits.cend());

	// push some more stuff
	auditQueue.push(MachineAuditIdentifier("coco2"), false);
	auditQueue.push(MachineAuditIdentifier("coco3"), false);

	// validate that the audit queue's collections are of the expected sizes
	QVERIFY(auditQueue.m_auditTaskMap.size() == 4);
	QVERIFY(auditQueue.m_undispatchedAudits.size() == 4);

	// validate the state of the queue
	iter = auditQueue.m_undispatchedAudits.cbegin();
	QVERIFY(*iter++ == AuditIdentifier(MachineAuditIdentifier("coco")));
	QVERIFY(*iter++ == AuditIdentifier(MachineAuditIdentifier("coco2")));
	QVERIFY(*iter++ == AuditIdentifier(MachineAuditIdentifier("coco2b")));
	QVERIFY(*iter++ == AuditIdentifier(MachineAuditIdentifier("coco3")));
	QVERIFY(iter == auditQueue.m_undispatchedAudits.cend());
}


//-------------------------------------------------

static TestFixture<AuditQueue::Test> fixture;
#include "auditqueue_test.moc"
