/***************************************************************************

    prefs.cpp

    Wrapper for preferences specific to BletchMAME

***************************************************************************/

#include <fstream>
#include <functional>
#include <locale>

#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>

#include "prefs.h"
#include "utility.h"
#include "xmlparser.h"
#include "validity.h"


//**************************************************************************
//  LOCAL VARIABLES
//**************************************************************************

static std::array<const char *, static_cast<size_t>(Preferences::global_path_type::COUNT)>	s_path_names =
{
	"emu",
	"roms",
	"samples",
	"config",
	"nvram",
	"hash",
	"artwork",
	"icons",
	"plugins",
	"profiles"
};


static const util::enum_parser_bidirectional<ColumnPrefs::sort_type> s_column_sort_type_parser =
{
	{ "ascending", ColumnPrefs::sort_type::ASCENDING, },
	{ "descending", ColumnPrefs::sort_type::DESCENDING }
};


static const util::enum_parser_bidirectional<Preferences::list_view_type> s_list_view_type_parser =
{
	{ "machine", Preferences::list_view_type::MACHINE, },
	{ "softwarelist", Preferences::list_view_type::SOFTWARELIST, },
	{ "profile", Preferences::list_view_type::PROFILE }
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

static QString GetDefaultPluginsDirectory();


//-------------------------------------------------
//  ValidateDimension
//-------------------------------------------------

static bool IsValidDimension(int dimension)
{
    // arbitrary validation of dimensions
    return dimension >= 10 && dimension <= 20000;
}


//-------------------------------------------------
//  GetListViewSelectionKey
//-------------------------------------------------

static QString GetListViewSelectionKey(const char *view_type, const QString &softlist)
{
	return QString(view_type) + QString(1, '\0') + softlist;
}


//-------------------------------------------------
//  SplitListViewSelectionKey
//-------------------------------------------------

static std::tuple<const QChar *, const QChar *> SplitListViewSelectionKey(const QString &key)
{
	// get the view type
	const QChar *view_type = key.constData();

	// get the machine key, if present
	int null_pos = key.indexOf((QChar)'\0');
	const QChar *machine = key.size() > null_pos + 1
		? view_type + null_pos + 1
		: nullptr;

	// and return them as a tuple
	return std::make_tuple(view_type, machine);
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

Preferences::Preferences()
	: m_size(950, 600)
	, m_menu_bar_shown(true)
	, m_selected_tab(list_view_type::MACHINE)
{
	// default paths
	SetGlobalPath(global_path_type::CONFIG, GetConfigDirectory(true));
	SetGlobalPath(global_path_type::NVRAM, GetConfigDirectory(true));
	SetGlobalPath(global_path_type::PLUGINS, GetDefaultPluginsDirectory());
	SetGlobalPath(global_path_type::PROFILES, GetConfigDirectory(true) + QString("\\profiles"));

    Load();
}


//-------------------------------------------------
//  GetPathCategory
//-------------------------------------------------

Preferences::path_category Preferences::GetPathCategory(global_path_type path_type)
{
	path_category result;
	switch (path_type)
	{
	case Preferences::global_path_type::EMU_EXECUTABLE:
		result = path_category::FILE;
		break;

	case Preferences::global_path_type::CONFIG:
	case Preferences::global_path_type::NVRAM:
		result = path_category::SINGLE_DIRECTORY;
		break;

	case Preferences::global_path_type::ROMS:
	case Preferences::global_path_type::SAMPLES:
	case Preferences::global_path_type::HASH:
	case Preferences::global_path_type::ARTWORK:
	case Preferences::global_path_type::PLUGINS:
	case Preferences::global_path_type::PROFILES:
	case Preferences::global_path_type::ICONS:
		result = path_category::MULTIPLE_DIRECTORIES;
		break;

	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  GetPathCategory
//-------------------------------------------------

Preferences::path_category Preferences::GetPathCategory(machine_path_type path_type)
{
	path_category result;
	switch (path_type)
	{
	case Preferences::machine_path_type::LAST_SAVE_STATE:
		result = path_category::FILE;
		break;

	case Preferences::machine_path_type::WORKING_DIRECTORY:
		result = path_category::SINGLE_DIRECTORY;
		break;

	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  EnsureDirectoryPathsHaveFinalPathSeparator
//-------------------------------------------------

void Preferences::EnsureDirectoryPathsHaveFinalPathSeparator(path_category category, QString &path)
{
	if (category != path_category::FILE && !path.isEmpty() && !wxFileName::IsPathSeparator(path[path.size() - 1]))
	{
		path += QDir::separator();
	}
}


//-------------------------------------------------
//  GetMachineInfo
//-------------------------------------------------

const Preferences::MachineInfo *Preferences::GetMachineInfo(const QString &machine_name) const
{
	const auto iter = m_machine_info.find(machine_name);
	return iter != m_machine_info.end()
		? &iter->second
		: nullptr;
}


//-------------------------------------------------
//  SetGlobalPath
//-------------------------------------------------

void Preferences::SetGlobalPath(global_path_type type, QString &&path)
{
	EnsureDirectoryPathsHaveFinalPathSeparator(GetPathCategory(type), path);
	m_paths[static_cast<size_t>(type)] = std::move(path);
}


void Preferences::SetGlobalPath(global_path_type type, const QString &path)
{
	SetGlobalPath(type, QString(path));
}


//-------------------------------------------------
//  GetSplitPaths
//-------------------------------------------------

std::vector<QString> Preferences::GetSplitPaths(global_path_type type) const
{
	const QString &paths_string = GetGlobalPath(type);
	return util::string_split(paths_string, [](const QChar ch) { return ch == ';'; });
}


//-------------------------------------------------
//  GetGlobalPathWithSubstitutions
//-------------------------------------------------

QString Preferences::GetGlobalPathWithSubstitutions(global_path_type type) const
{
	assert(GetPathCategory(type) != path_category::FILE);
	return ApplySubstitutions(GetGlobalPath(type));
}


//-------------------------------------------------
//  GetMachinePath
//-------------------------------------------------

const QString &Preferences::GetMachinePath(const QString &machine_name, machine_path_type path_type) const
{
	// find the machine path entry
	const MachineInfo *info = GetMachineInfo(machine_name);
	if (!info)
		return util::g_empty_string;

	switch (path_type)
	{
	case machine_path_type::WORKING_DIRECTORY:
		return info->m_working_directory;
	case machine_path_type::LAST_SAVE_STATE:
		return info->m_last_save_state;
	default:
		throw false;
	}
}


//-------------------------------------------------
//  GetListViewSelection
//-------------------------------------------------

const QString &Preferences::GetListViewSelection(const char *view_type, const QString &machine_name) const
{
	QString key = GetListViewSelectionKey(view_type, machine_name);
	auto iter = m_list_view_selection.find(key);
	return iter != m_list_view_selection.end()
		? iter->second
		: util::g_empty_string;
}


//-------------------------------------------------
//  SetListViewSelection
//-------------------------------------------------

void Preferences::SetListViewSelection(const char *view_type, const QString &machine_name, QString &&selection)
{
	QString key = GetListViewSelectionKey(view_type, machine_name);
	m_list_view_selection[key] = std::move(selection);
}


//-------------------------------------------------
//  SetMachinePath
//-------------------------------------------------

void Preferences::SetMachinePath(const QString &machine_name, machine_path_type path_type, QString &&path)
{
	// ensure that if we have a path, it has a path separator at the end
	EnsureDirectoryPathsHaveFinalPathSeparator(GetPathCategory(path_type), path);

	switch (path_type)
	{
	case machine_path_type::WORKING_DIRECTORY:
		m_machine_info[machine_name].m_working_directory = std::move(path);
		break;
	case machine_path_type::LAST_SAVE_STATE:
		m_machine_info[machine_name].m_last_save_state = std::move(path);
		break;
	default:
		throw false;
	}
}


//-------------------------------------------------
//  GetRecentDeviceFiles
//-------------------------------------------------

std::vector<QString> &Preferences::GetRecentDeviceFiles(const QString &machine_name, const QString &device_type)
{
	return m_machine_info[machine_name].m_recent_device_files[device_type];
}


const std::vector<QString> &Preferences::GetRecentDeviceFiles(const QString &machine_name, const QString &device_type) const
{
	static const std::vector<QString> empty_vector;
	const MachineInfo *info = GetMachineInfo(machine_name);
	if (!info)
		return empty_vector;

	auto iter = info->m_recent_device_files.find(device_type);
	if (iter == info->m_recent_device_files.end())
		return empty_vector;

	return iter->second;
}


//-------------------------------------------------
//  Load
//-------------------------------------------------

bool Preferences::Load()
{
	using namespace std::placeholders;

	QString file_name = GetFileName(false);

	// first check to see if the file exists
	bool success = false;
	if (wxFileExists(file_name))
	{
		QFile file(file_name);
		if (file.open(QFile::ReadOnly))
		{
			QDataStream file_stream(&file);
			success = Load(file_stream);
		}
	}
	return success;
}


//-------------------------------------------------
//  Load
//-------------------------------------------------

bool Preferences::Load(QDataStream &input)
{
	XmlParser xml;
	global_path_type type = global_path_type::COUNT;
	QString current_machine_name;
	QString current_device_type;
	QString *current_list_view_parameter = nullptr;

	// clear out state
	m_machine_info.clear();

	xml.OnElementBegin({ "preferences" }, [&](const XmlParser::Attributes &attributes)
	{
		bool menu_bar_shown;
		if (attributes.Get("menu_bar_shown", menu_bar_shown))
			SetMenuBarShown(menu_bar_shown);

		list_view_type selected_tab;
		if (attributes.Get("selected_tab", selected_tab, s_list_view_type_parser))
			SetSelectedTab(selected_tab);

	});
	xml.OnElementBegin({ "preferences", "path" }, [&](const XmlParser::Attributes &attributes)
	{
		QString type_string;
		if (attributes.Get("type", type_string))
		{
			auto iter = std::find(s_path_names.cbegin(), s_path_names.cend(), type_string);
			type = iter != s_path_names.cend()
				? static_cast<global_path_type>(iter - s_path_names.cbegin())
				: global_path_type::COUNT;
		}
	});
	xml.OnElementEnd({ "preferences", "path" }, [&](QString &&content)
	{
		if (type < global_path_type::COUNT)
			SetGlobalPath(type, std::move(content));
		type = global_path_type::COUNT;
	});
	xml.OnElementEnd({ "preferences", "mameextraarguments" }, [&](QString &&content)
	{
		SetMameExtraArguments(std::move(content));
	});
	xml.OnElementBegin({ "preferences", "size" }, [&](const XmlParser::Attributes &attributes)
	{
		int width, height;
		if (attributes.Get("width", width) && attributes.Get("height", height) && IsValidDimension(width) && IsValidDimension(height))
		{
			QSize size;
			size.setWidth(width);
			size.setHeight(height);
			SetSize(size);
		}
	});
	xml.OnElementBegin({ "preferences", "selection" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string list_view;
		std::string softlist;
		if (attributes.Get("view", list_view))
		{
			attributes.Get("softlist", softlist);
			QString key = GetListViewSelectionKey(list_view.c_str(), QString::fromStdString(softlist));
			current_list_view_parameter = &m_list_view_selection[key];
		}
	});
	xml.OnElementBegin({ "preferences", "searchboxtext" }, [&](const XmlParser::Attributes &attributes)
	{
		QString list_view;
		if (!attributes.Get("view", list_view))
			list_view = "machine";
		current_list_view_parameter = &m_list_view_filter[list_view];
	});
	xml.OnElementEnd({{ "preferences", "selection" },
					  { "preferences", "searchboxtext" }}, [&](QString &&content)
	{
		assert(current_list_view_parameter);
		*current_list_view_parameter = std::move(content);
		current_list_view_parameter = nullptr;
	});
	xml.OnElementBegin({ "preferences", "column" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string view_type, id;
		if (attributes.Get("type", view_type) && attributes.Get("id", id))
		{
			ColumnPrefs &col_prefs = m_column_prefs[view_type][id];
			attributes.Get("width", col_prefs.m_width);
			attributes.Get("order", col_prefs.m_order);
			attributes.Get("sort", col_prefs.m_sort, s_column_sort_type_parser);
		}
	});
	xml.OnElementBegin({ "preferences", "machine" }, [&](const XmlParser::Attributes &attributes)
	{
		if (!attributes.Get("name", current_machine_name))
			return XmlParser::element_result::SKIP;

		QString path;
		if (attributes.Get("working_directory", path))
			SetMachinePath(current_machine_name, machine_path_type::WORKING_DIRECTORY, std::move(path));
		if (attributes.Get("last_save_state", path))
			SetMachinePath(current_machine_name, machine_path_type::LAST_SAVE_STATE, std::move(path));
		return XmlParser::element_result::OK;
	});
	xml.OnElementBegin({ "preferences", "machine", "device" }, [&](const XmlParser::Attributes &attributes)
	{
		return attributes.Get("type", current_device_type)
			? XmlParser::element_result::OK
			: XmlParser::element_result::SKIP;
	});
	xml.OnElementEnd({ "preferences", "machine", "device", "recentfile" }, [&](QString &&content)
	{
		GetRecentDeviceFiles(current_machine_name, current_device_type).push_back(std::move(content));
	});
	bool success = xml.Parse(input);

	return success;
}


//-------------------------------------------------
//  Save
//-------------------------------------------------

void Preferences::Save()
{
	QString file_name = GetFileName(true);
	std::ofstream output(file_name.toStdString(), std::ios_base::out);
	Save(output);
}


//-------------------------------------------------
//  Save
//-------------------------------------------------

void Preferences::Save(std::ostream &output)
{
	output << "<!-- Preferences for BletchMAME -->" << std::endl;
	output << "<preferences menu_bar_shown=\"" << (m_menu_bar_shown ? "1" : "0") << "\" selected_tab=\"" << s_list_view_type_parser[GetSelectedTab()] << "\">" << std::endl;
	output << std::endl;

	output << "\t<!-- Paths -->" << std::endl;
	for (size_t i = 0; i < m_paths.size(); i++)
		output << "\t<path type=\"" << s_path_names[i] << "\">" << util::to_utf8_string(GetGlobalPath(static_cast<global_path_type>(i))) << "</path>" << std::endl;
	output << std::endl;

	output << "\t<!-- Miscellaneous -->" << std::endl;
	if (!m_mame_extra_arguments.isEmpty())
		output << "\t<mameextraarguments>" << util::to_utf8_string(m_mame_extra_arguments) << "</mameextraarguments>" << std::endl;
	output << "\t<size width=\"" << m_size.width() << "\" height=\"" << m_size.height() << "\"/>" << std::endl;

	for (const auto &pair : m_list_view_selection)
	{
		if (!pair.second.isEmpty())
		{
			auto [view_type, softlist] = SplitListViewSelectionKey(pair.first);
			output << "\t<selection view=\"" << util::to_utf8_string(QString(view_type));
			if (softlist)
				output << "\" softlist=\"" + util::to_utf8_string(QString(softlist));
			output << "\">" << XmlParser::Escape(pair.second) << "</selection>" << std::endl;
		}
	}

	for (const auto &[view_type, text] : m_list_view_filter)
	{
		if (!text.isEmpty())
			output << "\t<searchboxtext view=\"" << view_type.toStdString() << "\">" << text.toStdString() << "</searchboxtext>" << std::endl;
	}

	// column width/order
	for (const auto &view_prefs : m_column_prefs)
	{
		for (const auto &col_prefs : view_prefs.second)
		{
			output << "\t<column type=\"" << view_prefs.first << "\" id=\"" << col_prefs.first
				<< "\" width=\"" << col_prefs.second.m_width
				<< "\" order=\"" << col_prefs.second.m_order;

			if (col_prefs.second.m_sort.has_value())
				output << "\" sort=\"" << s_column_sort_type_parser[col_prefs.second.m_sort.value()];

			output << "\"/>" << std::endl;
		}
	}
	output << std::endl;

	output << "\t<!-- Machines -->" << std::endl;
	for (const auto &[machine_name, info] : m_machine_info)
	{
		if (!machine_name.isEmpty() && (!info.m_working_directory.isEmpty() || !info.m_last_save_state.isEmpty() || !info.m_recent_device_files.empty()))
		{
			output << "\t<machine name=\"" << XmlParser::Escape(machine_name) << "\"";
			
			if (!info.m_working_directory.isEmpty())
				output << " working_directory=\"" << XmlParser::Escape(info.m_working_directory) << "\"";
			if (!info.m_last_save_state.isEmpty())
				output << " last_save_state=\"" << XmlParser::Escape(info.m_last_save_state) << "\"";

			if (info.m_recent_device_files.empty())
			{
				output << "/>" << std::endl;
			}
			else
			{
				output << ">" << std::endl;
				for (const auto &[device_type, recents] : info.m_recent_device_files)
				{
					output << "\t\t<device type=\"" << XmlParser::Escape(device_type) << "\">" << std::endl;
					for (const QString &recent : recents)
						output << "\t\t\t<recentfile>" << recent.toStdString() << "</recentfile>" << std::endl;
					output << "\t\t</device>" << std::endl;
				}
				output << "\t</machine>" << std::endl;
			}
		}
	}
	output << std::endl;

	output << "</preferences>" << std::endl;
}


//-------------------------------------------------
//  InternalApplySubstitutions
//-------------------------------------------------

template<typename TStr, typename TFunc>
static TStr InternalApplySubstitutions(const TStr &src, TFunc func)
{
	QString result;
	result.reserve(src.size() + 100);

	enum class parse_state
	{
		NORMAL,
		AFTER_DOLLAR_SIGN,
		IN_VARIABLE_NAME
	};
	parse_state state = parse_state::NORMAL;
	typename TStr::const_iterator var_begin_iter = src.cbegin();

	for (typename TStr::const_iterator iter = src.cbegin(); iter < src.cend(); iter++)
	{
		bool handled = false;
		ushort ch = iter->unicode();
		switch (ch)
		{
		case '$':
			if (state == parse_state::NORMAL)
			{
				state = parse_state::AFTER_DOLLAR_SIGN;
				handled = true;
			}
			break;

		case '(':
			if (state == parse_state::AFTER_DOLLAR_SIGN)
			{
				state = parse_state::IN_VARIABLE_NAME;
				var_begin_iter = iter + 1;
				handled = true;
			}
			break;

		case ')':
			if (state == parse_state::IN_VARIABLE_NAME)
			{
				QString var_name(&*var_begin_iter, iter - var_begin_iter);
				TStr var_value = func(var_name);
				result += var_value;
				state = parse_state::NORMAL;
				handled = true;
			}
			break;
		}

		// if it wasn't handled, append the character
		if (!handled && state == parse_state::NORMAL)
			result += *iter;
	}
	return result;
}


//-------------------------------------------------
//  ApplySubstitutions
//-------------------------------------------------

QString Preferences::ApplySubstitutions(const QString &path) const
{

	return InternalApplySubstitutions(path, [this](const QString &var_name)
	{
		QString result;
		if (var_name == "MAMEPATH")
		{
			const QString &path = GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);
			wxFileName::SplitPath(path, &result, nullptr, nullptr);
		}
		else if (var_name == "BLETCHMAMEPATH")
		{
			result = QCoreApplication::applicationDirPath();
		}
		return result;
	});
}


//-------------------------------------------------
//  GetMameXmlDatabasePath
//-------------------------------------------------

QString Preferences::GetMameXmlDatabasePath(bool ensure_directory_exists) const
{
	// get the configuration directory
	QString config_dir = GetConfigDirectory(ensure_directory_exists);
	if (config_dir.isEmpty())
		return "";

	// get the MAME path
	const QString &mame_path = GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);
	if (mame_path.isEmpty())
		return "";

	// parse out the MAME filename
	QString mame_filename;
	wxFileName::SplitPath(mame_path, nullptr, &mame_filename, nullptr);

	// get the full name
	return QDir(config_dir).filePath(mame_filename + ".infodb");
}


//-------------------------------------------------
//  GetFileName
//-------------------------------------------------

QString Preferences::GetFileName(bool ensure_directory_exists)
{
	QString directory = GetConfigDirectory(ensure_directory_exists);
	return QDir(directory).filePath("BletchMAME.xml");
}


//-------------------------------------------------
//  GetConfigDirectory - gets the configuration
//	directory, and optionally ensuring it exists
//-------------------------------------------------

QString Preferences::GetConfigDirectory(bool ensure_directory_exists)
{
	// this is currently a thin wrapper on GetUserDataDir(), but hypothetically
	// we might want a command line option to override this directory
	QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

	// if appropriate, ensure the directory exists
	if (ensure_directory_exists)
	{
		QDir dir(directory);
		if (!dir.exists())
			dir.makeAbsolute();
	}
	return directory;
}


//-------------------------------------------------
//  GetDefaultPluginsDirectory
//-------------------------------------------------

static QString GetDefaultPluginsDirectory()
{
	QString path_separator(QDir::separator());
	QString plugins("plugins");

	return QString("$(BLETCHMAMEPATH)") + path_separator + plugins
		+ QString(";") + QString("$(MAMEPATH)") + path_separator + plugins;
}


//**************************************************************************
//  VALIDITY CHECKS
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

static void general()
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

	QByteArray byte_array(xml, strlen(xml));
	QDataStream input(byte_array);
	Preferences prefs;
	prefs.Load(input);

	assert(prefs.GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE)	== "C:\\mame64.exe");
	assert(prefs.GetGlobalPath(Preferences::global_path_type::ROMS)				== "C:\\roms\\");
	assert(prefs.GetGlobalPath(Preferences::global_path_type::SAMPLES)			== "C:\\samples\\");
	assert(prefs.GetGlobalPath(Preferences::global_path_type::CONFIG)			== "C:\\cfg\\");
	assert(prefs.GetGlobalPath(Preferences::global_path_type::NVRAM)			== "C:\\nvram\\");

	assert(prefs.GetMachinePath("echo", Preferences::machine_path_type::WORKING_DIRECTORY)		== "C:\\MyEchoGames\\");
	assert(prefs.GetMachinePath("echo", Preferences::machine_path_type::LAST_SAVE_STATE)		== "C:\\MyLastState.sta");
	assert(prefs.GetMachinePath("foxtrot", Preferences::machine_path_type::WORKING_DIRECTORY)	== "");
	assert(prefs.GetMachinePath("foxtrot", Preferences::machine_path_type::LAST_SAVE_STATE)		== "");
}


//-------------------------------------------------
//  path_names
//-------------------------------------------------

static void path_names()
{
	auto iter = std::find(s_path_names.begin(), s_path_names.end(), nullptr);
	assert(iter == s_path_names.end());
	(void)iter;
}


//-------------------------------------------------
//  global_get_path_category
//-------------------------------------------------

static void global_get_path_category()
{
	for (Preferences::global_path_type type : util::all_enums<Preferences::global_path_type>())
		Preferences::GetPathCategory(type);
}


//-------------------------------------------------
//  machine_get_path_category
//-------------------------------------------------

static void machine_get_path_category()
{
	for (Preferences::machine_path_type type : util::all_enums<Preferences::machine_path_type>())
		Preferences::GetPathCategory(type);
}


//-------------------------------------------------
//  substitutions
//-------------------------------------------------

static void substitutions(const char *input, const char *expected)
{
	auto func = [](const QString &var_name)
	{
		return var_name == "VARNAME"
			? "vardata"
			: QString();
	};

	QString actual = InternalApplySubstitutions(QString(input), func);
	assert(actual == expected);
	(void)actual;
	(void)expected;
}


//-------------------------------------------------
//  validity_checks
//-------------------------------------------------

static validity_check validity_checks[] =
{
	general,
	path_names,
	global_get_path_category,
	machine_get_path_category,
	[]() { substitutions("C:\\foo",					"C:\\foo"); },
	[]() { substitutions("C:\\foo (with parens)",	"C:\\foo (with parens)"); },
	[]() { substitutions("C:\\$(VARNAME)\\foo",		"C:\\vardata\\foo"); }
};
