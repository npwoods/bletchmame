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


//**************************************************************************
//  LOCAL VARIABLES
//**************************************************************************

std::array<const char *, static_cast<size_t>(Preferences::global_path_type::COUNT)>	Preferences::s_path_names =
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


static const util::enum_parser_bidirectional<Qt::SortOrder> s_column_sort_type_parser =
{
	{ "ascending", Qt::SortOrder::AscendingOrder, },
	{ "descending", Qt::SortOrder::DescendingOrder }
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
	SetGlobalPath(global_path_type::PROFILES, GetConfigDirectory(true) + QDir::separator() + QString("profiles"));

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
		result = path_category::MULTIPLE_DIRECTORIES;
		break;

	case Preferences::global_path_type::ICONS:
		result = path_category::MULTIPLE_MIXED;
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
	bool isDirectory = category == path_category::SINGLE_DIRECTORY
		|| category == path_category::MULTIPLE_DIRECTORIES;
	if (isDirectory && !path.isEmpty() && !wxFileName::IsPathSeparator(path[path.size() - 1]))
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

QStringList Preferences::GetSplitPaths(global_path_type type) const
{
	const QString &pathsString = GetGlobalPath(type);
	QStringList paths = pathsString.split(';');
	for (QString &path : paths)
	{
		// apply variable substituions
		path = ApplySubstitutions(path);

		// normalize path separators
		path = QDir::fromNativeSeparators(path);
	}
	return paths;
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

	xml.onElementBegin({ "preferences" }, [&](const XmlParser::Attributes &attributes)
	{
		bool menu_bar_shown;
		if (attributes.get("menu_bar_shown", menu_bar_shown))
			SetMenuBarShown(menu_bar_shown);

		list_view_type selected_tab;
		if (attributes.get("selected_tab", selected_tab, s_list_view_type_parser))
			SetSelectedTab(selected_tab);

	});
	xml.onElementBegin({ "preferences", "path" }, [&](const XmlParser::Attributes &attributes)
	{
		QString type_string;
		if (attributes.get("type", type_string))
		{
			auto iter = std::find(s_path_names.cbegin(), s_path_names.cend(), type_string);
			type = iter != s_path_names.cend()
				? static_cast<global_path_type>(iter - s_path_names.cbegin())
				: global_path_type::COUNT;
		}
	});
	xml.onElementEnd({ "preferences", "path" }, [&](QString &&content)
	{
		if (type < global_path_type::COUNT)
			SetGlobalPath(type, std::move(content));
		type = global_path_type::COUNT;
	});
	xml.onElementEnd({ "preferences", "mameextraarguments" }, [&](QString &&content)
	{
		SetMameExtraArguments(std::move(content));
	});
	xml.onElementBegin({ "preferences", "size" }, [&](const XmlParser::Attributes &attributes)
	{
		int width, height;
		if (attributes.get("width", width) && attributes.get("height", height) && IsValidDimension(width) && IsValidDimension(height))
		{
			QSize size;
			size.setWidth(width);
			size.setHeight(height);
			SetSize(size);
		}
	});
	xml.onElementBegin({ "preferences", "selection" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string list_view;
		std::string softlist;
		if (attributes.get("view", list_view))
		{
			attributes.get("softlist", softlist);
			QString key = GetListViewSelectionKey(list_view.c_str(), QString::fromStdString(softlist));
			current_list_view_parameter = &m_list_view_selection[key];
		}
	});
	xml.onElementBegin({ "preferences", "searchboxtext" }, [&](const XmlParser::Attributes &attributes)
	{
		QString list_view;
		if (!attributes.get("view", list_view))
			list_view = "machine";
		current_list_view_parameter = &m_list_view_filter[list_view];
	});
	xml.onElementEnd({{ "preferences", "selection" },
					  { "preferences", "searchboxtext" }}, [&](QString &&content)
	{
		assert(current_list_view_parameter);
		*current_list_view_parameter = std::move(content);
		current_list_view_parameter = nullptr;
	});
	xml.onElementBegin({ "preferences", "column" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string view_type, id;
		if (attributes.get("type", view_type) && attributes.get("id", id))
		{
			ColumnPrefs &col_prefs = m_column_prefs[view_type][id];
			attributes.get("width", col_prefs.m_width);
			attributes.get("order", col_prefs.m_order);
			attributes.get("sort", col_prefs.m_sort, s_column_sort_type_parser);
		}
	});
	xml.onElementBegin({ "preferences", "machine" }, [&](const XmlParser::Attributes &attributes)
	{
		if (!attributes.get("name", current_machine_name))
			return XmlParser::ElementResult::Skip;

		QString path;
		if (attributes.get("working_directory", path))
			SetMachinePath(current_machine_name, machine_path_type::WORKING_DIRECTORY, std::move(path));
		if (attributes.get("last_save_state", path))
			SetMachinePath(current_machine_name, machine_path_type::LAST_SAVE_STATE, std::move(path));
		return XmlParser::ElementResult::Ok;
	});
	xml.onElementBegin({ "preferences", "machine", "device" }, [&](const XmlParser::Attributes &attributes)
	{
		return attributes.get("type", current_device_type)
			? XmlParser::ElementResult::Ok
			: XmlParser::ElementResult::Skip;
	});
	xml.onElementEnd({ "preferences", "machine", "device", "recentfile" }, [&](QString &&content)
	{
		GetRecentDeviceFiles(current_machine_name, current_device_type).push_back(std::move(content));
	});
	bool success = xml.parse(input);

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
			output << "\">" << XmlParser::escape(pair.second) << "</selection>" << std::endl;
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
			output << "\t<machine name=\"" << XmlParser::escape(machine_name) << "\"";
			
			if (!info.m_working_directory.isEmpty())
				output << " working_directory=\"" << XmlParser::escape(info.m_working_directory) << "\"";
			if (!info.m_last_save_state.isEmpty())
				output << " last_save_state=\"" << XmlParser::escape(info.m_last_save_state) << "\"";

			if (info.m_recent_device_files.empty())
			{
				output << "/>" << std::endl;
			}
			else
			{
				output << ">" << std::endl;
				for (const auto &[device_type, recents] : info.m_recent_device_files)
				{
					output << "\t\t<device type=\"" << XmlParser::escape(device_type) << "\">" << std::endl;
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

QString Preferences::InternalApplySubstitutions(const QString &src, std::function<QString(const QString &)> func)
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
	QString::const_iterator var_begin_iter = src.cbegin();

	for (QString::const_iterator iter = src.cbegin(); iter < src.cend(); iter++)
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
				QString var_value = func(var_name);
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
		return QDir::toNativeSeparators(result);
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
			dir.mkpath(".");
	}
	return QDir::toNativeSeparators(directory);
}


//-------------------------------------------------
//  GetDefaultPluginsDirectory
//-------------------------------------------------

static QString GetDefaultPluginsDirectory()
{
	return QDir::toNativeSeparators("$(BLETCHMAMEPATH)/plugins/;$(MAMEPATH)/plugins/");
}
