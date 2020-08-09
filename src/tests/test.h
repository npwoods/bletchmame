/***************************************************************************

    test.h

    Testing infrastructure

***************************************************************************/

#pragma once

#ifndef TEST_H
#define TEST_H

#include <QTest>

#include <functional>

class TestFixtureBase
{
public:
    // virtuals
    virtual std::unique_ptr<QObject> createTestObject() const = 0;

    // accessor
    static const std::list<std::reference_wrapper<const TestFixtureBase>> &testFixtures();

protected:
    // ctor
    TestFixtureBase();

private:
    static std::list<std::reference_wrapper<const TestFixtureBase>> s_testFixtures;
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
