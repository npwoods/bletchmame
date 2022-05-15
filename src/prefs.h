/***************************************************************************

    prefs.h

    Wrapper for preferences specific to BletchMAME

***************************************************************************/

#pragma once

#ifndef PREFS_H
#define PREFS_H

// bletchmame headers
#include "utility.h"

// Qt headers
#include <QDataStream>
#include <QDir>
#include <QSize>

// standard headers
#include <array>
#include <ostream>
#include <map>
#include <set>


// ======================> AuditStatus

enum class AuditStatus
{
	Unknown,
	Found,
	MissingOptional,
	Missing
};


// ======================> MameIniImportActionPreference

enum class MameIniImportActionPreference
{
	Ignore,
	Supplement,
	Replace
};


struct ColumnPrefs
{
	int								m_width;
	std::optional<int>				m_order;
	std::optional<Qt::SortOrder>	m_sort;
};


class FolderPrefs
{
public:
	FolderPrefs();
	bool operator==(const FolderPrefs &) const = default;

	bool							m_shown;
};


class Preferences : public QObject
{
	Q_OBJECT
public:
	class Test;

	// paths that are "global"
	enum class global_path_type
	{
		EMU_EXECUTABLE,
		ROMS,
		SAMPLES,
		CONFIG,
		NVRAM,
		DIFF,
		HASH,
		ARTWORK,
		ICONS,
		PLUGINS,
		PROFILES,
		CHEATS,
		SNAPSHOTS,

		Max = SNAPSHOTS
	};

	// paths that are per-machine
	enum class machine_path_type
	{
		WORKING_DIRECTORY,
		LAST_SAVE_STATE,

		Max = LAST_SAVE_STATE
	};

	// is this path type for a file, a directory, or multiple directories?
	enum class PathCategory
	{
		SingleFile,
		SingleDirectory,
		MultipleDirectories,
		MultipleDirectoriesOrArchives
	};

	// main window state
	enum class WindowState
	{
		Normal,
		Maximized,
		FullScreen
	};

	// we have three different list views, each with a tab; this identifies them
	enum class list_view_type
	{
		MACHINE,
		SOFTWARELIST,
		PROFILE,

		Max = PROFILE
	};

	enum class AuditingState
	{
		Disabled,
		Automatic,
		Manual,

		Default = Disabled,
		Max = Manual
	};

	struct GlobalPathInfo
	{
		const char *	m_prefsName;		// name within prefs
		const char *	m_emuSettingName;	// name within emulation
		const char *	m_description;		// human readable description
	};

	typedef std::map<QString, std::set<QString>> CustomFoldersMap;

	// ctor
	Preferences(std::optional<QDir> &&configDirectory = std::nullopt, QObject *parent = nullptr);
	Preferences(const Preferences &) = delete;
	Preferences(Preferences &&) = delete;

	void resetToDefaults(bool resetUi, bool resetPaths, bool resetFolders);

	static PathCategory getPathCategory(global_path_type path_type);
	static PathCategory getPathCategory(machine_path_type path_type);

	const QString &getGlobalPath(global_path_type type) const											{ return m_globalPathsInfo.m_paths[static_cast<size_t>(type)]; }
	void setGlobalPath(global_path_type type, QString &&path);
	void setGlobalPath(global_path_type type, const QString &path);
	
	QStringList getSplitPaths(global_path_type type) const;

	QString getGlobalPathWithSubstitutions(global_path_type type) const;

	const QString &getMameExtraArguments() const														{ return m_mame_extra_arguments; }
	void setMameExtraArguments(QString &&extra_arguments)												{ m_mame_extra_arguments = std::move(extra_arguments); }

	const std::optional<QSize> &getSize() const								 							{ return m_size; }
	void setSize(const std::optional<QSize> &size)														{ m_size = size; }

	WindowState getWindowState() const																	{ return m_windowState; }
	void setWindowState(WindowState &state)																{ m_windowState = state; }

