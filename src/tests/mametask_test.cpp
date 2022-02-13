/***************************************************************************

    mametask_test.cpp

    Unit tests for mametask.cpp

***************************************************************************/

// bletchmame headers
#include "mametask.h"
#include "test.h"

class MameTask::Test : public QObject
{
    Q_OBJECT

private slots:
    void appendExtraArguments();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  appendExtraArguments
//-------------------------------------------------

void MameTask::Test::appendExtraArguments()
{
    QStringList argv = { "Alpha", "Bravo" };
	MameTask::appendExtraArguments(argv, "Charlie \"Delta Echo\" Foxtrot");

    QVERIFY(argv.size() == 5);
    QVERIFY(argv[0] == "Alpha");
    QVERIFY(argv[1] == "Bravo");
    QVERIFY(argv[2] == "Charlie");
    QVERIFY(argv[3] == "Delta Echo");
    QVERIFY(argv[4] == "Foxtrot");
}


static TestFixture<MameTask::Test> fixture;
#include "mametask_test.moc"
