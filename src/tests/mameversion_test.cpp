/***************************************************************************

    mameversion_test.cpp

    Unit tests for mameversion.cpp

***************************************************************************/

// bletchmame headers
#include "mameversion.h"
#include "test.h"

class MameVersion::Test : public QObject
{
    Q_OBJECT

private slots:
    void ctor1() { ctor("blah",																	0, 0, true); }
    void ctor2() { ctor("0.217 (mame0217)",														0, 217, false); }
    void ctor3() { ctor("0.217 (mame0217-90-g0cb747353b)",										0, 217, true); }
    void ctor4() { ctor([]() { return MameVersion(0, 217); },									0, 217, false); }
    void isAtLeast1() { isAtLeast(MameVersion("0.216 (mame0216)"),								SimpleMameVersion(0, 217), false); }
    void isAtLeast2() { isAtLeast(MameVersion("0.218 (mame0218)"),								SimpleMameVersion(0, 217), true); }
    void isAtLeast3() { isAtLeast(MameVersion("0.217 (mame0217)"),								SimpleMameVersion(0, 217), true); }
    void isAtLeast4() { isAtLeast(MameVersion("0.217 (mame0217-90-g0cb747353b)"),				SimpleMameVersion(0, 217), true); }
    void nextCleanVersion1() { nextCleanVersion(MameVersion("0.217 (mame0217)"),				MameVersion(0, 217)); }
    void nextCleanVersion2() { nextCleanVersion(MameVersion("0.217 (mame0217-90-g0cb747353b)"),	MameVersion(0, 218)); }
	void toPrettyString1() { toPrettyString("",													"MAME ???"); }
	void toPrettyString2() { toPrettyString("0.220 (mame0220)",									"MAME 0.220"); }
	void toPrettyString3() { toPrettyString("0.226 (mame0226-648-g98752caae1f)",				"MAME 0.226 (mame0226-648-g98752caae1f)"); }

private:
    void ctor(const char *versionString,                 int expectedMajor, int expectedMinor, bool expectedDirty);
    void ctor(const std::function<MameVersion()> &func,  int expectedMajor, int expectedMinor, bool expectedDirty);
    void isAtLeast(const MameVersion &version1, const SimpleMameVersion &version2, bool expectedResult);
    void nextCleanVersion(const MameVersion &version, const MameVersion &expectedResult);
	void toPrettyString(const char *versionString, const char *expectedResult);
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

	// verify that when we construct the string, toString() returns the exact same result
	QVERIFY(MameVersion(versionString).toString() == versionString);
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

void MameVersion::Test::ctor(const std::function<MameVersion()> &func, int expectedMajor, int expectedMinor, bool expectedDirty)
{
    MameVersion version = func();
    QVERIFY(version.major() == expectedMajor);
    QVERIFY(version.minor() == expectedMinor);
    QVERIFY(version.isDirty() == expectedDirty);
}


//-------------------------------------------------
//  isAtLeast
//-------------------------------------------------

void MameVersion::Test::isAtLeast(const MameVersion &version1, const SimpleMameVersion &version2, bool expectedResult)
{
    bool result = version1.isAtLeast(version2);
    QVERIFY(result == expectedResult);
}


//-------------------------------------------------
//  nextCleanVersion
//-------------------------------------------------

void MameVersion::Test::nextCleanVersion(const MameVersion &version, const MameVersion &expectedResult)
{
    MameVersion result = version.nextCleanVersion();
	QVERIFY(result == expectedResult);
}


//-------------------------------------------------
//  toPrettyString
//-------------------------------------------------

void MameVersion::Test::toPrettyString(const char *versionString, const char *expectedResult)
{
	MameVersion mameVersion(versionString);
	QString result = mameVersion.toPrettyString();
	QVERIFY(result == expectedResult);
}


//-------------------------------------------------

static TestFixture<MameVersion::Test> fixture;
#include "mameversion_test.moc"
