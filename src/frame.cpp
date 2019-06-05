/***************************************************************************

    frame.cpp

    MAME Front End Application

***************************************************************************/

#include "wx/wxprec.h"

#include <wx/listctrl.h>
#include <wx/textfile.h>

#include "frame.h"
#include "client.h"
#include "dlgpaths.h"
#include "prefs.h"
#include "listxmltask.h"
#include "runmachinetask.h"
#include "utility.h"
#include "virtuallistview.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

// IDs for the controls and the menu commands
enum
{
	// user IDs
	ID_PING_TIMER				= wxID_HIGHEST + 1,
	ID_LAST
};

wxDEFINE_EVENT(EVT_SPECIFY_MAME_PATH, wxCommandEvent);


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class MameFrame : public wxFrame
	{
	public:
		// ctor(s)
		MameFrame();

		// event handlers (these functions should _not_ be virtual)
		void OnSize(wxSizeEvent &event);
		void OnClose(wxCloseEvent &event);
		void OnMenuStop();
		void OnMenuAbout();
		void OnListItemSelected(wxListEvent &event);
		void OnListItemActivated(wxListEvent &event);
		void OnListColumnResized(wxListEvent &event);
		void OnPingTimer(wxTimerEvent &event);

		// Task notifications
		void OnListXmlCompleted(PayloadEvent<ListXmlResult> &event);
		void OnRunMachineCompleted(PayloadEvent<RunMachineResult> &event);
		void OnStatusUpdate(PayloadEvent<StatusUpdate> &event);
		void OnSpecifyMamePath();

	private:
		MameClient                  m_client;
		Preferences                 m_prefs;
		VirtualListView *           m_list_view;
		wxMenuBar *                 m_menu_bar;
		wxTimer						m_ping_timer;
		wxString					m_mame_build;
		std::vector<Machine>        m_machines;
		bool						m_pinging;

		// status of running emulation
		bool						m_status_paused;
		wxString					m_status_frameskip;
		bool						m_status_throttled;
		float						m_status_throttle_rate;

		static float s_throttle_rates[];

		void CreateMenuBar();
		void Issue(std::string &&command);
		void IssueThrottled(bool throttled);
		void IssueThrottleRate(float throttle_rate);
		void ChangeThrottleRate(int adjustment);
		bool IsEmulationSessionActive() const;
		void OnEmuMenuUpdateUI(wxUpdateUIEvent &event);
		void OnEmuMenuUpdateUI(wxUpdateUIEvent &event, bool checked);
		void UpdateMachineList();
		wxString GetListItemText(size_t item, long column) const;
		void UpdateEmulationSession();
		wxString GetTarget();
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

float MameFrame::s_throttle_rates[] = { 10.0f, 5.0f, 2.0f, 1.0f, 0.5f, 0.2f, 0.1f };

//-------------------------------------------------
//  ctor
//-------------------------------------------------

MameFrame::MameFrame()
	: wxFrame(nullptr, wxID_ANY, "BletchMAME")
	, m_client(*this, m_prefs)
	, m_list_view(nullptr)
	, m_menu_bar(nullptr)
	, m_ping_timer(this, ID_PING_TIMER)
	, m_pinging(false)
{
	// set the frame icon
	SetIcon(wxICON(bletchmame));

	// set the size
	SetSize(m_prefs.GetSize());

	// create the menu bar
	CreateMenuBar();

	// create a status bar just for fun (by default with 1 pane only)
	CreateStatusBar(2);
	SetStatusText("Welcome to BletchMAME!");

	// the ties that bind...
	Bind(EVT_LIST_XML_RESULT,       [this](auto &event) { OnListXmlCompleted(event);    });
	Bind(EVT_RUN_MACHINE_RESULT,    [this](auto &event) { OnRunMachineCompleted(event); });
	Bind(EVT_STATUS_UPDATE,         [this](auto &event) { OnStatusUpdate(event);        });
	Bind(EVT_SPECIFY_MAME_PATH,     [this](auto &)      { OnSpecifyMamePath();			});
	Bind(wxEVT_SIZE,	        	[this](auto &event) { OnSize(event);				});
	Bind(wxEVT_CLOSE_WINDOW,		[this](auto &event) { OnClose(event);				});
	Bind(wxEVT_LIST_ITEM_SELECTED,  [this](auto &event) { OnListItemSelected(event);    });
	Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](auto &event) { OnListItemActivated(event);   });
	Bind(wxEVT_LIST_COL_END_DRAG,   [this](auto &event) { OnListColumnResized(event);   });
	Bind(wxEVT_TIMER,				[this](auto &event) { OnPingTimer(event);           });

	// Create a list view
	m_list_view = new VirtualListView(this);
	m_list_view->SetOnGetItemText([this](long item, long column) { return GetListItemText(static_cast<size_t>(item), column); });
	m_list_view->ClearAll();
	m_list_view->AppendColumn("Name",           wxLIST_FORMAT_LEFT, m_prefs.GetColumnWidth(0));
	m_list_view->AppendColumn("Description",    wxLIST_FORMAT_LEFT, m_prefs.GetColumnWidth(1));
	m_list_view->AppendColumn("Year",           wxLIST_FORMAT_LEFT, m_prefs.GetColumnWidth(2));
	m_list_view->AppendColumn("Manufacturer",   wxLIST_FORMAT_LEFT, m_prefs.GetColumnWidth(3));
	m_list_view->SetColumnsOrder(m_prefs.GetColumnOrder());
	UpdateMachineList();

	// nothing is running yet...
	UpdateEmulationSession();

	// Connect to MAME
	if (wxFileExists(m_prefs.GetPath(Preferences::path_type::emu_exectuable)))
	{
		m_client.Launch(create_list_xml_task());
	}
	else
	{
		// Can't find MAME; ask the user
		util::PostEvent(*this, EVT_SPECIFY_MAME_PATH);
	}
}


