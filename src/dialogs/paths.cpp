/***************************************************************************

    dialogs/paths.cpp

    Paths dialog

***************************************************************************/

#include <QPushButton>
#include <QFileDialog>
#include <QTextStream>
#include <QStringListModel>

#include "dialogs/paths.h"
#include "ui_paths.h"
#include "prefs.h"
#include "utility.h"


//**************************************************************************
//  TYPES
//**************************************************************************

class PathsDialog::PathListModel : public QAbstractListModel
{
private:
	struct Entry;

public:
	PathListModel(QObject &parent, std::function<QString(const QString &)> &&applySubstitutionsFunc);

	// accessors
	bool expandable() const { return m_expandable; }

	// setting/getting full paths
	void setPaths(const QString &paths, bool applySubstitutions, bool expandable, bool supportsFiles, bool supportsDirectories);
	QString paths() const;

	// setting/getting individual paths
	void setPath(int index, QString &&path);
	const QString &path(int index) const;
	int pathCount() const;
	void insert(int index);
	void erase(int index);

	// virtuals
	virtual int rowCount(const QModelIndex &parent) const;
	virtual QVariant data(const QModelIndex &index, int role) const;
	virtual bool setData(const QModelIndex &index, const QVariant &value, int role);
	virtual Qt::ItemFlags flags(const QModelIndex &index) const;

private:
	struct Entry
	{
		Entry(QString &&path = "", bool isValid = true);

		QString			m_path;
		bool			m_isValid;
	};

	std::function<QString(const QString &)>	m_applySubstitutionsFunc;
	bool									m_applySubstitutions;
	bool									m_expandable;
	bool									m_supportsFiles;
	bool									m_supportsDirectories;
	std::vector<Entry>						m_entries;

