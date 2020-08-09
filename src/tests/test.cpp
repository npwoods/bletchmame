/***************************************************************************

    test.cpp

    Testing infrastructure

***************************************************************************/

#include <iostream>
#include "test.h"

static std::list<std::reference_wrapper<TestFixtureBase>> s_testFixtures;


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  TestFixtureBase ctor
//-------------------------------------------------

TestFixtureBase::TestFixtureBase()
{
    // s_testFixtures.push_back(*this);
}


//-------------------------------------------------
//  main
//-------------------------------------------------

int main(int argc, char *argv[])
{
    std::cout << "BletchMAME Test Harness" << std::endl;
    std::cout << s_testFixtures.size() << " total test(s)" << std::endl;

    bool anyFailed = false;
    for (const TestFixtureBase &testFixture : s_testFixtures)
    {
        // auto testObject = testFixture.createTestObject();
        // if (QTest::qExec(testObject.get(), argc, argv) != 0)
            // anyFailed = true;
    }
    return anyFailed ? -1 : 0;
}
