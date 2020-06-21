/***************************************************************************

    test.cpp

    Testing infrastructure

***************************************************************************/

#include "test.h"

static std::list<std::function<std::unique_ptr<QObject>()>> s_testFuncs;


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  accumulateTest
//-------------------------------------------------

void TestFixtureBase::accumulateTest(std::function<std::unique_ptr<QObject>()> &&func)
{
    s_testFuncs.push_back(std::move(func));
}


//-------------------------------------------------
//  main
//-------------------------------------------------

int main(int argc, char *argv[])
{
    bool anyFailed = false;

    for (const auto &func : s_testFuncs)
    {
        auto test = func();
        if (QTest::qExec(test.get(), argc, argv) != 0)
            anyFailed = true;
    }
    return anyFailed ? -1 : 0;
}
