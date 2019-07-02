/***************************************************************************

    frame.cpp

    MAME Front End Application

***************************************************************************/

#include "wx/wxprec.h"

#include <algorithm>
#include <optional>

#include <wx/listctrl.h>
#include <wx/textfile.h>
#include <wx/filename.h>

#include <observable/observable.hpp>

#include "frame.h"
#include "client.h"
#include "dlgconsole.h"
#include "dlgimages.h"
#include "dlginputs.h"
#include "dlgpaths.h"
#include "prefs.h"
#include "listxmltask.h"
#include "runmachinetask.h"
#include "utility.h"
#include "virtuallistview.h"
#include "info.h"
#include "dlgloading.h"


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


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class MameFrame : public wxFrame, private IConsoleDialogHost
	{
	public:
		// ctor(s)
		MameFrame();
		~MameFrame();

	private:
		enum class file_dialog_type
		{
			LOAD,
			SAVE
		};

		class Pauser
		{
		public:
			Pauser(MameFrame &host, bool actually_pause = true);
			~Pauser();

		private:
			MameFrame &		m_host;
			const Pauser *	m_last_pauser;
			bool			m_is_running;
		};

		class ImagesHost : public IImagesHost
		{
		public:
			ImagesHost(MameFrame &host);

			virtual observable::value<std::vector<Image>> &GetImages();
			virtual const wxString &GetWorkingDirectory() const;
			virtual void SetWorkingDirectory(wxString &&dir);
			virtual std::vector<wxString> GetExtensions(const wxString &tag) const;
			virtual void LoadImage(const wxString &tag, wxString &&path);
			virtual void UnloadImage(const wxString &tag);

		private:
			MameFrame &m_host;

			const wxString &GetMachineName() const;
		};

		class InputsHost : public IInputsHost
		{
		public:
			InputsHost(MameFrame &host);

			virtual const std::vector<Input> &GetInputs() override;
			virtual observable::value<bool> &GetPollingSeqChanged() override;
			virtual void StartPolling(const wxString &port_tag, ioport_value mask, InputSeq::inputseq_type seq_type) override;
			virtual void StopPolling() override;

		private:
			MameFrame & m_host;
		};

		MameClient								m_client;
		Preferences							    m_prefs;
		VirtualListView *					    m_list_view;
		wxMenuBar *							    m_menu_bar;
		wxAcceleratorTable						m_menu_bar_accelerators;
		wxTimer									m_ping_timer;
		bool									m_pinging;
		const Pauser *							m_current_pauser;
		std::function<void(Chatter &&)>			m_on_chatter;

		// information retrieved by -listxml
		info::database							m_info_db;
		bool									m_last_listxml_succeeded;

		// status of running emulation
		observable::value<bool>					m_status_paused;
		observable::value<bool>					m_status_polling_input_seq;
		wxString								m_status_frameskip;
		bool									m_status_throttled;
		float									m_status_throttle_rate;
		int										m_status_sound_attenuation;
		observable::value<std::vector<Image> >	m_status_images;
		std::vector<Input>						m_status_inputs;

		static const float s_throttle_rates[];
		static const wxString s_wc_saved_state;
		static const wxString s_wc_save_snapshot;

		const int SOUND_ATTENUATION_OFF = -32;
		const int SOUND_ATTENUATION_ON = 0;

		// event handlers (these functions should _not_ be virtual)
		void OnKeyDown(wxKeyEvent &event);
		void OnSize(wxSizeEvent &event);
		void OnClose(wxCloseEvent &event);
		void OnMenuStop();
		void OnMenuStateLoad();
		void OnMenuStateSave();
		void OnMenuSnapshotSave();
		void OnMenuImages();
		void OnMenuPaths();
		void OnMenuInputs();
		void OnMenuAbout();
		void OnListItemSelected(wxListEvent &event);
		void OnListItemActivated(wxListEvent &event);
		void OnListColumnResized(wxListEvent &event);

		// task notifications
		void OnListXmlCompleted(PayloadEvent<ListXmlResult> &event);
		void OnRunMachineCompleted(PayloadEvent<RunMachineResult> &event);
		void OnStatusUpdate(PayloadEvent<StatusUpdate> &event);
		void OnChatter(PayloadEvent<Chatter> &event);

		// miscellaneous
		void CreateMenuBar();
		void CheckMameInfoDatabase(bool prompt_mame_path, bool force_refresh);
		bool IsEmulationSessionActive() const;
		const info::machine &GetRunningMachine() const;
		void Run(int machine_index);
		int MessageBox(const wxString &message, long style = wxOK | wxCENTRE, const wxString &caption = wxTheApp->GetAppName());
		void OnEmuMenuUpdateUI(wxUpdateUIEvent &event, std::optional<bool> checked = { }, bool enabled = true);
		void UpdateMachineList();
		wxString GetListItemText(size_t item, long column) const;
		void UpdateEmulationSession();
		void UpdateTitleBar();
		void UpdateMenuBar();
		void SetChatterListener(std::function<void(Chatter &&chatter)> &&func);
		void FileDialogCommand(std::vector<wxString> &&commands, Preferences::machine_path_type path_type, bool path_is_file, const wxString &wildcard_string, file_dialog_type dlgtype);
		template<typename TStatus, typename TPayload> static bool GetStatusFromPayload(TStatus &value, std::optional<TPayload> &payload);

		// runtime control
		void Issue(const std::vector<wxString> &args);
		void Issue(const std::initializer_list<wxString> &args);
		void Issue(const char *command);
		void InvokePing();
		void ChangePaused(bool paused);
		void ChangeThrottled(bool throttled);
		void ChangeThrottleRate(float throttle_rate);
		void ChangeThrottleRate(int adjustment);
		void ChangeSound(bool sound_enabled);
		bool IsSoundEnabled() const;
	};
};


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

