/***************************************************************************

	dialogs/choosesw.h

	Choose Software dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_CHOOSESW_H
#define DIALOGS_CHOOSESW_H

// bletchmame headers
#include "softwarelist.h"
#include "tableviewmanager.h"

// Qt headers
#include <QDialog>

// standard headers
#include <memory>


QT_BEGIN_NAMESPACE
namespace Ui { class ChooseSoftlistPartDialog; }
QT_END_NAMESPACE
class SoftwareListItemModel;


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class ChooseSoftlistPartDialog : public QDialog
{
public:
	static const TableViewManager::Description s_tableViewDesc;

	ChooseSoftlistPartDialog(QWidget *parent, Preferences &prefs, const software_list_collection &software_col, const QString &dev_interface);
	~ChooseSoftlistPartDialog();
	QString selection() const;

private:
	std::unique_ptr<Ui::ChooseSoftlistPartDialog>	m_ui;
	SoftwareListItemModel *							m_itemModel;
};


#endif // DIALOGS_CHOOSESW_H
