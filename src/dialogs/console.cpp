/***************************************************************************

    dialogs/console.cpp

    Arbitrary command console dialog (essentially a development feature)

***************************************************************************/

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>
#include <wx/checkbox.h>

#include "dialogs/console.h"
#include "runmachinetask.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class ConsoleDialog : public wxDialog
	{
	public:
		ConsoleDialog(wxWindow &parent, std::shared_ptr<RunMachineTask> &&task, IConsoleDialogHost &host);
		~ConsoleDialog();

	private:
		IConsoleDialogHost & m_host;
		std::shared_ptr<RunMachineTask>		m_task;
		wxTextCtrl *						m_display_control;
		wxTextCtrl *						m_input_control;
		wxCheckBox *						m_filter_ping_control;

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

ConsoleDialog::ConsoleDialog(wxWindow &parent, std::shared_ptr<RunMachineTask> &&task, IConsoleDialogHost &host)
	: wxDialog(&parent, wxID_ANY, "Console", wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX | wxRESIZE_BORDER)
	, m_host(host)
	, m_task(std::move(task))
	, m_display_control(nullptr)
	, m_input_control(nullptr)
	, m_filter_ping_control(nullptr)
{
	assert(m_task);
	int id = wxID_LAST;

	// display control, where the results are displayed
	m_display_control = new wxTextCtrl(this, id++, wxEmptyString, wxDefaultPosition, wxSize(400, 150), wxTE_RICH | wxTE_READONLY | wxTE_MULTILINE);

	// input control, where commands are entered
	m_input_control = new wxTextCtrl(this, id++, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	m_input_control->SetFocus();

	// buttons
	m_filter_ping_control	= new wxCheckBox(this, id++,	wxT("Filter out pings"));
	wxButton *invoke_button	= new wxButton(this, id++,	wxT("Invoke"));
	wxButton *close_button	= new wxButton(this, wxID_OK,	wxT("Close"));
	m_filter_ping_control->SetValue(true);

	// specify the sizer
	SpecifySizerAndFit(*this, { boxsizer_orientation::VERTICAL, 4, {
		{ 1, wxALL | wxEXPAND,		*m_display_control },
		{ 0, wxALL | wxEXPAND,		*m_input_control },
		{ 0, wxALL | wxALIGN_RIGHT,	boxsizer_orientation::HORIZONTAL, 4, {
			{ 0, wxALL, *m_filter_ping_control },
			{ 0, wxALL, *invoke_button },
			{ 0, wxALL, *close_button }
		}}
	}});

	// events
	Bind(wxEVT_TEXT_ENTER,	[this](auto &)		{ OnInvoke(); },											m_input_control->GetId());
	Bind(wxEVT_BUTTON,		[this](auto &)		{ OnInvoke(); },											invoke_button->GetId());
	Bind(wxEVT_UPDATE_UI,	[this](auto &event)	{ event.Enable(!m_input_control->GetValue().IsEmpty()); },	invoke_button->GetId());
	(void)close_button;

	// listen to chatter
	m_host.SetChatterListener([this](Chatter &&chatter) { OnChatter(std::move(chatter)); });
	m_task->SetChatterEnabled(true);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

ConsoleDialog::~ConsoleDialog()
{
	m_task->SetChatterEnabled(false);
	m_host.SetChatterListener(nullptr);
}


//-------------------------------------------------
//  OnInvoke
//-------------------------------------------------

void ConsoleDialog::OnInvoke()
{
	// get the command line, and ensure that it is not blank or whitespace
	wxString command_line = m_input_control->GetValue();

	// if this just white space, bail
	if (std::find_if(command_line.begin(), command_line.end(), [](auto ch) { return ch != ' '; }) == command_line.end())
		return;

	// issue the command
	m_task->IssueFullCommandLine(command_line);

	// and clear
	m_input_control->Clear();
}


//-------------------------------------------------
//  OnChatter
//-------------------------------------------------

void ConsoleDialog::OnChatter(Chatter &&chatter)
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

bool ConsoleDialog::IsChatterPing(const Chatter &chatter)
{
	// hack, but good enough for now
	return (chatter.m_type == Chatter::chatter_type::COMMAND_LINE && chatter.m_text == "ping")
		|| (chatter.m_type == Chatter::chatter_type::RESPONSE && chatter.m_text.Contains("pong"));
}


//-------------------------------------------------
//  show_console_dialog
//-------------------------------------------------

bool show_console_dialog(wxWindow &parent, MameClient &client, IConsoleDialogHost &host)
{
	ConsoleDialog dialog(parent, client.GetCurrentTask<RunMachineTask>(), host);
	return dialog.ShowModal() == wxID_OK;
}
