/***************************************************************************

    frame.cpp

    MAME Front End Application

***************************************************************************/

#include "wx/wxprec.h"

#include <wx/listctrl.h>

#include "frame.h"
#include "client.h"
#include "prefs.h"
#include "listxmltask.h"
#include "runmachinetask.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

// IDs for the controls and the menu commands
enum
{
    // menu items
    ID_EXIT                     = wxID_EXIT,

    // it is important for the id corresponding to the "About" command to have
    // this standard value as otherwise it won't be handled properly under Mac
    // (where it is special and put into the "Apple" menu)
    ID_ABOUT                    = wxID_ABOUT,

    // user IDs
    ID_STOP                     = wxID_HIGHEST + 1,
    ID_PAUSE,
    ID_SOFT_RESET,
    ID_HARD_RESET,
    ID_THROTTLE_RATE_0,
    ID_THROTTLE_RATE_1,
    ID_THROTTLE_RATE_2,
    ID_THROTTLE_RATE_3,
    ID_THROTTLE_RATE_4,
    ID_THROTTLE_RATE_5,
    ID_THROTTLE_RATE_6,
    ID_THROTTLE_INCREASE,
    ID_THROTTLE_DECREASE,
    ID_THROTTLE_WARP,
    ID_TASK_LISTXML,
    ID_TASK_RUNMACHINE
};


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
    class MameFrame : public wxFrame, public IMameClientSite
    {
    public:
        // ctor(s)
        MameFrame();

        // event handlers (these functions should _not_ be virtual)
        void OnExit();
        void OnAbout();
        void OnSize(wxSizeEvent &event);
        void OnListItemSelected(wxListEvent &event);
        void OnListItemActivated(wxListEvent &event);
        void OnListColumnResized(wxListEvent &event);

        // Task notifications
        void OnListXmlCompleted(PayloadEvent<ListXmlResult> &event);
        void OnRunMachineCompleted(PayloadEvent<RunMachineResult> &event);
        void OnStatusUpdate(PayloadEvent<StatusUpdate> &event);

        // IMameClientSite implementation
        virtual wxEvtHandler &EventHandler() override;
        virtual const wxString &GetMameCommand() override;

    private:
        MameClient                  m_client;
        Preferences                 m_prefs;
        wxListView *                m_list_view;
        wxMenuBar *                 m_menu_bar;
        std::vector<Machine>        m_machines;

        static float s_throttle_rates[];

        void CreateMenuBar();
        void Issue(std::string &&command, bool exit = false);
        void UpdateMachineList();
        void UpdateEmulationSession();
        std::string GetTarget();
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
    , m_client(*this)
    , m_list_view(nullptr)
    , m_menu_bar(nullptr)
{
    // set the frame icon
    SetIcon(wxICON(sample));

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
    Bind(wxEVT_MENU,                [this](auto &)      { Issue("soft_reset");          }, ID_SOFT_RESET);
    Bind(wxEVT_MENU,                [this](auto &)      { Issue("exit", true);          }, ID_STOP);
    Bind(wxEVT_MENU,                [this](auto &)      { OnExit();                     }, ID_EXIT);
    Bind(wxEVT_MENU,                [this](auto &)      { OnAbout();                    }, ID_ABOUT);
    Bind(wxEVT_LIST_ITEM_SELECTED,  [this](auto &event) { OnListItemSelected(event);    });
    Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](auto &event) { OnListItemActivated(event);   });
    Bind(wxEVT_LIST_COL_END_DRAG,   [this](auto &event) { OnListColumnResized(event);   });

    // bind throttle rates
    for (int i = 0; i < sizeof(s_throttle_rates) / sizeof(s_throttle_rates[0]); i++)
    {
        Bind(wxEVT_MENU, [this, i](auto &) { Issue("throttle_rate " + std::to_string(s_throttle_rates[i])); }, ID_THROTTLE_RATE_0 + i);
    }

    // Create a list view
    m_list_view = new wxListView(this);
    UpdateMachineList();

    // nothing is running yet...
    UpdateEmulationSession();

    // Connect to MAME
    Task::ptr task = create_list_xml_task();
    m_client.Launch(std::move(task), ID_TASK_LISTXML);
}


//-------------------------------------------------
//  CreateMenuBar
//-------------------------------------------------

void MameFrame::CreateMenuBar()
{
    // create the "File" menu
    wxMenu *file_menu = new wxMenu();
    file_menu->Append(ID_STOP, "Stop");
    file_menu->Append(ID_PAUSE, "Pause");
    wxMenu *reset_menu = new wxMenu();
    reset_menu->Append(ID_SOFT_RESET, "Soft Reset");
    reset_menu->Append(ID_HARD_RESET, "Hard Reset");
    file_menu->AppendSubMenu(reset_menu, "Reset");
    file_menu->Append(ID_EXIT, "E&xit\tAlt-X");

    // create the "Devices" menu
    wxMenu *devices_menu = new wxMenu();

    // create the "Options" menu
    wxMenu *options_menu = new wxMenu();
    wxMenu *throttle_menu = new wxMenu();
    for (int i = 0; i < sizeof(s_throttle_rates) / sizeof(s_throttle_rates[0]); i++)
    {
        throttle_menu->Append(ID_THROTTLE_RATE_0 + i, std::to_string((int) (s_throttle_rates[i] * 100)) + "%");
    }
    throttle_menu->AppendSeparator();
    throttle_menu->Append(ID_THROTTLE_INCREASE, "Increase Speed");
    throttle_menu->Append(ID_THROTTLE_DECREASE, "Decrease Speed");
    throttle_menu->Append(ID_THROTTLE_WARP, "Warp Mode");
    options_menu->AppendSubMenu(throttle_menu, "Throttle");

    // create the "Settings" menu
    wxMenu *settings_menu = new wxMenu();

    // the "About" item should be in the help menu
    wxMenu *help_menu = new wxMenu();
    help_menu->Append(ID_ABOUT, "&About\tF1");

    // now append the freshly created menu to the menu bar...
    m_menu_bar = new wxMenuBar();
    m_menu_bar->Append(file_menu, "&File");
    m_menu_bar->Append(devices_menu, "&Devices");
    m_menu_bar->Append(options_menu, "&Options");
    m_menu_bar->Append(settings_menu, "&Settings");
    m_menu_bar->Append(help_menu, "&Help");

    // ... and attach this menu bar to the frame
    SetMenuBar(m_menu_bar);
}


