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
#include <wx/notebook.h>
#include <wx/dir.h>
#include <wx/fswatcher.h>

#include "frame.h"
#include "client.h"
#include "dialogs/console.h"
#include "dialogs/images.h"
#include "dialogs/inputs.h"
#include "dialogs/loading.h"
#include "dialogs/paths.h"
#include "dialogs/switches.h"
#include "prefs.h"
#include "profile.h"
#include "listxmltask.h"
#include "versiontask.h"
#include "runmachinetask.h"
#include "utility.h"
#include "virtuallistview.h"
#include "info.h"
#include "status.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

// IDs for the controls and the menu commands
enum
{
	// user IDs
	ID_PING_TIMER				= wxID_HIGHEST + 1,
	ID_LAST,

	ID_POPUP_MENU_BEGIN			= ID_LAST + 1000,

	// styles (we want to show the menu bar at full size)
	FULL_SCREEN_STYLE			= wxFULLSCREEN_NOTOOLBAR | wxFULLSCREEN_NOSTATUSBAR | wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOCAPTION
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

			virtual observable::value<std::vector<status::image>> &GetImages();
			virtual const wxString &GetWorkingDirectory() const;
			virtual void SetWorkingDirectory(wxString &&dir);
			virtual const std::vector<wxString> &GetRecentFiles(const wxString &tag) const;
			virtual std::vector<wxString> GetExtensions(const wxString &tag) const;
			virtual void CreateImage(const wxString &tag, wxString &&path);
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

			virtual const std::vector<status::input> &GetInputs() override;
			virtual observable::value<bool> &GetPollingSeqChanged() override;
			virtual void StartPolling(const wxString &port_tag, ioport_value mask, status::input_seq::type seq_type) override;
			virtual void StopPolling() override;

		private:
			MameFrame & m_host;
		};

		class SwitchesHost : public ISwitchesHost
		{
		public:
			SwitchesHost(MameFrame &host);

			virtual const std::vector<status::input> &GetInputs() override;
			virtual void SetInputValue(const wxString &port_tag, ioport_value mask, ioport_value value) override;

		private:
			MameFrame & m_host;
		};

		// status of MAME version checks
		enum class check_mame_info_status
		{
			SUCCESS,			// we've loaded an info DB that matches the expected MAME version
			MAME_NOT_FOUND,		// we can't find the MAME executable
			DB_NEEDS_REBUILD	// we've found MAME, but we must rebuild the database
		};

		MameClient								m_client;
		Preferences							    m_prefs;
		wxTextCtrl *							m_search_box;
		wxNotebook *							m_note_book;
		wxListView *						    m_machine_view;
		wxListView *						    m_profile_view;
		wxMenuBar *							    m_menu_bar;
		wxAcceleratorTable						m_menu_bar_accelerators;
		wxTimer									m_ping_timer;
		bool									m_pinging;
		const Pauser *							m_current_pauser;
		std::function<void(Chatter &&)>			m_on_chatter;
		observable::unique_subscription			m_watch_subscription;
		std::vector<std::function<void()>>		m_on_close_funcs;
		wxFileSystemWatcher						m_fsw_profiles;

		// information retrieved by -version
		wxString								m_mame_version;

		// information retrieved by -listxml
		info::database							m_info_db;

		// machine list indirection
		std::vector<int>						m_machine_list_indirections;

		// profile list
		observable::value<std::vector<profiles::profile>>	m_profiles;

		// status of running emulation
		std::optional<status::state>			m_state;

		static const float s_throttle_rates[];
		static const wxString s_wc_saved_state;
		static const wxString s_wc_save_snapshot;

		const int SOUND_ATTENUATION_OFF = -32;
		const int SOUND_ATTENUATION_ON = 0;

		// startup
		void CreateMenuBar();
		wxListView &CreateListView(wxWindow &parent, wxWindowID winid, Preferences::list_view_type type, wxString(MameFrame::*on_get_item_text_func)(long, long) const);

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
		void OnMenuInputs(status::input::input_class input_class);
		void OnMenuSwitches(status::input::input_class input_class);
		void OnMenuAbout();
		void OnListColumnResized(wxListEvent &evt, wxListView &list_view, Preferences::list_view_type type);
		void OnMachineListItemSelected(wxListEvent &event);
		void OnMachineListItemActivated(wxListEvent &event);
		void OnProfileListItemActivated(wxListEvent &event);
		void OnMachineListItemContextMenu(wxListEvent &event);
		void OnProfileListItemContextMenu(wxListEvent &event);
		void OnSearchBoxTextChanged();

		// task notifications
		void OnVersionCompleted(PayloadEvent<VersionResult> &event);
		void OnListXmlCompleted(PayloadEvent<ListXmlResult> &event);
		void OnRunMachineCompleted(PayloadEvent<RunMachineResult> &event);
		void OnStatusUpdate(PayloadEvent<status::update> &event);
		void OnChatter(PayloadEvent<Chatter> &event);

		// miscellaneous
		bool IsMameExecutablePresent() const;
		void InitialCheckMameInfoDatabase();
		check_mame_info_status CheckMameInfoDatabase();
		bool PromptForMameExecutable();
		bool RefreshMameInfoDatabase();
		info::machine GetRunningMachine() const;
		void Run(const info::machine &machine, std::unordered_map<wxString, wxString> &&images = {});
		void Run(const profiles::profile &profile);
		int MessageBox(const wxString &message, long style = wxOK | wxCENTRE, const wxString &caption = wxTheApp->GetAppName());
		void OnEmuMenuUpdateUI(wxUpdateUIEvent &event, std::optional<bool> checked = { }, bool enabled = true);
		void UpdateMachineList();
		void UpdateProfileDirectories(bool update_fsw);
		info::machine GetMachineFromIndex(long item) const;
		wxString GetMachineListItemText(long item, long column) const;
		wxString GetProfileListItemText(long item, long column) const;
		void UpdateEmulationSession();
		void UpdateTitleBar();
		void UpdateMenuBar();
		void UpdateStatusBar();
		void SetChatterListener(std::function<void(Chatter &&chatter)> &&func);
		void FileDialogCommand(std::vector<wxString> &&commands, Preferences::machine_path_type path_type, bool path_is_file, const wxString &wildcard_string, file_dialog_type dlgtype);
		static wxString InputClassText(status::input::input_class input_class, bool elipsis);
		void PlaceInRecentFiles(const wxString &tag, const wxString &path);
		static const wxString &GetDeviceType(const info::machine &machine, const wxString &tag);
		void WatchForImageMount(const wxString &tag);

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
//  VERSION INFO
//**************************************************************************

