/***************************************************************************

	profilelistitemmodel.cpp

	QAbstractItemModel implementation for the profile list

***************************************************************************/

#include <QDir>
#include <QFileSystemWatcher>

#include "profilelistitemmodel.h"
#include "prefs.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

ProfileListItemModel::ProfileListItemModel(QObject *parent, Preferences &prefs)
    : QAbstractItemModel(parent)
    , m_prefs(prefs)
    , m_fileSystemWatcher(*new QFileSystemWatcher(this))
{
    connect(&m_fileSystemWatcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path)
    {
        refresh(true, false);
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
    std::vector<QString> paths = m_prefs.GetSplitPaths(Preferences::global_path_type::PROFILES);

    // normalize them
    for (QString &path : paths)
        path = QDir::fromNativeSeparators(path);

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
        && index.row() < m_profiles.size()
        && role == Qt::DisplayRole)
    {
        const profiles::profile &p = m_profiles[index.row()];
        Column column = (Column)index.column();
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
