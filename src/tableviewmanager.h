/***************************************************************************

    tableviewmanager.h

    Governs behaviors of tab views, and syncs with preferences

***************************************************************************/

#pragma once

#ifndef TABLEVIEWMANAGER_H
#define TABLEVIEWMANAGER_H

#include <QObject>

QT_BEGIN_NAMESPACE
class QAbstractItemModel;
class QLineEdit;
class QSortFilterProxyModel;
class QTableView;
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

    // ctor
    TableViewManager(QTableView &tableView, QAbstractItemModel &itemModel, QLineEdit *lineEdit, Preferences &prefs, const Description &desc);

    // accessors
    QSortFilterProxyModel &sortFilterProxyModel() { return *m_proxyModel; }

    // methods
    void applyColumnPrefs();

private:
    Preferences &           m_prefs;
    const Description &     m_desc;
    int                     m_columnCount;
    QSortFilterProxyModel * m_proxyModel;
    bool                    m_currentlyApplyingColumnPrefs;

    const QTableView &parentAsTableView() const;
    void persistColumnPrefs();
};

#endif // TABLEVIEWMANAGER_H