#ifdef _MSC_VER
// we're not supporing build numbers for MSVC builds
static const char build_version[] = "MSVC";
static const char build_date_time[] = "MSVC";
#else
extern const char build_version[];
extern const char build_date_time[];
#endif


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
	, m_search_box(nullptr)
	, m_note_book(nullptr)
	, m_machine_view(nullptr)
	, m_profile_view(nullptr)
	, m_menu_bar(nullptr)
	, m_ping_timer(this, ID_PING_TIMER)
	, m_pinging(false)
	, m_current_pauser(nullptr)
{
	int id = wxID_LAST + 1;

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
		
	// create a notebook
	m_note_book = new wxNotebook(this, id++);

	// create the machine list panel
	wxPanel *machine_panel = new wxPanel(m_note_book, id++);

	// create the machine list search box
	m_search_box = new wxTextCtrl(machine_panel, id++, m_prefs.GetSearchBoxText());

	// create the machine list
	m_machine_view = &CreateListView(*machine_panel, id++, Preferences::list_view_type::machine, &MameFrame::GetMachineListItemText);
	UpdateMachineList();
	m_info_db.set_on_changed([this]() { UpdateMachineList(); });

	// specify the sizer on the machine panel
	SpecifySizer(*machine_panel, { boxsizer_orientation::VERTICAL, 0, {
		{ 0, wxEXPAND, *m_search_box },
		{ 1, wxEXPAND, *m_machine_view }
	}});

	// create the profile view
	m_profile_view = &CreateListView(*m_note_book, id++, Preferences::list_view_type::profile, &MameFrame::GetProfileListItemText);
	m_profiles.subscribe([this]
	{
		m_profile_view->SetItemCount(m_profiles.get().size());
		m_profile_view->RefreshItems(0, m_profiles.get().size() - 1);
	});
	UpdateProfileDirectories(true);

	// add the notebook pages
	m_note_book->AddPage(machine_panel, "Machines");
	m_note_book->AddPage(m_profile_view, "Profiles");
	m_note_book->SetSelection(static_cast<size_t>(m_prefs.GetSelectedTab()));

	// set up the file system watcher
	m_fsw_profiles.SetOwner(this);

	// the ties that bind...
	Bind(EVT_VERSION_RESULT,			[this](auto &event) { OnVersionCompleted(event);    															});
	Bind(EVT_LIST_XML_RESULT,		    [this](auto &event) { OnListXmlCompleted(event);    															});
	Bind(EVT_RUN_MACHINE_RESULT,	    [this](auto &event) { OnRunMachineCompleted(event);																});
	Bind(EVT_STATUS_UPDATE,			    [this](auto &event) { OnStatusUpdate(event);        															});
	Bind(EVT_CHATTER,					[this](auto &event)	{ OnChatter(event);																			});
	Bind(wxEVT_KEY_DOWN,				[this](auto &event) { OnKeyDown(event);             															});
	Bind(wxEVT_SIZE,					[this](auto &event) { OnSize(event);																			});
	Bind(wxEVT_CLOSE_WINDOW,			[this](auto &event) { OnClose(event);																			});
	Bind(wxEVT_NOTEBOOK_PAGE_CHANGED,	[this](auto &event) { m_prefs.SetSelectedTab(static_cast<Preferences::list_view_type>(event.GetSelection()));	}, m_note_book->GetId());
	Bind(wxEVT_LIST_COL_END_DRAG,		[this](auto &event) { OnListColumnResized(event, *m_machine_view, Preferences::list_view_type::machine);		}, m_machine_view->GetId());
	Bind(wxEVT_LIST_COL_END_DRAG,		[this](auto &event) { OnListColumnResized(event, *m_profile_view, Preferences::list_view_type::profile);		}, m_profile_view->GetId());
	Bind(wxEVT_LIST_ITEM_SELECTED,		[this](auto &event) { OnMachineListItemSelected(event);															}, m_machine_view->GetId());
	Bind(wxEVT_LIST_ITEM_ACTIVATED,		[this](auto &event) { OnMachineListItemActivated(event);														}, m_machine_view->GetId());
	Bind(wxEVT_LIST_ITEM_ACTIVATED,		[this](auto &event) { OnProfileListItemActivated(event);														}, m_profile_view->GetId());
	Bind(wxEVT_LIST_ITEM_RIGHT_CLICK,	[this](auto &event) { OnMachineListItemContextMenu(event);														}, m_machine_view->GetId());
	Bind(wxEVT_LIST_ITEM_RIGHT_CLICK,	[this](auto &event) { OnProfileListItemContextMenu(event);														}, m_profile_view->GetId());
	Bind(wxEVT_TIMER,					[this](auto &)		{ InvokePing();																				});
	Bind(wxEVT_TEXT,					[this](auto &)		{ OnSearchBoxTextChanged();																	}, m_search_box->GetId());
	Bind(wxEVT_FSWATCHER,				[this](auto &)		{ UpdateProfileDirectories(false);															});

	// nothing is running yet...
	UpdateEmulationSession();

	// time for the initial check
	InitialCheckMameInfoDatabase();
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
	wxMenuItem *full_screen_menu_item			= options_menu->Append(id++, "Full Screen\tF11", wxEmptyString, wxITEM_CHECK);
	wxMenuItem *sound_menu_item					= options_menu->Append(id++, "Sound", wxEmptyString, wxITEM_CHECK);
	options_menu->AppendSeparator();
	wxMenuItem *console_menu_item				= options_menu->Append(id++, "Console...");

	// create the "Settings" menu
	wxMenu *settings_menu = new wxMenu();
	wxMenuItem *show_input_controllers_dialog_menu_item	= settings_menu->Append(id++, InputClassText(status::input::input_class::CONTROLLER, true));
	wxMenuItem *show_input_keyboard_dialog_menu_item	= settings_menu->Append(id++, InputClassText(status::input::input_class::KEYBOARD, true));
	wxMenuItem *show_input_misc_dialog_menu_item		= settings_menu->Append(id++, InputClassText(status::input::input_class::MISC, true));
	wxMenuItem *show_input_config_dialog_menu_item		= settings_menu->Append(id++, InputClassText(status::input::input_class::CONFIG, true));
	wxMenuItem *show_input_dipswitch_dialog_menu_item	= settings_menu->Append(id++, InputClassText(status::input::input_class::DIPSWITCH, true));
	settings_menu->AppendSeparator();
	wxMenuItem *show_paths_dialog_menu_item				= settings_menu->Append(id++, "Paths...");

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
	Bind(wxEVT_MENU, [this](auto &)	{ ChangePaused(!m_state->paused().get());									}, pause_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuStateLoad();														}, load_state_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuStateSave();														}, save_state_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuSnapshotSave();														}, save_screenshot_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { Issue("soft_reset");														}, soft_reset_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { Issue("hard_reset");														}, hard_reset_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &)	{ Close(false);																}, exit_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &)	{ ChangeThrottleRate(-1);													}, increase_speed_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &)	{ ChangeThrottleRate(+1);													}, decrease_speed_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &)	{ ChangeThrottled(!m_state->throttled());									}, warp_mode_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuImages();															}, images_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { ShowFullScreen(!IsFullScreen(), FULL_SCREEN_STYLE);						}, full_screen_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { ChangeSound(!IsSoundEnabled());											}, sound_menu_item->GetId());
	
	Bind(wxEVT_MENU, [this](auto &) { show_console_dialog(*this, m_client, *this);								}, console_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuInputs(status::input::input_class::CONTROLLER);						}, show_input_controllers_dialog_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuInputs(status::input::input_class::KEYBOARD);						}, show_input_keyboard_dialog_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuInputs(status::input::input_class::MISC);							}, show_input_misc_dialog_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuSwitches(status::input::input_class::CONFIG);						}, show_input_config_dialog_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuSwitches(status::input::input_class::DIPSWITCH);					}, show_input_dipswitch_dialog_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuPaths();															}, show_paths_dialog_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { RefreshMameInfoDatabase();												}, refresh_mame_info_menu_item->GetId());
	Bind(wxEVT_MENU, [this](auto &) { OnMenuAbout();															}, about_menu_item->GetId());

	// Bind UI update events	
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);																					}, stop_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, m_state && m_state->paused().get());												}, pause_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);																					}, load_state_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);																					}, save_state_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);																					}, save_screenshot_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);																					}, soft_reset_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);																					}, hard_reset_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);																					}, increase_speed_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);																					}, decrease_speed_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, m_state && !m_state->throttled());													}, warp_mode_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, { }, m_state && !m_state->images().get().empty());									}, images_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { event.Check(IsFullScreen());																				}, full_screen_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, IsSoundEnabled());																	}, sound_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event);																					}, console_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, { }, m_state && m_state->has_input_class(status::input::input_class::CONTROLLER));	}, show_input_controllers_dialog_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, { }, m_state && m_state->has_input_class(status::input::input_class::KEYBOARD));	}, show_input_keyboard_dialog_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, { }, m_state && m_state->has_input_class(status::input::input_class::MISC));		}, show_input_misc_dialog_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, { }, m_state && m_state->has_input_class(status::input::input_class::CONFIG));		}, show_input_config_dialog_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { OnEmuMenuUpdateUI(event, { }, m_state && m_state->has_input_class(status::input::input_class::DIPSWITCH));	}, show_input_dipswitch_dialog_menu_item->GetId());
	Bind(wxEVT_UPDATE_UI, [this](auto &event) { event.Enable(!m_state.has_value());																			}, refresh_mame_info_menu_item->GetId());

	// special setup for throttle dynamic menu
	for (size_t i = 0; i < sizeof(s_throttle_rates) / sizeof(s_throttle_rates[0]); i++)
	{
		float throttle_rate	= s_throttle_rates[i];
		wxString text		= std::to_string((int)(throttle_rate * 100)) + "%";
		wxMenuItem *item	= throttle_menu->Insert(i, id++, text, wxEmptyString, wxITEM_CHECK);

		Bind(wxEVT_MENU,		[this, throttle_rate](auto &)		{ ChangeThrottleRate(throttle_rate);												}, item->GetId());
		Bind(wxEVT_UPDATE_UI,	[this, throttle_rate](auto &event)	{ OnEmuMenuUpdateUI(event, m_state && m_state->throttle_rate() == throttle_rate);	}, item->GetId());
	}

	// special setup for frameskip dynamic menu
	for (int i = -1; i <= 10; i++)
	{
		wxString text		= i == -1 ? "Auto" : std::to_string(i);
		wxMenuItem *item	= frameskip_menu->Insert(i + 1, id++, text, wxEmptyString, wxITEM_CHECK);
		std::string value	= i == -1 ? "auto" : std::to_string(i);
		std::string command = "frameskip " + value;

		Bind(wxEVT_MENU,		[this, value](auto &)		{ Issue({ "frameskip", value });										}, item->GetId());
		Bind(wxEVT_UPDATE_UI,	[this, value](auto &event)	{ OnEmuMenuUpdateUI(event, m_state && m_state->frameskip() == value);	}, item->GetId());		
	}
}


