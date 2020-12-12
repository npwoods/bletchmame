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
    void ctor1() { ctor("blah",                                      0, 0, true); }
    void ctor2() { ctor("0.217 (mame0217)",                          0, 217, false); }
    void ctor3() { ctor("0.217 (mame0217-90-g0cb747353b)",           0, 217, true); }
    void ctor4() { ctor([]() { return MameVersion(0, 217, false); }, 0, 217, false); }
    void ctor5() { ctor([]() { return MameVersion(0, 218, true); },  0, 218, true); }
    void isAtLeast1() { isAtLeast(MameVersion(0, 217, false), MameVersion(0, 216, false), true); }
    void isAtLeast2() { isAtLeast(MameVersion(0, 217, false), MameVersion(0, 218, false), false); }
    void isAtLeast3() { isAtLeast(MameVersion(0, 217, false), MameVersion(0, 217, false), true); }
    void isAtLeast4() { isAtLeast(MameVersion(0, 217, false), MameVersion(0, 217,  true), false); }
    void isAtLeast5() { isAtLeast(MameVersion(0, 217,  true), MameVersion(0, 217,  true), true); }
    void nextCleanVersion1() { nextCleanVersion(MameVersion(0, 217, false), MameVersion(0, 217, false)); }
    void nextCleanVersion2() { nextCleanVersion(MameVersion(0, 217,  true), MameVersion(0, 218, false)); }

private:
    void ctor(const char *versionString,                 int expectedMajor, int expectedMinor, bool expectedDirty);
    void ctor(const std::function<MameVersion()> &func,  int expectedMajor, int expectedMinor, bool expectedDirty);
    void isAtLeast(MameVersion version1, MameVersion version2, bool expectedResult);
    void nextCleanVersion(MameVersion version, MameVersion expectedNextCleanVersion);
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

void MameVersion::Test::ctor(const char *versionString, int expectedMajor, int expectedMinor, bool expectedDirty)
{
    ctor([versionString]() { return MameVersion(versionString); }, expectedMajor, expectedMinor, expectedDirty);
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

void MameVersion::Test::ctor(const std::function<MameVersion()> &func, int expectedMajor, int expectedMinor, bool expectedDirty)
{
    MameVersion version = func();
    QVERIFY(version.major() == expectedMajor);
    QVERIFY(version.minor() == expectedMinor);
    QVERIFY(version.dirty() == expectedDirty);
}


//-------------------------------------------------
//  isAtLeast
//-------------------------------------------------

void MameVersion::Test::isAtLeast(MameVersion version1, MameVersion version2, bool expectedResult)
{
    bool result = version1.isAtLeast(version2);
    QVERIFY(result == expectedResult);
}


//-------------------------------------------------
//  nextCleanVersion
//-------------------------------------------------

void MameVersion::Test::nextCleanVersion(MameVersion version, MameVersion expectedNextCleanVersion)
{
    MameVersion result = version.nextCleanVersion();
    QVERIFY(result.major() == expectedNextCleanVersion.major());
    QVERIFY(result.minor() == expectedNextCleanVersion.minor());
    QVERIFY(result.dirty() == expectedNextCleanVersion.dirty());
}


static TestFixture<MameVersion::Test> fixture;
#include "mameversion_test.moc"
