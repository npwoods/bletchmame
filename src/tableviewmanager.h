/***************************************************************************

    tableviewmanager.h

    Governs behaviors of tab views, and syncs with preferences

***************************************************************************/

#pragma once

#ifndef TABLEVIEWMANAGER_H
#define TABLEVIEWMANAGER_H

// Qt headers
#include <QObject>

// standard headers
#include <span>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

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
		const char8_t * m_id;
		int				m_defaultWidth;
		bool			m_defaultShown;
	};

	struct Description
	{
		const char8_t *				m_name;
		int							m_keyColumnIndex;
		std::span<const ColumnDesc>	m_columns;
	};

	// static methods
	static TableViewManager &setup(QTableView &tableView, QAbstractItemModel &itemModel, QLineEdit *lineEdit, Preferences &prefs, const Description &desc, std::function<void(const QString &)> &&selectionChangedCallback = { });

private:
	Preferences &                           m_prefs;
	const Description &                     m_desc;
	std::function<void(const QString &)>    m_selectionChangedCallback;
	QSortFilterProxyModel *                 m_proxyModel;
	bool                                    m_currentlyApplyingColumnPrefs;

	// ctor
	TableViewManager(QTableView &tableView, QAbstractItemModel &itemModel, QLineEdit *lineEdit, Preferences &prefs, const Description &desc, std::function<void(const QString &)> &&selectionChangedCallback);

	// methods
	const QTableView &parentAsTableView() const;
	QTableView &parentAsTableView();
	void applyColumnPrefs();
	void applyColumnOrdering(std::span<const std::optional<int>> logicalOrdering);
	void persistColumnPrefs();
	void applySelectedValue();
	void headerContextMenuRequested(const QPoint &pos);
	void customizeFields();
};

#endif // TABLEVIEWMANAGER_H
