/***************************************************************************

    dialogs/paths_test.cpp

    Unit tests for dialogs/paths.cpp

***************************************************************************/

#include "dialogs/paths.h"
#include "../test.h"

class PathsDialog::Test : public QObject
{
	Q_OBJECT

private slots:
	void buildComboBoxStrings();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  buildComboBoxStrings
//-------------------------------------------------

void PathsDialog::Test::buildComboBoxStrings()
{
	// get the strings
	auto strs = PathsDialog::buildComboBoxStrings();

	// ensure that none are empty
	for (const QString &s : strs)
	{
		QVERIFY(!s.isEmpty());
	}
}


static TestFixture<PathsDialog::Test> fixture;
#include "paths_test.moc"
