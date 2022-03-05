/***************************************************************************

    prefs_test.cpp

    Unit tests for prefs.cpp

***************************************************************************/

// bletchmame headers
#include "prefs.h"
#include "test.h"

// Qt headers
#include <QBuffer>
#include <QTextStream>

// standard headers
#include <sstream>


class Preferences::Test : public QObject
{
    Q_OBJECT

private slots:
	void generalWithRegurgitate()		{ general(true); }
	void generalWithoutRegurgitate()	{ general(false); }
	void load();
	void loadFiresEvents();
	void save();
	void defaults();
	void substitutions();
	void pathNames();
	void globalGetPathCategory();
	void machineGetPathCategory();
	void substitutions1();
	void substitutions2();
	void substitutions3();
	void setFolderPrefs();
	void customFolders();

private:
	static void loadSamplePrefsXml(QBuffer &buffer);
	static QString fixPaths(QString &&s);
	static QString fixPaths(const char16_t *s);
	void general(bool regurgitate);
	void substitutions(const char *input, const char *expected);
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void Preferences::Test::general(bool regurgitate)
{
	QBuffer buffer;
	loadSamplePrefsXml(buffer);

	Preferences prefs;
	if (regurgitate)
	{
		// we're regurgitatating; read the prefs into a separate Preferences object
		Preferences prefs2;
		prefs2.load(buffer);

		// and save it out
		QBuffer stringStream;
		QVERIFY(stringStream.open(QIODevice::ReadWrite));
		prefs2.save(stringStream);

		// seek back to the beginning
		QVERIFY(stringStream.seek(0));

		// and read the saved out bytes back
		QVERIFY(prefs.load(stringStream));
	}
	else
	{
		// read the prefs directly; no regurgitation
		QVERIFY(prefs.load(buffer));
	}

	// validate the results
	QVERIFY(prefs.getGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE)					== fixPaths("C:\\mame64.exe"));
	QVERIFY(prefs.getGlobalPath(Preferences::global_path_type::ROMS)							== fixPaths("C:\\roms\\"));
	QVERIFY(prefs.getGlobalPath(Preferences::global_path_type::SAMPLES)							== fixPaths("C:\\samples\\"));
	QVERIFY(prefs.getGlobalPath(Preferences::global_path_type::CONFIG)							== fixPaths("C:\\cfg\\"));
	QVERIFY(prefs.getGlobalPath(Preferences::global_path_type::NVRAM)							== fixPaths("C:\\nvram\\"));
	QVERIFY(prefs.getMachinePath("echo", Preferences::machine_path_type::WORKING_DIRECTORY)		== fixPaths(u"C:\\My\u20ACchoGames\\"));
	QVERIFY(prefs.getMachinePath("echo", Preferences::machine_path_type::LAST_SAVE_STATE)		== fixPaths(u"C:\\MyLastSt\u03B1te.sta"));
	QVERIFY(prefs.getMachinePath("foxtrot", Preferences::machine_path_type::WORKING_DIRECTORY)	== fixPaths(""));
	QVERIFY(prefs.getMachinePath("foxtrot", Preferences::machine_path_type::LAST_SAVE_STATE)	== fixPaths(""));
}


//-------------------------------------------------
//  load
//-------------------------------------------------

void Preferences::Test::load()
{
	// create a temporary directory
	QTemporaryDir tempDir;
	QVERIFY(tempDir.isValid());

	// try to load preferences; should return false because there is no file
	Preferences prefs(QDir(tempDir.path()));
	QVERIFY(!prefs.load());
}


//-------------------------------------------------
//  loadFiresEvents
//-------------------------------------------------

