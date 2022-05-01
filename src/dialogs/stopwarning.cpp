/***************************************************************************

    dialogs/stopwarning.cpp

    BletchMAME end emulation prompt

***************************************************************************/

// bletchmame headers
#include "stopwarning.h"
#include "ui_stopwarning.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

StopWarningDialog::StopWarningDialog(QWidget *parent, WarningType warningType)
	: QDialog(parent)
{
	// set up Qt UI widgets
	m_ui = std::make_unique<Ui::StopWarningDialog>();
	m_ui->setupUi(this);

	// adjust the warning string
	QString warningString;
	switch (warningType)
	{
	case WarningType::Stop:
		warningString = "stop";
		break;
	case WarningType::Exit:
		warningString = "exit";
		break;
	default:
		throw false;
	}
	QString text = m_ui->mainLabel->text().arg(warningString);
	m_ui->mainLabel->setText(text);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

StopWarningDialog::~StopWarningDialog()
{
}


//-------------------------------------------------
//  showThisChecked
//-------------------------------------------------

bool StopWarningDialog::showThisChecked() const
{
	return m_ui->showThisMessageCheckBox->isChecked();
}


//-------------------------------------------------
//  setShowThisChecked
//-------------------------------------------------

void StopWarningDialog::setShowThisChecked(bool checked)
{
	m_ui->showThisMessageCheckBox->setChecked(checked);
}
