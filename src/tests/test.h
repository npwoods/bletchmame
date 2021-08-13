/***************************************************************************

    test.h

    Testing infrastructure

***************************************************************************/

#pragma once

#ifndef TEST_H
#define TEST_H

#include <QTest>

#include <forward_list>
#include <functional>
#include <memory>

// ======================> TestFixtureBase
class TestFixtureBase
{
public:
    // virtuals
    virtual std::unique_ptr<QObject> createTestObject() const = 0;

    // accessors
    static std::forward_list<std::reference_wrapper<const TestFixtureBase>> &testFixtures();
    static int testFixtureCount();

protected:
    // ctor
    TestFixtureBase();
};


// ======================> TestFixture
template<typename T>
class TestFixture : public TestFixtureBase
{
public:
    virtual std::unique_ptr<QObject> createTestObject() const override
    {
        return std::make_unique<T>();
    }
};

// runners
int runAndExcerciseMame(int argc, char *argv[]);
int runAndExcerciseListXml(int argc, char *argv[], bool sequential);

// helper functions
QByteArray buildInfoDatabase(const QString &fileName = ":/resources/listxml_coco.xml", bool skipDtd = false);

#endif // TEST_H
