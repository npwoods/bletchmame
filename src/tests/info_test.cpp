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
        void general();
		void loadGarbage_0_0()			{ loadGarbage(0, 0); }
		void loadGarbage_0_1000()		{ loadGarbage(0, 1000); }
		void loadGarbage_1000_0()		{ loadGarbage(1000, 0); }
		void loadGarbage_1000_1000()	{ loadGarbage(1000, 1000); }

	private:
		void loadGarbage(int legitBytes, int garbageBytes);
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void Test::general()
{
	// set up a buffer
	QByteArray byteArray = buildInfoDatabase();
	QVERIFY(byteArray.size() > 0);
	QBuffer buffer(&byteArray);
	QVERIFY(buffer.open(QIODevice::ReadOnly));

	// and process it, validating we've done so successfully
	info::database db;
	bool dbChanged = false;
	db.setOnChanged([&dbChanged]() { dbChanged = true; });
	QVERIFY(db.load(buffer));
	QVERIFY(dbChanged);

	// verify that we're at the end of the buffer
	QVERIFY(buffer.pos() == buffer.size());

	// spelunk through the resulting db
	int settingCount = 0, softwareListCount = 0, ramOptionCount = 0;
	for (info::machine machine : db.machines())
	{
		// basic machine properties
		const QString &name = machine.name();
		const QString &description = machine.description();
		QVERIFY(!name.isEmpty());
		QVERIFY(!description.isEmpty());

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
	QVERIFY(settingCount > 0);
	QVERIFY(softwareListCount > 0);
	QVERIFY(ramOptionCount > 0);
}


//-------------------------------------------------
//  loadGarbage - tests to gauge our ability to
//	load garbage and gracefully error
//-------------------------------------------------

void Test::loadGarbage(int legitBytes, int garbageBytes)
{
	static char garbage[] = { 'D', 'E', 'A', 'D', 'B', 'E', 'E', 'F' };

	QByteArray byteArray = buildInfoDatabase();
	QVERIFY(byteArray.size() > 0);
	byteArray.resize(legitBytes + garbageBytes);

	// fill in garbage bytes
	for (int i = 0; i < garbageBytes; i++)
		byteArray[legitBytes + i] = garbage[i % sizeof(garbage)];

	// open the buffer
	QBuffer buffer(&byteArray);
	QVERIFY(buffer.open(QIODevice::ReadOnly));

	// try to load it
	info::database db;
	bool dbChanged = false;
	db.setOnChanged([&dbChanged]() { dbChanged = true; });
	QVERIFY(!db.load(buffer));
	QVERIFY(!dbChanged);
}


static TestFixture<Test> fixture;
#include "info_test.moc"