const float MameFrame::s_throttle_rates[] = { 10.0f, 5.0f, 2.0f, 1.0f, 0.5f, 0.2f, 0.1f };

const wxString MameFrame::s_wc_saved_state = wxT("MAME Saved State Files (*.sta)|*.sta|All Files (*.*)|*.*");
const wxString MameFrame::s_wc_save_snapshot = wxT("PNG Files (*.png)|*.png|All Files (*.*)|*.*");

//-------------------------------------------------
//  ctor
//-------------------------------------------------

MameFrame::MameFrame()
	: wxFrame(nullptr, wxID_ANY, wxTheApp->GetAppName())
	, m_client(*this, m_prefs)
	, m_list_view(nullptr)
	, m_menu_bar(nullptr)
	, m_ping_timer(this, ID_PING_TIMER)
	, m_pinging(false)
	, m_current_pauser(nullptr)
{
	// set the frame icon
	SetIcon(wxICON(bletchmame));

	// initial preferences read
	m_prefs.Load();
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
	Bind(EVT_CHATTER,				[this](auto &event)	{ OnChatter(event);				});
	Bind(wxEVT_KEY_DOWN,			[this](auto &event) { OnKeyDown(event);             });
	Bind(wxEVT_SIZE,	        	[this](auto &event) { OnSize(event);				});
	Bind(wxEVT_CLOSE_WINDOW,		[this](auto &event) { OnClose(event);				});
	Bind(wxEVT_LIST_ITEM_SELECTED,  [this](auto &event) { OnListItemSelected(event);    });
	Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](auto &event) { OnListItemActivated(event);   });
	Bind(wxEVT_LIST_COL_END_DRAG,   [this](auto &event) { OnListColumnResized(event);   });
	Bind(wxEVT_TIMER,				[this](auto &)		{ InvokePing();					});

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

	// we need to check the DB
	CheckMameInfoDatabase(true, false);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

MameFrame::~MameFrame()
{
	m_prefs.Save();
}


//-------------------------------------------------
//  CreateMenuBar
//-------------------------------------------------

