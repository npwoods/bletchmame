/***************************************************************************

	dialogs/loading.h

	"loading MAME Info..." modal dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_LOADING_H
#define DIALOGS_LOADING_H

// Qt headers
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
	LoadingDialog(QWidget &parent, std::function<void()> &&progressCallback);
	~LoadingDialog();

	// methods
	void progress(const QString &machineName, const QString &machineDescription);

private:
	std::unique_ptr<Ui::LoadingDialog>	m_ui;
	std::function<void()>				m_progressCallback;
};

#endif // DIALOGS_LOADING_H
