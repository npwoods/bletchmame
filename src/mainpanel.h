/***************************************************************************

	mainpanel.h

	Main BletchMAME panel

***************************************************************************/

#ifndef MAINPANEL_H
#define MAINPANEL_H

#include <QTreeView>
#include <QTableView>

#include "iconloader.h"
#include "profile.h"
#include "prefs.h"
#include "softwarelist.h"
#include "audittask.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainPanel; }
class QLineEdit;
class QTableWidgetItem;
class QAbstractItemModel;
class QTableView;
class QSortFilterProxyModel;
QT_END_NAMESPACE

class AuditDialog;
class MachineFolderTreeModel;
class MachineListItemModel;
class ProfileListItemModel;
class SessionBehavior;
class SoftwareListItemModel;


// ======================> IMainPanelHost

class IMainPanelHost
{
public:
	virtual void run(const info::machine &machine, std::unique_ptr<SessionBehavior> &&sessionBehavior) = 0;
	virtual void auditIfAppropriate(const info::machine &machine) = 0;
	virtual void auditIfAppropriate(const software_list::software &software) = 0;
	virtual void auditDialogStarted(AuditDialog &auditDialog, std::shared_ptr<AuditTask> &&auditTask) = 0;
	virtual software_list_collection &getSoftwareListCollection() = 0;
};


// ======================> MainPanel

class MainPanel : public QWidget
{
	Q_OBJECT

public:
	static const int STATUS_ENTRIES = 3;

	MainPanel(info::database &infoDb, Preferences &prefs, IMainPanelHost &host, QWidget *parent = nullptr);
	~MainPanel();

	// accessors
	const auto &status() const { return m_status; }

	// methods
	void updateTabContents();
	void pathsChanged(const std::vector<Preferences::global_path_type> &changedPaths);
	std::optional<info::machine> currentlySelectedMachine() const;
	const software_list::software *currentlySelectedSoftware() const;
	std::shared_ptr<profiles::profile> currentlySelectedProfile();

	// auditing
	void setAuditStatuses(const std::vector<AuditResult> &results);
	void machineAuditStatusesChanged();
	void softwareAuditStatusesChanged();
	void manualAudit(const info::machine &machine);
	void manualAudit(const software_list::software &software);
	static QString auditThisActionText(const QString &text);
	static QString auditThisActionText(QString &&text);

private slots:
	void setStatus(const std::array<QString, STATUS_ENTRIES> &status);

	void on_machinesFolderTreeView_customContextMenuRequested(const QPoint &pos);
	void on_machinesTableView_activated(const QModelIndex &index);
	void on_machinesTableView_customContextMenuRequested(const QPoint &pos);
	void on_softwareTableView_activated(const QModelIndex &index);
	void on_softwareTableView_customContextMenuRequested(const QPoint &pos);
	void on_profilesTableView_activated(const QModelIndex &index);
	void on_profilesTableView_customContextMenuRequested(const QPoint &pos);
	void on_tabWidget_currentChanged(int index);
	void on_machinesSplitter_splitterMoved(int pos, int index);

signals:
	void statusChanged(const std::array<QString, STATUS_ENTRIES> &newStatus);

private:
	class SnapshotViewEventFilter;

	// variables configured at startup
	std::unique_ptr<Ui::MainPanel>		m_ui;
	Preferences &						m_prefs;
	IMainPanelHost &					m_host;

	// information retrieved by -listxml
	info::database &					m_infoDb;

	// other
	QString								m_currentSoftwareList;
	IconLoader							m_iconLoader;
	QPixmap								m_currentSnapshot;
	std::vector<QString>				m_expandedTreeItems;
	std::array<QString, STATUS_ENTRIES>	m_status;

	// methods
	void run(const info::machine &machine, const software_list::software *software = nullptr);
	void run(std::shared_ptr<profiles::profile> &&profile);
	void run(const info::machine &machine, std::unique_ptr<SessionBehavior> &&sessionBehavior);
	void updateSoftwareList();
	void updateStatusFromSelection();
	QString machineStatusString(const info::machine &machine) const;
	void launchingListContextMenu(const QPoint &pos, const software_list::software *software = nullptr);
	void createProfile(const info::machine &machine, const software_list::software *software);
	static bool DirExistsOrMake(const QString &path);
	void duplicateProfile(const profiles::profile &profile);
	void deleteProfile(const profiles::profile &profile);
	void focusOnNewProfile(QString &&new_profile_path);
	void editSelection(QAbstractItemView &itemView);
	QString currentlySelectedCustomFolder() const;
	void deleteSelectedFolder();
	void showInGraphicalShell(const QString &path) const;
	info::machine machineFromModelIndex(const QModelIndex &index) const;
	const software_list::software &softwareFromModelIndex(const QModelIndex &index) const;
	const MachineFolderTreeModel &machineFolderTreeModel() const;
	MachineFolderTreeModel &machineFolderTreeModel();
	MachineListItemModel &machineListItemModel();
	const MachineListItemModel &machineListItemModel() const;
	SoftwareListItemModel &softwareListItemModel();
	const SoftwareListItemModel &softwareListItemModel() const;
	ProfileListItemModel &profileListItemModel();
	const QSortFilterProxyModel &sortFilterProxyModel(const QTableView &tableView) const;
	void machineFoldersTreeViewSelectionChanged(const QItemSelection &newSelection, const QItemSelection &oldSelection);
	void persistMachineSplitterSizes();
	void updateInfoPanel(const QString &machineName);
	void updateSnapshot();
	void identifyExpandedFolderTreeItems();
	static void iterateItemModelIndexes(QAbstractItemModel &model, const std::function<void(const QModelIndex &)> &func, const QModelIndex &index = QModelIndex());
	void runAuditDialog(const Audit &audit, const QString &name, const QString &description, const QPixmap &pixmap, AuditTask::ptr auditTask);
};


#endif // MAINPANEL_H