void MameFrame::CreateMenuBar()
{
	int id = ID_LAST;

	// create the "File" menu
	wxMenu *file_menu = new wxMenu();
	wxMenuItem *stop_menu_item					= file_menu->Append(id++, "Stop");
	wxMenuItem *pause_menu_item					= file_menu->Append(id++, "Pause\tPause", wxEmptyString, wxITEM_CHECK);
	file_menu->AppendSeparator();
	wxMenuItem *load_state_menu_item			= file_menu->Append(id++, "Load State...\tF7");
	wxMenuItem *save_state_menu_item			= file_menu->Append(id++, "Save State...\tShift+F7");
	wxMenuItem *save_screenshot_menu_item		= file_menu->Append(id++, "Save Screenshot...\tF12");
	file_menu->AppendSeparator();
	wxMenu *reset_menu = new wxMenu();
	wxMenuItem *soft_reset_menu_item			= reset_menu->Append(id++, "Soft Reset");
	wxMenuItem *hard_reset_menu_item			= reset_menu->Append(id++, "Hard Reset");
	file_menu->AppendSubMenu(reset_menu, "Reset");
	wxMenuItem *exit_menu_item					= file_menu->Append(wxID_EXIT, "E&xit\tCtrl+Alt+X");

	// create the "Options" menu
	wxMenu *options_menu = new wxMenu();
	wxMenu *throttle_menu = new wxMenu();
	throttle_menu->AppendSeparator();	// throttle menu items are added later
	wxMenuItem *increase_speed_menu_item		= throttle_menu->Append(id++, "Increase Speed\tF9");
	wxMenuItem *decrease_speed_menu_item		= throttle_menu->Append(id++, "Decrease Speed\tF8");
	wxMenuItem *warp_mode_menu_item				= throttle_menu->Append(id++, "Warp Mode\tF10", wxEmptyString, wxITEM_CHECK);
	options_menu->AppendSubMenu(throttle_menu, "Throttle");
	wxMenu *frameskip_menu = new wxMenu();
	options_menu->AppendSubMenu(frameskip_menu, "Frame Skip");
	wxMenuItem *images_menu_item				= options_menu->Append(id++, "Images...");
	wxMenuItem *sound_menu_item					= options_menu->Append(id++, "Sound", wxEmptyString, wxITEM_CHECK);
	options_menu->AppendSeparator();
	wxMenuItem *console_menu_item				= options_menu->Append(id++, "Console...");

	// create the "Settings" menu
	wxMenu *settings_menu = new wxMenu();
	wxMenuItem *show_paths_dialog_menu_item		= settings_menu->Append(id++, "Paths...");
	wxMenuItem *show_inputs_dialog_menu_item	= settings_menu->Append(id++, "Inputs...");

	// the "About" item should be in the help menu (it is important that this use wxID_ABOUT)
	wxMenu *help_menu = new wxMenu();
	wxMenuItem *refresh_mame_info_menu_item     = help_menu->Append(id++, "Refresh MAME machine info...");
	help_menu->AppendSeparator();
	wxMenuItem *about_menu_item			        = help_menu->Append(wxID_ABOUT, "&About\tF1");

	// now append the freshly created menu to the menu bar...
	m_menu_bar = new wxMenuBar();
	m_menu_bar->Append(file_menu, "&File");
	m_menu_bar->Append(options_menu, "&Options");
	m_menu_bar->Append(settings_menu, "&Settings");
	m_menu_bar->Append(help_menu, "&Help");

	// ... and attach this menu bar to the frame
	SetMenuBar(m_menu_bar);

	// get a copy of the accelerators; we need this so we can disable them when
	// we toggle the menu bar
	m_menu_bar_accelerators = *m_menu_bar->GetAcceleratorTable();

	// Bind menu item selected events
	Bind(wxEVT_MENU, [this](auto &)	{ OnMenuStop();																}, stop_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &)	{ ChangePaused(!m_status_paused.get());										}, pause_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuStateLoad();														}, load_state_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuStateSave();														}, save_state_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuSnapshotSave();														}, save_screenshot_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { Issue("soft_reset");														}, soft_reset_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { Issue("hard_reset");														}, hard_reset_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &)	{ Close(false);																}, exit_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &)	{ ChangeThrottleRate(-1);													}, increase_speed_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &)	{ ChangeThrottleRate(+1);													}, decrease_speed_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &)	{ ChangeThrottled(!m_status_throttled);										}, warp_mode_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuImages();															}, images_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { ChangeSound(!IsSoundEnabled());											}, sound_menu_item->GetId());
	
	Bind(wxEVT_MENU, [this](auto &) { show_console_dialog(*this, m_client, *this);								}, console_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuPaths();															}, show_paths_dialog_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuInputs();															}, show_inputs_dialog_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { CheckMameInfoDatabase(false, true);										}, refresh_mame_info_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuAbout();															}, about_menu_item->GetId());

	// Bind UI update events	
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);										}, stop_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, m_status_paused.get());				}, pause_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);										}, load_state_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);										}, save_state_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);										}, save_screenshot_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);										}, soft_reset_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);										}, hard_reset_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);										}, increase_speed_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);										}, decrease_speed_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, !m_status_throttled);					}, warp_mode_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, { }, !m_status_images.get().empty());	}, images_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, IsSoundEnabled());						}, sound_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);										}, console_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, { }, !m_status_inputs.empty());		}, show_inputs_dialog_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { event.Enable(!IsEmulationSessionActive());						}, refresh_mame_info_menu_item->GetId());

	// special setup for throttle dynamic menu
	for (size_t i = 0; i < sizeof(s_throttle_rates) / sizeof(s_throttle_rates[0]); i++)
	{
		float throttle_rate	= s_throttle_rates[i];
		wxString text		= std::to_string((int)(throttle_rate * 100)) + "%";
		wxMenuItem *item	= throttle_menu->Insert(i, id++, text, wxEmptyString, wxITEM_CHECK);

		Bind(wxEVT_MENU,		[this, throttle_rate](auto &)		{ ChangeThrottleRate(throttle_rate);										}, item->GetId());
		Bind(wxEVT_UPDATE_UI,	[this, throttle_rate](auto &event)	{ OnEmuMenuUpdateUI(event, m_status_throttle_rate == throttle_rate);	}, item->GetId());
	}

	// special setup for frameskip dynamic menu
	for (int i = -1; i <= 10; i++)
	{
		wxString text		= i == -1 ? "Auto" : std::to_string(i);
		wxMenuItem *item	= frameskip_menu->Insert(i + 1, id++, text, wxEmptyString, wxITEM_CHECK);
		std::string value	= i == -1 ? "auto" : std::to_string(i);
		std::string command = "frameskip " + value;

		Bind(wxEVT_MENU,		[this, value](auto &)		{ Issue({ "frameskip", value });							}, item->GetId());
		Bind(wxEVT_UPDATE_UI,	[this, value](auto &event)	{ OnEmuMenuUpdateUI(event, m_status_frameskip == value);	}, item->GetId());		
	}

	// subscribe to title bar updates
	m_status_paused.subscribe([this]() { UpdateTitleBar(); });
}


