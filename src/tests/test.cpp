/***************************************************************************

    test.cpp

    Testing infrastructure

***************************************************************************/

// bletchmame headers
#include "test.h"

// standard headers
#include <iostream>


//**************************************************************************
//  CONSTANTS
//**************************************************************************

// on Windows, for unclear reasons (Qt?) the process return code will be zero even
// if main() returns a non-zero value or std::exit is called, so we're using
// std::abort() on that platform (sigh)
//
// this happens on both MSYS2 and MSVC
#ifdef WIN32
#define ABORT_ON_FAILURE	1
#else // !WIN32
#define ABORT_ON_FAILURE	0
#endif // WIN32


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
        std::cout << "TEST FAILURES OCCURRED" << std::endl;
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
		argc -= 2;
		argv += 2;
		result = runAndExcerciseMame(argc, argv);
    }
    else if (argc >= 2 && !strcmp(argv[1], "--runlistxml"))
    {
		argc -= 2;
		argv += 2;
		result = runAndExcerciseListXml(argc, argv, false, 1);
    }
    else if (argc >= 2 && !strcmp(argv[1], "--runlistxml-sequential"))
    {
		argc -= 2;
		argv += 2;

		int run_count = 1;
		if (argc > 2 && !strcmp(argv[0], "--runs"))
		{
			run_count = std::max(1, atoi(argv[1]));
			argc -= 2;
			argv += 2;
		}

        result = runAndExcerciseListXml(argc, argv, true, run_count);
    }
    else
    {
        result = runTestFixtures(argc, argv);
    }

	// use std::abort() if we have to
	if (ABORT_ON_FAILURE && result != 0)
		std::abort();

    return result;
}
