/***************************************************************************

    client_test.cpp

    Unit tests for client.cpp

***************************************************************************/

#include "client.h"
#include "test.h"

class MameClient::Test : public QObject
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

void MameClient::Test::appendExtraArguments()
{
    QStringList argv = { "Alpha", "Bravo" };
    MameClient::appendExtraArguments(argv, "Charlie \"Delta Echo\" Foxtrot");

    QVERIFY(argv.size() == 5);
    QVERIFY(argv[0] == "Alpha");
    QVERIFY(argv[1] == "Bravo");
    QVERIFY(argv[2] == "Charlie");
    QVERIFY(argv[3] == "Delta Echo");
    QVERIFY(argv[4] == "Foxtrot");
}


// static TestFixture<MameClient::Test> fixture;
#include "client_test.moc"