//-------------------------------------------------
//  CheckMameInfoDatabase
//-------------------------------------------------

void MameFrame::CheckMameInfoDatabase(bool prompt_mame_path, bool force_refresh)
{
	wxString db_path = m_prefs.GetMameXmlDatabasePath();
	if (!force_refresh)
	{
		// try to load the info database
		if (m_info_db.load(db_path))
		{
			// we were able to do so; refresh the machine list and we're done
			UpdateMachineList();
			return;
		}
	}

	// does our path exist?
	if (!wxFileExists(m_prefs.GetPath(Preferences::path_type::emu_exectuable)))
	{
		// it doesn't... are we supposed to ask the user?
		if (!prompt_mame_path)
			return;

		// if so, ask the user
		wxString path = show_specify_single_path_dialog(*this, Preferences::path_type::emu_exectuable, m_prefs.GetPath(Preferences::path_type::emu_exectuable));
		if (path.IsEmpty())
			return;

		m_prefs.SetPath(Preferences::path_type::emu_exectuable, std::move(path));
	}

	// list XML
	m_client.Launch(create_list_xml_task(std::move(db_path)));

	// and show the dialog
	if (!show_loading_mame_info_dialog(*this, [this]() { return !m_client.IsTaskActive(); }))
		m_client.Abort();
}


//-------------------------------------------------
//  IsEmulationSessionActive
//-------------------------------------------------

bool MameFrame::IsEmulationSessionActive() const
{
	return m_client.IsTaskActive<RunMachineTask>();
}


//-------------------------------------------------
//  GetRunningMachine
//-------------------------------------------------

const info::machine &MameFrame::GetRunningMachine() const
{
	// get the currently running machine
	std::shared_ptr<const RunMachineTask> task = m_client.GetCurrentTask<const RunMachineTask>();

	// this call is only valid if we have a running machine
	assert(task);

	// return the machine
	return task->GetMachine();
}


//-------------------------------------------------
//  Run
//-------------------------------------------------

void MameFrame::Run(int machine_index)
{
	// we need to have full information to support the emulation session; retrieve
	// fake a pauser to forestall "PAUSED" from appearing in the menu bar
	Pauser fake_pauser(*this, false);

	// run the emulation
	Task::ptr task = std::make_unique<RunMachineTask>(
		m_info_db.machines()[machine_index],
		*this);
	m_client.Launch(std::move(task));
	UpdateEmulationSession();

	// wait for first ping
	m_pinging = true;
	while (m_pinging)
	{
		if (!IsEmulationSessionActive())
			return;
		wxYield();
	}

	// do we have any images that require images?
	auto iter = std::find_if(m_status_images.get().cbegin(), m_status_images.get().cend(), [](const Image &image)
	{
		return image.m_must_be_loaded && image.m_file_name.IsEmpty();
	});
	if (iter != m_status_images.get().cend())
	{
		// if so, show the dialog
		ImagesHost images_host(*this);
		if (!show_images_dialog_cancellable(images_host))
		{
			Issue("exit");
			return;
		}
	}

	// unpause
	ChangePaused(false);
}


//-------------------------------------------------
//  MessageBox
//-------------------------------------------------

int MameFrame::MessageBox(const wxString &message, long style, const wxString &caption)
{
	Pauser pauser(*this);
	return wxMessageBox(message, caption, style, this);
}


