/***************************************************************************

	profilelistitemmodel.cpp

	QAbstractItemModel implementation for the profile list

***************************************************************************/

#include <QDir>
#include <QFileSystemWatcher>

#include "profilelistitemmodel.h"
#include "iconloader.h"
#include "prefs.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

ProfileListItemModel::ProfileListItemModel(QObject *parent, Preferences &prefs, info::database &infoDb, IconLoader &iconLoader)
    : QAbstractItemModel(parent)
    , m_prefs(prefs)
    , m_infoDb(infoDb)
    , m_iconLoader(iconLoader)
    , m_fileSystemWatcher(*new QFileSystemWatcher(this))
{
    connect(&m_fileSystemWatcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path)
    {
        // do the refresh
        refresh(true, false);

        // do the one time callback, if present
        if (m_fswCallback)
        {
            m_fswCallback();
            m_fswCallback = std::function<void()>();
        }
    });
}


//-------------------------------------------------
//  refresh
//-------------------------------------------------

void ProfileListItemModel::refresh(bool updateProfileList, bool updateFileSystemWatcher)
{
    // sanity checks
    assert(updateProfileList || updateFileSystemWatcher);

    // get the paths
    QStringList paths = m_prefs.GetSplitPaths(Preferences::global_path_type::PROFILES);

    // now update the list if we are asked to
    if (updateProfileList)
    {
        beginResetModel();
        m_profiles = profiles::profile::scan_directories(paths);
        endResetModel();
    }

    // now update the file system watcher if we're asked to
    if (updateFileSystemWatcher)
    {
        // remove any existing directories
        QStringList currentDirs = m_fileSystemWatcher.directories();
        if (!currentDirs.empty())
            m_fileSystemWatcher.removePaths(currentDirs);

        for (const QString &path : paths)
        {
            QDir dir(path);
            if (dir.exists())
                m_fileSystemWatcher.addPath(path);
        }
    }
}


//-------------------------------------------------
//  setOneTimeFswCallback
//-------------------------------------------------

void ProfileListItemModel::setOneTimeFswCallback(std::function<void()> &&fswCallback)
{
    m_fswCallback = std::move(fswCallback);
}


//-------------------------------------------------
//  getProfileByIndex
//-------------------------------------------------

const profiles::profile &ProfileListItemModel::getProfileByIndex(int index) const
{
    return m_profiles[index];
}


//-------------------------------------------------
//  findProfileIndex
//-------------------------------------------------

QModelIndex ProfileListItemModel::findProfileIndex(const QString &path) const
{
    for (int i = 0; i < m_profiles.size(); i++)
    {
        if (m_profiles[i].path() == path)
            return createIndex(i, 0);
    }
    return QModelIndex();
}


//-------------------------------------------------
//  index
//-------------------------------------------------

QModelIndex ProfileListItemModel::index(int row, int column, const QModelIndex &parent) const
{
    return createIndex(row, column);
}


//-------------------------------------------------
//  parent
//-------------------------------------------------

QModelIndex ProfileListItemModel::parent(const QModelIndex &child) const
{
    return QModelIndex();
}


//-------------------------------------------------
//  rowCount
//-------------------------------------------------

int ProfileListItemModel::rowCount(const QModelIndex &parent) const
{
    return util::safe_static_cast<int>(m_profiles.size());
}


//-------------------------------------------------
//  columnCount
//-------------------------------------------------

int ProfileListItemModel::columnCount(const QModelIndex &parent) const
{
    return (int)Column::Count;
}


//-------------------------------------------------
//  data
//-------------------------------------------------

QVariant ProfileListItemModel::data(const QModelIndex &index, int role) const
{
    QVariant result;
    if (index.isValid()
        && index.row() >= 0
        && index.row() < m_profiles.size())
    {
        const profiles::profile &p = m_profiles[index.row()];
        Column column = (Column)index.column();

        switch (role)
        {
        case Qt::DisplayRole:
            switch (column)
            {
            case Column::Name:
                result = p.name();
                break;
            case Column::Machine:
                result = p.machine();
                break;
            case Column::Path:
                result = QDir::toNativeSeparators(p.path());
                break;
            }
            break;

        case Qt::DecorationRole:
            if (column == Column::Name)
            {
                std::optional<info::machine> machine = m_infoDb.find_machine(p.machine());
                if (machine)
                    result = m_iconLoader.getIcon(*machine);
            }
            break;
        }
    }
    return result;
}


//-------------------------------------------------
//  headerData
//-------------------------------------------------

QVariant ProfileListItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant result;
    if (orientation == Qt::Orientation::Horizontal && section >= 0 && section < (int)Column::Count)
    {
        Column column = (Column)section;
        switch (role)
        {
        case Qt::DisplayRole:
            switch (column)
            {
            case Column::Name:
                result = "Name";
                break;
            case Column::Machine:
                result = "Machine";
                break;
            case Column::Path:
                result = "Path";
                break;
            }
            break;
        }
    }
    return result;
}


//-------------------------------------------------
//  flags
//-------------------------------------------------

Qt::ItemFlags ProfileListItemModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags result = QAbstractItemModel::flags(index);
    if (index.column() == (int)Column::Name)
        result |= Qt::ItemIsEditable;
    return result;
}
