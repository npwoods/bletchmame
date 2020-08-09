/***************************************************************************

    test.cpp

    Testing infrastructure

***************************************************************************/

#include <iostream>
#include "test.h"

std::list<std::reference_wrapper<const TestFixtureBase>> TestFixtureBase::s_testFixtures;


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  TestFixtureBase ctor
//-------------------------------------------------

TestFixtureBase::TestFixtureBase()
{
    s_testFixtures.push_front(*this);
}


//-------------------------------------------------
//  TestFixtureBase::testFixtures
//-------------------------------------------------

const std::list<std::reference_wrapper<const TestFixtureBase>> &TestFixtureBase::testFixtures()
{
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
        // auto testObject = testFixture.createTestObject();
        // if (QTest::qExec(testObject.get(), argc, argv) != 0)
            // anyFailed = true;
    }
    return anyFailed ? -1 : 0;
}
