/***************************************************************************

    test.cpp

    Testing infrastructure

***************************************************************************/

#include <iostream>
#include "test.h"
#include "mamerunner.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  TestFixtureBase ctor
//-------------------------------------------------

TestFixtureBase::TestFixtureBase()
{
    testFixtures().push_front(*this);
}


//-------------------------------------------------
//  TestFixtureBase::testFixtures
//-------------------------------------------------

std::forward_list<std::reference_wrapper<const TestFixtureBase>> &TestFixtureBase::testFixtures()
{
    static std::forward_list<std::reference_wrapper<const TestFixtureBase>> s_testFixtures;
    return s_testFixtures;
}


//-------------------------------------------------
//  TestFixtureBase::testFixtureCount
//-------------------------------------------------

int TestFixtureBase::testFixtureCount()
{
    int count = 0;
    for (auto iter = testFixtures().begin(); iter != testFixtures().end(); iter++)
        count++;
    return count;
}


//-------------------------------------------------
//  runTestFixtures
//-------------------------------------------------

static int runTestFixtures(int argc, char *argv[])
{
    std::cout << TestFixtureBase::testFixtureCount() << " total test fixture(s)" << std::endl;

    bool anyFailed = false;
    for (const TestFixtureBase &testFixture : TestFixtureBase::testFixtures())
    {
        auto testObject = testFixture.createTestObject();
        if (QTest::qExec(testObject.get(), argc, argv) != 0)
            anyFailed = true;
    }

    if (anyFailed)
        std::cout << "TEST FAILURES OCCURRED - CRASHING" << std::endl;
    else
        std::cout << "All tests succeeded" << std::endl;
    return anyFailed ? 1 : 0;
}


//-------------------------------------------------
//  main
//-------------------------------------------------

int main(int argc, char *argv[])
{
    int result;
    std::cout << "BletchMAME Test Harness" << std::endl;

    // create a QCoreApplication to appease what might require it
    int fauxArgc = 0;
    char **fauxArgv = nullptr;
    QCoreApplication app(fauxArgc, fauxArgv);

    // we support different types of tests
    if (argc >= 2 && !strcmp(argv[1], "--runmame"))
    {
        result = runAndExcerciseMame(argc - 2, argv + 2);
    }
    else
    {
        result = runTestFixtures(argc, argv);
    }

    // did we have any failures?
    if (result != 0)
    {
        // the monstrosity below is a consequence of seemingly not being able to
        // report errors with exit codes under GitHub actions; for some reason exit
        // codes are not working but the code below does trigger a failure
        size_t i = 0;
        do
        {
            *((long *)i++) = 0xDEADBEEF;
        } while (rand() != rand());
    }

    return result;
}
