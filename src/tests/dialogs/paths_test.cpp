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
	void fileAndDirTypes();
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


//-------------------------------------------------
//  fileAndDirTypes
//-------------------------------------------------

void PathsDialog::Test::fileAndDirTypes()
{
	// ensure that all path types are either a file or dir
	for (int i = 0; i < static_cast<int>(Preferences::global_path_type::COUNT); i++)
	{
		Preferences::global_path_type path_type = static_cast<Preferences::global_path_type>(i);
		bool is_file_path_type = isFilePathType(path_type);
		bool is_dir_path_type = isDirPathType(path_type);
		QVERIFY(is_file_path_type || is_dir_path_type);
	}
}


// static TestFixture<PathsDialog::Test> fixture;
#include "paths_test.moc"
