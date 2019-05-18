/***************************************************************************

    runmachinetask.cpp

    Task for running an emulation session

***************************************************************************/

#include "wx/wxprec.h"

#include <wx/wx.h>
#include <wx/process.h>
#include <wx/txtstrm.h>
#include <wx/sstream.h>
#include <wx/xml/xml.h>
#include <iostream>
#include <thread>

#include "runmachinetask.h"
#include "utility.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

wxDEFINE_EVENT(EVT_RUN_MACHINE_RESULT, PayloadEvent<RunMachineResult>);
wxDEFINE_EVENT(EVT_STATUS_UPDATE, PayloadEvent<StatusUpdate>);


//-------------------------------------------------
//  ctor
//-------------------------------------------------

RunMachineTask::RunMachineTask(std::string &&machine_name, std::string &&target)
    : m_machine_name(std::move(machine_name))
    , m_target(std::move(target))
{
}


//-------------------------------------------------
//  Arguments
//-------------------------------------------------

std::string RunMachineTask::Arguments()
{
    return m_machine_name + " -window -keyboardprovider dinput -slave_ui " + m_target + " -skip_gameinfo -sound none";
}


//-------------------------------------------------
//  Process
//-------------------------------------------------

void RunMachineTask::Process(wxProcess &process, wxEvtHandler &handler)
{
    RunMachineResult result;

    wxTextInputStream input(*process.GetInputStream());
    wxTextOutputStream output(*process.GetOutputStream());
    bool exiting = false;
    bool first = true;

    while (!exiting)
    {
        Message message;

        if (first)
        {
            first = false;
        }
        else
        {
            m_message_queue.Receive(message);

            output.WriteString(message.m_command);
            output.WriteString("\r\n");
            wxLogDebug("MAME <== [%s]", message.m_command);

            exiting = message.m_exit;
        }

        std::string str = input.ReadLine();
        wxLogDebug("MAME ==> %s", str);

        // interpret the response
        util::string_truncate(str, '#');
        std::vector<std::string> args = util::string_split(str, [](char ch) { return ch == ' ' || ch == '\r' || ch == '\n'; });

        // did we get a status reponse
        if (args.size() >= 2 && args[0] == "OK" && args[1] == "STATUS")
        {
            StatusUpdate status_update;
            if (ReadStatusUpdate(input, status_update))
                QueuePayloadEvent(handler, EVT_STATUS_UPDATE, wxID_ANY, std::move(status_update));
        }
    }

    QueuePayloadEvent(handler, EVT_RUN_MACHINE_RESULT, wxID_ANY, std::move(result));
}


//-------------------------------------------------
//  OnBegin
//-------------------------------------------------

void RunMachineTask::Post(std::string &&command, bool exit)
{
    Message message;
    message.m_command = std::move(command);
    message.m_exit = exit;
    m_message_queue.Post(message);
}


//-------------------------------------------------
//  ReadStatusUpdate
//-------------------------------------------------

bool RunMachineTask::ReadStatusUpdate(wxTextInputStream &input, StatusUpdate &result)
{
    wxXmlDocument xml;
    if (!ReadXmlFromInputStream(xml, input))
        return false;

    result.m_speed_text = "dummy";
    return true;
}


//-------------------------------------------------
//  ReadStatusUpdate
//-------------------------------------------------

bool RunMachineTask::ReadXmlFromInputStream(wxXmlDocument &xml, wxTextInputStream &input)
{
    // because wxXmlDocument::Load() is not smart enough to read until XML ends, we are using this
    // crude mechanism to read the XML
    wxString buffer;
    bool done = false;
    while (!done)
    {
        wxString line = input.ReadLine();
        buffer.Append(line);

        if (input.GetInputStream().Eof() || line.StartsWith("</"))
            done = true;
    }

    wxStringInputStream input_buffer(buffer);
    return xml.Load(input_buffer);
}