//-------------------------------------------------
//  OnKeyDown
//-------------------------------------------------

void MameFrame::OnKeyDown(wxKeyEvent &event)
{
	// pressing ALT to bring up menus is not friendly when running the emulation
	bool swallow_event = IsEmulationSessionActive() && event.GetKeyCode() == WXK_ALT;

	if (IsEmulationSessionActive() && event.GetKeyCode() == WXK_SCROLL)
	{
		m_prefs.SetMenuBarShown(!m_prefs.GetMenuBarShown());
		UpdateMenuBar();
	}

	// counterintuitively, the way to swallow the event is to _not_ call Skip(), which really
	// means "Skip this event handler"
	if (!swallow_event)
		event.Skip();
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
		if (MessageBox(message, wxYES_NO | wxICON_QUESTION) != wxYES)
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
	if (MessageBox(message, wxYES_NO | wxICON_QUESTION) == wxYES)
		Issue("exit");
}


//-------------------------------------------------
//  OnMenuStateLoad
//-------------------------------------------------

void MameFrame::OnMenuStateLoad()
{
	FileDialogCommand(
		{ "state_load" },
		Preferences::machine_path_type::last_save_state,
		true,
		s_wc_saved_state,
		file_dialog_type::LOAD);
}


//-------------------------------------------------
//  OnMenuStateSave
//-------------------------------------------------

void MameFrame::OnMenuStateSave()
{
	FileDialogCommand(
		{ "state_save" },
		Preferences::machine_path_type::last_save_state,
		true,
		s_wc_saved_state,
		file_dialog_type::SAVE);
}


//-------------------------------------------------
//  OnMenuSnapshotSave
//-------------------------------------------------

void MameFrame::OnMenuSnapshotSave()
{
	FileDialogCommand(
		{ "save_snapshot", "0" },
		Preferences::machine_path_type::working_directory,
		false,
		s_wc_save_snapshot,
		file_dialog_type::SAVE);
}


//-------------------------------------------------
//  OnMenuImages
//-------------------------------------------------

void MameFrame::OnMenuImages()
{
	Pauser pauser(*this);
	ImagesHost images_host(*this);
	show_images_dialog(images_host);
}


//-------------------------------------------------
//  OnMenuPaths
//-------------------------------------------------

void MameFrame::OnMenuPaths()
{
	Pauser pauser(*this);
	show_paths_dialog(*this, m_prefs);
	m_prefs.Save();
}


//-------------------------------------------------
//  OnMenuInputs
//-------------------------------------------------

void MameFrame::OnMenuInputs()
{
	Pauser pauser(*this);
	InputsHost host(*this);
	show_inputs_dialog(*this, host);
}


//-------------------------------------------------
//  OnMenuAbout
//-------------------------------------------------

void MameFrame::OnMenuAbout()
{
	const wxString eoln = wxTextFile::GetEOL();
	wxString message = wxTheApp->GetAppName()
		+ eoln
		+ eoln;

	// MAME version
	if (!m_info_db.version().IsEmpty())
		message += wxT("MAME ") + m_info_db.version() + eoln;

	// wxWidgets version
	message += wxVERSION_STRING;

	MessageBox(message, wxOK | wxICON_INFORMATION, "About BletchMAME");
}


//-------------------------------------------------
//  OnEmuMenuUpdateUI
//-------------------------------------------------

void MameFrame::OnEmuMenuUpdateUI(wxUpdateUIEvent &event, std::optional<bool> checked, bool enabled)
{
	bool is_active = IsEmulationSessionActive();
	event.Enable(is_active && enabled);

	if (checked.has_value())
		event.Check(is_active && checked.value());
}


//-------------------------------------------------
//  OnListXmlCompleted
//-------------------------------------------------

void MameFrame::OnListXmlCompleted(PayloadEvent<ListXmlResult> &event)
{
	ListXmlResult &payload(event.Payload());

	// identify the results
	if (payload.m_status == ListXmlResult::status::SUCCESS)
	{
		// if it succeeded, try to load the DB
		wxString db_path = m_prefs.GetMameXmlDatabasePath();
		if (m_info_db.load(db_path))
			UpdateMachineList();
	}

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
	StatusUpdate &payload(event.Payload());

	GetStatusFromPayload(m_status_paused,				payload.m_paused);
	GetStatusFromPayload(m_status_polling_input_seq,	payload.m_polling_input_seq);
	GetStatusFromPayload(m_status_frameskip,			payload.m_frameskip);
	GetStatusFromPayload(m_status_throttled,			payload.m_throttled);
	GetStatusFromPayload(m_status_throttle_rate,		payload.m_throttle_rate);
	GetStatusFromPayload(m_status_sound_attenuation,	payload.m_sound_attenuation);
	GetStatusFromPayload(m_status_images,				payload.m_images);
	GetStatusFromPayload(m_status_inputs,				payload.m_inputs);

	if (payload.m_speed_text.has_value())
		SetStatusText(payload.m_speed_text.value());

	m_pinging = false;
}