//-------------------------------------------------
//  CreateMenuBar
//-------------------------------------------------

void MameFrame::CreateMenuBar()
{
	int id = ID_LAST;

	// create the "File" menu
	wxMenu *file_menu = new wxMenu();
	wxMenuItem *stop_menu_item				= file_menu->Append(id++, "Stop");
	wxMenuItem *pause_menu_item				= file_menu->Append(id++, "Pause");
	wxMenu *reset_menu = new wxMenu();
	wxMenuItem *soft_reset_menu_item		= reset_menu->Append(id++, "Soft Reset");
	wxMenuItem *hard_reset_menu_item		= reset_menu->Append(id++, "Hard Reset");
	file_menu->AppendSubMenu(reset_menu, "Reset");
	wxMenuItem *exit_menu_item				= file_menu->Append(wxID_EXIT, "E&xit\tAlt-X");

	// create the "Devices" menu
	wxMenu *devices_menu = new wxMenu();

	// create the "Options" menu
	wxMenu *options_menu = new wxMenu();
	wxMenu *throttle_menu = new wxMenu();
	throttle_menu->AppendSeparator();	// throttle menu items are added later
	wxMenuItem *increase_speed_menu_item	= throttle_menu->Append(id++, "Increase Speed");
	wxMenuItem *decrease_speed_menu_item	= throttle_menu->Append(id++, "Decrease Speed");
	wxMenuItem *warp_mode_menu_item			= throttle_menu->Append(id++, "Warp Mode", wxEmptyString, wxITEM_CHECK);
	options_menu->AppendSubMenu(throttle_menu, "Throttle");
	wxMenu *frameskip_menu = new wxMenu();
	options_menu->AppendSubMenu(frameskip_menu, "Frame Skip");

	// create the "Settings" menu
	wxMenu *settings_menu = new wxMenu();
	wxMenuItem *show_paths_dialog_menu_item	= settings_menu->Append(id++, "Paths...");

	// the "About" item should be in the help menu (it is important that this use wxID_ABOUT)
	wxMenu *help_menu = new wxMenu();
	wxMenuItem *about_menu_item				= help_menu->Append(wxID_ABOUT, "&About\tF1");

	// now append the freshly created menu to the menu bar...
	m_menu_bar = new wxMenuBar();
	m_menu_bar->Append(file_menu, "&File");
	m_menu_bar->Append(devices_menu, "&Devices");
	m_menu_bar->Append(options_menu, "&Options");
	m_menu_bar->Append(settings_menu, "&Settings");
	m_menu_bar->Append(help_menu, "&Help");

	// ... and attach this menu bar to the frame
	SetMenuBar(m_menu_bar);

	// Bind menu item selected events
	Bind(wxEVT_MENU, [this](auto &) { Issue("soft_reset");					}, soft_reset_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuStop();							}, stop_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { Close(false);							}, exit_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { ChangeThrottleRate(-1);				}, increase_speed_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { ChangeThrottleRate(+1);				}, decrease_speed_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { IssueThrottled(!m_status_throttled);	}, warp_mode_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { show_paths_dialog(m_prefs);			}, show_paths_dialog_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuAbout();						}, about_menu_item->GetId());

	// Bind UI update events
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);						}, stop_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);						}, pause_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);						}, soft_reset_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);						}, hard_reset_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);						}, increase_speed_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);						}, decrease_speed_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, !m_status_throttled);	}, warp_mode_menu_item->GetId());

	// special setup for throttle dynamic menu
	for (size_t i = 0; i < sizeof(s_throttle_rates) / sizeof(s_throttle_rates[0]); i++)
	{
		float throttle_rate	= s_throttle_rates[i];
		wxString text		= std::to_string((int)(throttle_rate * 100)) + "%";
		wxMenuItem *item	= throttle_menu->Insert(i, id++, text, wxEmptyString, wxITEM_CHECK);

		Bind(wxEVT_MENU,		[this, throttle_rate](auto &)		{ IssueThrottleRate(throttle_rate);										}, item->GetId());
		Bind(wxEVT_UPDATE_UI,	[this, throttle_rate](auto &event)	{ OnEmuMenuUpdateUI(event, m_status_throttle_rate == throttle_rate);	}, item->GetId());
	}

	// special setup for frameskip dynamic menu
	for (int i = -1; i <= 10; i++)
	{
		wxString text		= i == -1 ? "Auto" : std::to_string(i);
		wxMenuItem *item	= frameskip_menu->Insert(i + 1, id++, text, wxEmptyString, wxITEM_CHECK);
		std::string value	= i == -1 ? "auto" : std::to_string(i);
		std::string command = "frameskip " + value;

		Bind(wxEVT_MENU,		[this, command](auto &)		{ Issue(std::string(command));								}, item->GetId());
		Bind(wxEVT_UPDATE_UI,	[this, value](auto &event)	{ OnEmuMenuUpdateUI(event, m_status_frameskip == value);	}, item->GetId());		
	}
}