	// private methods
	bool validateAndCanonicalize(QString &path) const;
	bool hasExpandEntry() const;
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

const QStringList PathsDialog::s_combo_box_strings = buildComboBoxStrings();


//-------------------------------------------------
//  ctor
//-------------------------------------------------

PathsDialog::PathsDialog(QWidget &parent, Preferences &prefs)
	: QDialog(&parent)
	, m_prefs(prefs)
	, m_listViewModelCurrentPath({ })
{
	m_ui = std::make_unique<Ui::PathsDialog>();
	m_ui->setupUi(this);

	// path data
	for (size_t i = 0; i < PATH_COUNT; i++)
		m_pathLists[i] = m_prefs.GetGlobalPath(static_cast<Preferences::global_path_type>(i));

	// list view
	PathListModel &model = *new PathListModel(*this, [this](const QString &path) { return m_prefs.ApplySubstitutions(path); });
	m_ui->listView->setModel(&model);

	// combo box
	QStringListModel &comboBoxModel = *new QStringListModel(s_combo_box_strings, this);
	m_ui->comboBox->setModel(&comboBoxModel);

	// listen to selection changes
	connect(m_ui->listView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &, const QItemSelection &) { updateButtonsEnabled(); });
	updateButtonsEnabled();
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

PathsDialog::~PathsDialog()
{
}


//-------------------------------------------------
//  persist
//-------------------------------------------------

std::vector<Preferences::global_path_type> PathsDialog::persist()
{
	// first we want to make sure that m_pathLists is up to data
	extractPathsFromListView();

	// we want to return a vector identifying the paths that got changed
	std::vector<Preferences::global_path_type> changedPaths;
	for (Preferences::global_path_type type : util::all_enums<Preferences::global_path_type>())
	{
		QString &path = m_pathLists[static_cast<size_t>(type)];

		// has this path changed?
		if (path != m_prefs.GetGlobalPath(type))
		{
			// if so, record that it changed
			m_prefs.SetGlobalPath(type, std::move(path));
			changedPaths.push_back(type);
		}
	}
	return changedPaths;
}


//-------------------------------------------------
//  on_comboBox_currentIndexChanged
//-------------------------------------------------

void PathsDialog::on_comboBox_currentIndexChanged(int index)
{
	updateCurrentPathList();
}


//-------------------------------------------------
//  on_browseButton_clicked
//-------------------------------------------------

void PathsDialog::on_browseButton_clicked()
{
	QModelIndexList selectedIndexes = m_ui->listView->selectionModel()->selectedIndexes();
	int item = selectedIndexes.size() == 1
		? selectedIndexes[0].row()
		: listModel().pathCount();
	browseForPath(item);
}


//-------------------------------------------------
//  on_insertButton_clicked
//-------------------------------------------------

void PathsDialog::on_insertButton_clicked()
{
	// identify the correct item to edit
	int item = m_ui->listView->currentIndex().row();

	// if we're not appending, insert an item
	if (item < listModel().pathCount())
		listModel().insert(item);

	// and tell the list view to start editing
	QModelIndex index = listModel().index(item);
	m_ui->listView->edit(index);
}


//-------------------------------------------------
//  on_deleteButton_clicked
//-------------------------------------------------

void PathsDialog::on_deleteButton_clicked()
{
	listModel().erase(m_ui->listView->currentIndex().row());
}


//-------------------------------------------------
//  on_listView_activated
//-------------------------------------------------

void PathsDialog::on_listView_activated(const QModelIndex &index)
{
	browseForPath(index.row());
}


//-------------------------------------------------
//  updateButtonsEnabled
//-------------------------------------------------

void PathsDialog::updateButtonsEnabled()
{	
	std::optional<int> selectedItem = m_ui->listView->selectionModel()->hasSelection()
		? m_ui->listView->selectionModel()->selectedRows()[0].row()
		: std::optional<int>();
	bool isSelectingPath = selectedItem.has_value() && selectedItem.value() < listModel().pathCount();

	m_ui->insertButton->setEnabled(listModel().expandable());
	m_ui->deleteButton->setEnabled(listModel().expandable() && isSelectingPath);
}


//-------------------------------------------------
//  browseForPath
//-------------------------------------------------

bool PathsDialog::browseForPath(int index)
{
	// show the file dialog
	QString path = browseForPathDialog(
		*this,
		getCurrentPath(),
		index < listModel().pathCount() ? listModel().path(index) : "");
	if (path.isEmpty())
		return false;

	// specify it
	listModel().setPath(index, std::move(path));
	return true;
}


//-------------------------------------------------
//  extractPathsFromListView
//-------------------------------------------------

void PathsDialog::extractPathsFromListView()
{
	if (m_listViewModelCurrentPath.has_value())
	{
		// reflect changes on the m_current_path_list back into m_pathLists 
		QString paths = listModel().paths();
		m_pathLists[(int)m_listViewModelCurrentPath.value()] = std::move(paths);
	}
}


//-------------------------------------------------
//  updateCurrentPathList
//-------------------------------------------------

void PathsDialog::updateCurrentPathList()
{
	const Preferences::global_path_type currentPathType = getCurrentPath();
	bool applySubstitutions = currentPathType != Preferences::global_path_type::EMU_EXECUTABLE;
	Preferences::path_category category = Preferences::GetPathCategory(currentPathType);
	bool isMultiple = category == Preferences::path_category::MULTIPLE_DIRECTORIES
		|| category == Preferences::path_category::MULTIPLE_MIXED;
	bool supportsFiles = isFilePathType(currentPathType);
	bool supportsDirectories = isDirPathType(currentPathType);
	
	listModel().setPaths(
		m_pathLists[(int)currentPathType],
		applySubstitutions,
		isMultiple,
		supportsFiles,
		supportsDirectories);

	m_listViewModelCurrentPath = currentPathType;
}


//-------------------------------------------------
//  buildComboBoxStrings
//-------------------------------------------------

QStringList PathsDialog::buildComboBoxStrings()
{
	std::array<QString, PATH_COUNT> paths;
	paths[(size_t)Preferences::global_path_type::EMU_EXECUTABLE]	= "MAME Executable";
	paths[(size_t)Preferences::global_path_type::ROMS]				= "ROMs";
	paths[(size_t)Preferences::global_path_type::SAMPLES]			= "Samples";
	paths[(size_t)Preferences::global_path_type::CONFIG]			= "Config Files";
	paths[(size_t)Preferences::global_path_type::NVRAM]				= "NVRAM Files";
	paths[(size_t)Preferences::global_path_type::HASH]				= "Hash Files";
	paths[(size_t)Preferences::global_path_type::ARTWORK]			= "Artwork Files";
	paths[(size_t)Preferences::global_path_type::ICONS]				= "Icons";
	paths[(size_t)Preferences::global_path_type::PLUGINS]			= "Plugins";
	paths[(size_t)Preferences::global_path_type::PROFILES]			= "Profiles";
	paths[(size_t)Preferences::global_path_type::CHEATS]			= "Cheats";
	paths[(size_t)Preferences::global_path_type::SNAPSHOTS]			= "Snapshots";

	QStringList result;
	for (QString &str : paths)
		result.push_back(std::move(str));
	return result;
}


//-------------------------------------------------
//  getCurrentPath
//-------------------------------------------------

Preferences::global_path_type PathsDialog::getCurrentPath() const
{
	int selection = m_ui->comboBox->currentIndex();
	return static_cast<Preferences::global_path_type>(selection);
}


//-------------------------------------------------
//  isFilePathType
//-------------------------------------------------

bool PathsDialog::isFilePathType(Preferences::global_path_type type)
{
	return type == Preferences::global_path_type::EMU_EXECUTABLE
		|| type == Preferences::global_path_type::ICONS;
}


//-------------------------------------------------
//  isDirPathType
//-------------------------------------------------

bool PathsDialog::isDirPathType(Preferences::global_path_type type)
{
	return type == Preferences::global_path_type::ROMS
		|| type == Preferences::global_path_type::SAMPLES
		|| type == Preferences::global_path_type::CONFIG
		|| type == Preferences::global_path_type::NVRAM
		|| type == Preferences::global_path_type::HASH
		|| type == Preferences::global_path_type::ARTWORK
		|| type == Preferences::global_path_type::ICONS
		|| type == Preferences::global_path_type::PLUGINS
		|| type == Preferences::global_path_type::PROFILES
		|| type == Preferences::global_path_type::CHEATS
		|| type == Preferences::global_path_type::SNAPSHOTS;
}


//-------------------------------------------------
//  browseForPathDialog
//-------------------------------------------------

QString PathsDialog::browseForPathDialog(QWidget &parent, Preferences::global_path_type type, const QString &default_path)
{
	QString caption, filter;
	switch (type)
	{
	case Preferences::global_path_type::EMU_EXECUTABLE:
		caption = "Specify MAME Path";
#ifdef Q_OS_WIN32
		filter = "EXE files (*.exe);*.exe";
#endif
		break;

	default:
		caption = "Specify Path";
		break;
	};

	// determine the FileMode
	QFileDialog::FileMode fileMode = isDirPathType(type)
		? QFileDialog::FileMode::Directory
		: QFileDialog::FileMode::ExistingFile;

	// show the dialog
	QFileDialog dialog(&parent, caption, default_path, filter);
	dialog.setFileMode(fileMode);
	dialog.setAcceptMode(QFileDialog::AcceptMode::AcceptOpen);
	dialog.exec();
	return dialog.result() == QDialog::DialogCode::Accepted
		? dialog.selectedFiles().first()
		: QString();
}


//-------------------------------------------------
//  listModel
//-------------------------------------------------

PathsDialog::PathListModel &PathsDialog::listModel()
{
	return *dynamic_cast<PathListModel *>(m_ui->listView->model());
}


//-------------------------------------------------
//  PathsDialog ctor
//-------------------------------------------------

PathsDialog::PathListModel::PathListModel(QObject &parent, std::function<QString(const QString &)> &&applySubstitutionsFunc)
	: QAbstractListModel(&parent)
	, m_applySubstitutionsFunc(std::move(applySubstitutionsFunc))
{
}


//-------------------------------------------------
//  PathListModel::setPaths
//-------------------------------------------------

void PathsDialog::PathListModel::setPaths(const QString &paths, bool applySubstitutions, bool expandable, bool supportsFiles, bool supportsDirectories)
{
	beginResetModel();

	m_applySubstitutions = applySubstitutions;
	m_expandable = expandable;
	m_supportsFiles = supportsFiles;
	m_supportsDirectories = supportsDirectories;
	m_entries.clear();
	if (!paths.isEmpty())
	{
		for (const QString &nativePath : util::string_split(paths, [](auto ch) { return ch == ';'; }))
		{
			QString path = QDir::fromNativeSeparators(nativePath);
			Entry &entry = m_entries.emplace_back();
			entry.m_isValid = validateAndCanonicalize(path);
			entry.m_path = std::move(path);
		}
	}

	endResetModel();
}


//-------------------------------------------------
//  PathListModel::paths
//-------------------------------------------------

QString PathsDialog::PathListModel::paths() const
{
	QString result;
	{
		bool isFirst = true;
		QTextStream textStream(&result);
		for (const Entry &entry : m_entries)
		{
			// ignore empty paths
			if (!entry.m_path.isEmpty())
			{
				if (isFirst)
					isFirst = false;
				else
					textStream << ';';
				textStream << QDir::toNativeSeparators(entry.m_path);
			}
		}
	}
	return result;
}


//-------------------------------------------------
//  PathListModel::setPath
//-------------------------------------------------

void PathsDialog::PathListModel::setPath(int index, QString &&path)
{
	// sanity check
	int maxIndex = util::safe_static_cast<int>(m_entries.size()) + (hasExpandEntry() ? 1 : 0);
	if (index < 0 || index >= maxIndex)
		throw false;

	if (path.isEmpty())
	{
		// setting an empty path clears it out
		erase(index);
	}
	else
	{
		// we're updating or inserting something - tell Qt that something is changing
		beginResetModel();

		// are we appending?  if so create a new entry
		if (index == m_entries.size())
		{
			assert(hasExpandEntry());
			m_entries.insert(m_entries.end(), Entry("", true));
		}

		// set the path
		m_entries[index].m_isValid = validateAndCanonicalize(path);
		m_entries[index].m_path = std::move(path);

		// we're done!
		endResetModel();
	}
}


//-------------------------------------------------
//  PathListModel::path
//-------------------------------------------------

const QString &PathsDialog::PathListModel::path(int index) const
{
	assert(index >= 0 && index < m_entries.size());
	return m_entries[index].m_path;
}


//-------------------------------------------------
//  PathListModel::pathCount
//-------------------------------------------------

int PathsDialog::PathListModel::pathCount() const
{
	return util::safe_static_cast<int>(m_entries.size());
}


//-------------------------------------------------
//  PathListModel::insert
//-------------------------------------------------

void PathsDialog::PathListModel::insert(int position)
{
	assert(position >= 0 && position <= m_entries.size());

	beginResetModel();
	m_entries.insert(m_entries.begin() + position, Entry("", true));
	endResetModel();
}


//-------------------------------------------------
//  PathListModel::erase
//-------------------------------------------------

void PathsDialog::PathListModel::erase(int position)
{
	assert(position >= 0 && position < m_entries.size());

	beginResetModel();
	m_entries.erase(m_entries.begin() + position);
	endResetModel();
}


//-------------------------------------------------
//  PathListModel::rowCount
//-------------------------------------------------

int PathsDialog::PathListModel::rowCount(const QModelIndex &parent) const
{
	return pathCount() + (hasExpandEntry() ? 1 : 0);
}


//-------------------------------------------------
//  PathListModel::data
//-------------------------------------------------

QVariant PathsDialog::PathListModel::data(const QModelIndex &index, int role) const
{
	// identify the entry
	const Entry *entry = index.row() < m_entries.size()
		? &m_entries[index.row()]
		: nullptr;

	QVariant result;
	switch (role)
	{
	case Qt::DisplayRole:
		if (entry)
			result = QDir::toNativeSeparators(entry->m_path);
		else if (hasExpandEntry() && index.row() == m_entries.size())
			result = "<               >";
		break;

	case Qt::ForegroundRole:
		result = !entry || entry->m_isValid
			? QColorConstants::Black
			: QColorConstants::Red;
		break;

	case Qt::EditRole:
		if (entry)
			result = QDir::toNativeSeparators(entry->m_path);
		break;
	}
	return result;
}


//-------------------------------------------------
//  PathListModel::setData
//-------------------------------------------------

bool PathsDialog::PathListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	bool success = false;

