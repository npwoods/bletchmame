/***************************************************************************

    profile_test.cpp

    Unit tests for profile.cpp

***************************************************************************/

// bletchmame headers
#include "profile.h"
#include "test.h"

// Qt headers
#include <QBuffer>


class profiles::profile::Test : public QObject
{
	Q_OBJECT

private slots:
	void general();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void profiles::profile::Test::general()
{
	// load an infodb and get a machine
	info::database db;
	QVERIFY(db.load(buildInfoDatabase()));
	std::optional<info::machine> machine = db.find_machine("coco2b");
	QVERIFY(machine);

	// create a profile in a buffer
	QBuffer buffer;
	QVERIFY(buffer.open(QIODevice::ReadWrite));
	profile::create(buffer, *machine, nullptr);

	// load the profile
	QVERIFY(buffer.seek(0));
	std::optional<profile> p = profile::load(buffer, "/MyDir/MyProfile.bletchmameprofile", "MyProfile");
	QVERIFY(p);
	QVERIFY(p->name() == "MyProfile");
	QVERIFY(p->path() == "/MyDir/MyProfile.bletchmameprofile");
	QVERIFY(p->devslots().size() == 0);
	QVERIFY(p->images().size() == 0);

	// create a slot
	profiles::slot &slot = p->devslots().emplace_back();
	slot.m_name = "ext";
	slot.m_value = "fdcv11";

	// create an image
	profiles::image &image = p->images().emplace_back();
	image.m_tag = "ext:fdcv11:wd17xx:0:qd";
	image.m_path = "/MyDir/MyDisk.dsk";

	// persist the profile
	QVERIFY(buffer.seek(0));
	p->save_as(buffer);

	// try reloading the profile
	QVERIFY(buffer.seek(0));
	std::optional<profile> p2 = profile::load(buffer, "/MyDir/MyProfile.bletchmameprofile", "MyProfile");
	QVERIFY(p2);
	QVERIFY(p2->name() == "MyProfile");
	QVERIFY(p2->path() == "/MyDir/MyProfile.bletchmameprofile");
	QVERIFY(p2->devslots().size() == 1);
	QVERIFY(p2->devslots()[0].m_name == "ext");
	QVERIFY(p2->devslots()[0].m_value == "fdcv11");
	QVERIFY(p2->images().size() == 1);
	QVERIFY(p2->images()[0].m_tag == "ext:fdcv11:wd17xx:0:qd");
	QVERIFY(p2->images()[0].m_path == "/MyDir/MyDisk.dsk");
	
	// verify that they are equal
	QVERIFY(p == p2);

	// and verify that it isn't equal to an empty profile
	QVERIFY(p != profiles::profile());
}


//-------------------------------------------------

static TestFixture<profiles::profile::Test> fixture;
#include "profile_test.moc"