//-------------------------------------------------
//  CreateListViewColumns
//-------------------------------------------------

wxListView &MameFrame::CreateListView(wxWindow &parent, wxWindowID winid, Preferences::list_view_type type, wxString(MameFrame::*on_get_item_text_func)(long, long) const)
{
	// create the list view
	VirtualListView *list_view = new VirtualListView(&parent, winid);
	list_view->SetOnGetItemText([this, on_get_item_text_func](long item, long column) -> wxString
	{
		return ((*this).*(on_get_item_text_func))(item, column);
	});

	// create each of the columns
	list_view->ClearAll();
	const ColumnDesc *column_descs = Preferences::GetColumnDescs(type);
	int column_count = 0;
	while (column_descs[column_count].m_id)
	{
		int width = m_prefs.GetColumnWidth(type, column_count);
		list_view->AppendColumn(column_descs[column_count].m_description, wxLIST_FORMAT_LEFT, width);
		column_count++;
	}

	// set the column order
	const auto &order = m_prefs.GetColumnsOrder(type);
	list_view->SetColumnsOrder(wxArrayInt(order.begin(), order.end()));

	// add a callback to read the columns order on exit
	m_on_close_funcs.emplace_back([this, list_view, column_count, type]
	{
		std::vector<int> order;
		order.resize(column_count);

		for (int i = 0; i < column_count; i++)
			order[i] = list_view->GetColumnOrder(i);

		m_prefs.SetColumnsOrder(type, std::move(order));
	});

	return *list_view;
}


