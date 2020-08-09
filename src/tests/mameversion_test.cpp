/***************************************************************************

    mameversion_test.cpp

    Unit tests for mameversion.cpp

***************************************************************************/

#include "mameversion.h"
#include "test.h"

class MameVersion::Test : public QObject
{
    Q_OBJECT

private slots:
    void test1() { general("blah", 0, 0, true); }
    void test2() { general("0.217 (mame0217)", 0, 217, false); }
    void test3() { general("0.217 (mame0217-90-g0cb747353b)", 0, 217, true); }

private:
    void general(const char *version_string, int expected_major, int expected_minor, bool expected_dirty);
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void MameVersion::Test::general(const char *version_string, int expected_major, int expected_minor, bool expected_dirty)
{
    auto version = MameVersion(version_string);
    QVERIFY(version.Major() == expected_major);
    QVERIFY(version.Minor() == expected_minor);
    QVERIFY(version.Dirty() == expected_dirty);
}


// static TestFixture<MameVersion::Test> fixture;
#include "mameversion_test.moc"
