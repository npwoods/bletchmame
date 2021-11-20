/***************************************************************************

	dialogs/resetprefs.cpp

	Reset Preferences dialog

***************************************************************************/

#include <QPushButton>

#include "resetprefs.h"
#include "ui_resetprefs.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

ResetPreferencesDialog::ResetPreferencesDialog(QWidget &parent)
	: QDialog(&parent)
{
	// set up the UI
	m_ui = std::make_unique<Ui::ResetPreferencesDialog>();
	m_ui->setupUi(this);

	// connect clicked events
	connect(m_ui->resetUiCheckBox,		&QAbstractButton::toggled, this, [this](bool checked) { updateOkButton(); });
	connect(m_ui->resetPathsCheckBox,	&QAbstractButton::toggled, this, [this](bool checked) { updateOkButton(); });

	// update the OK button
	updateOkButton();
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

ResetPreferencesDialog::~ResetPreferencesDialog()
{
}


//-------------------------------------------------
//  updateOkButton
//-------------------------------------------------

void ResetPreferencesDialog::updateOkButton()
{
	// anything checked?
	bool anyChecked = isResetUiChecked() || isResetPathsChecked();

	// update the ok button accordingly
	QPushButton &okButton = *m_ui->buttonBox->button(QDialogButtonBox::Ok);
	okButton.setEnabled(anyChecked);
}


//-------------------------------------------------
//  isResetUiChecked
//-------------------------------------------------

bool ResetPreferencesDialog::isResetUiChecked() const
{
	return m_ui->resetUiCheckBox->isChecked();
}


//-------------------------------------------------
//  isResetPathsChecked
//-------------------------------------------------

bool ResetPreferencesDialog::isResetPathsChecked() const
{
	return m_ui->resetPathsCheckBox->isChecked();
}
