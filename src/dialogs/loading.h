/***************************************************************************

	dialogs/loading.h

	"loading MAME Info..." modal dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_LOADING_H
#define DIALOGS_LOADING_H

#include <QDialog>
#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui { class LoadingDialog; }
QT_END_NAMESPACE


// ======================> LoadingDialog
class LoadingDialog : public QDialog
{
	Q_OBJECT

public:
	LoadingDialog(QWidget &parent);
	~LoadingDialog();

	// methods
	void progress(const QString &machineName, const QString &machineDescription);

private:
	std::unique_ptr<Ui::LoadingDialog> m_ui;
};

#endif // DIALOGS_LOADING_H