void Preferences::Test::loadFiresEvents()
{
	// create preferences
	Preferences prefs;

	// listen to events
	int selectedTabChanged = 0;
	int auditingStateChanged = 0;
	int emuExecutableChangedCount = 0;
	int romsChangedCount = 0;
	int samplesChangedCount = 0;
	int iconsChangedCount = 0;
	int profilesChangedCount = 0;
	int snapshotsChnagedCount = 0;
	connect(&prefs, &Preferences::selectedTabChanged,				this, [&](list_view_type)	{ selectedTabChanged++; });
	connect(&prefs, &Preferences::auditingStateChanged,				this, [&]()					{ auditingStateChanged++; });
	connect(&prefs, &Preferences::globalPathEmuExecutableChanged,	this, [&](const QString &)	{ emuExecutableChangedCount++; });
	connect(&prefs, &Preferences::globalPathRomsChanged,			this, [&](const QString &)	{ romsChangedCount++; });
	connect(&prefs, &Preferences::globalPathSamplesChanged,			this, [&](const QString &)	{ samplesChangedCount++; });
	connect(&prefs, &Preferences::globalPathIconsChanged,			this, [&](const QString &)	{ iconsChangedCount++; });
	connect(&prefs, &Preferences::globalPathProfilesChanged,		this, [&](const QString &)	{ profilesChangedCount++; });
	connect(&prefs, &Preferences::globalPathSnapshotsChanged,		this, [&](const QString &)	{ snapshotsChnagedCount++; });

	// prepare sample prefs XML
	QBuffer buffer;
	loadSamplePrefsXml(buffer);

	// load it!
	QVERIFY(prefs.load(buffer));

	// validate the counts
	QVERIFY(selectedTabChanged			== 0);
	QVERIFY(auditingStateChanged		== 0);
	QVERIFY(emuExecutableChangedCount	== 1);
	QVERIFY(romsChangedCount			== 1);
	QVERIFY(samplesChangedCount			== 1);
	QVERIFY(iconsChangedCount			== 0);
	QVERIFY(profilesChangedCount		== 0);
	QVERIFY(snapshotsChnagedCount		== 0);
}


//-------------------------------------------------
//  save
//-------------------------------------------------

void Preferences::Test::save()
{
	// create a temporary directory
	QTemporaryDir tempDir;
	QVERIFY(tempDir.isValid());

	// try to save preferences; this should succeed
	Preferences prefs(QDir(tempDir.path() + "/subdir"));
	prefs.save();

	// ensure that BletchMAME.xml got created in 'subdir'
	QVERIFY(QFileInfo(tempDir.path() + "/subdir/BletchMAME.xml").isFile());
}


//-------------------------------------------------
//  defaults
//-------------------------------------------------

void Preferences::Test::defaults()
{
	// create a temporary directory
	QTemporaryDir tempDir;
	QVERIFY(tempDir.isValid());

	// create Preferences
	Preferences prefs(QDir(tempDir.path()));

	// validate defaults
	QVERIFY(prefs.getWindowBarsShown()									== true);
	QVERIFY(prefs.getWindowState()										== WindowState::Normal);
	QVERIFY(prefs.getSelectedTab()										== list_view_type::MACHINE);
	QVERIFY(prefs.getAuditingState()									== AuditingState::Default);
	QVERIFY(prefs.getGlobalPath(global_path_type::EMU_EXECUTABLE)		== "");
	QVERIFY(prefs.getGlobalPath(global_path_type::ROMS)					== "");
	QVERIFY(prefs.getGlobalPath(global_path_type::SAMPLES)				== "");
	QVERIFY(prefs.getGlobalPath(global_path_type::CONFIG)				== tempDir.path());
	QVERIFY(prefs.getGlobalPath(global_path_type::NVRAM)				== tempDir.path());
	QVERIFY(prefs.getGlobalPath(global_path_type::PROFILES)				== QDir(tempDir.path()).filePath("profiles"));
}


//-------------------------------------------------
//  substitutions
//-------------------------------------------------

