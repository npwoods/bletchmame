/***************************************************************************

    dialogs/switches.h

    Configuration & DIP Switches customization dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_SWITCHES_H
#define DIALOGS_SWITCHES_H

#include <QDialog>
#include <vector>

#include "runmachinetask.h"
#include "info.h"
#include "status.h"

QT_BEGIN_NAMESPACE
namespace Ui { class InputsDialog; }
QT_END_NAMESPACE

struct Input;


// ======================> ISwitchesHost

class ISwitchesHost
{
public:
	virtual const std::vector<status::input> &GetInputs() = 0;
	virtual void SetInputValue(const QString &port_tag, ioport_value mask, ioport_value value) = 0;
};


// ======================> SwitchesDialog

class SwitchesDialog : public QDialog
{
	Q_OBJECT
public:
	SwitchesDialog(QWidget &parent, const QString &title, ISwitchesHost &host, status::input::input_class input_class, info::machine machine);
	~SwitchesDialog();

private:
	std::unique_ptr<Ui::InputsDialog>	m_ui;
	ISwitchesHost &						m_host;
	status::input::input_class			m_input_class;
	info::machine						m_machine;

	void UpdateInputs();
	std::unordered_map<std::uint32_t, QString> GetChoices(const status::input &input) const;
};


#endif // DIALOGS_SWITCHES_H
