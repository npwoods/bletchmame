/***************************************************************************

    runmachinetask_test.cpp

    Unit tests for runmachinetask.cpp

***************************************************************************/

#include "runmachinetask.h"
#include "test.h"

class RunMachineTask::Test : public QObject
{
    Q_OBJECT

private slots:
    void buildCommand();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  buildCommand
//-------------------------------------------------

void RunMachineTask::Test::buildCommand()
{
    QString command = RunMachineTask::buildCommand({ "alpha", "bravo", "charlie" });
    QVERIFY(command == "alpha bravo charlie\r\n");
}


static TestFixture<RunMachineTask::Test> fixture;
#include "runmachinetask_test.moc"
