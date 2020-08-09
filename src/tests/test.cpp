/***************************************************************************

    test.cpp

    Testing infrastructure

***************************************************************************/

#include <iostream>
#include "test.h"

static TestFixtureBase *s_testFixtureList = nullptr;


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  TestFixtureBase ctor
//-------------------------------------------------

TestFixtureBase::TestFixtureBase()
{
    m_next = s_testFixtureList;
    s_testFixtureList = this;
}


//-------------------------------------------------
//  main
//-------------------------------------------------

int main(int argc, char *argv[])
{
    std::cout << "BletchMAME Test Harness" << std::endl;
    std::cout << "??? total test(s)" << std::endl;

    bool anyFailed = false;
    for (TestFixtureBase *testFixture = s_testFixtureList; testFixture; testFixture = testFixture->next())
//    for (const TestFixtureBase &testFixture : s_testFixtures)
    {
        printf("testFixture=%p\n", testFixture);

        // auto testObject = testFixture.createTestObject();
        // if (QTest::qExec(testObject.get(), argc, argv) != 0)
            // anyFailed = true;
    }
    return anyFailed ? -1 : 0;
}
