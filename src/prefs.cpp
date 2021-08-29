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
	"profiles",
	"cheats",
	"snap"
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

static QString getDefaultPluginsDirectory();


//-------------------------------------------------
//  isValidDimension
//-------------------------------------------------

static bool isValidDimension(int dimension)
{
    // arbitrary validation of dimensions
    return dimension >= 10 && dimension <= 20000;
}


//-------------------------------------------------
//  getListViewSelectionKey
//-------------------------------------------------

static QString getListViewSelectionKey(const char *view_type, const QString &softlist)
{
	return QString(view_type) + QString(1, '\0') + softlist;
}


//-------------------------------------------------
//  splitListViewSelectionKey
//-------------------------------------------------

static std::tuple<const QChar *, const QChar *> splitListViewSelectionKey(const QString &key)
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
//  intListFromString
//-------------------------------------------------

static QList<int> intListFromString(const QString &s)
{
	QList<int> result;
	for (const QString &part : s.split(','))
	{
		bool ok = false;
		int i = part.toInt(&ok);
		if (ok)
			result.push_back(i);
	}
	return result;
}


//-------------------------------------------------
//  stringFromIntList
//-------------------------------------------------

static std::string stringFromIntList(const QList<int> &list)
{
	std::string result;
	for (int i : list)
	{
		if (result.size() > 0)
			result += ",";
		result += std::to_string(i);
	}
	return result;
}


//-------------------------------------------------
//  FolderPrefs ctor
//-------------------------------------------------

FolderPrefs::FolderPrefs()
	: m_shown(true)
{
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
	setGlobalPath(global_path_type::CONFIG, getConfigDirectory(true));
	setGlobalPath(global_path_type::NVRAM, getConfigDirectory(true));
	setGlobalPath(global_path_type::PLUGINS, getDefaultPluginsDirectory());
	setGlobalPath(global_path_type::PROFILES, getConfigDirectory(true) + QDir::separator() + QString("profiles"));

    load();
}


//-------------------------------------------------
//  getPathCategory
//-------------------------------------------------

Preferences::path_category Preferences::getPathCategory(global_path_type path_type)
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
	case Preferences::global_path_type::CHEATS:
		result = path_category::MULTIPLE_DIRECTORIES;
		break;

	case Preferences::global_path_type::ICONS:
	case Preferences::global_path_type::SNAPSHOTS:
		result = path_category::MULTIPLE_MIXED;
		break;

	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  getPathCategory
//-------------------------------------------------

Preferences::path_category Preferences::getPathCategory(machine_path_type path_type)
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
//  ensureDirectoryPathsHaveFinalPathSeparator
//-------------------------------------------------

void Preferences::ensureDirectoryPathsHaveFinalPathSeparator(path_category category, QString &path)
{
	bool isDirectory = category == path_category::SINGLE_DIRECTORY
		|| category == path_category::MULTIPLE_DIRECTORIES;
	if (isDirectory && !path.isEmpty() && !wxFileName::IsPathSeparator(path[path.size() - 1]))
	{
		path += QDir::separator();
	}
}


//-------------------------------------------------
//  getMachineInfo
//-------------------------------------------------

const Preferences::MachineInfo *Preferences::getMachineInfo(const QString &machine_name) const
{
	const auto iter = m_machine_info.find(machine_name);
	return iter != m_machine_info.end()
		? &iter->second
		: nullptr;
}


//-------------------------------------------------
//  setGlobalPath
//-------------------------------------------------

void Preferences::setGlobalPath(global_path_type type, QString &&path)
{
	ensureDirectoryPathsHaveFinalPathSeparator(getPathCategory(type), path);
	m_paths[static_cast<size_t>(type)] = std::move(path);
}


void Preferences::setGlobalPath(global_path_type type, const QString &path)
{
	setGlobalPath(type, QString(path));
}


//-------------------------------------------------
//  getSplitPaths
//-------------------------------------------------

QStringList Preferences::getSplitPaths(global_path_type type) const
{
	QStringList paths;

	const QString &pathsString = getGlobalPath(type);
	if (!pathsString.isEmpty())
	{
		paths = pathsString.split(';');
		for (QString &path : paths)
		{
			// apply variable substituions
			path = applySubstitutions(path);

			// normalize path separators
			path = QDir::fromNativeSeparators(path);
		}
	}
	return paths;
}


//-------------------------------------------------
//  getGlobalPathWithSubstitutions
//-------------------------------------------------

QString Preferences::getGlobalPathWithSubstitutions(global_path_type type) const
{
	assert(getPathCategory(type) != path_category::FILE);
	return applySubstitutions(getGlobalPath(type));
}


//-------------------------------------------------
//  getMachinePath
//-------------------------------------------------

