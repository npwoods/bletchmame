/***************************************************************************

	devstatusdisplay_test.cpp

	Unit tests for devstatusdisplay.cpp

***************************************************************************/

// bletchmame headers
#include "devstatusdisplay.h"
#include "test.h"


// ======================> DevicesStatusDisplay::Test

class DevicesStatusDisplay::Test : public QObject
{
	Q_OBJECT

private slots:
	void formatTime_1()		{ formatTime(62.3f, "1:02"); }
	void imagesLoad();

private:
	void formatTime(float seconds, const QString &expected);
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  formatTime
//-------------------------------------------------

void DevicesStatusDisplay::Test::formatTime(float seconds, const QString &expected)
{
	QString actual = DevicesStatusDisplay::formatTime(seconds);
	QVERIFY(expected == actual);
}


//-------------------------------------------------
//  imagesLoad
//-------------------------------------------------

void DevicesStatusDisplay::Test::imagesLoad()
{
	QVERIFY(!tryLoadImage(s_cassettePixmapResourceName));
	QVERIFY(!tryLoadImage(s_cassettePlayPixmapResourceName));
	QVERIFY(!tryLoadImage(s_cassetteRecordPixmapResourceName));
}


//-------------------------------------------------

static TestFixture<DevicesStatusDisplay::Test> fixture;
#include "devstatusdisplay_test.moc"
