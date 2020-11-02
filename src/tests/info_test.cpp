/***************************************************************************

    info_test.cpp

    Unit tests for info.cpp

***************************************************************************/

#include <QBuffer>

#include "info.h"
#include "test.h"

namespace
{
    class Test : public QObject
    {
        Q_OBJECT

    private slots:
        void general_coco_dtd()			{ general(":/resources/listxml_coco.xml", false, 15, 796, 32, 48); }
		void general_coco_noDtd()		{ general(":/resources/listxml_coco.xml", true, 15, 796, 32, 48); }
        void general_alienar_dtd()		{ general(":/resources/listxml_alienar.xml", false, 1, 0, 0, 0); }
        void general_alienar_noDtd()	{ general(":/resources/listxml_alienar.xml", true, 1, 0, 0, 0); }
		void machineLookup_coco()		{ machineLookup(":/resources/listxml_coco.xml"); }
		void machineLookup_alienar()	{ machineLookup(":/resources/listxml_alienar.xml"); }
		void viewIterators();
		void loadGarbage_0_0()			{ loadGarbage(0, 0); }
		void loadGarbage_0_1000()		{ loadGarbage(0, 1000); }
		void loadGarbage_1000_0()		{ loadGarbage(1000, 0); }
		void loadGarbage_1000_1000()	{ loadGarbage(1000, 1000); }
		void loadFailuresDontMutate();
		void readsAllBytes();

