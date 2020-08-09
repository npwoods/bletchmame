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
    // virtual std::unique_ptr<QObject> createTestObject() const = 0;

protected:
    TestFixtureBase();
};


template<typename T>
class TestFixture : public TestFixtureBase
{
public:
    // virtual std::unique_ptr<QObject> createTestObject() const override
    // {
        // return std::make_unique<T>();
    // }
};

#endif // TEST_H