//-------------------------------------------------
//  IsEmulationSessionActive
//-------------------------------------------------

bool MameFrame::IsEmulationSessionActive() const
{
	return m_client.GetCurrentTask<RunMachineTask>() != nullptr;
}


//-------------------------------------------------
//  OnSize
//-------------------------------------------------

void MameFrame::OnSize(wxSizeEvent &event)
{
	// update the window size in preferences
	m_prefs.SetSize(event.GetSize());

	// skip this event handler so the default behavior applies
	event.Skip();
}


//-------------------------------------------------
//  OnClose
//-------------------------------------------------

void MameFrame::OnClose(wxCloseEvent &event)
{
	if (IsEmulationSessionActive())
	{
		wxString message = "Do you really want to exit?\n"
			"\n"
			"All data in emulated RAM will be lost";
		if (wxMessageBox(message, "BletchMAME", wxYES_NO | wxICON_QUESTION, this) != wxYES)
		{
			event.Veto();
			return;
		}
	}

	for (int i = 0; i < Preferences::COLUMN_COUNT; i++)
	{
		int order = m_list_view->GetColumnOrder(i);
		m_prefs.SetColumnOrder(i, order);
	}
	Destroy();
}


//-------------------------------------------------
//  OnMenuStop
//-------------------------------------------------

