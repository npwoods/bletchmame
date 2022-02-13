/***************************************************************************

	dialogs/resetprefs.cpp

	Reset Preferences dialog

***************************************************************************/

// bletchmame headers
#include "resetprefs.h"
#include "ui_resetprefs.h"

// Qt headers
#include <QPushButton>


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  allCheckBoxes
//-------------------------------------------------

auto ResetPreferencesDialog::allCheckBoxes()
{
	std::array<QCheckBox *, 4> results =
	{
		m_ui->resetUiCheckBox,
		m_ui->resetUiCheckBox,
		m_ui->resetPathsCheckBox,
		m_ui->resetFoldersCheckBox
	};
	return results;
}


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
	for (QCheckBox *checkBox : allCheckBoxes())
		connect(checkBox, &QAbstractButton::toggled, this, [this](bool checked) { updateOkButton(); });

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
	bool anyChecked = false;
	for (QCheckBox *checkBox : allCheckBoxes())
	{
		if (checkBox->isChecked())
		{
			anyChecked = true;
			break;
		}
	}

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


//-------------------------------------------------
//  isResetFoldersChecked
//-------------------------------------------------

bool ResetPreferencesDialog::isResetFoldersChecked() const
{
	return m_ui->resetFoldersCheckBox->isChecked();
}