const QString &Preferences::getMachinePath(const QString &machine_name, machine_path_type path_type) const
{
	// find the machine path entry
	const MachineInfo *info = getMachineInfo(machine_name);
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
//  getFolderPrefs
//-------------------------------------------------

FolderPrefs Preferences::getFolderPrefs(const QString &folder) const
{
	auto iter = m_folder_prefs.find(folder);
	return iter != m_folder_prefs.end()
		? iter->second
		: FolderPrefs();
}


//-------------------------------------------------
//  setFolderPrefs
//-------------------------------------------------

void Preferences::setFolderPrefs(const QString &folder, FolderPrefs &&prefs)
{
	if (prefs == FolderPrefs())
		m_folder_prefs.erase(folder);
	else
		m_folder_prefs[folder] = std::move(prefs);
}


//-------------------------------------------------
//  getListViewSelection
//-------------------------------------------------

const QString &Preferences::getListViewSelection(const char *view_type, const QString &machine_name) const
{
	QString key = getListViewSelectionKey(view_type, machine_name);
	auto iter = m_list_view_selection.find(key);
	return iter != m_list_view_selection.end()
		? iter->second
		: util::g_empty_string;
}


//-------------------------------------------------
//  setListViewSelection
//-------------------------------------------------

void Preferences::setListViewSelection(const char *view_type, const QString &machine_name, QString &&selection)
{
	QString key = getListViewSelectionKey(view_type, machine_name);
	m_list_view_selection[key] = std::move(selection);
}


//-------------------------------------------------
//  setMachinePath
//-------------------------------------------------

void Preferences::setMachinePath(const QString &machine_name, machine_path_type path_type, QString &&path)
{
	// ensure that if we have a path, it has a path separator at the end
	ensureDirectoryPathsHaveFinalPathSeparator(getPathCategory(path_type), path);

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
//  getRecentDeviceFiles
//-------------------------------------------------

std::vector<QString> &Preferences::getRecentDeviceFiles(const QString &machine_name, const QString &device_type)
{
	return m_machine_info[machine_name].m_recent_device_files[device_type];
}


const std::vector<QString> &Preferences::getRecentDeviceFiles(const QString &machine_name, const QString &device_type) const
{
	static const std::vector<QString> empty_vector;
	const MachineInfo *info = getMachineInfo(machine_name);
	if (!info)
		return empty_vector;

	auto iter = info->m_recent_device_files.find(device_type);
	if (iter == info->m_recent_device_files.end())
		return empty_vector;

	return iter->second;
}


//-------------------------------------------------
//  load
//-------------------------------------------------

bool Preferences::load()
{
	using namespace std::placeholders;

	QString fileName = getFileName(false);

	// first check to see if the file exists
	bool success = false;
	if (QFileInfo(fileName).exists())
	{
		QFile file(fileName);
		if (file.open(QFile::ReadOnly))
			success = load(file);
	}
	return success;
}


//-------------------------------------------------
//  load
//-------------------------------------------------

bool Preferences::load(QIODevice &input)
{
	XmlParser xml;
	global_path_type type = global_path_type::COUNT;
	QString current_machine_name;
	QString current_device_type;
	QString *current_list_view_parameter = nullptr;
	std::set<QString> *current_custom_folder = nullptr;

	// clear out state
	m_machine_info.clear();
	m_custom_folders.clear();

	xml.onElementBegin({ "preferences" }, [&](const XmlParser::Attributes &attributes)
	{
		std::optional<bool> menu_bar_shown = attributes.get<bool>("menu_bar_shown");
		if (menu_bar_shown)
			setMenuBarShown(*menu_bar_shown);

		std::optional<list_view_type> selected_tab = attributes.get<list_view_type>("selected_tab", s_list_view_type_parser);
		if (selected_tab)
			setSelectedTab(*selected_tab);
	});
	xml.onElementBegin({ "preferences", "path" }, [&](const XmlParser::Attributes &attributes)
	{
		std::optional<QString> type_string = attributes.get<QString>("type");
		if (type_string)
		{
			auto iter = std::find(s_path_names.cbegin(), s_path_names.cend(), *type_string);
			type = iter != s_path_names.cend()
				? static_cast<global_path_type>(iter - s_path_names.cbegin())
				: global_path_type::COUNT;
		}
	});
	xml.onElementEnd({ "preferences", "path" }, [&](QString &&content)
	{
		if (type < global_path_type::COUNT)
			setGlobalPath(type, std::move(content));
		type = global_path_type::COUNT;
	});
	xml.onElementEnd({ "preferences", "mameextraarguments" }, [&](QString &&content)
	{
		setMameExtraArguments(std::move(content));
	});
	xml.onElementBegin({ "preferences", "size" }, [&](const XmlParser::Attributes &attributes)
	{
		std::optional<int> width = attributes.get<int>("width");
		std::optional<int> height = attributes.get<int>("height");

		if (width && height && isValidDimension(*width) && isValidDimension(*height))
		{
			QSize size;
			size.setWidth(*width);
			size.setHeight(*height);
			setSize(size);
		}
	});	
	xml.onElementEnd({ "preferences", "machinelistsplitters" }, [&](QString &&content)
	{
		QList<int> splitterSizes = intListFromString(content);
		if (!splitterSizes.isEmpty())
			setMachineSplitterSizes(std::move(splitterSizes));
	});
	xml.onElementBegin({ "preferences", "folder" }, [&](const XmlParser::Attributes &attributes)
	{
		std::optional<QString> id = attributes.get<QString>("id");
		if (id)
		{
			FolderPrefs folderPrefs = getFolderPrefs(*id);
			std::optional<bool> isShown = attributes.get<bool>("shown");
			if (isShown)
				folderPrefs.m_shown = *isShown;
			setFolderPrefs(*id, std::move(folderPrefs));
			
			if (attributes.get<bool>("selected") == true)
				setMachineFolderTreeSelection(std::move(id.value()));
		}
	});
	xml.onElementBegin({ "preferences", "customfolder" }, [&](const XmlParser::Attributes &attributes)
	{
		std::optional<QString> name = attributes.get<QString>("name");
		if (name)
			current_custom_folder = &m_custom_folders.emplace(name.value(), std::set<QString>()).first->second;
	});
	xml.onElementEnd({ "preferences", "customfolder" }, [&](QString &&content)
	{
		current_custom_folder = nullptr;
	});
	xml.onElementEnd({ "preferences", "customfolder", "system" }, [&](QString &&content)
	{
		if (current_custom_folder)
			current_custom_folder->emplace(std::move(content));
	});
	xml.onElementBegin({ "preferences", "selection" }, [&](const XmlParser::Attributes &attributes)
	{
		std::optional<std::string> list_view = attributes.get<std::string>("view");
		if (list_view)
		{
			std::string softlist = attributes.get<std::string>("softlist").value_or("");
			QString key = getListViewSelectionKey(list_view->c_str(), QString::fromStdString(softlist));
			current_list_view_parameter = &m_list_view_selection[key];
		}
	});
	xml.onElementBegin({ "preferences", "searchboxtext" }, [&](const XmlParser::Attributes &attributes)
	{
		QString list_view = attributes.get<QString>("view").value_or("machine");
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
		std::optional<std::string> view_type = attributes.get<std::string>("type");
		std::optional<std::string> id = attributes.get<std::string>("id");
		if (view_type && id)
		{
			ColumnPrefs &col_prefs = m_column_prefs[*view_type][*id];
			col_prefs.m_width = attributes.get<int>("width").value_or(col_prefs.m_width);
			col_prefs.m_order = attributes.get<int>("order").value_or(col_prefs.m_order);
			col_prefs.m_sort = attributes.get<Qt::SortOrder>("sort", s_column_sort_type_parser);
		}
	});
	xml.onElementBegin({ "preferences", "machine" }, [&](const XmlParser::Attributes &attributes)
	{
		std::optional<QString> name = attributes.get<QString>("name");
		if (!name)
			return XmlParser::ElementResult::Skip;
		current_machine_name = *name;

		std::optional<QString> workingDirectory = attributes.get<QString>("working_directory");
		if (workingDirectory)
			setMachinePath(current_machine_name, machine_path_type::WORKING_DIRECTORY, std::move(*workingDirectory));

		std::optional<QString> lastSaveState = attributes.get<QString>("last_save_state");
		if (lastSaveState)
			setMachinePath(current_machine_name, machine_path_type::LAST_SAVE_STATE, std::move(*lastSaveState));

		return XmlParser::ElementResult::Ok;
	});
	xml.onElementBegin({ "preferences", "machine", "device" }, [&](const XmlParser::Attributes &attributes)
	{
		std::optional<QString> type = attributes.get<QString>("type");
		if (!type)
			return XmlParser::ElementResult::Skip;

		current_device_type = std::move(*type);
		return XmlParser::ElementResult::Ok;
	});
	xml.onElementEnd({ "preferences", "machine", "device", "recentfile" }, [&](QString &&content)
	{
		getRecentDeviceFiles(current_machine_name, current_device_type).push_back(std::move(content));
	});
	bool success = xml.parse(input);

	return success;
}


//-------------------------------------------------
//  save
//-------------------------------------------------

void Preferences::save()
{
	QString file_name = getFileName(true);
	std::ofstream output(file_name.toStdString(), std::ios_base::out);
	save(output);
}


//-------------------------------------------------
//  save
//-------------------------------------------------

void Preferences::save(std::ostream &output)
{
	output << "<!-- Preferences for BletchMAME -->" << std::endl;
	output << "<preferences menu_bar_shown=\"" << (m_menu_bar_shown ? "1" : "0") << "\" selected_tab=\"" << s_list_view_type_parser[getSelectedTab()] << "\">" << std::endl;
	output << std::endl;

	output << "\t<!-- Paths -->" << std::endl;
	for (size_t i = 0; i < m_paths.size(); i++)
		output << "\t<path type=\"" << s_path_names[i] << "\">" << XmlParser::escape(getGlobalPath(static_cast<global_path_type>(i))) << "</path>" << std::endl;
	output << std::endl;

	output << "\t<!-- Miscellaneous -->" << std::endl;
	if (!m_mame_extra_arguments.isEmpty())
		output << "\t<mameextraarguments>" << XmlParser::escape(m_mame_extra_arguments) << "</mameextraarguments>" << std::endl;
	output << "\t<size width=\"" << m_size.width() << "\" height=\"" << m_size.height() << "\"/>" << std::endl;
	if (!m_machine_splitter_sizes.isEmpty())
		output << "\t<machinelistsplitters>" << stringFromIntList(m_machine_splitter_sizes) << "</machinelistsplitters>" << std::endl;

	// folder prefs
	if (!m_machine_folder_tree_selection.isEmpty() && m_folder_prefs.find(m_machine_folder_tree_selection) == m_folder_prefs.end())
		m_folder_prefs.emplace(m_machine_folder_tree_selection, FolderPrefs());
	for (const auto &pair : m_folder_prefs)
	{
		output << "\t<folder id=\"" << XmlParser::escape(pair.first) << '\"'
			<< " shown=\"" << (pair.second.m_shown ? "true" : "false") << '\"'
			<< (pair.first == m_machine_folder_tree_selection ? " selected=\"true\"" : "")
			<< "/>" << std::endl;
	}

	// custom folders
	for (const auto &pair : m_custom_folders)
	{
		output << "\t<customfolder name=\"" << XmlParser::escape(pair.first) << "\">" << std::endl;
		for (const QString &system : pair.second)
			output << "\t\t<system>" << XmlParser::escape(system) << "</system>" << std::endl;
		output << "\t</customfolder>" << std::endl;
	}

	for (const auto &pair : m_list_view_selection)
	{
		if (!pair.second.isEmpty())
		{
			auto [view_type, softlist] = splitListViewSelectionKey(pair.first);
			output << "\t<selection view=\"" << XmlParser::escape(QString(view_type));
			if (softlist)
				output << "\" softlist=\"" + XmlParser::escape(QString(softlist));
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
//  internalApplySubstitutions
//-------------------------------------------------

QString Preferences::internalApplySubstitutions(const QString &src, std::function<QString(const QString &)> func)
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
//  applySubstitutions
//-------------------------------------------------

QString Preferences::applySubstitutions(const QString &path) const
{
	return internalApplySubstitutions(path, [this](const QString &var_name)
	{
		QString result;
		if (var_name == "MAMEPATH")
		{
			const QString &path = getGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);
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
//  getMameXmlDatabasePath
//-------------------------------------------------

QString Preferences::getMameXmlDatabasePath(bool ensure_directory_exists) const
{
	// get the configuration directory
	QString config_dir = getConfigDirectory(ensure_directory_exists);
	if (config_dir.isEmpty())
		return "";

	// get the MAME path
	const QString &mame_path = getGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);
	if (mame_path.isEmpty())
		return "";

	// parse out the MAME filename
	QString mame_filename;
	wxFileName::SplitPath(mame_path, nullptr, &mame_filename, nullptr);

	// get the full name
	return QDir(config_dir).filePath(mame_filename + ".infodb");
}


//-------------------------------------------------
//  getFileName
//-------------------------------------------------

QString Preferences::getFileName(bool ensure_directory_exists)
{
	QString directory = getConfigDirectory(ensure_directory_exists);
	return QDir(directory).filePath("BletchMAME.xml");
}


//-------------------------------------------------
//  getConfigDirectory - gets the configuration
//	directory, and optionally ensuring it exists
//-------------------------------------------------

QString Preferences::getConfigDirectory(bool ensure_directory_exists)
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
//  getDefaultPluginsDirectory
//-------------------------------------------------

static QString getDefaultPluginsDirectory()
{
	return QDir::toNativeSeparators("$(BLETCHMAMEPATH)/plugins/;$(MAMEPATH)/plugins/");
}
