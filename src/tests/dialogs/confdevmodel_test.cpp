/***************************************************************************

    dialogs/confdevmodel_test.cpp

    Unit tests for dialogs/confdevmodel.cpp

***************************************************************************/

// bletchmame headers
#include "dialogs/confdevmodel.h"
#include "../test.h"

class ConfigurableDevicesModel::Test : public QObject
{
	Q_OBJECT

private slots:
	void relativeTag1() { relativeTag("wd17xx:0",	"ext:fdcv11:wd17xx:0",		"ext:fdcv11"); }
	void relativeTag2() { relativeTag("qd",			"ext:fdcv11:wd17xx:0:qd",	"ext:fdcv11:wd17xx:0:qd"); }
	void relativeTag3() { relativeTag("ext",		"ext",						""); }
	void test();

private:
	static void updateModelWithStatus(ConfigurableDevicesModel &model, const QString &statusUpdateFileName);
	void relativeTag(const QString &expected, const QString &tag, const QString &baseTag);
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  test
//-------------------------------------------------

void ConfigurableDevicesModel::Test::test()
{
	// load an info DB
	QByteArray byteArray = buildInfoDatabase(":/resources/listxml_coco.xml");
	info::database infoDb;
	QVERIFY(infoDb.load(byteArray));

	// find a machine
	std::optional<info::machine> machine = infoDb.find_machine("coco2b");
	QVERIFY(machine.has_value());

	// load the model
	ConfigurableDevicesModel model(nullptr, *machine, software_list_collection());

	// load the status
	updateModelWithStatus(model, ":/resources/status_mame0227_coco2b_1.xml");

	// validate the root
	QVERIFY(model.rowCount(QModelIndex()) == 5);
	QVERIFY(model.data(model.index(0, 0, QModelIndex()), Qt::DisplayRole) == "cassette");
	QVERIFY(model.data(model.index(1, 0, QModelIndex()), Qt::DisplayRole) == "ext");
	QVERIFY(model.data(model.index(2, 0, QModelIndex()), Qt::DisplayRole) == "rs232");
	QVERIFY(model.data(model.index(3, 0, QModelIndex()), Qt::DisplayRole) == "vhd0");
	QVERIFY(model.data(model.index(4, 0, QModelIndex()), Qt::DisplayRole) == "vhd1");

	// validate "ext"
	QModelIndex extModelIndex = model.index(1, 0, QModelIndex());
	QVERIFY(model.rowCount(extModelIndex) == 4);
	QVERIFY(model.data(model.index(0, 0, extModelIndex), Qt::DisplayRole) == "slot1");
	QVERIFY(model.data(model.index(1, 0, extModelIndex), Qt::DisplayRole) == "slot2");
	QVERIFY(model.data(model.index(2, 0, extModelIndex), Qt::DisplayRole) == "slot3");
	QVERIFY(model.data(model.index(3, 0, extModelIndex), Qt::DisplayRole) == "slot4");
}


//-------------------------------------------------
//  updateModelWithStatus
//-------------------------------------------------

void ConfigurableDevicesModel::Test::updateModelWithStatus(ConfigurableDevicesModel &model, const QString &statusUpdateFileName)
{
	QFile statusUpdateFile(statusUpdateFileName);
	QVERIFY(statusUpdateFile.open(QIODevice::ReadOnly));
	status::update statusUpdate = status::update::read(statusUpdateFile);
	
	if (statusUpdate.m_slots.has_value())
		model.setSlots(*statusUpdate.m_slots);
	if (statusUpdate.m_images.has_value())
		model.setImages(*statusUpdate.m_images);
}


//-------------------------------------------------
//  relativeTag
//-------------------------------------------------

void ConfigurableDevicesModel::Test::relativeTag(const QString &expected, const QString &tag, const QString &baseTag)
{
	QString actual = ConfigurableDevicesModel::relativeTag(tag, baseTag);
	QVERIFY(actual == expected);
}


static TestFixture<ConfigurableDevicesModel::Test> fixture;
#include "confdevmodel_test.moc"
