/***************************************************************************

    dialogs/switches.h

    Configuration & DIP Switches customization dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_SWITCHES_H
#define DIALOGS_SWITCHES_H

// bletchmame headers
#include "dialogs/inputs_base.h"
#include "runmachinetask.h"
#include "info.h"

// standard headers
#include <vector>


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

class SwitchesDialog : public InputsDialogBase
{
public:
	SwitchesDialog(QWidget *parent, ISwitchesHost &host, status::input::input_class input_class, info::machine machine);

protected:
	virtual void OnRestoreButtonPressed() override;

private:
	ISwitchesHost &						m_host;
	info::machine						m_machine;

	void UpdateInputs();
	std::unordered_map<std::uint32_t, QString> GetChoices(const status::input &input) const;
};


#endif // DIALOGS_SWITCHES_H
