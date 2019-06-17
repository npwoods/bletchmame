/***************************************************************************

    dlginvoke.cpp

    Arbitrary command invocation dialog (essentially a development feature)

***************************************************************************/

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>
#include <wx/checkbox.h>

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
		InvokeArbitraryCommandDialog(wxWindow &parent, std::shared_ptr<RunMachineTask> &&task, IInvokeArbitraryCommandDialogHost &host);
		~InvokeArbitraryCommandDialog();

	private:
		IInvokeArbitraryCommandDialogHost & m_host;
		std::shared_ptr<RunMachineTask>		m_task;
		wxTextCtrl *						m_display_control;
		wxTextCtrl *						m_input_control;
		wxCheckBox *						m_filter_ping_control;

		template<typename TControl, typename... TArgs>
		TControl &AddControl(std::unique_ptr<wxBoxSizer> &sizer, int proportion, int flags, TArgs&&... args);

		void OnInvoke();
		void OnChatter(Chatter &&chatter);
		static bool IsChatterPing(const Chatter &chatter);
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

InvokeArbitraryCommandDialog::InvokeArbitraryCommandDialog(wxWindow &parent, std::shared_ptr<RunMachineTask> &&task, IInvokeArbitraryCommandDialogHost &host)
	: wxDialog(&parent, wxID_ANY, "Invoke Arbitrary Command", wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX | wxRESIZE_BORDER)
	, m_host(host)
	, m_task(std::move(task))
	, m_display_control(nullptr)
	, m_input_control(nullptr)
	, m_filter_ping_control(nullptr)
{
	assert(m_task);
	int id = wxID_LAST;
	std::unique_ptr<wxBoxSizer> main_sizer = std::make_unique<wxBoxSizer>(wxVERTICAL);

	// display control, where the results are displayed
	m_display_control = &AddControl<wxTextCtrl>(main_sizer, 1, wxALL | wxEXPAND, id++, wxEmptyString, wxDefaultPosition, wxSize(400, 150), wxTE_RICH | wxTE_READONLY | wxTE_MULTILINE);

	// input control, where commands are entered
	m_input_control = &AddControl<wxTextCtrl>(main_sizer, 0, wxALL | wxEXPAND, id++, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	m_input_control->SetFocus();

	// buttons
	std::unique_ptr<wxBoxSizer> button_sizer = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
	m_filter_ping_control	= &AddControl<wxCheckBox>(button_sizer, 0, wxALL, id++,	wxT("Filter out pings"));
	wxButton &invoke_button	= AddControl<wxButton>(button_sizer, 0, wxALL, id++,	wxT("Invoke"));
	wxButton &close_button	= AddControl<wxButton>(button_sizer, 0, wxALL, wxID_OK,	wxT("Close"));
	m_filter_ping_control->SetValue(true);
	main_sizer->Add(button_sizer.release(), 0, wxALIGN_RIGHT);

	// specify main sizer
	SetSizerAndFit(main_sizer.release());

	// events
	Bind(wxEVT_TEXT_ENTER,	[this](auto &)		{ OnInvoke(); },											m_input_control->GetId());
	Bind(wxEVT_BUTTON,		[this](auto &)		{ OnInvoke(); },											invoke_button.GetId());
	Bind(wxEVT_UPDATE_UI,	[this](auto &event)	{ event.Enable(!m_input_control->GetValue().IsEmpty()); },	invoke_button.GetId());
	(void)close_button;

	// listen to chatter
	m_host.SetChatterListener([this](Chatter &&chatter) { OnChatter(std::move(chatter)); });
	m_task->SetChatterEnabled(true);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

InvokeArbitraryCommandDialog::~InvokeArbitraryCommandDialog()
{
	m_task->SetChatterEnabled(false);
	m_host.SetChatterListener(nullptr);
}


//-------------------------------------------------
//  AddControl
//-------------------------------------------------

template<typename TControl, typename... TArgs>
TControl &InvokeArbitraryCommandDialog::AddControl(std::unique_ptr<wxBoxSizer> &sizer, int proportion, int flags, TArgs&&... args)
{
	TControl *control = new TControl(this, std::forward<TArgs>(args)...);
	sizer->Add(control, proportion, flags, 4);
	return *control;
}


//-------------------------------------------------
//  OnInvoke
//-------------------------------------------------

void InvokeArbitraryCommandDialog::OnInvoke()
{
	// issue the command
	wxString command_line = m_input_control->GetValue();
	m_task->IssueFullCommandLine(command_line);

	// and clear
	m_input_control->Clear();
}


//-------------------------------------------------
//  OnChatter
//-------------------------------------------------

void InvokeArbitraryCommandDialog::OnChatter(Chatter &&chatter)
{
	// remove line endings
	while (chatter.m_text.EndsWith("\r") || chatter.m_text.EndsWith("\n"))
		chatter.m_text.resize(chatter.m_text.size() - 1);

	// ignore pings; they are noisy
	if (IsChatterPing(chatter) && m_filter_ping_control->GetValue())
		return;

	wxTextAttr style;
	switch (chatter.m_type)
	{
	case Chatter::chatter_type::COMMAND_LINE:
		style = wxTextAttr(*wxBLUE);
		break;
	case Chatter::chatter_type::RESPONSE:
		style = wxTextAttr(chatter.m_text.StartsWith("OK") ? *wxBLACK : *wxRED);
		break;
	default:
		throw false;
	}
	m_display_control->SetDefaultStyle(style);
	m_display_control->AppendText(chatter.m_text);
	m_display_control->AppendText("\r\n");
}


//-------------------------------------------------
//  IsChatterPing
//-------------------------------------------------

bool InvokeArbitraryCommandDialog::IsChatterPing(const Chatter &chatter)
{
	// hack, but good enough for now
	return (chatter.m_type == Chatter::chatter_type::COMMAND_LINE && chatter.m_text == "ping")
		|| (chatter.m_type == Chatter::chatter_type::RESPONSE && chatter.m_text.Contains("pong"));
}


//-------------------------------------------------
//  show_invoke_arbitrary_command_dialog
//-------------------------------------------------

bool show_invoke_arbitrary_command_dialog(wxWindow &parent, MameClient &client, IInvokeArbitraryCommandDialogHost &host)
{
	InvokeArbitraryCommandDialog dialog(parent, client.GetCurrentTask<RunMachineTask>(), host);
	return dialog.ShowModal() == wxID_OK;
}