	list_view_type getSelectedTab() const																{ return m_selected_tab; }
	void setSelectedTab(list_view_type type);

	const QString &getMachineFolderTreeSelection() const												{ return m_machine_folder_tree_selection; }
	void setMachineFolderTreeSelection(QString &&selection)												{ m_machine_folder_tree_selection = std::move(selection); }

	const QList<int> &getMachineSplitterSizes() const													{ return m_machine_splitter_sizes; }
	void setMachineSplitterSizes(QList<int> &&sizes)													{ m_machine_splitter_sizes = std::move(sizes); }

	FolderPrefs getFolderPrefs(const QString &folder) const;
	void setFolderPrefs(const QString &folder, FolderPrefs &&prefs);

	const CustomFoldersMap &getCustomFolders() const													{ return m_customFolders; }
	bool addMachineToCustomFolder(const QString &customFolderName, const QString &machineName);
	bool removeMachineFromCustomFolder(const QString &customFolderName, const QString &machineName);
	bool renameCustomFolder(const QString &oldCustomFolderName, QString &&newCustomFolderName);
	bool deleteCustomFolder(const QString &customFolderName);

	const std::unordered_map<std::u8string, ColumnPrefs> &getColumnPrefs(const char8_t *view_type)			{ return m_column_prefs[view_type]; }
	void setColumnPrefs(const char8_t *view_type, std::unordered_map<std::u8string, ColumnPrefs> &&prefs)	{ m_column_prefs[view_type]  = std::move(prefs); }

	const QString &getListViewSelection(const char8_t *view_type, const QString &softlist_name) const;
	void setListViewSelection(const char8_t *view_type, const QString &softlist_name, QString &&selection);
	void setListViewSelection(const char8_t *view_type, QString &&selection)								{ setListViewSelection(view_type, util::g_empty_string, std::move(selection)); }

	const QString &getSearchBoxText(const char8_t *view_type) const										{ return m_list_view_filter[view_type]; }
	void setSearchBoxText(const char8_t *view_type, QString &&search_box_text)							{ m_list_view_filter[view_type] = std::move(search_box_text); }

	bool getWindowBarsShown() const																		{ return m_globalUiInfo.m_windowBarsShown; }
	void setWindowBarsShown(bool windowBarsShown);

	AuditingState getAuditingState() const																{ return m_globalUiInfo.m_auditingState; }
	void setAuditingState(AuditingState auditingState);

	bool getShowStopEmulationWarning() const															{ return m_globalUiInfo.m_showStopEmulationWarning;	}
	void setShowStopEmulationWarning(bool show)															{ m_globalUiInfo.m_showStopEmulationWarning = show;	}

	const QString &getMachinePath(const QString &machine_name, machine_path_type path_type) const;
	void setMachinePath(const QString &machine_name, machine_path_type path_type, QString &&path);

	const std::vector<QString> &getRecentDeviceFiles(const QString &machine_name, const QString &device_type) const;
	void placeInRecentDeviceFiles(const QString &machine_name, const QString &device_type, QString &&path);

	AuditStatus getMachineAuditStatus(const QString & machine_name) const;
	void setMachineAuditStatus(const QString &machine_name, AuditStatus status);
	void bulkDropMachineAuditStatuses(const std::function<bool(const QString &machineName)> &predicate = {});

	AuditStatus getSoftwareAuditStatus(const QString &softwareList, const QString &software) const;
	void setSoftwareAuditStatus(const QString &softwareList, const QString &software, AuditStatus status);
	void bulkDropSoftwareAuditStatuses();

	std::optional<MameIniImportActionPreference> getMameIniImportActionPreference(global_path_type type) const;
	void setMameIniImportActionPreference(global_path_type type, const std::optional<MameIniImportActionPreference> &importActionPreference);

	QString getMameXmlDatabasePath(bool ensure_directory_exists = true) const;
	QString applySubstitutions(const QString &path) const;
	static QString internalApplySubstitutions(const QString &src, std::function<QString(const QString &)> func);

