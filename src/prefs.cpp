/***************************************************************************

    prefs.cpp

    Wrapper for preferences specific to BletchMAME

***************************************************************************/

#include <fstream>
#include <functional>
#include <locale>

#include <QDir>
#include <QCoreApplication>
#include <QXmlStreamWriter>

#include "prefs.h"
#include "utility.h"
#include "xmlparser.h"


//**************************************************************************
//  LOCAL VARIABLES
//**************************************************************************

std::array<const char *, util::enum_count<Preferences::global_path_type>()>	Preferences::s_path_names =
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


static const util::enum_parser_bidirectional<Preferences::WindowState> s_windowState_parser =
{
	{ "normal", Preferences::WindowState::Normal, },
	{ "maximized", Preferences::WindowState::Maximized, },
	{ "fullscreen", Preferences::WindowState::FullScreen }
};


static const util::enum_parser_bidirectional<Preferences::list_view_type> s_list_view_type_parser =
{
	{ "machine", Preferences::list_view_type::MACHINE, },
	{ "softwarelist", Preferences::list_view_type::SOFTWARELIST, },
	{ "profile", Preferences::list_view_type::PROFILE }
};


static const util::enum_parser_bidirectional<AuditStatus> s_audit_status_parser =
{
	{ "unknown", AuditStatus::Unknown, },
	{ "found", AuditStatus::Found, },
	{ "missing", AuditStatus::Missing },
	{ "missingoptional", AuditStatus::MissingOptional }
};


