/***************************************************************************

    info_builder_test.cpp

    Unit tests for info_builder.cpp

***************************************************************************/

#include <QBuffer>

#include "info_builder.h"
#include "test.h"

namespace
{
    class Test : public QObject
    {
        Q_OBJECT

    private slots:
        void general();

	private:
		void readSampleListXml(QDataStream &output);
    };
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  readSampleListXml
//-------------------------------------------------

void Test::readSampleListXml(QDataStream &output)
{
	// get the test asset
	QFile testAsset(":/resources/listxml.xml");
	QVERIFY(testAsset.open(QFile::ReadOnly));
	QDataStream input(&testAsset);

	// process the sample -listxml output
	info::database_builder builder;
	QString error_message;
	bool success = builder.process_xml(input, error_message);
	QVERIFY(success && error_message.isEmpty());

	// and emit the results into the memory stream
	builder.emit_info(output);
}


//-------------------------------------------------
//  test
//-------------------------------------------------

void Test::general()
{
	// build the sample database
	QByteArray byteArray;
	{
		QBuffer buffer(&byteArray);
		buffer.open(QIODevice::WriteOnly);
		QDataStream bufferStream(&buffer);
		readSampleListXml(bufferStream);
	}
	QVERIFY(byteArray.size() > 0);

	// and process it, validating we've done so successfully
	QDataStream input(byteArray);

	info::database db;
	bool db_changed = false;
	db.set_on_changed([&db_changed]() { db_changed = true; });
	bool success = db.load(input);
	QVERIFY(success);
	QVERIFY(db_changed);

	// spelunk through the resulting db
	int setting_count = 0, software_list_count = 0, ram_option_count = 0;
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
				setting_count++;
		}

		for (info::software_list swlist : machine.software_lists())
			software_list_count++;

		for (info::ram_option ramopt : machine.ram_options())
			ram_option_count++;
	}
	QVERIFY(setting_count > 0);
	QVERIFY(software_list_count > 0);
	QVERIFY(ram_option_count > 0);
}


// static TestFixture<Test> fixture;
#include "info_builder_test.moc"
