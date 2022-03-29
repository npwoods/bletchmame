/***************************************************************************

    info_test.cpp

    Unit tests for info.cpp

***************************************************************************/

// bletchmame headers
#include "info.h"
#include "test.h"

// Qt headers
#include <QBuffer>


namespace
{
    class Test : public QObject
    {
        Q_OBJECT

    private slots:
		void general_coco_dtd()				{ general(":/resources/listxml_coco.xml", false, 104, 15, 1501, 32, 48, 99, 488); }
		void general_coco_noDtd()			{ general(":/resources/listxml_coco.xml", true, 104, 15, 1501, 32, 48, 99, 488); }
		void general_alienar_dtd()			{ general(":/resources/listxml_alienar.xml", false, 13, 1, 0, 0, 0, 0, 0); }
		void general_alienar_noDtd()		{ general(":/resources/listxml_alienar.xml", true, 13, 1, 0, 0, 0, 0, 0); }
		void machineLookup_coco()			{ machineLookup(":/resources/listxml_coco.xml"); }
		void machineLookup_alienar()		{ machineLookup(":/resources/listxml_alienar.xml"); }
		void deviceLookup_coco2b()			{ deviceLookup(":/resources/listxml_coco.xml", "coco2b"); }
		void deviceLookup_coco3()			{ deviceLookup(":/resources/listxml_coco.xml", "coco3"); }
		void loadExpectedVersion_coco()		{ loadExpectedVersion(":/resources/listxml_coco.xml", "0.229 (mame0229)"); }
		void loadExpectedVersion_alienar()	{ loadExpectedVersion(":/resources/listxml_alienar.xml", "0.229 (mame0229)"); }
		void badMachineLookup();
		void viewIterators();
		void viewIndexOutOfRange();
		void viewIteratorDerefOutOfRange();
		void loadGarbage_0_0()				{ loadGarbage(0, 0); }
		void loadGarbage_0_1000()			{ loadGarbage(0, 1000); }
		void loadGarbage_1000_0()			{ loadGarbage(1000, 0); }
		void loadGarbage_1000_1000()		{ loadGarbage(1000, 1000); }
		void loadFailuresDontMutate();
		void readsAllBytes();
		void sortable();
		void localeSensitivity();
		void scrutinize_alienar();
		void scrutinize_coco();
		void scrutinize_coco2b();