	bool load();
	bool load(QIODevice &input);
	void save();

	// statics
	static const std::array<GlobalPathInfo, util::enum_count<Preferences::global_path_type>()> s_globalPathInfo;

signals:
	// general status
	void selectedTabChanged(list_view_type newSelectedTab);
	void auditingStateChanged();
	void windowBarsShownChanged(bool windowBarsShown);

	// folders
	void folderPrefsChanged();
	void customFoldersChanged();

	// global paths changed
	void globalPathEmuExecutableChanged(const QString &newPath);
	void globalPathRomsChanged(const QString &newPath);
	void globalPathSamplesChanged(const QString &newPath);
	void globalPathIconsChanged(const QString &newPath);
	void globalPathProfilesChanged(const QString &newPath);
	void globalPathSnapshotsChanged(const QString &newPath);

	// bulk dropping of audit statuses
	void bulkDroppedMachineAuditStatuses();
	void bulkDroppedSoftwareAuditStatuses();

private:
	// info pertinent to global paths state
	struct GlobalUiInfo
	{
		// ctor
		GlobalUiInfo();
		GlobalUiInfo(const GlobalUiInfo &) = delete;
		GlobalUiInfo(GlobalUiInfo &&) = default;

		// members
		bool																					m_windowBarsShown;
		AuditingState																			m_auditingState;
		bool																					m_showStopEmulationWarning;
	};

	// info pertinent to global paths state
	struct GlobalPathsInfo
	{
		// ctor
		GlobalPathsInfo(const std::optional<QDir> &configDirectory);
		GlobalPathsInfo(const GlobalPathsInfo &) = delete;
		GlobalPathsInfo(GlobalPathsInfo &&) = default;

		// members
		std::array<QString, util::enum_count<Preferences::global_path_type>()>					m_paths;
	};

	// info specific to each machine
	struct MachineInfo
	{
		MachineInfo();
		~MachineInfo();
		MachineInfo(const MachineInfo &) = delete;
		MachineInfo(MachineInfo &&) = default;

		bool operator==(const MachineInfo &) const = default;

		QString										m_workingDirectory;
		QString										m_lastSaveState;
		AuditStatus									m_auditStatus;
		std::map<QString, std::vector<QString>>     m_recentDeviceFiles;
	};

	// members
	std::optional<QDir>																			m_configDirectory;
	GlobalUiInfo																				m_globalUiInfo;
	GlobalPathsInfo																				m_globalPathsInfo;
	QString                                                                 					m_mame_extra_arguments;
	std::optional<QSize>																		m_size;
	WindowState																					m_windowState;
	mutable std::unordered_map<std::u8string, std::unordered_map<std::u8string, ColumnPrefs>>	m_column_prefs;
	std::map<QString, MachineInfo>																m_machine_info;
	std::map<std::tuple<QString, QString>, AuditStatus>											m_softwareAuditStatus;
	list_view_type																				m_selected_tab;
	QString																						m_machine_folder_tree_selection;
	QList<int>																					m_machine_splitter_sizes;
	std::map<QString, FolderPrefs>																m_folderPrefs;
	std::map<QString, std::set<QString>>														m_customFolders;
	std::unordered_map<QString, QString>														m_list_view_selection;
	mutable std::unordered_map<QString, QString>												m_list_view_filter;
	std::unordered_map<global_path_type, MameIniImportActionPreference>							m_importActionPreferences;

	// private methods
	void save(QIODevice &output);
	QString getPreferencesFileName(bool ensureDirectoryExists) const;
	const MachineInfo *getMachineInfo(const QString &machine_name) const;
	void garbageCollectMachineInfo();
	void setGlobalInfo(GlobalUiInfo &&globalInfo);
	void setGlobalInfo(GlobalPathsInfo &&globalInfo);
	void internalSetGlobalPath(global_path_type type, QString &&path);
};

#endif // PREFS_H