void MameFrame::OnMenuStop()
{
	wxString message = "Do you really want to stop?\n"
		"\n"
		"All data in emulated RAM will be lost";
	if (wxMessageBox(message, "BletchMAME", wxYES_NO | wxICON_QUESTION, this) == wxYES)
		Issue("exit");
}


//-------------------------------------------------
//  OnMenuAbout
//-------------------------------------------------

void MameFrame::OnMenuAbout()
{
	wxString message = "BletchMAME";

	if (!m_mame_build.IsEmpty())
	{
		wxString eoln = wxTextFile::GetEOL();
		message += eoln
			+ eoln
			+ "MAME Build: " + m_mame_build;
	}

	wxMessageBox(message, "About BletchMAME", wxOK | wxICON_INFORMATION, this);
}


//-------------------------------------------------
//  OnEmuMenuUpdateUI
//-------------------------------------------------

void MameFrame::OnEmuMenuUpdateUI(wxUpdateUIEvent &event)
{
	event.Enable(IsEmulationSessionActive());
}


void MameFrame::OnEmuMenuUpdateUI(wxUpdateUIEvent &event, bool checked)
{
	OnEmuMenuUpdateUI(event);
	event.Check(checked);
}


//-------------------------------------------------
//  OnListXmlCompleted
//-------------------------------------------------

void MameFrame::OnListXmlCompleted(PayloadEvent<ListXmlResult> &event)
{
	// identify the results
	if (!event.Payload().m_success)
		return;

	// take over the machine list
	m_mame_build = std::move(event.Payload().m_build);
	m_machines = std::move(event.Payload().m_machines);

	// update!
	UpdateMachineList();

	m_client.Reset();
}


//-------------------------------------------------
//  OnRunMachineCompleted
//-------------------------------------------------

void MameFrame::OnRunMachineCompleted(PayloadEvent<RunMachineResult> &event)
{
	const RunMachineResult &payload(event.Payload());

	m_client.Reset();
    UpdateEmulationSession();

	// report any errors
	if (!payload.m_error_message.IsEmpty())
	{
		wxMessageBox(payload.m_error_message, "Error", wxOK | wxCENTRE, this);
	}
}


//-------------------------------------------------
//  OnStatusUpdate
//-------------------------------------------------

void MameFrame::OnStatusUpdate(PayloadEvent<StatusUpdate> &event)
{
	const StatusUpdate &payload(event.Payload());

	if (payload.m_paused_specified)
		m_status_paused = payload.m_paused;
	if (payload.m_frameskip_specified)
		m_status_frameskip = payload.m_frameskip;
	if (payload.m_speed_text_specified)
		SetStatusText(payload.m_speed_text);
	if (payload.m_throttled_specified)
		m_status_throttled = payload.m_throttled;
	if (payload.m_throttle_rate_specified)
		m_status_throttle_rate = payload.m_throttle_rate;

	m_pinging = false;
}


//-------------------------------------------------
//  OnSpecifyMamePath
//-------------------------------------------------

void MameFrame::OnSpecifyMamePath()
{
	wxString path = show_specify_single_path_dialog(*this, Preferences::path_type::emu_exectuable, m_prefs.GetPath(Preferences::path_type::emu_exectuable));
	if (path.IsEmpty())
		return;

	m_prefs.SetPath(Preferences::path_type::emu_exectuable, std::move(path));
	m_client.Launch(create_list_xml_task());
}


//-------------------------------------------------
//  OnListItemSelected
//-------------------------------------------------

void MameFrame::OnListItemSelected(wxListEvent &evt)
{
    long index = evt.GetIndex();
    m_prefs.SetSelectedMachine(m_machines[index].m_name);
}


//-------------------------------------------------
//  OnListItemActivated
//-------------------------------------------------

void MameFrame::OnListItemActivated(wxListEvent &evt)
{
	long index = evt.GetIndex();
	if (index < 0 || index >= (long)m_machines.size())
		return;

	Task::ptr task = std::make_unique<RunMachineTask>(wxString(m_machines[index].m_name), GetTarget());
	m_client.Launch(std::move(task));
	UpdateEmulationSession();
}


