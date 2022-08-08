/***************************************************************************

    prefs.cpp

    Wrapper for preferences specific to BletchMAME

***************************************************************************/

// bletchmame headers
#include "prefs.h"
#include "utility.h"
#include "xmlparser.h"

// Qt headers
#include <QDir>
#include <QCoreApplication>
#include <QXmlStreamWriter>

// standard headers
#include <fstream>
#include <functional>
#include <locale>


//**************************************************************************
//  LOCAL VARIABLES
//**************************************************************************

const std::array<Preferences::GlobalPathInfo, util::enum_count<Preferences::global_path_type>()> Preferences::s_globalPathInfo
{
	{
		{ "emu",		nullptr,			"MAME Executable" },
		{ "roms",		"rompath",			"ROMs" },
		{ "samples",	"samplepath",		"Samples" },
		{ "config",		"cfg_directory",	"Config Files" },
		{ "nvram",		"nvram_directory",	"NVRAM Files" },
		{ "diff",		"diff_directory",	"CHD Diff Files" },
		{ "hash",		"hashpath",			"Hash Files" },
		{ "artwork",	"artpath",			"Artwork Files" },
		{ "icons",		nullptr,			"Icons" },
		{ "plugins",	"pluginspath",		"Plugins" },
		{ "profiles",	nullptr,			"Profiles" },
		{ "cheats",		"cheatpath",		"Cheats" },
		{ "snap",		nullptr,			"Snapshots" },
		{ "history",	nullptr,			"History File" }
	}
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


namespace
{
	class GlobalPathTypeMameSettingParser
	{
	public:
		bool operator()(std::u8string_view text, Preferences::global_path_type &value) const
		{
			auto iter = std::ranges::find_if(Preferences::s_globalPathInfo, [&text](const Preferences::GlobalPathInfo &x)
			{
				return x.m_emuSettingName && text == std::u8string_view((char8_t *)x.m_emuSettingName);
			});
			if (iter != Preferences::s_globalPathInfo.end())
				value = (Preferences::global_path_type) (iter - Preferences::s_globalPathInfo.begin());
			return iter != Preferences::s_globalPathInfo.end();
		}
	};
}
static const GlobalPathTypeMameSettingParser s_globalPathTypeMameSettingParser;


static const util::enum_parser_bidirectional<MameIniImportActionPreference> g_mameIniImportActionPreferenceParser =
{
	{ "ignore", MameIniImportActionPreference::Ignore, },
	{ "supplement", MameIniImportActionPreference::Supplement, },
	{ "replace", MameIniImportActionPreference::Replace }
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
//  coalesce
//-------------------------------------------------

template<class T>
static T coalesce(T lhs, T rhs)
{
	return lhs ? lhs : rhs;
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

static QList<int> intListFromString(std::u8string_view s)
{
	QList<int> result;
	for (const QString &part : util::toQString(s).split(','))
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

void Preferences::resetToDefaults(bool resetUi, bool resetPaths, bool resetFolders)
{
	if (resetUi)
		setGlobalInfo(GlobalUiInfo());
	if (resetPaths)
		setGlobalInfo(GlobalPathsInfo(m_configDirectory));

	if (resetFolders)
	{
		if (!m_folderPrefs.empty())
		{
			m_folderPrefs.clear();
			emit folderPrefsChanged();
		}

		if (!m_customFolders.empty())
		{
			m_customFolders.clear();
			emit customFoldersChanged();
		}
	}
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
	case Preferences::global_path_type::HISTORY:
		result = PathCategory::SingleFile;
		break;

	case Preferences::global_path_type::CONFIG:
	case Preferences::global_path_type::NVRAM:
	case Preferences::global_path_type::DIFF:
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
	// we have to copy each of the properties individually because these might fire signals
	setWindowBarsShown(globalInfo.m_windowBarsShown);
	setAuditingState(globalInfo.m_auditingState);
	setShowStopEmulationWarning(globalInfo.m_showStopEmulationWarning);
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
	case global_path_type::HISTORY:
		emit globalPathHistoryChanged(dest);
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
	paths = splitPathList(pathsString);

	for (QString &path : paths)
	{
		// apply variable substituions
		path = applySubstitutions(path);

		// normalize path separators
		path = QDir::fromNativeSeparators(path);
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
//  placeInRecentDeviceFiles
//-------------------------------------------------

void Preferences::placeInRecentDeviceFiles(const QString &machine_name, const QString &device_type, QString &&path)
{
	// don't add empty stuff
	if (path.trimmed().isEmpty())
		return;

	// actually edit the recent files; start by getting recent files
	std::vector<QString> &recentFiles = m_machine_info[machine_name].m_recentDeviceFiles[device_type];

	// ...and clearing out places where that entry already exists
	QFileInfo pathFi(path);
	auto endIter = std::remove_if(recentFiles.begin(), recentFiles.end(), [&pathFi](const QString &x)
	{
		return areFileInfosEquivalent(QFileInfo(x), pathFi);
	});
	recentFiles.resize(endIter - recentFiles.begin());

	// ...insert the new value
	recentFiles.insert(recentFiles.begin(), std::move(path));

	// and cull the list
	const size_t MAXIMUM_RECENT_FILES = 10;
	recentFiles.resize(std::min(recentFiles.size(), MAXIMUM_RECENT_FILES));
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
//  setWindowBarsShown
//-------------------------------------------------

void Preferences::setWindowBarsShown(bool windowBarsShown)
{
	if (m_globalUiInfo.m_windowBarsShown != windowBarsShown)
	{
		m_globalUiInfo.m_windowBarsShown = windowBarsShown;
		emit windowBarsShownChanged(m_globalUiInfo.m_windowBarsShown);
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
//  getMameIniImportActionPreference
//-------------------------------------------------

std::optional<MameIniImportActionPreference> Preferences::getMameIniImportActionPreference(global_path_type type) const
{
	auto iter = m_importActionPreferences.find(type);
	return iter != m_importActionPreferences.end()
		? iter->second
		: std::optional<MameIniImportActionPreference>();
}


//-------------------------------------------------
//  setMameIniImportActionPreference
//-------------------------------------------------

void Preferences::setMameIniImportActionPreference(global_path_type type, const std::optional<MameIniImportActionPreference> &importActionPreference)
{
	if (importActionPreference)
		m_importActionPreferences[type] = *importActionPreference;
	else
		m_importActionPreferences.erase(type);
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
		// windowBarsShown is called menu_bar_shown in the XML for purely historical reasons
		const auto [windowBarsShown, windowStateAttr, selectedTabAttr, auditing, showStopEmulationWarning] = attributes.get("menu_bar_shown", "window_state", "selected_tab", "auditing", "show_stop_emulation_warning");

		globalUiInfo.m_windowBarsShown = windowBarsShown.as<bool>().value_or(globalUiInfo.m_windowBarsShown);
		globalUiInfo.m_auditingState = auditing.as<AuditingState>(s_auditingStateParser).value_or(globalUiInfo.m_auditingState);
		globalUiInfo.m_showStopEmulationWarning = showStopEmulationWarning.as<bool>().value_or(globalUiInfo.m_showStopEmulationWarning);

		std::optional<WindowState> windowState = windowStateAttr.as<WindowState>(s_windowState_parser);
		if (windowState)
			setWindowState(*windowState);

		std::optional<list_view_type> selectedTab = selectedTabAttr.as<list_view_type>(s_list_view_type_parser);
		if (selectedTab)
			setSelectedTab(*selectedTab);
	});
	xml.onElementBegin({ "preferences", "path" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [typeAttr] = attributes.get("type");
		std::optional<QString> typeString = typeAttr.as<QString>();
		if (typeString)
		{
			auto iter = std::ranges::find_if(s_globalPathInfo, [&typeString](const auto &x)
			{
				return x.m_prefsName == *typeString;
			});
			type.reset();
			if (iter != s_globalPathInfo.cend())
				type = static_cast<global_path_type>(iter - s_globalPathInfo.cbegin());
		}
	});
	xml.onElementEnd({ "preferences", "path" }, [&](std::u8string &&content)
	{
		if (type)
			globalPathsInfo.m_paths[(size_t) *type] = QDir::fromNativeSeparators(util::toQString(content));
		type.reset();
	});
	xml.onElementEnd({ "preferences", "mameextraarguments" }, [&](std::u8string &&content)
	{
		setMameExtraArguments(util::toQString(content));
	});
	xml.onElementBegin({ "preferences", "size" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [widthAttr, heightAttr] = attributes.get("width", "height");
		std::optional<int> width = widthAttr.as<int>();
		std::optional<int> height = heightAttr.as<int>();

		if (width && height && isValidDimension(*width) && isValidDimension(*height))
		{
			QSize size;
			size.setWidth(*width);
			size.setHeight(*height);
			setSize(size);
		}
	});	
	xml.onElementEnd({ "preferences", "machinelistsplitters" }, [&](std::u8string &&content)
	{
		QList<int> splitterSizes = intListFromString(content);
		if (!splitterSizes.isEmpty())
			setMachineSplitterSizes(std::move(splitterSizes));
	});
	xml.onElementEnd({ "preferences", "softwarelistsplitters" }, [&](std::u8string &&content)
	{
		QList<int> splitterSizes = intListFromString(content);
		if (!splitterSizes.isEmpty())
			setSoftwareSplitterSizes(std::move(splitterSizes));
	});
	xml.onElementBegin({ "preferences", "folder" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [idAttr, isShownAttr, selectedAttr] = attributes.get("id", "shown", "selected");
		std::optional<QString> id = idAttr.as<QString>();
		if (id)
		{
			FolderPrefs folderPrefs = getFolderPrefs(*id);
			std::optional<bool> isShown = isShownAttr.as<bool>();
			if (isShown)
				folderPrefs.m_shown = *isShown;
			setFolderPrefs(*id, std::move(folderPrefs));
			
			if (selectedAttr.as<bool>() == true)
				setMachineFolderTreeSelection(std::move(*id));
		}
	});
	xml.onElementBegin({ "preferences", "customfolder" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [nameAttr] = attributes.get("name");
		std::optional<QString> name = nameAttr.as<QString>();
		if (name)
			current_custom_folder = &m_customFolders.emplace(*name, std::set<QString>()).first->second;
	});
	xml.onElementEnd({ "preferences", "customfolder" }, [&]()
	{
		current_custom_folder = nullptr;
	});
	xml.onElementEnd({ "preferences", "customfolder", "system" }, [&](std::u8string &&content)
	{
		if (current_custom_folder)
			current_custom_folder->emplace(util::toQString(content));
	});
	xml.onElementBegin({ "preferences", "selection" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [listViewAttr, softlistAttr] = attributes.get("view", "softlist");
		std::optional<std::u8string_view> listView = listViewAttr.as<std::u8string_view>();
		if (listView)
		{
			QString softlist = softlistAttr.as<QString>().value_or("");
			QString key = getListViewSelectionKey(*listView, std::move(softlist));
			current_list_view_parameter = &m_list_view_selection[key];
		}
	});
	xml.onElementBegin({ "preferences", "searchboxtext" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [listViewAttr] = attributes.get("view");
		QString list_view = listViewAttr.as<QString>().value_or("machine");
		current_list_view_parameter = &m_list_view_filter[list_view];
	});
	xml.onElementEnd({{ "preferences", "selection" },
					  { "preferences", "searchboxtext" }}, [&](std::u8string &&content)
	{
		assert(current_list_view_parameter);
		*current_list_view_parameter = util::toQString(content);
		current_list_view_parameter = nullptr;
	});
	xml.onElementBegin({ "preferences", "column" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [viewTypeAttr, idAttr, widthAttr, orderAttr, sortAttr] = attributes.get("type", "id", "width", "order", "sort");
		std::optional<std::u8string_view> viewType = viewTypeAttr.as<std::u8string_view>();
		std::optional<std::u8string_view> id = idAttr.as<std::u8string_view>();
		if (viewType && id)
		{
			ColumnPrefs &col_prefs = m_column_prefs[std::u8string(*viewType)][std::u8string(*id)];
			col_prefs.m_width = widthAttr.as<int>().value_or(col_prefs.m_width);
			if (orderAttr.as<std::u8string_view>() == u8"hidden")
				col_prefs.m_order = std::nullopt;
			else
				col_prefs.m_order = coalesce(orderAttr.as<int>(), col_prefs.m_order);
			col_prefs.m_sort = sortAttr.as<Qt::SortOrder>(s_column_sort_type_parser);
		}
	});
	xml.onElementBegin({ "preferences", "machine" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [nameAttr, workingDirectoryAttr, lastSaveStateAttr, auditStatusAttr] = attributes.get("name", "working_directory", "last_save_state", "audit_status");

		std::optional<QString> name = nameAttr.as<QString>();
		if (!name)
			return XmlParser::ElementResult::Skip;
		current_machine_name = *name;

		std::optional<QString> workingDirectory = workingDirectoryAttr.as<QString>();
		if (workingDirectory)
			setMachinePath(current_machine_name, machine_path_type::WORKING_DIRECTORY, QDir::fromNativeSeparators(*workingDirectory));

		std::optional<QString> lastSaveState = lastSaveStateAttr.as<QString>();
		if (lastSaveState)
			setMachinePath(current_machine_name, machine_path_type::LAST_SAVE_STATE, QDir::fromNativeSeparators(*lastSaveState));

		AuditStatus status = auditStatusAttr.as<AuditStatus>(s_audit_status_parser).value_or(AuditStatus::Unknown);
		setMachineAuditStatus(current_machine_name, status);

		return XmlParser::ElementResult::Ok;
	});
	xml.onElementBegin({ "preferences", "machine", "device" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [typeAttr] = attributes.get("type");
		std::optional<QString> type = typeAttr.as<QString>();
		if (!type)
			return XmlParser::ElementResult::Skip;

		current_device_type = std::move(*type);
		return XmlParser::ElementResult::Ok;
	});
	xml.onElementEnd({ "preferences", "machine", "device", "recentfile" }, [&](std::u8string &&content)
	{
		QString path = QDir::fromNativeSeparators(util::toQString(content));
		if (!path.trimmed().isEmpty())
			m_machine_info[current_machine_name].m_recentDeviceFiles[current_device_type].push_back(std::move(path));
	});
	xml.onElementBegin({ "preferences", "software" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [listAttr, nameAttr, statusAttr] = attributes.get("list", "name", "audit_status");
		std::optional<QString> list = listAttr.as<QString>();
		std::optional<QString> name = nameAttr.as<QString>();
		if (list && name)
		{
			AuditStatus status = statusAttr.as<AuditStatus>(s_audit_status_parser).value_or(AuditStatus::Unknown);
			setSoftwareAuditStatus(*list, *name, status);
		}
	});
	xml.onElementBegin({ "preferences", "mameiniimport" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [settingAttr, preferenceAttr] = attributes.get("setting", "preference");
		std::optional<global_path_type> setting = settingAttr.as<global_path_type>(s_globalPathTypeMameSettingParser);
		std::optional<MameIniImportActionPreference> preference = preferenceAttr.as<MameIniImportActionPreference>(g_mameIniImportActionPreferenceParser);
		if (setting)
			setMameIniImportActionPreference(*setting, preference);
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
	writer.writeAttribute("menu_bar_shown", QString::number(getWindowBarsShown() ? 1 : 0));
	writer.writeAttribute("window_state", s_windowState_parser[getWindowState()]);
	writer.writeAttribute("selected_tab", s_list_view_type_parser[getSelectedTab()]);
	writer.writeAttribute("auditing", s_auditingStateParser[getAuditingState()]);
	writer.writeAttribute("show_stop_emulation_warning", QString::number(getShowStopEmulationWarning() ? 1 : 0));

	// paths
	writer.writeComment("Paths");
	for (global_path_type pathType : util::all_enums<global_path_type>())
	{
		writer.writeStartElement("path");
		writer.writeAttribute("type", s_globalPathInfo[(size_t)pathType].m_prefsName);
		writer.writeCharacters(QDir::toNativeSeparators(getGlobalPath(pathType)));
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
	if (!m_software_splitter_sizes.isEmpty())
		writer.writeTextElement("softwarelistsplitters", stringFromIntList(m_software_splitter_sizes));

	// import preference
	for (const auto &[pathType, importPref] : m_importActionPreferences)
	{
		const char *settingString = Preferences::s_globalPathInfo[(size_t)pathType].m_emuSettingName;
		const char *preferenceString = g_mameIniImportActionPreferenceParser[importPref];

		writer.writeStartElement("mameiniimport");
		writer.writeAttribute("setting", settingString);
		writer.writeAttribute("preference", preferenceString);
		writer.writeEndElement();
	}

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
			writer.writeAttribute("order", col_prefs.second.m_order.has_value()
				? QString::number(*col_prefs.second.m_order)
				: "hidden");
			if (col_prefs.second.m_sort)
				writer.writeAttribute("sort", s_column_sort_type_parser[*col_prefs.second.m_sort]);
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
				writer.writeAttribute("working_directory", QDir::toNativeSeparators(info.m_workingDirectory));
			if (!info.m_lastSaveState.isEmpty())
				writer.writeAttribute("last_save_state", QDir::toNativeSeparators(info.m_lastSaveState));
			if (info.m_auditStatus != AuditStatus::Unknown)
				writer.writeAttribute("audit_status", s_audit_status_parser[info.m_auditStatus]);

			if (!info.m_recentDeviceFiles.empty())
			{
				for (const auto &[device_type, recents] : info.m_recentDeviceFiles)
				{
					writer.writeStartElement("device");
					writer.writeAttribute("type", device_type);
					for (const QString &recent : recents)
						writer.writeTextElement("recentfile", QDir::toNativeSeparators(recent));
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
		return result;
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
	: m_windowBarsShown(true)
	, m_auditingState(AuditingState::Default)
	, m_showStopEmulationWarning(true)
{
}


//-------------------------------------------------
//  GlobalPathsInfo ctor
//-------------------------------------------------

Preferences::GlobalPathsInfo::GlobalPathsInfo(const std::optional<QDir> &configDirectory)
{
	// set up default paths - some of these require a config directory
	m_paths[(size_t)global_path_type::PLUGINS]		= joinPathList(QStringList() << "$(BLETCHMAMEPATH)/plugins/" << "$(MAMEPATH)/plugins/");
	m_paths[(size_t)global_path_type::HASH]			= "$(MAMEPATH)/hash/";
	if (configDirectory)
	{
		m_paths[(size_t)global_path_type::CONFIG]	= configDirectory->absolutePath();
		m_paths[(size_t)global_path_type::NVRAM]	= configDirectory->absolutePath();
		m_paths[(size_t)global_path_type::DIFF]		= configDirectory->absolutePath();
		m_paths[(size_t)global_path_type::PROFILES] = configDirectory->filePath("profiles");
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
