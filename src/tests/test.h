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
protected:
    static void accumulateTest(std::function<std::unique_ptr<QObject>()> &&func);
};


template<typename T>
class TestFixture : public TestFixtureBase
{
public:
    TestFixture()
    {
        accumulateTest([] { return std::make_unique<T>(); });
    }
};

#endif // TEST_H
