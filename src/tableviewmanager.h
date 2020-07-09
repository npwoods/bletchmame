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
QT_END_NAMESPACE

class Preferences;
struct CollectionViewDesc;

class TableViewManager : public QObject
{
public:
    TableViewManager(QTableView &tableView, Preferences &prefs, const CollectionViewDesc &desc);

private:
    Preferences &               m_prefs;
    const CollectionViewDesc &  m_desc;

    const QTableView &parentAsTableView() const;
    void persistColumnPrefs();
};

#endif // TABLEVIEWMANAGER_H
