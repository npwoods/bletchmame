/***************************************************************************

    dlginvoke.cpp

    Arbitrary command invocation dialog (essentially a development feature)

***************************************************************************/

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>

#include "dlginvoke.h"
#include "runmachinetask.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class InvokeArbitraryCommandDialog : public wxDialog
	{
	public:
		InvokeArbitraryCommandDialog(wxWindow &parent, std::shared_ptr<RunMachineTask> &&task);

	private:
		std::shared_ptr<RunMachineTask>	m_task;
		wxTextCtrl *					m_text_control;

		template<typename TControl, typename... TArgs>
		TControl &AddControl(std::unique_ptr<wxBoxSizer> &sizer, int flags, TArgs&&... args);

		void OnInvoke();
	};
};

InvokeArbitraryCommandDialog::InvokeArbitraryCommandDialog(wxWindow &parent, std::shared_ptr<RunMachineTask> &&task)
	: wxDialog(&parent, wxID_ANY, "Invoke Arbitrary Command", wxDefaultPosition, wxSize(400, 100))
	, m_task(std::move(task))
	, m_text_control(nullptr)
{
	assert(m_task);
	int id = wxID_LAST;
	std::unique_ptr<wxBoxSizer> main_sizer = std::make_unique<wxBoxSizer>(wxVERTICAL);

	// main text control
	m_text_control = &AddControl<wxTextCtrl>(main_sizer, wxALL | wxEXPAND, id++);

	// buttons
	std::unique_ptr<wxBoxSizer> button_sizer = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
	wxButton &invoke_button(AddControl<wxButton>(button_sizer, wxALL, id++,		wxT("Invoke")));
	wxButton &close_button(	AddControl<wxButton>(button_sizer, wxALL, wxID_OK,	wxT("Close")));
	main_sizer->Add(button_sizer.release(), 1, wxALL | wxALIGN_RIGHT);

	// specify main sizer
	SetSizer(main_sizer.release());

	// button invocations
	Bind(wxEVT_BUTTON,		[this](auto &)		{ OnInvoke(); },											invoke_button.GetId());
	Bind(wxEVT_UPDATE_UI,	[this](auto &event)	{ event.Enable(true/*!m_text_control->GetLabel().IsEmpty()*/); },	invoke_button.GetId());
	(void)close_button;
}

//-------------------------------------------------
//  AddControl
//-------------------------------------------------

template<typename TControl, typename... TArgs>
TControl &InvokeArbitraryCommandDialog::AddControl(std::unique_ptr<wxBoxSizer> &sizer, int flags, TArgs&&... args)
{
	TControl *control = new TControl(this, std::forward<TArgs>(args)...);
	sizer->Add(control, 0, flags, 4);
	return *control;
}


//-------------------------------------------------
//  OnInvoke
//-------------------------------------------------

void InvokeArbitraryCommandDialog::OnInvoke()
{
	// issue the command
	wxString command_line = m_text_control->GetValue();
	m_task->IssueFullCommandLine(command_line);

	// and clear
	m_text_control->Clear();
}


//-------------------------------------------------
//  show_invoke_arbitrary_command_dialog
//-------------------------------------------------

bool show_invoke_arbitrary_command_dialog(wxWindow &parent, MameClient &client)
{
	InvokeArbitraryCommandDialog dialog(parent, client.GetCurrentTask<RunMachineTask>());
	return dialog.ShowModal() == wxID_OK;
}
