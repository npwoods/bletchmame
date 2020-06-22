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

LoadingDialog::LoadingDialog(QWidget &parent, std::function<bool()> &&pollCompletionCheck)
	: m_pollCompletionCheck(pollCompletionCheck)
{
	m_ui = std::make_unique<Ui::LoadingDialog>();
	m_ui->setupUi(this);

	// set up the timer
	QTimer &timer = *new QTimer(this);
	connect(&timer, &QTimer::timeout, this, &LoadingDialog::poll);
	timer.start(100);
}


//-------------------------------------------------
//	dtor
//-------------------------------------------------

LoadingDialog::~LoadingDialog()
{
}


//-------------------------------------------------
//	poll
//-------------------------------------------------

void LoadingDialog::poll()
{
	if (m_pollCompletionCheck())
		done(QDialog::DialogCode::Accepted);
}