	private:
		void general(const QString &fileName, bool skipDtd, int expectedMachineCount, int expectedSettingCount, int expectedSoftwareListCount, int expectedRamOptionCount);
		void machineLookup(const QString &filename);
		void loadGarbage(int legitBytes, int garbageBytes);
		static void garbagifyByteArray(QByteArray &byteArray, int garbageStart, int garbageCount);
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void Test::general(const QString &fileName, bool skipDtd, int expectedMachineCount, int expectedSettingCount, int expectedSoftwareListCount, int expectedRamOptionCount)
{
	// read the db, validating we've done so successfully
	info::database db;
	bool dbChanged = false;
	db.setOnChanged([&dbChanged]() { dbChanged = true; });
	QVERIFY(db.load(buildInfoDatabase(fileName, skipDtd)));
	QVERIFY(dbChanged);

	// spelunk through the resulting db
	int machineCount = 0, settingCount = 0, softwareListCount = 0, ramOptionCount = 0;
	for (info::machine machine : db.machines())
	{
		// basic machine properties
		const QString &name = machine.name();
		const QString &description = machine.description();
		QVERIFY(!name.isEmpty());
		QVERIFY(!description.isEmpty());

		// count machines
		machineCount++;

		for (info::device dev : machine.devices())
		{
			const QString &type = dev.type();
			const QString &tag = dev.tag();
			const QString &instance_name = dev.instance_name();
			const QString &extensions = dev.extensions();
			QVERIFY(!type.isEmpty());
			QVERIFY(!tag.isEmpty());
			QVERIFY(!instance_name.isEmpty());
			QVERIFY(!extensions.isEmpty());
		}

		for (info::configuration cfg : machine.configurations())
		{
			for (info::configuration_setting setting : cfg.settings())
				settingCount++;
		}

		for (info::software_list swlist : machine.software_lists())
			softwareListCount++;

		for (info::ram_option ramopt : machine.ram_options())
			ramOptionCount++;
	}
	QVERIFY(machineCount == db.machines().size());
	QVERIFY(machineCount == expectedMachineCount);
	QVERIFY(settingCount == expectedSettingCount);
	QVERIFY(softwareListCount == expectedSoftwareListCount);
	QVERIFY(ramOptionCount == expectedRamOptionCount);
}


//-------------------------------------------------
//	machineLookup - test find_machine()'s ability
//	to find all machines
//-------------------------------------------------

void Test::machineLookup(const QString &fileName)
{
	// and process it, validating we've done so successfully
	info::database db;
	QVERIFY(db.load(buildInfoDatabase(fileName)));
	QVERIFY(db.machines().size() > 0);

	// for all machines...
	for (info::machine machine : db.machines())
	{
		// ...look it up
		std::optional<info::machine> foundMachine = db.find_machine(machine.name());

		// and check that we have the same results
		QVERIFY(foundMachine.has_value());
		QVERIFY(foundMachine->name() == machine.name());
		QVERIFY(foundMachine->manufacturer() == machine.manufacturer());
		QVERIFY(foundMachine->year() == machine.year());
		QVERIFY(foundMachine->description() == machine.description());
		QVERIFY(foundMachine->devices().size() == machine.devices().size());
	}
}


//-------------------------------------------------
//  viewIterators
//-------------------------------------------------

void Test::viewIterators()
{
	// load the db, validating we've done so successfully
	info::database db;
	QVERIFY(db.load(buildInfoDatabase()));
	QVERIFY(db.machines().size() >= 7);

	// perform a number of operations
	auto iter1 = db.machines().begin();
	auto iter2 = iter1++;
	auto iter3 = ++iter1;
	iter1 += 5;

	// and validate that the iterators are what we expect
	QVERIFY(iter1 - db.machines().begin() == 7);
	QVERIFY(iter2 - db.machines().begin() == 0);
	QVERIFY(iter3 - db.machines().begin() == 2);

	// and ensure that dereferencing works consistently
	QVERIFY(iter1->name() == db.machines()[7].name());
	QVERIFY((*iter1).name() == db.machines()[7].name());
	QVERIFY(iter2->name() == db.machines()[0].name());
	QVERIFY((*iter2).name() == db.machines()[0].name());
	QVERIFY(iter3->name() == db.machines()[2].name());
	QVERIFY((*iter3).name() == db.machines()[2].name());
}


//-------------------------------------------------
//  loadGarbage - tests to gauge our ability to
//	load garbage and gracefully error
//-------------------------------------------------

void Test::loadGarbage(int legitBytes, int garbageBytes)
{
	QByteArray byteArray = buildInfoDatabase();
	garbagifyByteArray(byteArray, legitBytes, garbageBytes);

	// try to load it
	info::database db;
	bool dbChanged = false;
	db.setOnChanged([&dbChanged]() { dbChanged = true; });
	QVERIFY(!db.load(byteArray));
	QVERIFY(!dbChanged);
}


//-------------------------------------------------
//  loadFailuresDontMutate - if we have a DB loaded
//	and we try to load another one, the existing DB
//	needs to be kept intact
//-------------------------------------------------

void Test::loadFailuresDontMutate()
{
	// set up a buffer
	QByteArray goodByteArray = buildInfoDatabase(":/resources/listxml_coco.xml");
	QVERIFY(goodByteArray.size() > 0);

	// and process it, validating we've done so successfully
	info::database db;
	int dbChangedCount = 0;
	db.setOnChanged([&dbChangedCount]() { dbChangedCount++; });
	QVERIFY(db.load(goodByteArray));
	QVERIFY(dbChangedCount == 1);

	// get the machine count
	size_t machineCount = db.machines().size();
	QVERIFY(machineCount > 0);

	// now create a bad buffer
	QByteArray badByteArray = buildInfoDatabase(":/resources/listxml_alienar.xml");
	QVERIFY(badByteArray.size() > 0);
	garbagifyByteArray(badByteArray, 1000, 1000);

	// process it - we expect this to fail
	QVERIFY(!db.load(badByteArray));

	// we also expect the telltale signs of mutation to not apply
	QVERIFY(dbChangedCount == 1);
	QVERIFY(db.machines().size() == machineCount);
}


//-------------------------------------------------
//  garbagifyByteArray
//-------------------------------------------------

void Test::garbagifyByteArray(QByteArray &byteArray, int garbageStart, int garbageCount)
{
	// resize the array
	byteArray.resize(garbageStart + garbageCount);

	// fill in garbage bytes
	static char garbage[] = { 'D', 'E', 'A', 'D', 'B', 'E', 'E', 'F' };
	for (int i = 0; i < garbageCount; i++)
		byteArray[garbageStart + i] = garbage[i % sizeof(garbage)];
}


//-------------------------------------------------
//  readsAllBytes
//-------------------------------------------------

void Test::readsAllBytes()
{
	// set up a buffer
	QByteArray byteArray = buildInfoDatabase();
	QVERIFY(byteArray.size() > 0);
	QBuffer buffer(&byteArray);
	QVERIFY(buffer.open(QIODevice::ReadOnly));

	// read the db, validating we've done so successfully
	info::database db;
	bool dbChanged = false;
	db.setOnChanged([&dbChanged]() { dbChanged = true; });
	QVERIFY(db.load(buffer));
	QVERIFY(dbChanged);

	// verify that we're at the end of the buffer
	QVERIFY(buffer.pos() == buffer.size());
}


static TestFixture<Test> fixture;
#include "info_test.moc"
