/***************************************************************************

    about_test.cpp

    Unit tests for about.cpp

***************************************************************************/

#include "dialogs/about.h"
#include "../test.h"

class AboutDialog::Test : public QObject
{
    Q_OBJECT

private slots:
    void getPrettyMameVersion1() { getPrettyMameVersion("",                                     ""); }
    void getPrettyMameVersion2() { getPrettyMameVersion("0.220 (mame0220)",                     "MAME 0.220"); }
    void getPrettyMameVersion3() { getPrettyMameVersion("0.226 (mame0226-648-g98752caae1f)",    "MAME 0.226 (mame0226-648-g98752caae1f)"); }

private:
    void getPrettyMameVersion(const QString &mameVersion, const QString &expectedResult);
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

void AboutDialog::Test::getPrettyMameVersion(const QString &mameVersion, const QString &expectedResult)
{
    QString result = AboutDialog::getPrettyMameVersion(mameVersion);
    QVERIFY(result == expectedResult);
}


static TestFixture<AboutDialog::Test> fixture;
#include "about_test.moc"