	switch (role)
	{
	case Qt::EditRole:
		// convert the variant data to the format we want it in
		QString pathString = QDir::fromNativeSeparators(value.toString());

		// and set it
		setPath(index.row(), std::move(pathString));

		// success!
		success = true;
		break;
	}
	return success;
}


//-------------------------------------------------
//  PathListModel::flags
//-------------------------------------------------

Qt::ItemFlags PathsDialog::PathListModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::ItemIsEnabled;

	return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}


//-------------------------------------------------
//  PathListModel::validateAndCanonicalize
//-------------------------------------------------

bool PathsDialog::PathListModel::validateAndCanonicalize(QString &path) const
{
	// apply substitutions (e.g. - $(MAMEPATH) with actual MAME path)
	QString pathAfterSubstitutions = m_applySubstitutions
		? m_applySubstitutionsFunc(path)
		: path;

	// check the file
	bool isValid = false;
	QFileInfo fileInfo(pathAfterSubstitutions);
	if (fileInfo.isFile())
	{
		isValid = m_supportsFiles;
	}
	else if (fileInfo.isDir())
	{
		isValid = m_supportsDirectories;
		if (!path.endsWith('/'))
			path += '/';
	}
	return isValid;
}


//-------------------------------------------------
//  PathListModel::hasExpandEntry
//-------------------------------------------------

bool PathsDialog::PathListModel::hasExpandEntry() const
{
	return m_entries.size() == 0
		|| (m_expandable && !m_entries[m_entries.size() - 1].m_path.isEmpty());
}


//-------------------------------------------------
//  PathListModel::Entry ctor
//-------------------------------------------------

PathsDialog::PathListModel::Entry::Entry(QString &&path, bool isValid)
	: m_path(std::move(path))
	, m_isValid(isValid)
{
}
