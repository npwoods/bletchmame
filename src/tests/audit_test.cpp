/***************************************************************************

	audit_test.cpp

	Unit tests for audit.cpp

***************************************************************************/

#include "audit.h"
#include "test.h"

// ======================> AuditTask::Test

class Audit::Test : public QObject
{
	Q_OBJECT

private slots:
	void general_1()				{ general(false, false, false, false, AuditStatus::Missing); }
	void general_2()				{ general(false, false, false, true,  AuditStatus::Missing); }
	void general_3()				{ general(false, false, true,  false, AuditStatus::Missing); }
	void general_4()				{ general(false, true,  false, false, AuditStatus::Missing); }
	void general_5()				{ general(true,  false, false, false, AuditStatus::Missing); }
	void general_6()				{ general(true,  true,  true,  true,  AuditStatus::Found); }
	void addMediaForMachine();

private:
	void general(bool hasRom, bool hasNoDumpRom, bool hasSample, bool hasDisk, AuditStatus expectedResult);
};


// ======================> MockAuditCallback

namespace
{
	class MockAuditCallback : public Audit::ICallback
	{
	public:
		MockAuditCallback(std::vector<Audit::Verdict> &verdicts)
			: m_verdicts(verdicts)
		{
		}

		virtual bool reportProgress(int entryIndex, std::uint64_t bytesProcessed, std::uint64_t total) override
		{
			// ignore
			return false;
		}

		virtual void reportVerdict(int entryIndex, const Audit::Verdict &verdict) override
		{
			m_verdicts.push_back(verdict);
		}

	private:
		std::vector<Audit::Verdict> &m_verdicts;
	};
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void Audit::Test::general(bool hasRom, bool hasNoDumpRom, bool hasSample, bool hasDisk, AuditStatus expectedResult)
{
	// create a temporary directory
	QTemporaryDir tempDir;
	QVERIFY(tempDir.isValid());
	QString tempDirPath = tempDir.path();

	// create a directory structure under that directory
	QString romDir = QDir(tempDirPath).filePath("./rom");
	QString sampleDir = QDir(tempDirPath).filePath("./sample");
	QDir().mkdir(romDir);
	QDir().mkdir(romDir + "/fake");
	QDir().mkdir(sampleDir);
	QDir().mkdir(sampleDir + "/fake");

	// and set up media
	if (hasRom)
		QFile::copy(":/resources/garbage.bin", romDir + "/fake/garbage.bin");
	if (hasNoDumpRom)
		QFile::copy(":/resources/garbage.bin", romDir + "/fake/nodump.bin");
	if (hasSample)
		QFile::copy(":/resources/garbage.bin", sampleDir + "/fake/fakesample.wav");
	if (hasDisk)
		QFile::copy(":/resources/samplechd.chd", romDir + "/fake/samplechd.chd");

	// set up preferences pointing at the fake directory
	Preferences prefs;
	prefs.setGlobalPath(Preferences::global_path_type::ROMS, romDir);
	prefs.setGlobalPath(Preferences::global_path_type::SAMPLES, sampleDir);

	// set up an info DB
	info::database db;
	QVERIFY(db.load(buildInfoDatabase(":/resources/listxml_fake.xml", false)));
	std::optional<info::machine> machine = db.find_machine("fake");
	QVERIFY(machine.has_value());

	// prepare an audit
	Audit audit;
	audit.addMediaForMachine(prefs, machine.value());

	// and run the audit!
	std::vector<Audit::Verdict> verdicts;
	MockAuditCallback callback(verdicts);
	std::optional<AuditStatus> result = audit.run(callback);

	// validate the verdicts
	QVERIFY(verdicts.size() == 4);
	QVERIFY(verdicts[0].type() == (hasRom		? Audit::Verdict::Type::Ok :			Audit::Verdict::Type::NotFound));
	QVERIFY(verdicts[1].type() == (hasNoDumpRom	? Audit::Verdict::Type::OkNoGoodDump :	Audit::Verdict::Type::NotFound));
	QVERIFY(verdicts[2].type() == (hasDisk		? Audit::Verdict::Type::Ok :			Audit::Verdict::Type::NotFound));
	QVERIFY(verdicts[3].type() == (hasSample	? Audit::Verdict::Type::Ok :			Audit::Verdict::Type::NotFound));

	// validate the final status
	QVERIFY(result == expectedResult);
}


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
	QVERIFY(audit.entries().size() == 14);
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
	QVERIFY(audit.entries()[12].type() == Audit::Entry::Type::Disk);
	QVERIFY(audit.entries()[12].name() == "fakedisk.chd");
	QVERIFY(audit.entries()[13].type() == Audit::Entry::Type::Sample);
	QVERIFY(audit.entries()[13].name() == "fakesample.wav");
}


//-------------------------------------------------

static TestFixture<Audit::Test> fixture;
#include "audit_test.moc"
