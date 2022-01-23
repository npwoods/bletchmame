/***************************************************************************

	audittask_test.cpp

	Unit tests for audittask.cpp

***************************************************************************/

#include "audittask.h"
#include "test.h"


// ======================> AuditTask::Test

class AuditTask::Test : public QObject
{
	Q_OBJECT

private slots:
	void generalWithAssets()		{ general(true, AuditStatus::Found); }
	void generalWithoutAssets()		{ general(false, AuditStatus::Missing); }
	void auditIdentifierHash();
	void auditIdentifierEqualTo();

private:
	void general(bool hasMedia, AuditStatus expectedResult);
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void AuditTask::Test::general(bool hasMedia, AuditStatus expectedResult)
{
	// create a temporary directory
	QTemporaryDir tempDir;
	QVERIFY(tempDir.isValid());
	QString tempDirPath = tempDir.path();

	// create a directory structure under that directory
	QString romDir = QDir(tempDirPath).filePath("./rom");
	QString sampleDir = QDir(tempDirPath).filePath("./sample");

	// and set up media (if we have it)
	if (hasMedia)
	{
		QDir().mkdir(romDir);
		QDir().mkdir(romDir + "/fake");
		QDir().mkdir(sampleDir);
		QDir().mkdir(sampleDir + "/fake");
		QFile::copy(":/resources/garbage.bin", romDir + "/fake/garbage.bin");
		QFile::copy(":/resources/garbage.bin", romDir + "/fake/nodump.bin");
		QFile::copy(":/resources/garbage.bin", sampleDir + "/fake/fakesample.wav");
		QFile::copy(":/resources/samplechd.chd", romDir + "/fake/samplechd.chd");
	}

	// set up preferences pointing at the fake directory
	Preferences prefs;
	prefs.setGlobalPath(Preferences::global_path_type::ROMS, romDir);
	prefs.setGlobalPath(Preferences::global_path_type::SAMPLES, sampleDir);

	// set up an info DB
	info::database db;
	QVERIFY(db.load(buildInfoDatabase(":/resources/listxml_fake.xml", false)));
	info::machine machine = db.find_machine("fake").value();

	// prepare a task
	AuditTask task(true, 123);
	task.addMachineAudit(prefs, machine);

	// process it!
	std::vector<std::unique_ptr<QEvent>> events;
	auto callback = [&events](std::unique_ptr<QEvent> &&event)
	{
		// filter out the intermediate progess events; they are not deterministic
		AuditProgressEvent *auditProgressEvent = dynamic_cast<AuditProgressEvent *>(event.get());
		if (!auditProgressEvent || auditProgressEvent->verdict().has_value())
			events.push_back(std::move(event));
	};
	task.prepare(prefs, callback);
	task.run();

	// validate the audit progress events
	QVERIFY(events.size() == 5);
	AuditProgressEvent *auditProgressEvents[4] =
	{
		dynamic_cast<AuditProgressEvent *>(events[0].get()),
		dynamic_cast<AuditProgressEvent *>(events[1].get()),
		dynamic_cast<AuditProgressEvent *>(events[2].get()),
		dynamic_cast<AuditProgressEvent *>(events[3].get())
	};
	for (auto i = 0; i < std::size(auditProgressEvents); i++)
	{
		QVERIFY(auditProgressEvents[i]);
		QVERIFY(auditProgressEvents[i]->entryIndex() == (int)i);
		QVERIFY(auditProgressEvents[i]->verdict().has_value());
	}
	QVERIFY(auditProgressEvents[0]->verdict()->type() == (hasMedia ? Audit::Verdict::Type::Ok			: Audit::Verdict::Type::NotFound));
	QVERIFY(auditProgressEvents[1]->verdict()->type() == (hasMedia ? Audit::Verdict::Type::OkNoGoodDump	: Audit::Verdict::Type::NotFound));
	QVERIFY(auditProgressEvents[2]->verdict()->type() == (hasMedia ? Audit::Verdict::Type::Ok			: Audit::Verdict::Type::NotFound));
	QVERIFY(auditProgressEvents[3]->verdict()->type() == (hasMedia ? Audit::Verdict::Type::Ok			: Audit::Verdict::Type::NotFound));

	// validate the result event
	AuditResultEvent *auditResultEvent = dynamic_cast<AuditResultEvent *>(events[4].get());
	QVERIFY(auditResultEvent);
	QVERIFY(auditResultEvent->cookie() == 123);
	QVERIFY(auditResultEvent->results().size() == 1);
	QVERIFY(auditResultEvent->results()[0].status() == expectedResult);
}


//-------------------------------------------------
//  auditIdentifierHash
//-------------------------------------------------

void AuditTask::Test::auditIdentifierHash()
{
	AuditIdentifier ident1 = MachineAuditIdentifier("blah");
	AuditIdentifier ident2 = MachineAuditIdentifier("blah");
	QVERIFY(std::hash<AuditIdentifier>()(ident1) == std::hash<AuditIdentifier>()(ident2));

	AuditIdentifier ident3 = SoftwareAuditIdentifier("alpha", "bravo");
	AuditIdentifier ident4 = SoftwareAuditIdentifier("alpha", "bravo");
	QVERIFY(std::hash<AuditIdentifier>()(ident3) == std::hash<AuditIdentifier>()(ident4));
}


//-------------------------------------------------
//  auditIdentifierEqualTo
//-------------------------------------------------

void AuditTask::Test::auditIdentifierEqualTo()
{
	QVERIFY(std::equal_to<AuditIdentifier>()(
		MachineAuditIdentifier("alpha"),
		MachineAuditIdentifier("alpha")));

	QVERIFY(!std::equal_to<AuditIdentifier>()(
		MachineAuditIdentifier("alpha"),
		MachineAuditIdentifier("bravo")));

	QVERIFY(std::equal_to<AuditIdentifier>()(
		SoftwareAuditIdentifier("alpha", "bravo"),
		SoftwareAuditIdentifier("alpha", "bravo")));

	QVERIFY(!std::equal_to<AuditIdentifier>()(
		SoftwareAuditIdentifier("alpha", "bravo"),
		SoftwareAuditIdentifier("charlie", "delta")));

	QVERIFY(!std::equal_to<AuditIdentifier>()(
		SoftwareAuditIdentifier("alpha", "bravo"),
		MachineAuditIdentifier("charlie")));
}


//-------------------------------------------------

static TestFixture<AuditTask::Test> fixture;
#include "audittask_test.moc"
