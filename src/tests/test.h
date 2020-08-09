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


template<typename T>
class TestFixture : public TestFixtureBase
{
public:
    virtual std::unique_ptr<QObject> createTestObject() const override
    {
        return std::make_unique<T>();
    }
};

#endif // TEST_H
