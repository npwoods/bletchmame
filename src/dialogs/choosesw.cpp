﻿/***************************************************************************

	dialogs/choosesw.cpp

	Choose Software dialog

***************************************************************************/

// bletchmame headers
#include "dialogs/choosesw.h"
#include "ui_choosesw.h"
#include "softwarelistitemmodel.h"
#include "tableviewmanager.h"

// Qt headers
#include <QSortFilterProxyModel>


//**************************************************************************
//  CONSTANTS
//**************************************************************************

static const TableViewManager::ColumnDesc s_softwareListTableViewColumns[] =
{
	{ u8"name",			85,		true },
	{ u8"description",	220,	true },
	{ u8"year",			50,		true },
	{ u8"publisher",	190,	true },
	{ u8"file",			120,	false }
};

const TableViewManager::Description ChooseSoftlistPartDialog::s_tableViewDesc =
{
	SOFTLIST_VIEW_DESC_NAME,
	(int)SoftwareListItemModel::Column::Name,
	s_softwareListTableViewColumns
};


static_assert(std::size(s_softwareListTableViewColumns) == util::enum_count<SoftwareListItemModel::Column>());


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

ChooseSoftlistPartDialog::ChooseSoftlistPartDialog(QWidget *parent, Preferences &prefs, const software_list_collection &software_col, const QString &dev_interface)
	: QDialog(parent)
	, m_itemModel(nullptr)
{
	// set up UI
	m_ui = std::make_unique<Ui::ChooseSoftlistPartDialog>();
	m_ui->setupUi(this);

	// use the same model used for software lists
	m_itemModel = new SoftwareListItemModel(nullptr, {}, this);
	m_itemModel->load(software_col, true, dev_interface);

	// set up a TableViewManager
	TableViewManager::setup(*m_ui->tableView, *m_itemModel, m_ui->searchBox, prefs, s_tableViewDesc);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

ChooseSoftlistPartDialog::~ChooseSoftlistPartDialog()
{
}


//-------------------------------------------------
//  selection
//-------------------------------------------------

QString ChooseSoftlistPartDialog::selection() const
{
	QModelIndexList selection = m_ui->tableView->selectionModel()->selectedIndexes();
	QModelIndex actualIndex = dynamic_cast<QSortFilterProxyModel *>(m_ui->tableView->model())->mapToSource(selection[0]);
	const software_list::software &sw = m_itemModel->getSoftwareByIndex(actualIndex.row());
	return sw.name();
}