//-------------------------------------------------
//  OnExit
//-------------------------------------------------

void MameFrame::OnExit()
{
    // true is to force the frame to close
    Close(true);
}


//-------------------------------------------------
//  OnAbout
//-------------------------------------------------

void MameFrame::OnAbout()
{
    wxMessageBox("BletchMAME", "About BletchMAME", wxOK | wxICON_INFORMATION, this);
}


//-------------------------------------------------
//  OnSize
//-------------------------------------------------

void MameFrame::OnSize(wxSizeEvent &evt)
{
    m_prefs.SetSize(evt.GetSize());
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
    m_machines = std::move(event.Payload().m_machines);

    // update!
    UpdateMachineList();

    m_client.Reset();
}


//-------------------------------------------------
//  OnRunMachineCompleted
//-------------------------------------------------

void MameFrame::OnRunMachineCompleted(PayloadEvent<RunMachineResult> &)
{
    m_client.Reset();
    UpdateEmulationSession();
}


//-------------------------------------------------
//  OnStatusUpdate
//-------------------------------------------------

void MameFrame::OnStatusUpdate(PayloadEvent<StatusUpdate> &event)
{
    SetStatusText(event.Payload().m_speed_text);
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
    if (index < 0 || index >= m_machines.size())
        return;

    Task *task = new RunMachineTask(m_machines[index].m_name.ToStdString(), GetTarget());
    m_client.Launch(Task::ptr(task), ID_TASK_RUNMACHINE);
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
//  Issue
//-------------------------------------------------

void MameFrame::Issue(std::string &&command, bool exit)
{
    RunMachineTask *task = m_client.GetCurrentTask<RunMachineTask>();
    if (!task)
        return;

    task->Post(std::move(command), exit);
}


//-------------------------------------------------
//  UpdateMachineList
//-------------------------------------------------

void MameFrame::UpdateMachineList()
{
    m_list_view->ClearAll();
    m_list_view->AppendColumn("Name",           wxLIST_FORMAT_LEFT, m_prefs.GetColumnWidth(0));
    m_list_view->AppendColumn("Description",    wxLIST_FORMAT_LEFT, m_prefs.GetColumnWidth(1));
    m_list_view->AppendColumn("Year",           wxLIST_FORMAT_LEFT, m_prefs.GetColumnWidth(2));
    m_list_view->AppendColumn("Manufacturer",   wxLIST_FORMAT_LEFT, m_prefs.GetColumnWidth(3));

    int index = 0;
    long selected_index = -1;
    for (auto &ent : m_machines)
    {
        m_list_view->InsertItem(index, ent.m_name);
        m_list_view->SetItem(index, 1, ent.m_description);
        m_list_view->SetItem(index, 2, ent.m_year);
        m_list_view->SetItem(index, 3, ent.m_manfacturer);

        if (ent.m_name == m_prefs.GetSelectedMachine())
            selected_index = index;
        index++;
    }

    // if we found something to select, select it
    if (selected_index >= 0)
    {
        m_list_view->Select(selected_index);
        m_list_view->EnsureVisible(selected_index);
    }
}


//-------------------------------------------------
//  UpdateEmulationSession
//-------------------------------------------------

void MameFrame::UpdateEmulationSession()
{
    static int m_emulation_session_item_ids[] =
    {
        ID_STOP,
        ID_PAUSE,
        ID_SOFT_RESET,
        ID_HARD_RESET,
        ID_THROTTLE_RATE_0,
        ID_THROTTLE_RATE_1,
        ID_THROTTLE_RATE_2,
        ID_THROTTLE_RATE_3,
        ID_THROTTLE_RATE_4,
        ID_THROTTLE_RATE_5,
        ID_THROTTLE_RATE_6,
        ID_THROTTLE_INCREASE,
        ID_THROTTLE_DECREASE,
        ID_THROTTLE_WARP
    };

    bool is_active = m_client.GetCurrentTask<RunMachineTask>() != nullptr;

    for (int i = 0; i < sizeof(m_emulation_session_item_ids) / sizeof(m_emulation_session_item_ids[0]); i++)
    {
        m_menu_bar->Enable(m_emulation_session_item_ids[i], is_active);
    }

    m_list_view->Show(!is_active);
}


//-------------------------------------------------
//  GetTarget
//-------------------------------------------------

std::string MameFrame::GetTarget()
{
    return std::to_string((std::int64_t)GetHWND());
}


//-------------------------------------------------
//  EventHandler
//-------------------------------------------------

wxEvtHandler &MameFrame::EventHandler()
{
    return *this;
}


//-------------------------------------------------
//  GetMameCommand
//-------------------------------------------------

const wxString &MameFrame::GetMameCommand()
{
    return m_prefs.GetMameCommand();
}


//-------------------------------------------------
//  create_mame_frame
//-------------------------------------------------

wxFrame *create_mame_frame()
{
    return new MameFrame();
}