//-------------------------------------------------
//  IsMameExecutablePresent
//-------------------------------------------------

bool MameFrame::IsMameExecutablePresent() const
{
	const wxString &path = m_prefs.GetPath(Preferences::path_type::emu_exectuable);
	return !path.empty() && wxFileExists(path);
}


//-------------------------------------------------
//  InitialCheckMameInfoDatabase - called when we
//	load up for the very first time
//-------------------------------------------------

void MameFrame::InitialCheckMameInfoDatabase()
{
	bool done = false;
	while (!done)
	{
		switch (CheckMameInfoDatabase())
		{
		case check_mame_info_status::SUCCESS:
			// we're good!
			done = true;
			break;

		case check_mame_info_status::MAME_NOT_FOUND:
			// prompt the user for the MAME executable
			if (!PromptForMameExecutable())
			{
				// the (l)user gave up; guess we're done...
				done = true;
			}
			break;

		case check_mame_info_status::DB_NEEDS_REBUILD:
			// start a rebuild; whether the process succeeds or fails, we're done
			RefreshMameInfoDatabase();
			done = true;
			break;

		default:
			throw false;
		}
	}
}


//-------------------------------------------------
//  CheckMameInfoDatabase - checks the version and
//	the MAME info DB
//
//	how to respond to failure conditions is up to
//	the caller
//-------------------------------------------------

MameFrame::check_mame_info_status MameFrame::CheckMameInfoDatabase()
{
	// first thing, check to see if the executable is there
	if (!IsMameExecutablePresent())
		return check_mame_info_status::MAME_NOT_FOUND;

	// get the version - this should be blazingly fast
	m_client.Launch(create_version_task());
	while (m_client.IsTaskActive())
		wxYield();

	// we didn't get a version?  treat this as if we cannot find the
	// executable
	if (m_mame_version.empty())
		return check_mame_info_status::MAME_NOT_FOUND;

	// now let's try to open the info DB; we expect a specific version
	wxString db_path = m_prefs.GetMameXmlDatabasePath();
	if (!m_info_db.load(db_path, m_mame_version))
		return check_mame_info_status::DB_NEEDS_REBUILD;

	// success!  we can update the machine list
	return check_mame_info_status::SUCCESS;
}


//-------------------------------------------------
//  PromptForMameExecutable
//-------------------------------------------------

bool MameFrame::PromptForMameExecutable()
{
	wxString path = show_specify_single_path_dialog(*this, Preferences::path_type::emu_exectuable, m_prefs.GetPath(Preferences::path_type::emu_exectuable));
	if (path.empty())
		return false;

	m_prefs.SetPath(Preferences::path_type::emu_exectuable, std::move(path));
	return true;
}


//-------------------------------------------------
//  RefreshMameInfoDatabase
//-------------------------------------------------

