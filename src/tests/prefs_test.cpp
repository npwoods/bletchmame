/***************************************************************************

    prefs_test.cpp

    Unit tests for prefs.cpp

***************************************************************************/

#include <QBuffer>
#include <QTextStream>
#include <sstream>

#include "prefs.h"
#include "test.h"

class Preferences::Test : public QObject
{
    Q_OBJECT

private slots:
	void generalWithRegurgitate()		{ general(true); }
	void generalWithoutRegurgitate()	{ general(false); }
	void load();
	void save();
	void defaults();
	void pathNames();
	void globalGetPathCategory();
	void machineGetPathCategory();
	void substitutions1();
	void substitutions2();
	void substitutions3();

private:
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
	QBuffer buffer;
	QVERIFY(buffer.open(QIODevice::ReadWrite));
	{
		QTextStream textStream(&buffer);
		textStream << text;
	}
	QVERIFY(buffer.seek(0));

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
	QVERIFY(prefs.getMenuBarShown()										== true);
	QVERIFY(prefs.getWindowState()										== WindowState::Normal);
	QVERIFY(prefs.getSelectedTab()										== list_view_type::MACHINE);
	QVERIFY(prefs.getAuditingState()									== AuditingState::Default);
	QVERIFY(prefs.getGlobalPath(global_path_type::EMU_EXECUTABLE)		== "");
	QVERIFY(prefs.getGlobalPath(global_path_type::ROMS)					== "");
	QVERIFY(prefs.getGlobalPath(global_path_type::SAMPLES)				== "");
	QVERIFY(prefs.getGlobalPath(global_path_type::CONFIG)				== QDir::toNativeSeparators(tempDir.path()));
	QVERIFY(prefs.getGlobalPath(global_path_type::NVRAM)				== QDir::toNativeSeparators(tempDir.path()));
	QVERIFY(prefs.getGlobalPath(global_path_type::PROFILES)				== QDir::toNativeSeparators(QDir(tempDir.path()).filePath("profiles")));
}


//-------------------------------------------------
//  pathNames
//-------------------------------------------------

void Preferences::Test::pathNames()
{
	auto iter = std::find(s_path_names.begin(), s_path_names.end(), nullptr);
	QVERIFY(iter == s_path_names.end());
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


static TestFixture<Preferences::Test> fixture;
#include "prefs_test.moc"
