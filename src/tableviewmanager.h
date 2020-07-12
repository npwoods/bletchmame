/***************************************************************************

    tableviewmanager.h

    Governs behaviors of tab views, and syncs with preferences

***************************************************************************/

#pragma once

#ifndef TABLEVIEWMANAGER_H
#define TABLEVIEWMANAGER_H

#include <QObject>

QT_BEGIN_NAMESPACE
class QTableView;
class QAbstractItemModel;
class QSortFilterProxyModel;
QT_END_NAMESPACE

class Preferences;


// ======================> TableViewManager

class TableViewManager : public QObject
{
public:
    struct ColumnDesc
    {
        const char *    m_id;
        int				m_defaultWidth;
    };

    struct Description
    {
        const char *        m_name;
        int                 m_keyColumnIndex;
        const ColumnDesc *  m_columns;
    };

    TableViewManager(QTableView &tableView, QAbstractItemModel &itemModel, QSortFilterProxyModel &proxyModel, Preferences &prefs, const Description &desc);

private:
    Preferences &       m_prefs;
    const Description & m_desc;
    int                 m_columnCount;

    const QTableView &parentAsTableView() const;
    void persistColumnPrefs();
};

#endif // TABLEVIEWMANAGER_H
