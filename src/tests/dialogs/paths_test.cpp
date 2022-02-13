/***************************************************************************

    dialogs/paths_test.cpp

    Unit tests for dialogs/paths.cpp

***************************************************************************/

// bletchmame headers
#include "dialogs/paths.h"
#include "dialogs/pathslistviewmodel.h"
#include "../test.h"

class PathsDialog::Test : public QObject
{
	Q_OBJECT

private slots:
	void model();
	void buildComboBoxStrings();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  model
//-------------------------------------------------

void PathsDialog::Test::model()
{
	// create preferences
	Preferences prefs;
	prefs.setGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE,	"/mame_dir/mame");
	prefs.setGlobalPath(Preferences::global_path_type::ROMS,			Model::joinPaths(QStringList() << "/mame_dir/roms0" << "/mame_dir/roms1"));
	prefs.setGlobalPath(Preferences::global_path_type::SAMPLES,			Model::joinPaths(QStringList() << "/mame_dir/samples0" << "/mame_dir/samples1" << "/mame_dir/samples2"));

	// validate the prefs
	QStringList initialEmuExecutablePath = Model::splitPaths(prefs.getGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE));
	QVERIFY(initialEmuExecutablePath.size() == 1);
	QVERIFY(initialEmuExecutablePath[0] == "/mame_dir/mame");
	QStringList initialRomsPath = Model::splitPaths(prefs.getGlobalPath(Preferences::global_path_type::ROMS));
	QVERIFY(initialRomsPath.size() == 2);
	QVERIFY(initialRomsPath[0] == "/mame_dir/roms0");
	QVERIFY(initialRomsPath[1] == "/mame_dir/roms1");
	QStringList initialSamplesPath = Model::splitPaths(prefs.getGlobalPath(Preferences::global_path_type::SAMPLES));
	QVERIFY(initialSamplesPath.size() == 3);
	QVERIFY(initialSamplesPath[0] == "/mame_dir/samples0");
	QVERIFY(initialSamplesPath[1] == "/mame_dir/samples1");
	QVERIFY(initialSamplesPath[2] == "/mame_dir/samples2");

	// create some models
	PathsListViewModel listViewModel;
	Model model(prefs, listViewModel);

	// try EMU_EXECUTABLE
	model.setCurrentPathType(Preferences::global_path_type::EMU_EXECUTABLE);
	QVERIFY(listViewModel.rowCount(QModelIndex()) == 1);
	QVERIFY(listViewModel.data(listViewModel.index(0, 0), Qt::DisplayRole) == "/mame_dir/mame");

	// try ROMS
	model.setCurrentPathType(Preferences::global_path_type::ROMS);
	QVERIFY(listViewModel.rowCount(QModelIndex()) == 3);
	QVERIFY(listViewModel.data(listViewModel.index(0, 0), Qt::DisplayRole) == "/mame_dir/roms0");
	QVERIFY(listViewModel.data(listViewModel.index(1, 0), Qt::DisplayRole) == "/mame_dir/roms1");
	QVERIFY(listViewModel.data(listViewModel.index(2, 0), Qt::DisplayRole) == PathsListViewModel::s_entryText);

	// try SAMPLES
	model.setCurrentPathType(Preferences::global_path_type::SAMPLES);
	QVERIFY(listViewModel.rowCount(QModelIndex()) == 4);
	QVERIFY(listViewModel.data(listViewModel.index(0, 0), Qt::DisplayRole) == "/mame_dir/samples0");
	QVERIFY(listViewModel.data(listViewModel.index(1, 0), Qt::DisplayRole) == "/mame_dir/samples1");
	QVERIFY(listViewModel.data(listViewModel.index(2, 0), Qt::DisplayRole) == "/mame_dir/samples2");
	QVERIFY(listViewModel.data(listViewModel.index(3, 0), Qt::DisplayRole) == PathsListViewModel::s_entryText);

	// change some data
	listViewModel.setData(listViewModel.index(2, 0), "/mame_dir/samples_new_2", Qt::EditRole);
	listViewModel.setData(listViewModel.index(3, 0), "/mame_dir/samples_new_3", Qt::EditRole);

	// validate that the paths have not changed yet
	QStringList samplesPathList1 = Model::splitPaths(prefs.getGlobalPath(Preferences::global_path_type::SAMPLES));
	QVERIFY(samplesPathList1.size() == 3);
	QVERIFY(samplesPathList1[0] == "/mame_dir/samples0");
	QVERIFY(samplesPathList1[1] == "/mame_dir/samples1");
	QVERIFY(samplesPathList1[2] == "/mame_dir/samples2");

	// persist and verify that they've changed
	model.persist();
	QStringList samplesPathList2 = Model::splitPaths(prefs.getGlobalPath(Preferences::global_path_type::SAMPLES));
	QVERIFY(samplesPathList2.size() == 4);
	QVERIFY(samplesPathList2[0] == "/mame_dir/samples0");
	QVERIFY(samplesPathList2[1] == "/mame_dir/samples1");
	QVERIFY(samplesPathList2[2] == "/mame_dir/samples_new_2");
	QVERIFY(samplesPathList2[3] == "/mame_dir/samples_new_3");
}


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
