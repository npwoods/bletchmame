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
	void path_names();
	void global_get_path_category();
	void machine_get_path_category();
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
		"<path type=\"emu\">C:\\mame64.exe</path>"
		"<path type=\"roms\">C:\\roms</path>"
		"<path type=\"samples\">C:\\samples</path>"
		"<path type=\"config\">C:\\cfg</path>"
		"<path type=\"nvram\">C:\\nvram</path>"

		"<size width=\"1230\" height=\"765\"/>"
		"<selectedmachine>nes</selectedmachine>"
		"<column id=\"name\" width=\"84\" order=\"0\" />"
		"<column id=\"description\" width=\"165\" order=\"1\" />"
		"<column id=\"year\" width=\"50\" order=\"2\" />"
		"<column id=\"manufacturer\" width=\"320\" order=\"3\" />"

		"<machine name=\"echo\" working_directory=\"C:\\MyEchoGames\" last_save_state=\"C:\\MyLastState.sta\" />"
		"</preferences>";

	QByteArray byte_array(xml, util::safe_static_cast<int>(strlen(xml)));
	QDataStream input(byte_array);
	Preferences prefs;
	prefs.Load(input);

	QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE) == "C:\\mame64.exe");
	QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::ROMS) == "C:\\roms\\");
	QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::SAMPLES) == "C:\\samples\\");
	QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::CONFIG) == "C:\\cfg\\");
	QVERIFY(prefs.GetGlobalPath(Preferences::global_path_type::NVRAM) == "C:\\nvram\\");

	QVERIFY(prefs.GetMachinePath("echo", Preferences::machine_path_type::WORKING_DIRECTORY) == "C:\\MyEchoGames\\");
	QVERIFY(prefs.GetMachinePath("echo", Preferences::machine_path_type::LAST_SAVE_STATE) == "C:\\MyLastState.sta");
	QVERIFY(prefs.GetMachinePath("foxtrot", Preferences::machine_path_type::WORKING_DIRECTORY) == "");
	QVERIFY(prefs.GetMachinePath("foxtrot", Preferences::machine_path_type::LAST_SAVE_STATE) == "");
}


//-------------------------------------------------
//  path_names
//-------------------------------------------------

void Preferences::Test::path_names()
{
	auto iter = std::find(s_path_names.begin(), s_path_names.end(), nullptr);
	QVERIFY(iter == s_path_names.end());
}


//-------------------------------------------------
//  global_get_path_category
//-------------------------------------------------

void Preferences::Test::global_get_path_category()
{
	for (Preferences::global_path_type type : util::all_enums<Preferences::global_path_type>())
		Preferences::GetPathCategory(type);
}


//-------------------------------------------------
//  machine_get_path_category
//-------------------------------------------------

void Preferences::Test::machine_get_path_category()
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
