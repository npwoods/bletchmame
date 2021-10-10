/***************************************************************************

	audittask_test.cpp

	Unit tests for audittask.cpp

***************************************************************************/

#include "audittask.h"
#include "test.h"


namespace
{
	class Test : public QObject
	{
		Q_OBJECT

	private slots:
		void auditIdentifierHash();
		void auditIdentifierEqualTo();
	};
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  auditIdentifierHash
//-------------------------------------------------

void Test::auditIdentifierHash()
{
	AuditIdentifier ident1 = MachineAuditIdentifier("blah");
	AuditIdentifier ident2 = MachineAuditIdentifier("blah");
	QVERIFY(std::hash<AuditIdentifier>()(ident1) == std::hash<AuditIdentifier>()(ident2));
}


//-------------------------------------------------
//  auditIdentifierEqualTo
//-------------------------------------------------

void Test::auditIdentifierEqualTo()
{
	QVERIFY(std::equal_to<AuditIdentifier>()(
		MachineAuditIdentifier("alpha"),
		MachineAuditIdentifier("alpha")));

	QVERIFY(!std::equal_to<AuditIdentifier>()(
		MachineAuditIdentifier("alpha"),
		MachineAuditIdentifier("bravo")));
}


static TestFixture<Test> fixture;
#include "audittask_test.moc"