	private:
		void general(const QString &fileName, bool skipDtd, int expectedMachineCount, int expectedRunnableMachineCount, int expectedSettingCount, int expectedSoftwareListCount,
			int expectedRamOptionCount, int expectedSlotCount, int expectedSlotOptionCount);
		void machineLookup(const QString &filename);
		void deviceLookup(const QString &fileName, const QString &machineName);
		void loadGarbage(int legitBytes, int garbageBytes);
		void loadExpectedVersion(const QString &fileName, const QString &expectedVersion);
		static void garbagifyByteArray(QByteArray &byteArray, int garbageStart, int garbageCount);
	};
}


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
	bool dbChangedAlt = false;
	db.addOnChangedHandler([&dbChanged]() { dbChanged = true; });
	db.addOnChangedHandler([&dbChangedAlt]() { dbChangedAlt = true; });
	QVERIFY(db.load(buildInfoDatabase(fileName, skipDtd)));
	QVERIFY(dbChanged);
	QVERIFY(dbChangedAlt);

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
			{
				(void)setting;
				settingCount++;
			}
		}

		for (info::software_list swlist : machine.software_lists())
		{
			(void)swlist;
			softwareListCount++;
		}

		for (info::ram_option ramopt : machine.ram_options())
		{
			(void)ramopt;
			ramOptionCount++;
		}

		for (info::slot slot : machine.devslots())
		{
			slotCount++;
			for (info::slot_option opt : slot.options())
			{
				(void)opt;
				slotOptionCount++;
			}
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
//	badMachineLookup
//-------------------------------------------------

void Test::badMachineLookup()
{
	info::database db;
	QVERIFY(db.load(buildInfoDatabase()));
	QVERIFY(db.machines().size() > 0);

	std::optional<info::machine> result = db.find_machine("this_is_an_invalid_machine");
	QVERIFY(!result.has_value());
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
	auto iter8 = decltype(iter1)();
	iter8 = iter1;

	// and validate that the iterators are what we expect
	QVERIFY(iter1 - db.machines().begin() == 7);
	QVERIFY(iter2 - db.machines().begin() == 0);
	QVERIFY(iter3 - db.machines().begin() == 2);
	QVERIFY(iter4 - db.machines().begin() == 7);
	QVERIFY(iter5 - db.machines().begin() == 5);
	QVERIFY(iter6 - db.machines().begin() == 6);
	QVERIFY(iter7 - db.machines().begin() == 6);
	QVERIFY(iter8 - db.machines().begin() == 7);

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
//  viewIndexOutOfRange
//-------------------------------------------------

void Test::viewIndexOutOfRange()
{
	// load the db, validating we've done so successfully
	info::database db;
	QVERIFY(db.load(buildInfoDatabase()));
	QVERIFY(db.machines().size() > 2);

	// acceptable indexes
	QVERIFY(!db.machines()[0].name().isEmpty());
	QVERIFY(!db.machines()[1].name().isEmpty());
	QVERIFY(!db.machines()[2].name().isEmpty());
	QVERIFY(!db.machines()[db.machines().size() - 2].name().isEmpty());
	QVERIFY(!db.machines()[db.machines().size() - 1].name().isEmpty());

	// these indexes are out of range
	size_t outOfRangeIndexes[] =
	{
		db.machines().size(),
		db.machines().size() + 100,
		0xDEADBEEF,
		0xFFFFFFFF,
		~((size_t)0)
	};
	for (size_t index : outOfRangeIndexes)
	{
		bool caughtOutOfRange = false;
		try
		{
			(void)db.machines()[index];
		}
		catch (const std::out_of_range &)
		{
			caughtOutOfRange = true;
		}
		QVERIFY(caughtOutOfRange);
	}
}


//-------------------------------------------------
//  viewIteratorDerefOutOfRange
//-------------------------------------------------

void Test::viewIteratorDerefOutOfRange()
{
	// there is probably a better way to get the type of a view iterator
	info::database db;
	QVERIFY(db.load(buildInfoDatabase()));
	auto originalIter = db.machines().begin();
	(void)originalIter;

	// ensure that a deference of a default instantiated iterator throws an exception
	decltype(originalIter) iter;
	bool caughtOutOfRange = false;
	try
	{
		(void) *iter;
	}
	catch (const std::out_of_range &)
	{
		caughtOutOfRange = true;
	}
	QVERIFY(caughtOutOfRange);
}


//-------------------------------------------------
//  localeSensitivity - checks to see if we have
//	problems due to sensitivity on the current locale
// 
//	this was bug #143
//-------------------------------------------------

void Test::localeSensitivity()
{
	// set the locale to a locale that does not use periods as decimal separators
	TestLocaleOverride override("it-IT");

	// try loading the info DB
	info::database db;
	bool dbChanged = false;
	db.addOnChangedHandler([&dbChanged]() { dbChanged = true; });
	QVERIFY(db.load(buildInfoDatabase()));
	QVERIFY(dbChanged);
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
	db.addOnChangedHandler([&dbChanged]() { dbChanged = true; });
	QVERIFY(!db.load(byteArray));
	QVERIFY(!dbChanged);
}


//-------------------------------------------------
//  loadExpectedVersion - tests that
//	info::database::load() with an expected version
//	functions properly
//-------------------------------------------------

void Test::loadExpectedVersion(const QString &fileName, const QString &expectedVersion)
{
	info::database db;
	QVERIFY(db.load(buildInfoDatabase(fileName), expectedVersion));
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
	db.addOnChangedHandler([&dbChangedCount]() { dbChangedCount++; });
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
	db.addOnChangedHandler([&dbChanged]() { dbChanged = true; });
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


//-------------------------------------------------
//  scrutinize_alienar
//-------------------------------------------------

void Test::scrutinize_alienar()
{
	info::database db;
	QVERIFY(db.load(buildInfoDatabase(":/resources/listxml_alienar.xml")));

	std::optional<info::machine> machine = db.find_machine("alienar");
	QVERIFY(machine.has_value());
	QVERIFY(machine->name() == "alienar");
	QVERIFY(machine->description() == "Alien Arena");
	QVERIFY(machine->manufacturer() == "Duncan Brown");
	QVERIFY(!machine->clone_of().has_value());
	QVERIFY(!machine->rom_of().has_value());
	QVERIFY(machine->sound_channels() == 1);

	QVERIFY(machine->roms().size() == 12);
	QVERIFY(machine->roms()[ 0].name() == "aarom10");
	QVERIFY(machine->roms()[ 1].name() == "aarom11");
	QVERIFY(machine->roms()[ 2].name() == "aarom12");
	QVERIFY(machine->roms()[ 3].name() == "aarom01");
	QVERIFY(machine->roms()[ 4].name() == "aarom02");
	QVERIFY(machine->roms()[ 5].name() == "aarom03");
	QVERIFY(machine->roms()[ 6].name() == "aarom04");
	QVERIFY(machine->roms()[ 7].name() == "aarom05");
	QVERIFY(machine->roms()[ 8].name() == "aarom06");
	QVERIFY(machine->roms()[ 9].name() == "aarom07");
	QVERIFY(machine->roms()[10].name() == "aarom08");
	QVERIFY(machine->roms()[11].name() == "aarom09");

	for (info::rom rom : machine->roms())
	{
		QVERIFY(rom.bios().isEmpty());
		QVERIFY(rom.merge().isEmpty());
	}
}


//-------------------------------------------------
//  scrutinize_coco
//-------------------------------------------------

void Test::scrutinize_coco()
{
	info::database db;
	QVERIFY(db.load(buildInfoDatabase(":/resources/listxml_coco.xml")));

	std::optional<info::machine> machine = db.find_machine("coco");
	QVERIFY(machine.has_value());
	QVERIFY(machine->name() == "coco");
	QVERIFY(machine->manufacturer() == "Tandy Radio Shack");
	QVERIFY(!machine->clone_of().has_value());
	QVERIFY(!machine->rom_of().has_value());
	QVERIFY(machine->sound_channels() == 1);
}


//-------------------------------------------------
//  scrutinize_coco2b
//-------------------------------------------------

void Test::scrutinize_coco2b()
{
	info::database db;
	QVERIFY(db.load(buildInfoDatabase(":/resources/listxml_coco.xml")));

	std::optional<info::machine> machine = db.find_machine("coco2b");
	QVERIFY(machine.has_value());
	QVERIFY(machine->name() == "coco2b");
	QVERIFY(machine->manufacturer() == "Tandy Radio Shack");
	QVERIFY(machine->clone_of().has_value());
	QVERIFY(machine->clone_of()->name() == "coco");
	QVERIFY(machine->rom_of().has_value());
	QVERIFY(machine->rom_of()->name() == "coco");
	QVERIFY(machine->sound_channels() == 3);
}


//-------------------------------------------------

static TestFixture<Test> fixture;
#include "info_test.moc"
