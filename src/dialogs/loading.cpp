/***************************************************************************

	dialogs/loading.cpp

	"loading MAME Info..." modal dialog

***************************************************************************/

#include <memory>
#include <QTimer>

#include "dialogs/loading.h"
#include "ui_loading.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

LoadingDialog::LoadingDialog(QWidget &parent)
{
	m_ui = std::make_unique<Ui::LoadingDialog>();
	m_ui->setupUi(this);
}


//-------------------------------------------------
//	dtor
//-------------------------------------------------

LoadingDialog::~LoadingDialog()
{
}


//-------------------------------------------------
//	progress
//-------------------------------------------------

void LoadingDialog::progress(const QString &machineName, const QString &machineDescription)
{
	QString text = machineName == machineDescription
		? machineDescription
		: QString("%1 (%2)").arg(machineDescription, machineName);
	m_ui->progressLabel->setText(text);
}
