/***************************************************************************

    dialogs/console.h

    Arbitrary command invocation dialog (essentially a development feature)

***************************************************************************/

#pragma once

#ifndef DIALOGS_CONSOLE_H
#define DIALOGS_CONSOLE_H

#include <QDialog.h>
#include "runmachinetask.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ConsoleDialog; }
QT_END_NAMESPACE


class ChatterEvent;

class IConsoleDialogHost
{
public:
	virtual void SetChatterListener(std::function<void(const ChatterEvent &)> &&func) = 0;
};


class ConsoleDialog : public QDialog
{
public:
	ConsoleDialog(QWidget *parent, std::shared_ptr<RunMachineTask> &&task, IConsoleDialogHost &host);
	~ConsoleDialog();

private:
	IConsoleDialogHost &				m_host;
	std::shared_ptr<RunMachineTask>		m_task;
	std::unique_ptr<Ui::ConsoleDialog>	m_ui;

	void onInvoke();
	void onChatter(const ChatterEvent &evt);
	static bool isChatterPing(const ChatterEvent &evt);
};

#endif // DIALOGS_CONSOLE_H