//-------------------------------------------------
//  GetStatusFromPayload
//-------------------------------------------------

template<typename TStatus, typename TPayload>
bool MameFrame::GetStatusFromPayload(TStatus &value, std::optional<TPayload> &payload)
{
	bool result = payload.has_value();
	if (result)
	{
		TPayload extracted_value = std::move(payload.value());
		value = std::move(extracted_value);
	}
	return result;
}


//-------------------------------------------------
//  FileDialogCommand
//-------------------------------------------------

void MameFrame::SetChatterListener(std::function<void(Chatter &&chatter)> &&func)
{
	m_on_chatter = std::move(func);
}


//-------------------------------------------------
//  FileDialogCommand
//-------------------------------------------------

void MameFrame::OnChatter(PayloadEvent<Chatter> &event)
{
	if (m_on_chatter)
		m_on_chatter(std::move(event.Payload()));
}


//-------------------------------------------------
//  FileDialogCommand
//-------------------------------------------------

void MameFrame::FileDialogCommand(std::vector<wxString> &&commands, Preferences::machine_path_type path_type, bool path_is_file, const wxString &wildcard_string, MameFrame::file_dialog_type dlgtype)
{
	// determine the style
	long style;
	switch (dlgtype)
	{
	case MameFrame::file_dialog_type::LOAD:
		style = wxFD_OPEN | wxFD_FILE_MUST_EXIST;
		break;
	case MameFrame::file_dialog_type::SAVE:
		style = wxFD_SAVE | wxFD_OVERWRITE_PROMPT;
		break;
	default:
		throw false;
	}

	// prepare the default dir/file
	const wxString &running_machine_name(GetRunningMachine().name());
	const wxString &default_path(m_prefs.GetMachinePath(running_machine_name, path_type));
	wxString default_dir;
	wxString default_file;
	wxString default_ext;
	wxFileName::SplitPath(default_path, &default_dir, &default_file, &default_ext);
	if (!default_ext.IsEmpty())
		default_file += "." + default_ext;

	// show the dialog
	Pauser pauser(*this);
	wxFileDialog dialog(
		this,
		wxFileSelectorPromptStr,
		default_dir,
		default_file,
		wildcard_string,
		style);
	if (dialog.ShowModal() != wxID_OK)
		return;
	wxString path = dialog.GetPath();

	// append the resulting path to the command list
	commands.push_back(path);

	// put back the default
	if (path_is_file)
	{
		m_prefs.SetMachinePath(running_machine_name, path_type, std::move(path));
	}
	else
	{
		wxString new_dir;
		wxFileName::SplitPath(path, &new_dir, nullptr, nullptr);
		m_prefs.SetMachinePath(running_machine_name, path_type, std::move(new_dir));
	}

	// finally issue the actual commands
	Issue(commands);
}


//-------------------------------------------------
//  OnListItemSelected
//-------------------------------------------------

void MameFrame::OnListItemSelected(wxListEvent &evt)
{
    long index = evt.GetIndex();
    m_prefs.SetSelectedMachine(m_info_db.machines()[index].name());
}


//-------------------------------------------------
//  OnListItemActivated
//-------------------------------------------------

