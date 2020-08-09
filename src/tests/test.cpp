/***************************************************************************

    test.cpp

    Testing infrastructure

***************************************************************************/

#include <iostream>
#include "test.h"


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
//  main
//-------------------------------------------------

int main(int argc, char *argv[])
{
    std::cout << "BletchMAME Test Harness" << std::endl;
    std::cout << TestFixtureBase::testFixtureCount() << " total test fixture(s)" << std::endl;

    bool anyFailed = false;
    for (const TestFixtureBase &testFixture : TestFixtureBase::testFixtures())
    {
        auto testObject = testFixture.createTestObject();
        if (QTest::qExec(testObject.get(), argc, argv) != 0)
            anyFailed = true;
    }
    return anyFailed ? -1 : 0;
}
