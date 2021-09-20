/***************************************************************************

	liveinstancetracker_test.cpp

	Unit tests for liveinstancetracker.cpp

***************************************************************************/

#include "liveinstancetracker.h"
#include "test.h"

namespace
{
	class Test : public QObject
	{
		Q_OBJECT

	private slots:
		void general();
	};
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void Test::general()
{
	LiveInstanceTracker<QObject> tracker;
	QVERIFY(!tracker);

	{
		QObject obj;
		tracker.track(obj);

		QVERIFY(tracker);
		QVERIFY(&*tracker == &obj);
	}

	QVERIFY(!tracker);
}


//-------------------------------------------------

static TestFixture<Test> fixture;
#include "liveinstancetracker_test.moc"
