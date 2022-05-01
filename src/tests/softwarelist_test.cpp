/***************************************************************************

    softwarelist_test.cpp

    Unit tests for softwarelist_test.cpp

***************************************************************************/

// bletchmame headers
#include "softwarelist.h"
#include "test.h"


// ======================> software_list::test

class software_list::test : public QObject
{
    Q_OBJECT

private slots:
	void general1()		{ general(":/resources/softlist_coco_cart.xml"); }
	void general2()		{ general(":/resources/softlist_msx1_cart.xml"); }

private:
	void general(const QString &resourceName);
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void software_list::test::general(const QString &resourceName)
{
	// get the test asset
	QFile testAsset(resourceName);
	QVERIFY(testAsset.open(QFile::ReadOnly));

	// and read it
	software_list softlist;
	QString error_message;
	bool success = softlist.load(testAsset, error_message);

	// did we succeed?
	QVERIFY(success);
	QVERIFY(error_message.isEmpty());

	for (const software_list::software &sw : softlist.get_software())
	{
		for (const software_list::part &part : sw.parts())
		{
			QVERIFY(!part.interface().isEmpty());
			QVERIFY(!part.name().isEmpty());
			QVERIFY(!part.dataareas().empty());
			
			for (const software_list::dataarea &area : part.dataareas())
			{
				QVERIFY(!area.name().isEmpty());
			}
		}
	}
}


static TestFixture<software_list::test> fixture;
#include "softwarelist_test.moc"