//-------------------------------------------------
//  OnListColumnResized
//-------------------------------------------------

void MameFrame::OnListColumnResized(wxListEvent &evt)
{
    int column_index = evt.GetColumn();
    int column_width = m_list_view->GetColumnWidth(column_index);
    m_prefs.SetColumnWidth(column_index, column_width);
}


//-------------------------------------------------
//  OnPingTimer
//-------------------------------------------------

void MameFrame::OnPingTimer(wxTimerEvent &)
{
	// only issue a ping if there is an active session, and there is no ping in flight
	if (!m_pinging && IsEmulationSessionActive())
	{
		m_pinging = true;
		Issue("ping");
	}
}


//-------------------------------------------------
//  Issue
//-------------------------------------------------

void MameFrame::Issue(std::string &&command)
{
    RunMachineTask *task = m_client.GetCurrentTask<RunMachineTask>();
    if (!task)
        return;

    task->Post(std::move(command));
}


//-------------------------------------------------
//  IssueThrottled
//-------------------------------------------------

void MameFrame::IssueThrottled(bool throttled)
{
	Issue("throttled " + std::to_string(throttled ? 1 : 0));
}


//-------------------------------------------------
//  IssueThrottleRate
//-------------------------------------------------

void MameFrame::IssueThrottleRate(float throttle_rate)
{
	Issue("throttle_rate " + std::to_string(throttle_rate));
}


//-------------------------------------------------
//  ChangeThrottleRate
//-------------------------------------------------

void MameFrame::ChangeThrottleRate(int adjustment)
{
	// find where we are in the array
	int index;
	for (index = 0; index < sizeof(s_throttle_rates) / sizeof(s_throttle_rates[0]); index++)
	{
		if (m_status_throttle_rate >= s_throttle_rates[index])
			break;
	}

	// apply the adjustment
	index += adjustment;
	index = std::max(index, 0);
	index = std::min(index, (int)(sizeof(s_throttle_rates) / sizeof(s_throttle_rates[0]) - 1));

	// and change the throttle rate
	IssueThrottleRate(s_throttle_rates[index]);
}


//-------------------------------------------------
//  UpdateMachineList
//-------------------------------------------------

void MameFrame::UpdateMachineList()
{
	m_list_view->SetItemCount(m_machines.size());
	m_list_view->RefreshItems(0, m_machines.size() - 1);

	// find the currently selected machine
	auto iter = std::find_if(m_machines.begin(), m_machines.end(), [this](const Machine &machine)
	{
		return machine.m_name == m_prefs.GetSelectedMachine();
	});

	// if successful, select it
	if (iter < m_machines.end())
	{
		long selected_index = iter - m_machines.begin();
		m_list_view->Select(selected_index);
		m_list_view->EnsureVisible(selected_index);
	}
}


//-------------------------------------------------
//  GetListItemText
//-------------------------------------------------

wxString MameFrame::GetListItemText(size_t item, long column) const
{
	wxString result;
	switch (column)
	{
	case 0:
		result = m_machines[item].m_name;
		break;
	case 1:
		result = m_machines[item].m_description;
		break;
	case 2:
		result = m_machines[item].m_year;
		break;
	case 3:
		result = m_machines[item].m_manfacturer;
		break;
	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  UpdateEmulationSession
//-------------------------------------------------

void MameFrame::UpdateEmulationSession()
{
	bool is_active = IsEmulationSessionActive();

	m_list_view->Show(!is_active);

	if (is_active)
		m_ping_timer.Start(500);
	else
		m_ping_timer.Stop();
}


//-------------------------------------------------
//  GetTarget
//-------------------------------------------------

wxString MameFrame::GetTarget()
{
	return std::to_string((std::int64_t)GetHWND());
}


//-------------------------------------------------
//  create_mame_frame
//-------------------------------------------------

wxFrame *create_mame_frame()
{
    return new MameFrame();
}

