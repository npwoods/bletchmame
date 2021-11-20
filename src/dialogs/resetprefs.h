/***************************************************************************

	dialogs/resetprefs.h

	Reset Preferences dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_RESETPREFS_H
#define DIALOGS_RESETPREFS_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class ResetPreferencesDialog; }
QT_END_NAMESPACE


// ======================> ResetPreferencesDialog

class ResetPreferencesDialog : public QDialog
{
	Q_OBJECT

public:
	ResetPreferencesDialog(QWidget &parent);
	~ResetPreferencesDialog();

	// accessors
	bool isResetUiChecked() const;
	bool isResetPathsChecked() const;

private:
	std::unique_ptr<Ui::ResetPreferencesDialog> m_ui;

	void updateOkButton();
};

#endif // DIALOGS_RESETPREFS_H
