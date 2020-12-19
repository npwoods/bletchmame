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
        void general_coco_dtd()			{ general(":/resources/listxml_coco.xml", false, 88, 15, 1221, 32, 48, 97, 413); }
		void general_coco_noDtd()		{ general(":/resources/listxml_coco.xml", true, 88, 15, 1221, 32, 48, 97, 413); }
        void general_alienar_dtd()		{ general(":/resources/listxml_alienar.xml", false, 14, 1, 0, 0, 0, 0, 0); }
        void general_alienar_noDtd()	{ general(":/resources/listxml_alienar.xml", true, 14, 1, 0, 0, 0, 0, 0); }
		void machineLookup_coco()		{ machineLookup(":/resources/listxml_coco.xml"); }
		void machineLookup_alienar()	{ machineLookup(":/resources/listxml_alienar.xml"); }
		void deviceLookup_coco2b()		{ deviceLookup(":/resources/listxml_coco.xml", "coco2b"); }
		void deviceLookup_coco3()		{ deviceLookup(":/resources/listxml_coco.xml", "coco3"); }
		void viewIterators();
		void loadGarbage_0_0()			{ loadGarbage(0, 0); }
		void loadGarbage_0_1000()		{ loadGarbage(0, 1000); }
		void loadGarbage_1000_0()		{ loadGarbage(1000, 0); }
		void loadGarbage_1000_1000()	{ loadGarbage(1000, 1000); }
		void loadFailuresDontMutate();
		void readsAllBytes();
		void sortable();

	private:
		void general(const QString &fileName, bool skipDtd, int expectedMachineCount, int expectedRunnableMachineCount, int expectedSettingCount, int expectedSoftwareListCount,
			int expectedRamOptionCount, int expectedSlotCount, int expectedSlotOptionCount);
		void machineLookup(const QString &filename);
		void deviceLookup(const QString &fileName, const QString &machineName);
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

void Test::general(const QString &fileName, bool skipDtd, int expectedMachineCount, int expectedRunnableMachineCount, int expectedSettingCount, int expectedSoftwareListCount,
	int expectedRamOptionCount, int expectedSlotCount, int expectedSlotOptionCount)
{
	// read the db, validating we've done so successfully
	info::database db;
	bool dbChanged = false;
	db.setOnChanged([&dbChanged]() { dbChanged = true; });
	QVERIFY(db.load(buildInfoDatabase(fileName, skipDtd)));
	QVERIFY(dbChanged);

	// spelunk through the resulting db
	int machineCount = 0, runnableMachineCount = 0, settingCount = 0, softwareListCount = 0, ramOptionCount = 0;
	int slotCount = 0, slotOptionCount = 0;
	for (info::machine machine : db.machines())
	{
		// basic machine properties
		const QString &name = machine.name();
		const QString &description = machine.description();
		QVERIFY(!name.isEmpty());
		QVERIFY(!description.isEmpty());

		// count machines
		machineCount++;
		if (machine.runnable())
			runnableMachineCount++;

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

		for (info::slot slot : machine.devslots())
		{
			slotCount++;
			for (info::slot_option opt : slot.options())
				slotOptionCount++;
		}
	}
	QVERIFY(machineCount == db.machines().size());
	QVERIFY(machineCount == expectedMachineCount);
	QVERIFY(runnableMachineCount == expectedRunnableMachineCount);
	QVERIFY(settingCount == expectedSettingCount);
	QVERIFY(softwareListCount == expectedSoftwareListCount);
	QVERIFY(ramOptionCount == expectedRamOptionCount);
	QVERIFY(slotCount == expectedSlotCount);
	QVERIFY(slotOptionCount == expectedSlotOptionCount);
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
//	deviceLookup - test machine::find_device()'s
//	ability to find all machines
//-------------------------------------------------

void Test::deviceLookup(const QString &fileName, const QString &machineName)
{
	// and process it, validating we've done so successfully
	info::database db;
	QVERIFY(db.load(buildInfoDatabase(fileName)));

	// get the machine
	std::optional<info::machine> machine = db.find_machine(machineName);
	QVERIFY(machine.has_value());

	// for all devices...
	for (info::device device : machine.value().devices())
	{
		// ...look it up
		std::optional<info::device> foundDevice = machine->find_device(device.tag());

		// and check that we have the same results
		QVERIFY(foundDevice.has_value());
		QVERIFY(foundDevice->tag() == device.tag());
		QVERIFY(foundDevice->type() == device.type());
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
	auto iter4 = iter1;
	auto iter5 = iter1;
	auto iter6 = --iter5;
	auto iter7 = iter5--;

	// and validate that the iterators are what we expect
	QVERIFY(iter1 - db.machines().begin() == 7);
	QVERIFY(iter2 - db.machines().begin() == 0);
	QVERIFY(iter3 - db.machines().begin() == 2);
	QVERIFY(iter4 - db.machines().begin() == 7);
	QVERIFY(iter5 - db.machines().begin() == 5);
	QVERIFY(iter6 - db.machines().begin() == 6);
	QVERIFY(iter7 - db.machines().begin() == 6);

	// and ensure that dereferencing works consistently
	QVERIFY(iter1->name() == db.machines()[7].name());
	QVERIFY((*iter1).name() == db.machines()[7].name());
	QVERIFY(iter2->name() == db.machines()[0].name());
	QVERIFY((*iter2).name() == db.machines()[0].name());
	QVERIFY(iter3->name() == db.machines()[2].name());
	QVERIFY((*iter3).name() == db.machines()[2].name());

	// and the equals operator works correctly
	QVERIFY(!(iter1 == iter2));
	QVERIFY(!(iter1 == iter3));
	QVERIFY(!(iter2 == iter3));
	QVERIFY(iter1 == iter4);
	QVERIFY(!(iter2 == iter4));
	QVERIFY(!(iter3 == iter4));

	// and the not-equals operator works correctly
	QVERIFY(iter1 != iter2);
	QVERIFY(iter1 != iter3);
	QVERIFY(iter2 != iter3);
	QVERIFY(!(iter1 != iter4));
	QVERIFY(iter2 != iter4);
	QVERIFY(iter3 != iter4);
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


//-------------------------------------------------
//  sortable - not really about sorting but rather
//	ensuring that the info/bindata copy/move/assignment
//	capabilities are up to snuff
//-------------------------------------------------

void Test::sortable()
{
	info::database db;
	QVERIFY(db.load(buildInfoDatabase()));
	QVERIFY(db.machines().size() > 0);

	// put "all the things" into a vector
	std::vector<std::optional<info::machine>> vec;
	vec.push_back(std::nullopt);
	for (info::machine machine : db.machines())
		vec.push_back(machine);
	vec.push_back(std::nullopt);

	// and then sort it, first with the empty items and then in reverse order
	std::sort(
		vec.begin(),
		vec.end(),
		[](const std::optional<info::machine> &a, const std::optional<info::machine> &b)
		{
			return a.has_value() && b.has_value()
				? a->name() > b->name()
				: b.has_value();
		});

	// validate the order
	QVERIFY(!vec[0].has_value());
	QVERIFY(!vec[1].has_value());
	for (size_t i = 2; i < vec.size(); i++)
	{
		QVERIFY(vec[i].has_value());
		if (i > 2)
			QVERIFY(vec[i - 1]->name() > vec[i]->name());
	}
}


static TestFixture<Test> fixture;
#include "info_test.moc"