static const util::enum_parser_bidirectional<Preferences::AuditingState> s_auditingStateParser =
{
	{ "disabled", Preferences::AuditingState::Disabled, },
	{ "automatic", Preferences::AuditingState::Automatic, },
	{ "manual", Preferences::AuditingState::Manual }
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

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

static QString getListViewSelectionKey(std::u8string_view view_type, const QString &softlist)
{
	return util::toQString(view_type) + QString(1, '\0') + softlist;
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

static QString stringFromIntList(const QList<int> &list)
{
	QString result;
	for (int i : list)
	{
		if (result.size() > 0)
			result += ",";
		result += QString::number(i);
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

Preferences::Preferences(std::optional<QDir> &&configDirectory, QObject *parent)
	: QObject(parent)
	, m_configDirectory(std::move(configDirectory))
	, m_globalPathsInfo(m_configDirectory)
	, m_windowState(WindowState::Normal)
	, m_selected_tab(list_view_type::MACHINE)
{
}


//-------------------------------------------------
//  resetToDefaults
//-------------------------------------------------

void Preferences::resetToDefaults(bool resetUi, bool resetPaths)
{
	if (resetUi)
		setGlobalInfo(GlobalUiInfo());
	if (resetPaths)
		setGlobalInfo(GlobalPathsInfo(m_configDirectory));
}


//-------------------------------------------------
//  getPathCategory
//-------------------------------------------------

Preferences::PathCategory Preferences::getPathCategory(global_path_type path_type)
{
	PathCategory result;
	switch (path_type)
	{
	case Preferences::global_path_type::EMU_EXECUTABLE:
		result = PathCategory::SingleFile;
		break;

	case Preferences::global_path_type::CONFIG:
	case Preferences::global_path_type::NVRAM:
		result = PathCategory::SingleDirectory;
		break;

	case Preferences::global_path_type::ROMS:
	case Preferences::global_path_type::SAMPLES:
	case Preferences::global_path_type::HASH:
	case Preferences::global_path_type::ARTWORK:
	case Preferences::global_path_type::PLUGINS:
	case Preferences::global_path_type::PROFILES:
	case Preferences::global_path_type::CHEATS:
		result = PathCategory::MultipleDirectories;
		break;

	case Preferences::global_path_type::ICONS:
	case Preferences::global_path_type::SNAPSHOTS:
		result = PathCategory::MultipleDirectoriesOrArchives;
		break;

	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  getPathCategory
//-------------------------------------------------

Preferences::PathCategory Preferences::getPathCategory(machine_path_type path_type)
{
	PathCategory result;
	switch (path_type)
	{
	case Preferences::machine_path_type::LAST_SAVE_STATE:
		result = PathCategory::SingleFile;
		break;

	case Preferences::machine_path_type::WORKING_DIRECTORY:
		result = PathCategory::SingleDirectory;
		break;

	default:
		throw false;
	}
	return result;
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
//  setGlobalInfo
//-------------------------------------------------

void Preferences::setGlobalInfo(Preferences::GlobalUiInfo &&globalInfo)
{
	setMenuBarShown(globalInfo.m_menuBarShown);
	setAuditingState(globalInfo.m_auditingState);
}


//-------------------------------------------------
//  setGlobalInfo
//-------------------------------------------------

void Preferences::setGlobalInfo(Preferences::GlobalPathsInfo &&globalInfo)
{
	for (global_path_type type : util::all_enums<global_path_type>())
		setGlobalPath(type, std::move(globalInfo.m_paths[static_cast<size_t>(type)]));
}


//-------------------------------------------------
//  setGlobalPath
//-------------------------------------------------

void Preferences::setGlobalPath(global_path_type type, QString &&path)
{
	if (m_globalPathsInfo.m_paths[static_cast<size_t>(type)] != path)
		internalSetGlobalPath(type, std::move(path));
}


//-------------------------------------------------
//  setGlobalPath
//-------------------------------------------------

void Preferences::setGlobalPath(global_path_type type, const QString &path)
{
	if (m_globalPathsInfo.m_paths[static_cast<size_t>(type)] != path)
		internalSetGlobalPath(type, QString(path));
}


//-------------------------------------------------
//  internalSetGlobalPath
//-------------------------------------------------

void Preferences::internalSetGlobalPath(global_path_type type, QString &&path)
{
	// find the destination, and this had better be a real change
	QString &dest = m_globalPathsInfo.m_paths[static_cast<size_t>(type)];
	assert(dest != path);

	// copy the data
	dest = std::move(path);

	// and raise the event
	switch (type)
	{
	case global_path_type::EMU_EXECUTABLE:
		emit globalPathEmuExecutableChanged(dest);
		break;
	case global_path_type::ROMS:
		emit globalPathRomsChanged(dest);
		break;
	case global_path_type::SAMPLES:
		emit globalPathSamplesChanged(dest);
		break;
	case global_path_type::ICONS:
		emit globalPathIconsChanged(dest);
		break;
	case global_path_type::PROFILES:
		emit globalPathProfilesChanged(dest);
		break;
	case global_path_type::SNAPSHOTS:
		emit globalPathSnapshotsChanged(dest);
		break;
	default:
		// do nothing
		break;
	}
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
	assert(getPathCategory(type) != PathCategory::SingleFile);
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
		return info->m_workingDirectory;
	case machine_path_type::LAST_SAVE_STATE:
		return info->m_lastSaveState;
	default:
		throw false;
	}
}


//-------------------------------------------------
//  getFolderPrefs
//-------------------------------------------------

FolderPrefs Preferences::getFolderPrefs(const QString &folder) const
{
	auto iter = m_folderPrefs.find(folder);
	return iter != m_folderPrefs.end()
		? iter->second
		: FolderPrefs();
}


//-------------------------------------------------
//  setFolderPrefs
//-------------------------------------------------

void Preferences::setFolderPrefs(const QString &folder, FolderPrefs &&prefs)
{
	auto iter = m_folderPrefs.find(folder);
	bool isSame = (prefs == FolderPrefs())
		? iter == m_folderPrefs.end()
		: iter != m_folderPrefs.end() && iter->second == prefs;

	if (!isSame)
	{
		if (prefs == FolderPrefs())
			m_folderPrefs.erase(folder);
		else
			m_folderPrefs[folder] = std::move(prefs);

		emit folderPrefsChanged();
	}
}


//-------------------------------------------------
//  addMachineToCustomFolder
//-------------------------------------------------

bool Preferences::addMachineToCustomFolder(const QString &customFolderName, const QString &machineName)
{
	// access the set of custom folders - this will create an entry if necessary
	std::set<QString> &customFolderMachines = m_customFolders[customFolderName];

	// is this machine present?
	bool result = !customFolderMachines.contains(machineName);
	if (result)
	{
		// if not, add it
		customFolderMachines.insert(machineName);
		emit customFoldersChanged();
	}
	return result;
}


//-------------------------------------------------
//  removeMachineFromCustomFolder
//-------------------------------------------------

bool Preferences::removeMachineFromCustomFolder(const QString &customFolderName, const QString &machineName)
{
	// find the folder
	auto folderIter = m_customFolders.find(customFolderName);
	if (folderIter == m_customFolders.end())
		return false;

	// find the machine
	auto machineIter = folderIter->second.find(machineName);
	if (machineIter == folderIter->second.end())
		return false;

	// remove it, fire the event and we're done
	folderIter->second.erase(machineIter);
	emit customFoldersChanged();
	return true;
}


//-------------------------------------------------
//  renameCustomFolder
//-------------------------------------------------

bool Preferences::renameCustomFolder(const QString &oldCustomFolderName, QString &&newCustomFolderName)
{
	// renaming the folder to itself is silly
	if (oldCustomFolderName == newCustomFolderName)
		return false;

	// find this entry
	auto iter = m_customFolders.find(oldCustomFolderName);
	if (iter == m_customFolders.end())
		return false;

	// detatch the list of machines
	std::set<QString> machines = std::move(iter->second);
	m_customFolders.erase(iter);

	// and readd it
	m_customFolders.emplace(std::move(newCustomFolderName), std::move(machines));

	// fire the event and we're done
	emit customFoldersChanged();
	return true;
}


//-------------------------------------------------
//  deleteCustomFolder
//-------------------------------------------------

bool Preferences::deleteCustomFolder(const QString &customFolderName)
{
	// perform the erase
	bool result = m_customFolders.erase(customFolderName) > 0;

	// fire the event if necessary
	if (result)
		emit customFoldersChanged();

	// and we're done
	return result;
}


//-------------------------------------------------
//  getListViewSelection
//-------------------------------------------------

const QString &Preferences::getListViewSelection(const char8_t *view_type, const QString &machine_name) const
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

void Preferences::setListViewSelection(const char8_t *view_type, const QString &machine_name, QString &&selection)
{
	QString key = getListViewSelectionKey(view_type, machine_name);
	m_list_view_selection[key] = std::move(selection);
}


//-------------------------------------------------
//  setMachinePath
//-------------------------------------------------

void Preferences::setMachinePath(const QString &machine_name, machine_path_type path_type, QString &&path)
{
	switch (path_type)
	{
	case machine_path_type::WORKING_DIRECTORY:
		m_machine_info[machine_name].m_workingDirectory = std::move(path);
		break;
	case machine_path_type::LAST_SAVE_STATE:
		m_machine_info[machine_name].m_lastSaveState = std::move(path);
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
	return m_machine_info[machine_name].m_recentDeviceFiles[device_type];
}


const std::vector<QString> &Preferences::getRecentDeviceFiles(const QString &machine_name, const QString &device_type) const
{
	static const std::vector<QString> empty_vector;
	const MachineInfo *info = getMachineInfo(machine_name);
	if (!info)
		return empty_vector;

	auto iter = info->m_recentDeviceFiles.find(device_type);
	if (iter == info->m_recentDeviceFiles.end())
		return empty_vector;

	return iter->second;
}


//-------------------------------------------------
//  setSelectedTab
//-------------------------------------------------

void Preferences::setSelectedTab(list_view_type type)
{
	if (m_selected_tab != type)
	{
		m_selected_tab = type;
		emit selectedTabChanged(m_selected_tab);
	}
}



//-------------------------------------------------
//  setAuditingState
//-------------------------------------------------

void Preferences::setAuditingState(AuditingState auditingState)
{
	if (m_globalUiInfo.m_auditingState != auditingState)
	{
		m_globalUiInfo.m_auditingState = auditingState;
		emit auditingStateChanged();
	}
}


//-------------------------------------------------
//  getMachineAuditStatus
//-------------------------------------------------

AuditStatus Preferences::getMachineAuditStatus(const QString &machine_name) const
{
	AuditStatus result = AuditStatus::Unknown;

	auto iter = m_machine_info.find(machine_name);
	if (iter != m_machine_info.end())
		result = iter->second.m_auditStatus;

	return result;
}


//-------------------------------------------------
//  setMachineAuditStatus
//-------------------------------------------------

void Preferences::setMachineAuditStatus(const QString &machine_name, AuditStatus status)
{
	assert(status == AuditStatus::Unknown || status == AuditStatus::Found
		|| status == AuditStatus::MissingOptional || status == AuditStatus::Missing);
	m_machine_info[machine_name].m_auditStatus = status;
}


//-------------------------------------------------
//  bulkDropMachineAuditStatuses
//-------------------------------------------------

void Preferences::bulkDropMachineAuditStatuses(const std::function<bool(const QString &machineName)> &predicate)
{
	int count = 0;

	// drop all statuses
	for (auto &[machineName, info] : m_machine_info)
	{
		if ((!predicate || predicate(machineName)) && info.m_auditStatus != AuditStatus::Unknown)
		{
			info.m_auditStatus = AuditStatus::Unknown;
			count++;
		}
	}

	// did we drop anything?
	if (count > 0)
	{
		// if so its highly likely we have data worthy of garbage collection
		garbageCollectMachineInfo();

		// and emit the event
		emit bulkDroppedMachineAuditStatuses();
	}
}


//-------------------------------------------------
//  getSoftwareAuditStatus
//-------------------------------------------------

AuditStatus Preferences::getSoftwareAuditStatus(const QString &softwareList, const QString &software) const
{
	auto key = std::make_tuple(softwareList, software);
	auto iter = m_softwareAuditStatus.find(key);
	return iter != m_softwareAuditStatus.end()
		? iter->second
		: AuditStatus::Unknown;
}


//-------------------------------------------------
//  setSoftwareAuditStatus
//-------------------------------------------------

void Preferences::setSoftwareAuditStatus(const QString &softwareList, const QString &software, AuditStatus status)
{
	auto key = std::make_tuple(softwareList, software);
	if (status != AuditStatus::Unknown)
		m_softwareAuditStatus[key] = status;
	else
		m_softwareAuditStatus.erase(key);
}


//-------------------------------------------------
//  bulkDropSoftwareAuditStatuses
//-------------------------------------------------

void Preferences::bulkDropSoftwareAuditStatuses()
{
	// do we have any audit statuses?
	if (!m_softwareAuditStatus.empty())
	{
		// if so drop them
		m_softwareAuditStatus.clear();

		// and emit the event
		emit bulkDroppedSoftwareAuditStatuses();
	}
}


//-------------------------------------------------
//  garbageCollectMachineInfo
//-------------------------------------------------

void Preferences::garbageCollectMachineInfo()
{
	// remove all items from m_machine_info with default data
	std::erase_if(m_machine_info, [](const auto &item)
	{
		const auto &[key, value] = item;
		return value == MachineInfo();
	});
}


//-------------------------------------------------
//  load
//-------------------------------------------------

bool Preferences::load()
{
	using namespace std::placeholders;

	QString fileName = getPreferencesFileName(false);

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
	std::optional<global_path_type> type;
	QString current_machine_name;
	QString current_device_type;
	QString *current_list_view_parameter = nullptr;
	std::set<QString> *current_custom_folder = nullptr;

	// clear out state
	m_windowState = WindowState::Normal;
	m_machine_info.clear();
	m_softwareAuditStatus.clear();
	m_customFolders.clear();

	// set up fresh global state
	GlobalUiInfo globalUiInfo;
	GlobalPathsInfo globalPathsInfo(m_configDirectory);

	xml.onElementBegin({ "preferences" }, [&](const XmlParser::Attributes &attributes)
	{
		std::optional<bool> menuBarShown = attributes.get<bool>("menu_bar_shown");
		if (menuBarShown)
			globalUiInfo.m_menuBarShown = menuBarShown.value();

		std::optional<WindowState> windowState = attributes.get<WindowState>("window_state", s_windowState_parser);
		if (windowState)
			setWindowState(*windowState);

		std::optional<list_view_type> selected_tab = attributes.get<list_view_type>("selected_tab", s_list_view_type_parser);
		if (selected_tab)
			setSelectedTab(*selected_tab);

		std::optional<AuditingState> auditingState = attributes.get<AuditingState>("auditing", s_auditingStateParser);
		if (auditingState)
			globalUiInfo.m_auditingState = auditingState.value();
	});
	xml.onElementBegin({ "preferences", "path" }, [&](const XmlParser::Attributes &attributes)
	{
		std::optional<QString> type_string = attributes.get<QString>("type");
		if (type_string)
		{
			auto iter = std::find(s_path_names.cbegin(), s_path_names.cend(), *type_string);
			type.reset();
			if (iter != s_path_names.cend())
				type = static_cast<global_path_type>(iter - s_path_names.cbegin());
		}
	});
	xml.onElementEnd({ "preferences", "path" }, [&](QString &&content)
	{
		if (type)
			globalPathsInfo.m_paths[(size_t) type.value()] = std::move(content);
		type.reset();
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
			current_custom_folder = &m_customFolders.emplace(name.value(), std::set<QString>()).first->second;
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
		std::optional<std::u8string_view> list_view = attributes.get<std::u8string_view>("view");
		if (list_view)
		{
			QString softlist = attributes.get<QString>("softlist").value_or("");
			QString key = getListViewSelectionKey(*list_view, std::move(softlist));
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
		std::optional<std::u8string_view> view_type = attributes.get<std::u8string_view>("type");
		std::optional<std::u8string_view> id = attributes.get<std::u8string_view>("id");
		if (view_type && id)
		{
			ColumnPrefs &col_prefs = m_column_prefs[std::u8string(*view_type)][std::u8string(*id)];
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

		AuditStatus status = attributes.get<AuditStatus>("audit_status", s_audit_status_parser).value_or(AuditStatus::Unknown);
		setMachineAuditStatus(current_machine_name, status);

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
	xml.onElementBegin({ "preferences", "software" }, [&](const XmlParser::Attributes &attributes)
	{
		std::optional<QString> list = attributes.get<QString>("list");
		std::optional<QString> name = attributes.get<QString>("name");
		if (list && name)
		{
			AuditStatus status = attributes.get<AuditStatus>("audit_status", s_audit_status_parser).value_or(AuditStatus::Unknown);
			setSoftwareAuditStatus(list.value(), name.value(), status);
		}
	});
	bool success = xml.parse(input);

	// if we're successful, update global state
	if (success)
	{
		setGlobalInfo(std::move(globalUiInfo));
		setGlobalInfo(std::move(globalPathsInfo));
	}

	return success;
}


//-------------------------------------------------
//  save
//-------------------------------------------------

void Preferences::save()
{
	QString fileName = getPreferencesFileName(true);
	QFile file(fileName);
	if (file.open(QIODevice::WriteOnly | QIODevice::Text))
		save(file);
}


//-------------------------------------------------
//  save
//-------------------------------------------------

void Preferences::save(QIODevice &output)
{
	QXmlStreamWriter writer(&output);
	writer.setAutoFormatting(true);

	writer.writeStartDocument();
	writer.writeComment("Preferences for BletchMAME");
	writer.writeStartElement("preferences");
	writer.writeAttribute("menu_bar_shown", QString::number(getMenuBarShown() ? 1 : 0));
	writer.writeAttribute("window_state", s_windowState_parser[getWindowState()]);
	writer.writeAttribute("selected_tab", s_list_view_type_parser[getSelectedTab()]);
	writer.writeAttribute("auditing", s_auditingStateParser[getAuditingState()]);

	// paths
	writer.writeComment("Paths");
	for (global_path_type pathType : util::all_enums<global_path_type>())
	{
		writer.writeStartElement("path");
		writer.writeAttribute("type", s_path_names[(size_t)pathType]);
		writer.writeCharacters(getGlobalPath(pathType));
		writer.writeEndElement();
	}

	writer.writeComment("Miscellaneous");
	if (!m_mame_extra_arguments.isEmpty())
		writer.writeTextElement("mameextraarguments", m_mame_extra_arguments);
	if (m_size)
	{
		writer.writeStartElement("size");
		writer.writeAttribute("width", QString::number(m_size->width()));
		writer.writeAttribute("height", QString::number(m_size->height()));
		writer.writeEndElement();
	}
	if (!m_machine_splitter_sizes.isEmpty())
		writer.writeTextElement("machinelistsplitters", stringFromIntList(m_machine_splitter_sizes));

	// folder prefs
	if (!m_machine_folder_tree_selection.isEmpty() && m_folderPrefs.find(m_machine_folder_tree_selection) == m_folderPrefs.end())
		m_folderPrefs.emplace(m_machine_folder_tree_selection, FolderPrefs());
	for (const auto &pair : m_folderPrefs)
	{
		writer.writeStartElement("folder");
		writer.writeAttribute("id", pair.first);
		writer.writeAttribute("shown", pair.second.m_shown ? "true" : "false");
		if (pair.first == m_machine_folder_tree_selection)
			writer.writeAttribute("selected", "true");
		writer.writeEndElement();
	}

	// custom folders
	for (const auto &pair : m_customFolders)
	{
		writer.writeStartElement("customfolder");
		writer.writeAttribute("name", pair.first);
		for (const QString &system : pair.second)
			writer.writeTextElement("system", system);
		writer.writeEndElement();
	}

	// list view selection
	for (const auto &pair : m_list_view_selection)
	{
		if (!pair.second.isEmpty())
		{
			auto [view_type, softlist] = splitListViewSelectionKey(pair.first);
			writer.writeStartElement("selection");
			writer.writeAttribute("view", QString(view_type));
			if (softlist)
				writer.writeAttribute("softlist", QString("softlist"));
			writer.writeCharacters(pair.second);
			writer.writeEndElement();
		}
	}

	// list view filter
	for (const auto &[view_type, text] : m_list_view_filter)
	{
		if (!text.isEmpty())
		{
			writer.writeStartElement("searchboxtext");
			writer.writeAttribute("view", view_type);
			writer.writeCharacters(text);
			writer.writeEndElement();
		}
	}

	// column width/order
	for (const auto &view_prefs : m_column_prefs)
	{
		for (const auto &col_prefs : view_prefs.second)
		{
			writer.writeStartElement("column");
			writer.writeAttribute("type", util::toQString(view_prefs.first));
			writer.writeAttribute("id", util::toQString(col_prefs.first));
			writer.writeAttribute("width", QString::number(col_prefs.second.m_width));
			writer.writeAttribute("order", QString::number(col_prefs.second.m_order));
			if (col_prefs.second.m_sort.has_value())
				writer.writeAttribute("sort", s_column_sort_type_parser[col_prefs.second.m_sort.value()]);
			writer.writeEndElement();
		}
	}

	// machines
	writer.writeComment("Machines");
	for (const auto &[machineName, info] : m_machine_info)
	{
		// only write this info out if the data is-non default
		if (info != MachineInfo())
		{
			writer.writeStartElement("machine");
			writer.writeAttribute("name", machineName);

			if (!info.m_workingDirectory.isEmpty())
				writer.writeAttribute("working_directory", info.m_workingDirectory);
			if (!info.m_lastSaveState.isEmpty())
				writer.writeAttribute("last_save_state", info.m_lastSaveState);
			if (info.m_auditStatus != AuditStatus::Unknown)
				writer.writeAttribute("audit_status", s_audit_status_parser[info.m_auditStatus]);

			if (!info.m_recentDeviceFiles.empty())
			{
				for (const auto &[device_type, recents] : info.m_recentDeviceFiles)
				{
					writer.writeStartElement("device");
					writer.writeAttribute("type", device_type);
					for (const QString &recent : recents)
						writer.writeTextElement("recentfile", recent);
					writer.writeEndElement();
				}
			}

			writer.writeEndElement();
		}
	}

	// software list audits
	if (!m_softwareAuditStatus.empty())
	{
		writer.writeComment("Software");
		for (const auto & [ident, auditStatus] : m_softwareAuditStatus)
		{
			const QString &softwareList = std::get<0>(ident);
			const QString &software = std::get<1>(ident);

			writer.writeStartElement("software");
			writer.writeAttribute("list", softwareList);
			writer.writeAttribute("name", software);
			if (auditStatus != AuditStatus::Unknown)
				writer.writeAttribute("audit_status", s_audit_status_parser[auditStatus]);
			writer.writeEndElement();
		}
	}
	
	writer.writeEndElement();
	writer.writeEndDocument();
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
			QFileInfo fileInfo(QDir::fromNativeSeparators(path));
			result = fileInfo.dir().absolutePath();
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
	// do we have a config directory?
	if (!m_configDirectory)
		return "";

	// get the emulator executable (ie.- MAME) path
	const QString &emuExecutablePath = getGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);
	if (emuExecutablePath.isEmpty())
		return "";

	// parse out the MAME base name (no file extension)
	QString emuExecutableBaseName = QFileInfo(emuExecutablePath).baseName();

	// get the full name
	return m_configDirectory->filePath(emuExecutableBaseName + ".infodb");
}


//-------------------------------------------------
//  getPreferencesFileName
//-------------------------------------------------

QString Preferences::getPreferencesFileName(bool ensureDirectoryExists) const
{
	// its illegal to call this if we don't have a config directory specified
	assert(m_configDirectory);

	// if appropriate, ensure the directory is present
	if (ensureDirectoryExists && !m_configDirectory->exists())
		m_configDirectory->mkpath(".");

	// return the path to BletchMAME.xml
	return m_configDirectory->filePath("BletchMAME.xml");
}


//**************************************************************************
//  GLOBAL INFO - initializes slices of prefs with pertinent defaults
//**************************************************************************

//-------------------------------------------------
//  GlobalUiInfo ctor
//-------------------------------------------------

Preferences::GlobalUiInfo::GlobalUiInfo()
	: m_menuBarShown(true)
	, m_auditingState(AuditingState::Default)
{
}


//-------------------------------------------------
//  GlobalPathsInfo ctor
//-------------------------------------------------

Preferences::GlobalPathsInfo::GlobalPathsInfo(const std::optional<QDir> &configDirectory)
{
	// set up default paths - some of these require a config directory
	m_paths[(size_t)global_path_type::PLUGINS]		= QDir::toNativeSeparators("$(BLETCHMAMEPATH)/plugins/;$(MAMEPATH)/plugins/");
	m_paths[(size_t)global_path_type::HASH]			= QDir::toNativeSeparators("$(MAMEPATH)/hash/");
	if (configDirectory)
	{
		m_paths[(size_t)global_path_type::CONFIG]	= QDir::toNativeSeparators(configDirectory->absolutePath());
		m_paths[(size_t)global_path_type::NVRAM]	= QDir::toNativeSeparators(configDirectory->absolutePath());
		m_paths[(size_t)global_path_type::PROFILES]	= QDir::toNativeSeparators(configDirectory->filePath("profiles"));
	}
}


//**************************************************************************
//  MACHINE INFO
//**************************************************************************

//-------------------------------------------------
//  MachineInfo ctor
//-------------------------------------------------

Preferences::MachineInfo::MachineInfo()
	: m_auditStatus(AuditStatus::Unknown)
{
}


//-------------------------------------------------
//  MachineInfo dtor
//-------------------------------------------------

Preferences::MachineInfo::~MachineInfo()
{
}