void MameFrame::OnListItemActivated(wxListEvent &evt)
{
	// get the index
	long index = evt.GetIndex();
	if (index < 0 || index >= (long) m_info_db.machines().size())
		return;

	// and run the machine
	Run(index);
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
//  UpdateMachineList
//-------------------------------------------------

void MameFrame::UpdateMachineList()
{
	m_list_view->SetItemCount(m_info_db.machines().size());
	m_list_view->RefreshItems(0, m_info_db.machines().size() - 1);

	// find the currently selected machine
	auto iter = std::find_if(m_info_db.machines().begin(), m_info_db.machines().end(), [this](info::machine machine)
	{
		return machine.name() == m_prefs.GetSelectedMachine();
	});

	// if successful, select it
	if (iter < m_info_db.machines().end())
	{
		long selected_index = iter - m_info_db.machines().begin();
		m_list_view->Select(selected_index);
		m_list_view->EnsureVisible(selected_index);
	}
}


//-------------------------------------------------
//  GetListItemText
//-------------------------------------------------

wxString MameFrame::GetListItemText(size_t item, long column) const
{
	const info::machine machine(m_info_db.machines()[item]);

	wxString result;
	switch (column)
	{
	case 0:
		result = machine.name();
		break;
	case 1:
		result = machine.description();
		break;
	case 2:
		result = machine.year();
		break;
	case 3:
		result = machine.manufacturer();
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
	// is the emulation session active?
	bool is_active = IsEmulationSessionActive();

	// if so, hide the list view...
	m_list_view->Show(!is_active);

	// ...and enable pinging
	if (is_active)
		m_ping_timer.Start(500);
	else
		m_ping_timer.Stop();

	// ...and cascade other updates
	UpdateTitleBar();
	UpdateMenuBar();
}


//-------------------------------------------------
//  UpdateTitleBar
//-------------------------------------------------

void MameFrame::UpdateTitleBar()
{
	wxString title_text = wxTheApp->GetAppName();
	if (IsEmulationSessionActive())
	{
		title_text += ": " + m_client.GetCurrentTask<RunMachineTask>()->GetMachine().description();

		// we want to append "PAUSED" if and only if the user paused, not as a consequence of a menu
		if (m_status_paused.get() && !m_current_pauser)
			title_text += " PAUSED";
	}
	SetLabel(title_text);
}


//-------------------------------------------------
//  UpdateMenuBar
//-------------------------------------------------

void MameFrame::UpdateMenuBar()
{
	bool menu_bar_shown = !IsEmulationSessionActive() || m_prefs.GetMenuBarShown();

	// when we hide the menu bar, we disable the accelerators
	m_menu_bar->SetAcceleratorTable(menu_bar_shown ? m_menu_bar_accelerators : wxAcceleratorTable());

#ifdef WIN32
	// Win32 specific code
	SetMenu(GetHWND(), menu_bar_shown ? m_menu_bar->GetHMenu() : nullptr);
#else
	throw false;
#endif
}


//**************************************************************************
//  RUNTIME CONTROL
//
//	Actions that affect MAME at runtime go here.  The naming convention is
//	that "invocation actions" take the form InvokeXyz(), whereas methods
//	that change something take the form ChangeXyz()
//**************************************************************************

//-------------------------------------------------
//  Issue
//-------------------------------------------------

void MameFrame::Issue(const std::vector<wxString> &args)
{
	std::shared_ptr<RunMachineTask> task = m_client.GetCurrentTask<RunMachineTask>();
	if (!task)
		return;

	task->Issue(args);
}


void MameFrame::Issue(const std::initializer_list<wxString> &args)
{
	Issue(std::vector<wxString>(args));
}


void MameFrame::Issue(const char *command)
{
	wxString command_string = command;
	Issue({ command_string });
}


//-------------------------------------------------
//  InvokePing
//-------------------------------------------------

void MameFrame::InvokePing()
{
	// only issue a ping if there is an active session, and there is no ping in flight
	if (!m_pinging && IsEmulationSessionActive())
	{
		m_pinging = true;
		Issue("ping");
	}
}


//-------------------------------------------------
//  ChangePaused
//-------------------------------------------------

void MameFrame::ChangePaused(bool paused)
{
	Issue(paused ? "pause" : "resume");
}


//-------------------------------------------------
//  ChangeThrottled
//-------------------------------------------------

void MameFrame::ChangeThrottled(bool throttled)
{
	Issue({ "throttled", std::to_string(throttled ? 1 : 0) });
}


//-------------------------------------------------
//  ChangeThrottleRate
//-------------------------------------------------

void MameFrame::ChangeThrottleRate(float throttle_rate)
{
	Issue({ "throttle_rate", std::to_string(throttle_rate) });
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
	ChangeThrottleRate(s_throttle_rates[index]);
}


//-------------------------------------------------
//  ChangeSound
//-------------------------------------------------

void MameFrame::ChangeSound(bool sound_enabled)
{
	Issue({ "set_attenuation", std::to_string(sound_enabled ? SOUND_ATTENUATION_ON : SOUND_ATTENUATION_OFF) });
}


//-------------------------------------------------
//  IsSoundEnabled
//-------------------------------------------------

bool MameFrame::IsSoundEnabled() const
{
	return m_status_sound_attenuation != SOUND_ATTENUATION_OFF;
}


//**************************************************************************
//  PAUSER
//**************************************************************************

//-------------------------------------------------
//  Pauser ctor
//-------------------------------------------------

MameFrame::Pauser::Pauser(MameFrame &host, bool actually_pause)
	: m_host(host)
	, m_last_pauser(host.m_current_pauser)
{
	// if we're running and not pause, pause while the message box is up
	m_is_running = actually_pause && m_host.IsEmulationSessionActive() && !m_host.m_status_paused.get();
	if (m_is_running)
		m_host.ChangePaused(true);

	// track the chain of pausers
	m_host.m_current_pauser = this;
}


//-------------------------------------------------
//  Pauser dtor
//-------------------------------------------------

MameFrame::Pauser::~Pauser()
{
	if (m_is_running)
		m_host.ChangePaused(false);
	m_host.m_current_pauser = m_last_pauser;
}


//**************************************************************************
//  ImagesHost
//**************************************************************************

//-------------------------------------------------
//  ImagesHost ctor
//-------------------------------------------------

MameFrame::ImagesHost::ImagesHost(MameFrame &host)
	: m_host(host)
{
}


//-------------------------------------------------
//  GetImages
//-------------------------------------------------

observable::value<std::vector<Image>> &MameFrame::ImagesHost::GetImages()
{
	return m_host.m_status_images;
}


//-------------------------------------------------
//  GetWorkingDirectory
//-------------------------------------------------

const wxString &MameFrame::ImagesHost::GetWorkingDirectory() const
{
	return m_host.m_prefs.GetMachinePath(GetMachineName(), Preferences::machine_path_type::working_directory);
}


//-------------------------------------------------
//  SetWorkingDirectory
//-------------------------------------------------

void MameFrame::ImagesHost::SetWorkingDirectory(wxString &&dir)
{
	m_host.m_prefs.SetMachinePath(GetMachineName(), Preferences::machine_path_type::working_directory, std::move(dir));
}


//-------------------------------------------------
//  GetExtensions
//-------------------------------------------------

std::vector<wxString> MameFrame::ImagesHost::GetExtensions(const wxString &tag) const
{
	// find the device declaration
	auto devices = m_host.GetRunningMachine().devices();

	auto iter = std::find_if(devices.begin(), devices.end(), [&tag](info::device dev)
	{
		return dev.tag() == tag;
	});
	assert(iter != devices.end());

	// and return it!
	return util::string_split((*iter).extensions(), [](wchar_t ch) { return ch == ','; });
}


//-------------------------------------------------
//  LoadImage
//-------------------------------------------------

void MameFrame::ImagesHost::LoadImage(const wxString &tag, wxString &&path)
{
	m_host.Issue({ "load", tag, std::move(path) });
}


//-------------------------------------------------
//  UnloadImage
//-------------------------------------------------

void MameFrame::ImagesHost::UnloadImage(const wxString &tag)
{
	m_host.Issue({ "unload", tag });
}


//-------------------------------------------------
//  GetMachineName
//-------------------------------------------------

const wxString &MameFrame::ImagesHost::GetMachineName() const
{
	return m_host.GetRunningMachine().name();
}


//**************************************************************************
//  InputsHost
//**************************************************************************

//-------------------------------------------------
//  InputsHost ctor
//-------------------------------------------------

MameFrame::InputsHost::InputsHost(MameFrame &host)
	: m_host(host)
{
}


//-------------------------------------------------
//  GetInputs
//-------------------------------------------------

const std::vector<Input> &MameFrame::InputsHost::GetInputs()
{
	return m_host.m_status_inputs;
}


//-------------------------------------------------
//  GetPollingSeqChanged
//-------------------------------------------------

observable::value<bool> &MameFrame::InputsHost::GetPollingSeqChanged()
{
	return m_host.m_status_polling_input_seq;
}


//-------------------------------------------------
//  StartPolling
//-------------------------------------------------

void MameFrame::InputsHost::StartPolling(const wxString &port_tag, ioport_value mask, InputSeq::inputseq_type seq_type)
{
	wxString seq_type_string;
	switch (seq_type)
	{
	case InputSeq::inputseq_type::STANDARD:
		seq_type_string = "standard";
		break;
	case InputSeq::inputseq_type::INCREMENT:
		seq_type_string = "increment";
		break;
	case InputSeq::inputseq_type::DECREMENT:
		seq_type_string = "decrement";
		break;
	default:
		throw false;
	}

	m_host.Issue({ "seq_poll_start", port_tag, std::to_string(mask), seq_type_string });
}


//-------------------------------------------------
//  StopPolling
//-------------------------------------------------

void MameFrame::InputsHost::StopPolling()
{
	m_host.Issue({ "seq_poll_stop" });
}


//**************************************************************************
//  INSTANTIATION
//**************************************************************************

//-------------------------------------------------
//  create_mame_frame
//-------------------------------------------------

wxFrame *create_mame_frame()
{
    return new MameFrame();
}

