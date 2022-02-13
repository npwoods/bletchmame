/***************************************************************************

    dialogs/console.h

    Arbitrary command invocation dialog (essentially a development feature)

***************************************************************************/

#pragma once

#ifndef DIALOGS_CONSOLE_H
#define DIALOGS_CONSOLE_H

// bletchmame headers
#include "runmachinetask.h"

// Qt headers
#include <QDialog>


QT_BEGIN_NAMESPACE
namespace Ui { class ConsoleDialog; }
QT_END_NAMESPACE


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class ChatterEvent;

// ======================> IConsoleDialogHost

class IConsoleDialogHost
{
public:
	virtual void setChatterListener(std::function<void(const ChatterEvent &)> &&func) = 0;
};


// ======================> ConsoleDialog

class ConsoleDialog : public QDialog
{
public:
	ConsoleDialog(QWidget *parent, RunMachineTask::ptr &&task, IConsoleDialogHost &host);
	~ConsoleDialog();

private:
	IConsoleDialogHost &				m_host;
	RunMachineTask::ptr					m_task;
	std::unique_ptr<Ui::ConsoleDialog>	m_ui;

	void onInvoke();
	void onChatter(const ChatterEvent &evt);
	static bool isChatterPing(const ChatterEvent &evt);
};

#endif // DIALOGS_CONSOLE_H
