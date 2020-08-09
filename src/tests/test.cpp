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

std::list<std::reference_wrapper<const TestFixtureBase>> &TestFixtureBase::testFixtures()
{
    static std::list<std::reference_wrapper<const TestFixtureBase>> s_testFixtures;
    return s_testFixtures;
}


//-------------------------------------------------
//  main
//-------------------------------------------------

int main(int argc, char *argv[])
{
    std::cout << "BletchMAME Test Harness" << std::endl;
    std::cout << TestFixtureBase::testFixtures().size() << " total test(s)" << std::endl;

    bool anyFailed = false;
    for (const TestFixtureBase &testFixture : TestFixtureBase::testFixtures())
    {
        auto testObject = testFixture.createTestObject();
        if (QTest::qExec(testObject.get(), argc, argv) != 0)
            anyFailed = true;
    }
    return anyFailed ? -1 : 0;
}
