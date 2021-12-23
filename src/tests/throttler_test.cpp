/***************************************************************************

	throttler_test.cpp

	Unit tests for throttler.cpp

***************************************************************************/

#include "throttler.h"
#include "test.h"

namespace
{
	class Test : public QObject
	{
		Q_OBJECT

	private slots:
		void test();
	};
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  test
//-------------------------------------------------

void Throttler::Test::test()
{
	using namespace std::chrono_literals;

	Throttler throttler(10s);

	QVERIFY( thottler.check( 1s));
	QVERIFY(!thottler.check( 2s));
	QVERIFY(!thottler.check( 4s));
	QVERIFY(!thottler.check( 6s));
	QVERIFY(!thottler.check( 8s));
	QVERIFY(!thottler.check(10s));
	QVERIFY( thottler.check(12s));
	QVERIFY(!thottler.check(14s));
	QVERIFY(!thottler.check(16s));
	QVERIFY( thottler.check(23s));
}


//-------------------------------------------------

static TestFixture<Test> fixture;
#include "throttler_test.moc"