void Preferences::Test::substitutions()
{
	// create a temporary directory
	QTemporaryDir tempDir;
	QVERIFY(tempDir.isValid());

	// create Preferences with defaults
	Preferences prefs(QDir(tempDir.path()));
	prefs.setGlobalPath(global_path_type::EMU_EXECUTABLE, QDir(tempDir.path()).filePath("mame_dir/mame.exe"));

	// validate the "plugins" dir is set up
	QStringList pluginsPath = prefs.getSplitPaths(global_path_type::PLUGINS);
	QVERIFY(pluginsPath.size() == 2);
	QVERIFY(pluginsPath[0] == QDir(QCoreApplication::applicationDirPath()).filePath("plugins/"));
	QVERIFY(pluginsPath[1] == QDir(tempDir.path()).filePath("mame_dir/plugins/"));

	// validate the "hash" dir is set up
	QStringList hashPath = prefs.getSplitPaths(global_path_type::HASH);
	QVERIFY(hashPath.size() == 1);
	QVERIFY(hashPath[0] == QDir(tempDir.path()).filePath("mame_dir/hash/"));
}


//-------------------------------------------------
//  pathNames
//-------------------------------------------------

void Preferences::Test::pathNames()
{
	for (const GlobalPathInfo &pi : s_globalPathInfo)
	{
		QVERIFY(pi.m_prefsName);
	}
}


//-------------------------------------------------
//  globalGetPathCategory
//-------------------------------------------------

void Preferences::Test::globalGetPathCategory()
{
	for (Preferences::global_path_type type : util::all_enums<Preferences::global_path_type>())
		Preferences::getPathCategory(type);
}


//-------------------------------------------------
//  machineGetPathCategory
//-------------------------------------------------

void Preferences::Test::machineGetPathCategory()
{
	for (Preferences::machine_path_type type : util::all_enums<Preferences::machine_path_type>())
		Preferences::getPathCategory(type);
}


//-------------------------------------------------
//  substitutions
//-------------------------------------------------

void Preferences::Test::substitutions(const char *input, const char *expected)
{
	auto func = [](const QString &var_name)
	{
		return var_name == "VARNAME"
			? "vardata"
			: QString();
	};

	QString actual = Preferences::internalApplySubstitutions(QString(input), func);
	assert(actual == expected);
	(void)actual;
	(void)expected;
}


void Preferences::Test::substitutions1() { substitutions("C:\\foo", "C:\\foo"); }
void Preferences::Test::substitutions2() { substitutions("C:\\foo (with parens)", "C:\\foo (with parens)"); }
void Preferences::Test::substitutions3() { substitutions("C:\\$(VARNAME)\\foo", "C:\\vardata\\foo"); }


//-------------------------------------------------
//  setFolderPrefs
//-------------------------------------------------

void Preferences::Test::setFolderPrefs()
{
	Preferences prefs;
	int folderPrefsChanged = 0;
	connect(&prefs, &Preferences::folderPrefsChanged, this, [&]() { folderPrefsChanged++; });

	// initial verifications
	QVERIFY(prefs.getFolderPrefs("foo").m_shown);
	QVERIFY(folderPrefsChanged == 0);

	// set shown to true - should not change anything
	FolderPrefs folderPrefs = FolderPrefs();
	folderPrefs.m_shown = true;
	prefs.setFolderPrefs("foo", std::move(folderPrefs));
	QVERIFY(prefs.getFolderPrefs("foo").m_shown);
	QVERIFY(folderPrefsChanged == 0);

	// set shown to false - this should be a change
	folderPrefs = FolderPrefs();
	folderPrefs.m_shown = false;
	prefs.setFolderPrefs("foo", std::move(folderPrefs));
	QVERIFY(!prefs.getFolderPrefs("foo").m_shown);
	QVERIFY(folderPrefsChanged == 1);
}


//-------------------------------------------------
//  customFolders
//-------------------------------------------------

