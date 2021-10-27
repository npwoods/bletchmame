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
//  TestLocaleOverride ctor
//-------------------------------------------------

TestLocaleOverride::TestLocaleOverride(const char *localeOverride)
{
    // get the old locale
    const char *oldLocale = setlocale(LC_ALL, nullptr);
    if (oldLocale)
        m_oldLocale = oldLocale;

    // not exactly thread safe, but good enough for now
    setlocale(LC_ALL, localeOverride);
}


//-------------------------------------------------
//  TestLocaleOverride dtor
//-------------------------------------------------

TestLocaleOverride::~TestLocaleOverride()
{
    // restore the old locale
    setlocale(LC_ALL, m_oldLocale.c_str());
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
    else if (argc >= 2 && !strcmp(argv[1], "--runlistxml"))
    {
        result = runAndExcerciseListXml(argc - 2, argv + 2, false);
    }
    else if (argc >= 2 && !strcmp(argv[1], "--runlistxml-sequential"))
    {
        result = runAndExcerciseListXml(argc - 2, argv + 2, true);
    }
    else
    {
        result = runTestFixtures(argc, argv);
    }

#ifdef WIN32
	// for some reason the MSYS2 builds don't handle this
	if (result)
		throw false;
#endif

    return result;
}
