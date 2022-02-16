/***************************************************************************

	dialogs/loading.cpp

	"loading MAME Info..." modal dialog

***************************************************************************/

// bletchmame headers
#include "dialogs/loading.h"
#include "ui_loading.h"

// Qt headers
#include <QTimer>

// standard headers
#include <memory>


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

LoadingDialog::LoadingDialog(QWidget &parent, std::function<void()> &&progressCallback)
	: m_progressCallback(std::move(progressCallback))
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
	// update the progress label
	QString text = machineName == machineDescription
		? machineDescription
		: QString("%1 (%2)").arg(machineDescription, machineName);
	m_ui->progressLabel->setText(text);

	// invoke the callback
	if (m_progressCallback)
		m_progressCallback();
}
