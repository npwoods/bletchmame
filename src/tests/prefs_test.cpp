/***************************************************************************

    prefs_test.cpp

    Unit tests for prefs.cpp

***************************************************************************/

#include "prefs.h"
#include "test.h"

class Preferences::Test : public QObject
{
    Q_OBJECT

private slots:
	void general();
	void pathNames();
	void globalGetPathCategory();
	void machineGetPathCategory();
	void substitutions1();
	void substitutions2();
	void substitutions3();

private:
	void substitutions(const char *input, const char *expected);
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void Preferences::Test::general()
{
	const char *xml =
		"<preferences menu_bar_shown=\"1\">"
#ifdef Q_OS_WIN32
		"<path type=\"emu\">C:\\mame64.exe</path>"
		"<path type=\"roms\">C:\\roms</path>"
		"<path type=\"samples\">C:\\samples</path>"
		"<path type=\"config\">C:\\cfg</path>"
		"<path type=\"nvram\">C:\\nvram</path>"
#else
                "<path type=\"emu\">/mame64</path>"
                "<path type=\"roms\">/roms</path>"
                "<path type=\"samples\">/samples</path>"
                "<path type=\"config\">/cfg</path>"
                "<path type=\"nvram\">/nvram</path>"
#endif

		"<size width=\"1230\" height=\"765\"/>"
		"<selectedmachine>nes</selectedmachine>"
		"<column id=\"name\" width=\"84\" order=\"0\" />"
		"<column id=\"description\" width=\"165\" order=\"1\" />"
		"<column id=\"year\" width=\"50\" order=\"2\" />"
		"<column id=\"manufacturer\" width=\"320\" order=\"3\" />"

#ifdef Q_OS_WIN32
		"<machine name=\"echo\" working_directory=\"C:\\MyEchoGames\" last_save_state=\"C:\\MyLastState.sta\" />"
#else
		"<machine name=\"echo\" working_directory=\"/MyEchoGames\" last_save_state=\"/MyLastState.sta\" />"
#endif
		"</preferences>";

	QByteArray byte_array(xml, util::safe_static_cast<int>(strlen(xml)));
	QDataStream input(byte_array);
	Preferences prefs;
	prefs.Load(input);

#ifdef Q_OS_WIN32
	QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE) == "C:\\mame64.exe");
	QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::ROMS) == "C:\\roms\\");
	QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::SAMPLES) == "C:\\samples\\");
	QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::CONFIG) == "C:\\cfg\\");
	QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::NVRAM) == "C:\\nvram\\");

	QVERIFY(prefs.GetMachinePath("echo", Preferences::machine_path_type::WORKING_DIRECTORY) == "C:\\MyEchoGames\\");
	QVERIFY(prefs.GetMachinePath("echo", Preferences::machine_path_type::LAST_SAVE_STATE) == "C:\\MyLastState.sta");
#else
        QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE) == "/mame64");
        QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::ROMS) == "/roms/");
        QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::SAMPLES) == "/samples/");
        QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::CONFIG) == "/cfg/");
        QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::NVRAM) == "/nvram/");

        QVERIFY(prefs.GetMachinePath("echo", Preferences::machine_path_type::WORKING_DIRECTORY) == "/MyEchoGames/");
        QVERIFY(prefs.GetMachinePath("echo", Preferences::machine_path_type::LAST_SAVE_STATE) == "/MyLastState.sta");
#endif
	QVERIFY(prefs.GetMachinePath("foxtrot", Preferences::machine_path_type::WORKING_DIRECTORY) == "");
	QVERIFY(prefs.GetMachinePath("foxtrot", Preferences::machine_path_type::LAST_SAVE_STATE) == "");
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
		Preferences::GetPathCategory(type);
}


//-------------------------------------------------
//  machineGetPathCategory
//-------------------------------------------------

void Preferences::Test::machineGetPathCategory()
{
	for (Preferences::machine_path_type type : util::all_enums<Preferences::machine_path_type>())
		Preferences::GetPathCategory(type);
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

	QString actual = Preferences::InternalApplySubstitutions(QString(input), func);
	assert(actual == expected);
	(void)actual;
	(void)expected;
}


void Preferences::Test::substitutions1() { substitutions("C:\\foo", "C:\\foo"); }
void Preferences::Test::substitutions2() { substitutions("C:\\foo (with parens)", "C:\\foo (with parens)"); }
void Preferences::Test::substitutions3() { substitutions("C:\\$(VARNAME)\\foo", "C:\\vardata\\foo"); }


static TestFixture<Preferences::Test> fixture;
#include "prefs_test.moc"