bool MameFrame::RefreshMameInfoDatabase()
{
	// sanity check; bail if we can't find the executable
	if (!IsMameExecutablePresent())
		return false;

	// list XML
	wxString db_path = m_prefs.GetMameXmlDatabasePath();
	m_client.Launch(create_list_xml_task(wxString(db_path)));

	// and show the dialog
	if (!show_loading_mame_info_dialog(*this, [this]() { return !m_client.IsTaskActive(); }))
	{
		m_client.Abort();
		return false;
	}

	// we've succeeded; load the DB
	if (!m_info_db.load(db_path))
	{
		// a failure here is likely due to a very strange condition (e.g. - someone deleting the infodb
		// file out from under me)
		return false;
	}

	return true;
}


//-------------------------------------------------
//  GetRunningMachine
//-------------------------------------------------

info::machine MameFrame::GetRunningMachine() const
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

void MameFrame::Run(const info::machine &machine, std::unordered_map<wxString, wxString> &&images)
{
	// we need to have full information to support the emulation session; retrieve
	// fake a pauser to forestall "PAUSED" from appearing in the menu bar
	Pauser fake_pauser(*this, false);

	// run the emulation
	Task::ptr task = std::make_unique<RunMachineTask>(
		machine,
		*this);
	m_client.Launch(std::move(task));

	// set up running state and subscribe to events
	m_state.emplace();
	m_state->paused().subscribe(				[this]() { UpdateTitleBar(); });
	m_state->phase().subscribe(					[this]() { UpdateStatusBar(); });
	m_state->speed_percent().subscribe(			[this]() { UpdateStatusBar(); });
	m_state->effective_frameskip().subscribe(	[this]() { UpdateStatusBar(); });
	m_state->startup_text().subscribe(			[this]() { UpdateStatusBar(); });
	m_state->images().subscribe(				[this]() { UpdateStatusBar(); });

	// we have a session running; hide/show things respectively
	UpdateEmulationSession();

	// wait for first ping
	m_pinging = true;
	while (m_pinging)
	{
		if (!m_state.has_value())
			return;
		wxYield();
	}

	// mount images
	for (auto &i : images)
	{
		Issue({ "load", std::move(i.first), std::move(i.second) });
	}

	// do we have any images that require images?
	auto iter = std::find_if(m_state->images().get().cbegin(), m_state->images().get().cend(), [](const status::image &image)
	{
		return image.m_must_be_loaded && image.m_file_name.IsEmpty();
	});
	if (iter != m_state->images().get().cend())
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


void MameFrame::Run(const profiles::profile &profile)
{
	// find the machine
	std::optional<info::machine> machine = m_info_db.find_machine(profile.machine());
	if (!machine)
	{
		MessageBox("Unknown machine: " + profile.machine());
		return;
	}

	// get a list of images
	std::unordered_map<wxString, wxString> images;
	for (const profiles::image &i : profile.images())
	{
		images[i.m_tag] = i.m_path;
	}

	Run(machine.value(), std::move(images));
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
	bool swallow_event = m_state.has_value() && event.GetKeyCode() == WXK_ALT;

	if (m_state.has_value() && event.GetKeyCode() == WXK_SCROLL)
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
	if (m_state.has_value())
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

	// call all on close functions
	for (const auto &func : m_on_close_funcs)
		func();

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
	std::vector<Preferences::path_type> changed_paths;

	// show the dialog
	{
		Pauser pauser(*this);
		changed_paths = show_paths_dialog(*this, m_prefs);
		m_prefs.Save();
	}

	// lambda to simplify "is this path changed?"
	auto is_changed = [&changed_paths](Preferences::path_type type) -> bool
	{
		auto iter = std::find(changed_paths.begin(), changed_paths.end(), type);
		return iter != changed_paths.end();
	};

	// did the user change the executable path?
	if (is_changed(Preferences::path_type::emu_exectuable))
	{
		// they did; check the MAME info DB
		check_mame_info_status status = CheckMameInfoDatabase();
		switch (status)
		{
		case check_mame_info_status::SUCCESS:
			// we're good!
			break;

		case check_mame_info_status::MAME_NOT_FOUND:
		case check_mame_info_status::DB_NEEDS_REBUILD:
			// in both of these scenarios, we need to clear out the list
			m_info_db.reset();

			// start a rebuild if that is the only problem
			if (status == check_mame_info_status::DB_NEEDS_REBUILD)
				RefreshMameInfoDatabase();
			break;

		default:
			throw false;
		}
	}

	// did the user change the profiles path?
	if (is_changed(Preferences::path_type::profiles))
		UpdateProfileDirectories(true);
}


//-------------------------------------------------
//  OnMenuInputs
//-------------------------------------------------

void MameFrame::OnMenuInputs(status::input::input_class input_class)
{
	wxString title = InputClassText(input_class, false);
	Pauser pauser(*this);
	InputsHost host(*this);
	show_inputs_dialog(*this, title, host, input_class);
}


//-------------------------------------------------
//  OnMenuSwitches
//-------------------------------------------------

void MameFrame::OnMenuSwitches(status::input::input_class input_class)
{
	wxString title = InputClassText(input_class, false);
	Pauser pauser(*this);
	SwitchesHost host(*this);
	show_switches_dialog(*this, title, host, input_class, GetRunningMachine());
}


//-------------------------------------------------
//  OnMenuAbout
//-------------------------------------------------

void MameFrame::OnMenuAbout()
{
	const wxString eoln = wxTextFile::GetEOL();
	wxString message = wxTheApp->GetAppName() + eoln
		+ build_version + eoln
		+ build_date_time + eoln
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
	bool is_active = m_state.has_value();
	event.Enable(is_active && enabled);

	if (checked.has_value())
		event.Check(is_active && checked.value());
}


//-------------------------------------------------
//  OnVersionCompleted
//-------------------------------------------------

void MameFrame::OnVersionCompleted(PayloadEvent<VersionResult> &event)
{
	VersionResult &payload(event.Payload());
	m_mame_version = std::move(payload.m_version);

	// warn the user if this is not a worker-ui compatible build of MAME; looking forward
	// to this going away
	if (!payload.m_version.empty() && !payload.m_version.EndsWith("-worker-ui)"))
		MessageBox("This does not appear to be a worker-ui build of MAME; BletchMAME may not function correctly");

	m_client.Reset();
}


//-------------------------------------------------
//  OnListXmlCompleted
//-------------------------------------------------

void MameFrame::OnListXmlCompleted(PayloadEvent<ListXmlResult> &event)
{
	ListXmlResult &payload(event.Payload());

	// check the status
	switch (payload.m_status)
	{
	case ListXmlResult::status::SUCCESS:
		// if it succeeded, try to load the DB
		{
			wxString db_path = m_prefs.GetMameXmlDatabasePath();
			m_info_db.load(db_path);
		}
		break;

	case ListXmlResult::status::ABORTED:
		// if we aborted, do nothing
		break;

	case ListXmlResult::status::ERROR:
		// present an error message
		MessageBox(!payload.m_error_message.empty()
			? payload.m_error_message
			: wxT("Error building MAME info database"));
		break;

	default:
		throw false;
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
	m_state.reset();
    UpdateEmulationSession();
	UpdateStatusBar();

	// report any errors
	if (!payload.m_error_message.IsEmpty())
	{
		wxMessageBox(payload.m_error_message, "Error", wxOK | wxCENTRE, this);
	}
}


//-------------------------------------------------
//  OnStatusUpdate
//-------------------------------------------------

void MameFrame::OnStatusUpdate(PayloadEvent<status::update> &event)
{
	status::update &payload = event.Payload();
	m_state->update(std::move(payload));
	m_pinging = false;
}


//-------------------------------------------------
//  WatchForImageMount
//-------------------------------------------------

void MameFrame::WatchForImageMount(const wxString &tag)
{
	// find the current value; we want to monitor for this value changing
	wxString current_value;
	const status::image *image = m_state->find_image(tag);
	if (image)
		current_value = image->m_file_name;

	// start watching
	m_watch_subscription = m_state->images().subscribe([this, current_value{std::move(current_value)}, tag{std::move(tag)}]
	{
		// did the value change?
		const status::image *image = m_state->find_image(tag);
		if (image && image->m_file_name != current_value)
		{
			// it did!  place the new file in recent files
			PlaceInRecentFiles(tag, image->m_file_name);

			// and stop subscribing
			m_watch_subscription = observable::unique_subscription();
		}
	});	
}


//-------------------------------------------------
//  PlaceInRecentFiles
//-------------------------------------------------

void MameFrame::PlaceInRecentFiles(const wxString &tag, const wxString &path)
{
	// get the machine and device type to update recents
	info::machine machine = GetRunningMachine();
	const wxString &device_type = GetDeviceType(machine, tag);

	// actually edit the recent files; start by getting recent files
	std::vector<wxString> &recent_files = m_prefs.GetRecentDeviceFiles(machine.name(), device_type);

	// ...and clearing out places where that entry already exists
	std::vector<wxString>::iterator iter;
	while ((iter = std::find(recent_files.begin(), recent_files.end(), path)) != recent_files.end())
		recent_files.erase(iter);

	// ...insert the new value
	recent_files.insert(recent_files.begin(), path);

	// and cull the list
	const size_t MAXIMUM_RECENT_FILES = 10;
	if (recent_files.size() > MAXIMUM_RECENT_FILES)
		recent_files.erase(recent_files.begin() + MAXIMUM_RECENT_FILES, recent_files.end());
}


//-------------------------------------------------
//  GetDeviceType
//-------------------------------------------------

const wxString &MameFrame::GetDeviceType(const info::machine &machine, const wxString &tag)
{
	static const wxString s_empty;

	auto iter = std::find_if(
		machine.devices().begin(),
		machine.devices().end(),
		[&tag](const info::device device)
		{
			return device.tag() == tag;
		});

	return iter != machine.devices().end()
		? (*iter).type()
		: s_empty;
}


//-------------------------------------------------
//  SetChatterListener
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
//  OnMachineListItemSelected
//-------------------------------------------------

void MameFrame::OnMachineListItemSelected(wxListEvent &evt)
{
	long index = evt.GetIndex();
	const info::machine machine = GetMachineFromIndex(index);
	m_prefs.SetSelectedMachine(machine.name());
}


//-------------------------------------------------
//  OnMachineListItemActivated
//-------------------------------------------------

void MameFrame::OnMachineListItemActivated(wxListEvent &evt)
{
	long index = evt.GetIndex();
	const info::machine machine = GetMachineFromIndex(index);
	Run(machine);
}


//-------------------------------------------------
//  OnListColumnResized
//-------------------------------------------------

void MameFrame::OnListColumnResized(wxListEvent &evt, wxListView &list_view, Preferences::list_view_type type)
{
	int column_index = evt.GetColumn();
	int column_width = list_view.GetColumnWidth(column_index);
	m_prefs.SetColumnWidth(type, column_index, column_width);
}


//-------------------------------------------------
//  OnProfileListItemActivated
//-------------------------------------------------

void MameFrame::OnProfileListItemActivated(wxListEvent &evt)
{
	// get the profile
	long index = evt.GetIndex();
	const profiles::profile &p = m_profiles.get()[index];
	Run(p);
}


//-------------------------------------------------
//  OnMachineListItemContextMenu
//-------------------------------------------------

void MameFrame::OnMachineListItemContextMenu(wxListEvent &event)
{
	// find the machine
	long index = event.GetIndex();
	const info::machine machine = GetMachineFromIndex(index);

	// build the popup menu
	int id = ID_POPUP_MENU_BEGIN;
	wxMenu popup_menu;
	wxMenuItem &run_menu_item = *popup_menu.Append(id++, "Run \"" + machine.description() + "\"");
	Bind(wxEVT_MENU, [this, machine](auto &) { Run(machine); }, run_menu_item.GetId());

	// and display it
	PopupMenu(&popup_menu);
}


//-------------------------------------------------
//  OnSearchBoxTextChanged
//-------------------------------------------------

void MameFrame::OnProfileListItemContextMenu(wxListEvent &event)
{
	// find the profile
	long index = event.GetIndex();
	const profiles::profile &profile = m_profiles.get()[index];

	// build the popup menu
	int id = ID_POPUP_MENU_BEGIN;
	wxMenu popup_menu;
	wxMenuItem &run_menu_item = *popup_menu.Append(id++, "Run \"" + profile.name() + "\"");
	Bind(wxEVT_MENU, [this, &profile](auto &) { Run(profile); }, run_menu_item.GetId());

	// and display it
	PopupMenu(&popup_menu);
}


//-------------------------------------------------
//  OnSearchBoxTextChanged
//-------------------------------------------------

void MameFrame::OnSearchBoxTextChanged()
{
	m_prefs.SetSearchBoxText(m_search_box->GetValue());
	UpdateMachineList();
}


//-------------------------------------------------
//  UpdateMachineList
//-------------------------------------------------

void MameFrame::UpdateMachineList()
{
	wxString filter_text = m_search_box->GetValue();

	// rebuild the indirection list
	m_machine_list_indirections.clear();
	m_machine_list_indirections.reserve(m_info_db.machines().size());
	for (int i = 0; i < static_cast<int>(m_info_db.machines().size()); i++)
	{
		const info::machine &machine(m_info_db.machines()[i]);
		if (filter_text.empty() || util::string_icontains(machine.name(), filter_text) || util::string_icontains(machine.description(), filter_text))
		{
			m_machine_list_indirections.push_back(i);
		}
	}

	// set the list view size
	m_machine_view->SetItemCount(m_machine_list_indirections.size());
	m_machine_view->RefreshItems(0, m_machine_list_indirections.size() - 1);

	// find the currently selected machine
	auto iter = std::find_if(m_machine_list_indirections.begin(), m_machine_list_indirections.end(), [this](int index)
	{
		return m_info_db.machines()[index].name() == m_prefs.GetSelectedMachine();
	});

	// if successful, select it
	if (iter < m_machine_list_indirections.end())
	{
		long selected_index = iter - m_machine_list_indirections.begin();
		m_machine_view->Select(selected_index);
		m_machine_view->EnsureVisible(selected_index);
	}
}


//-------------------------------------------------
//  UpdateProfileDirectories
//-------------------------------------------------

void MameFrame::UpdateProfileDirectories(bool update_fsw)
{
	// first update the actual list
	const wxString &paths_string = m_prefs.GetPath(Preferences::path_type::profiles);
	std::vector<wxString> paths = util::string_split(paths_string, [](const wchar_t ch) { return ch == ';'; });
	m_profiles = profiles::profile::scan_directories(paths);

	// now update the file system watcher if we're asked to
	if (update_fsw)
	{
		m_fsw_profiles.RemoveAll();
		for (const wxString &path : paths)
			m_fsw_profiles.Add(path);
	}
}


//-------------------------------------------------
//  GetMachineFromIndex
//-------------------------------------------------

info::machine MameFrame::GetMachineFromIndex(long item) const
{
	// look up the indirection
	int machine_index = m_machine_list_indirections[item];

	// and look up in the info DB
	return m_info_db.machines()[machine_index];
}


//-------------------------------------------------
//  GetMachineListItemText
//-------------------------------------------------

wxString MameFrame::GetMachineListItemText(long item, long column) const
{
	const info::machine machine = GetMachineFromIndex(item);

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
//  GetProfileListItemText
//-------------------------------------------------

wxString MameFrame::GetProfileListItemText(long item, long column) const
{
	const profiles::profile &p = m_profiles.get()[item];

	wxString result;
	switch (column)
	{
	case 0:
		result = p.name();
		break;
	case 1:
		result = p.machine();
		break;
	case 2:
		result = p.path();	
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
	bool is_active = m_state.has_value();

	// if so, hide the machine list UX
	m_note_book->Show(!is_active);
	m_machine_view->Show(!is_active);
	m_search_box->Show(!is_active);

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
	if (m_state.has_value())
	{
		title_text += ": " + m_client.GetCurrentTask<RunMachineTask>()->GetMachine().description();

		// we want to append "PAUSED" if and only if the user paused, not as a consequence of a menu
		if (m_state->paused().get() && !m_current_pauser)
			title_text += " PAUSED";
	}
	SetLabel(title_text);
}


//-------------------------------------------------
//  UpdateMenuBar
//-------------------------------------------------

void MameFrame::UpdateMenuBar()
{
	bool menu_bar_shown = !m_state.has_value() || m_prefs.GetMenuBarShown();

	// when we hide the menu bar, we disable the accelerators
	m_menu_bar->SetAcceleratorTable(menu_bar_shown ? m_menu_bar_accelerators : wxAcceleratorTable());

#ifdef WIN32
	// Win32 specific code
	SetMenu(GetHWND(), menu_bar_shown ? m_menu_bar->GetHMenu() : nullptr);
#else
	throw false;
#endif
}


//-------------------------------------------------
//  UpdateStatusBar
//-------------------------------------------------

void MameFrame::UpdateStatusBar()
{
	wxString speed_text;

	// prepare a vector with the status text
	std::vector<std::reference_wrapper<const wxString>> status_text;
	
	// is there a running emulation?
	if (m_state.has_value())
	{
		// first entry depends on whether we are running
		if (m_state->phase().get() == status::machine_phase::RUNNING)
		{
			speed_text = wxString::Format(
				m_state->effective_frameskip().get() == 0 ? "%d%%" : "%d%% (frameskip %d/10)",
				(int) (m_state->speed_percent().get() * 100.0 + 0.5),
				(int) m_state->effective_frameskip().get());
			status_text.push_back(speed_text);
		}
		else
		{
			status_text.push_back(m_state->startup_text().get());
		}

		// next entries come from device displays
		for (auto iter = m_state->images().get().cbegin(); iter < m_state->images().get().cend(); iter++)
		{
			if (!iter->m_display.empty())
				status_text.push_back(iter->m_display);
		}
	}

	// and specify it
	wxStatusBar *status_bar = GetStatusBar();
	for (int i = 0; i < status_bar->GetFieldsCount(); i++)
	{
		const wxString &text = i < (int)status_text.size() ? status_text[i] : std::reference_wrapper<const wxString>(util::g_empty_string);
		SetStatusText(text, i);
	}
}


//-------------------------------------------------
//  InputClassText
//-------------------------------------------------

wxString MameFrame::InputClassText(status::input::input_class input_class, bool elipsis)
{
	wxString result;
	switch (input_class)
	{
	case status::input::input_class::CONTROLLER:
		result = "Joysticks and Controllers";
		break;
	case status::input::input_class::KEYBOARD:
		result = "Keyboard";
		break;
	case status::input::input_class::MISC:
		result = "Miscellaneous Input";
		break;
	case status::input::input_class::CONFIG:
		result = "Configuration";
		break;
	case status::input::input_class::DIPSWITCH:
		result = "DIP Switches";
		break;
	default:
		throw false;
	}

	if (elipsis)
		result += "...";

	return result;
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
	if (!m_pinging && m_state.has_value())
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
		if (m_state->throttle_rate() >= s_throttle_rates[index])
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
	return m_state && m_state->sound_attenuation() != SOUND_ATTENUATION_OFF;
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
	m_is_running = actually_pause && m_host.m_state.has_value() && !m_host.m_state->paused().get();
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

observable::value<std::vector<status::image>> &MameFrame::ImagesHost::GetImages()
{
	return m_host.m_state->images();
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
//  GetRecentFiles
//-------------------------------------------------

const std::vector<wxString> &MameFrame::ImagesHost::GetRecentFiles(const wxString &tag) const
{
	info::machine machine = m_host.GetRunningMachine();
	const wxString &device_type = GetDeviceType(machine, tag);
	return m_host.m_prefs.GetRecentDeviceFiles(machine.name(), device_type);
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
//  CreateImage
//-------------------------------------------------

void MameFrame::ImagesHost::CreateImage(const wxString &tag, wxString &&path)
{
	m_host.WatchForImageMount(tag);
	m_host.Issue({ "create", tag, std::move(path) });
}


//-------------------------------------------------
//  LoadImage
//-------------------------------------------------

void MameFrame::ImagesHost::LoadImage(const wxString &tag, wxString &&path)
{
	m_host.WatchForImageMount(tag);
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

const std::vector<status::input> &MameFrame::InputsHost::GetInputs()
{
	return m_host.m_state->inputs().get();
}


//-------------------------------------------------
//  GetPollingSeqChanged
//-------------------------------------------------

observable::value<bool> &MameFrame::InputsHost::GetPollingSeqChanged()
{
	return m_host.m_state->polling_input_seq();
}


//-------------------------------------------------
//  StartPolling
//-------------------------------------------------

void MameFrame::InputsHost::StartPolling(const wxString &port_tag, ioport_value mask, status::input_seq::type seq_type)
{
	wxString seq_type_string;
	switch (seq_type)
	{
	case status::input_seq::type::STANDARD:
		seq_type_string = "standard";
		break;
	case status::input_seq::type::INCREMENT:
		seq_type_string = "increment";
		break;
	case status::input_seq::type::DECREMENT:
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
//  SwitchesHost
//**************************************************************************

//-------------------------------------------------
//  SwitchesHost ctor
//-------------------------------------------------

MameFrame::SwitchesHost::SwitchesHost(MameFrame &host)
	: m_host(host)
{
}


//-------------------------------------------------
//  GetInputs
//-------------------------------------------------

const std::vector<status::input> &MameFrame::SwitchesHost::GetInputs()
{
	return m_host.m_state->inputs().get();
}


//-------------------------------------------------
//  SetInputValue
//-------------------------------------------------

void MameFrame::SwitchesHost::SetInputValue(const wxString &port_tag, ioport_value mask, ioport_value value)
{
	m_host.Issue({ "set_input_value", port_tag, std::to_string(mask), std::to_string(value) });
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