void Preferences::Test::customFolders()
{
	Preferences prefs;
	int customFoldersChanged = 0;
	connect(&prefs, &Preferences::customFoldersChanged,	this, [&]() { customFoldersChanged++; });

	// initial verifications
	QVERIFY(prefs.getCustomFolders().empty());
	QVERIFY(customFoldersChanged == 0);

	// add a custom folder
	QVERIFY(prefs.addMachineToCustomFolder("MyFolder", "mymachine"));
	QVERIFY(customFoldersChanged == 1);
	QVERIFY(prefs.getCustomFolders().find("MyFolder")->second.size() == 1);
	QVERIFY(prefs.getCustomFolders().find("MyFolder")->second.contains("mymachine"));

	// try to add it again - nothing should happen
	QVERIFY(!prefs.addMachineToCustomFolder("MyFolder", "mymachine"));
	QVERIFY(customFoldersChanged == 1);
	QVERIFY(prefs.getCustomFolders().find("MyFolder")->second.size() == 1);
	QVERIFY(prefs.getCustomFolders().find("MyFolder")->second.contains("mymachine"));

	// add another machine
	QVERIFY(prefs.addMachineToCustomFolder("MyFolder", "myothermachine"));
	QVERIFY(customFoldersChanged == 2);
	QVERIFY(prefs.getCustomFolders().find("MyFolder")->second.size() == 2);
	QVERIFY(prefs.getCustomFolders().find("MyFolder")->second.contains("mymachine"));
	QVERIFY(prefs.getCustomFolders().find("MyFolder")->second.contains("myothermachine"));

	// rename the folder to itself - nothing should change
	QVERIFY(!prefs.renameCustomFolder("MyFolder", "MyFolder"));
	QVERIFY(customFoldersChanged == 2);
	QVERIFY(prefs.getCustomFolders().find("MyFolder")->second.size() == 2);
	QVERIFY(prefs.getCustomFolders().find("MyFolder")->second.contains("mymachine"));
	QVERIFY(prefs.getCustomFolders().find("MyFolder")->second.contains("myothermachine"));

	// rename the folder to a new name
	QVERIFY(prefs.renameCustomFolder("MyFolder", "ThatFolder"));
	QVERIFY(customFoldersChanged == 3);
	QVERIFY(prefs.getCustomFolders().find("ThatFolder")->second.size() == 2);
	QVERIFY(prefs.getCustomFolders().find("ThatFolder")->second.contains("mymachine"));
	QVERIFY(prefs.getCustomFolders().find("ThatFolder")->second.contains("myothermachine"));

	// perform a remove
	QVERIFY(prefs.removeMachineFromCustomFolder("ThatFolder", "mymachine"));
	QVERIFY(customFoldersChanged == 4);
	QVERIFY(prefs.getCustomFolders().find("ThatFolder")->second.size() == 1);
	QVERIFY(prefs.getCustomFolders().find("ThatFolder")->second.contains("myothermachine"));

	// delete the folder outright
	QVERIFY(prefs.deleteCustomFolder("ThatFolder"));
	QVERIFY(prefs.getCustomFolders().empty());
	QVERIFY(customFoldersChanged == 5);
}


//-------------------------------------------------
//  loadSamplePrefsXml
//-------------------------------------------------

void Preferences::Test::loadSamplePrefsXml(QBuffer &buffer)
{
	// read the prefs.xml test case into a QString and fix it (see comments
	// for fixPaths() for details)
	QString text;
	{
		QFile file(":/resources/prefs.xml");
		QVERIFY(file.open(QIODevice::ReadOnly));
		QTextStream textStream(&file);
		text = fixPaths(textStream.readAll());
	}

	// and put the text back into a buffer
	QVERIFY(buffer.open(QIODevice::ReadWrite));
	{
		QTextStream textStream(&buffer);
		textStream << text;
	}
	QVERIFY(buffer.seek(0));
}


//-------------------------------------------------
//  fixPaths - make it easier to write a test case
//	that doesn't involve a lot of path nonsense
//-------------------------------------------------

QString Preferences::Test::fixPaths(QString &&s)
{
#ifdef Qd_OS_WIN32
	bool applyFix = false;
#else
	bool applyFix = true;
#endif

	if (applyFix)
	{
		// this is hacky, but it works in practice
		s = s.replace("C:\\", "/");
		s = s.replace("\\", "/");
		s = s.replace(".exe", "");
	}
	return std::move(s);
}


//-------------------------------------------------
//  fixPaths
//-------------------------------------------------

QString Preferences::Test::fixPaths(const char16_t *s)
{
	QString str = QString::fromUtf16(s);
	return fixPaths(std::move(str));
}


//-------------------------------------------------

static TestFixture<Preferences::Test> fixture;
#include "prefs_test.moc"
