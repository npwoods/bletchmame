/***************************************************************************

    runmachinetask.cpp

    Task for running an emulation session

***************************************************************************/

#include "wx/wxprec.h"

#include <wx/wx.h>
#include <wx/process.h>
#include <wx/txtstrm.h>
#include <wx/sstream.h>
#include <iostream>
#include <thread>

#include "runmachinetask.h"
#include "utility.h"
#include "prefs.h"
#include "xmlparser.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

wxDEFINE_EVENT(EVT_RUN_MACHINE_RESULT, PayloadEvent<RunMachineResult>);
wxDEFINE_EVENT(EVT_STATUS_UPDATE, PayloadEvent<StatusUpdate>);


//-------------------------------------------------
//  ctor
//-------------------------------------------------

RunMachineTask::RunMachineTask(wxString &&machine_name, wxString &&target)
    : m_machine_name(std::move(machine_name))
    , m_target(std::move(target))
{
}


//-------------------------------------------------
//  GetArguments
//-------------------------------------------------

std::vector<wxString> RunMachineTask::GetArguments(const Preferences &prefs) const
{
	return
	{
		m_machine_name,
		"-rompath",
		prefs.GetPath(Preferences::path_type::roms),
		"-samplepath",
		prefs.GetPath(Preferences::path_type::samples),
		"-cfg_directory",
		prefs.GetPath(Preferences::path_type::config),
		"-nvram_directory",
		prefs.GetPath(Preferences::path_type::nvram),
		"-window",
		"-keyboardprovider",
		"dinput",
		"-slave_ui",
		m_target,
		"-skip_gameinfo",
		"-sound",
		"none"
	};
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
			wxLogDebug("MAME <== [%s]", message.m_command);
			exiting = message.m_exit;

			// special case - with an abort command, we continue immediately
			if (message.m_command == "abort")
				continue;

			// emit this command to MAME
            output.WriteString(message.m_command);
            output.WriteString("\r\n");
        }

		wxString str = input.ReadLine();
        wxLogDebug("MAME ==> %s", str);

        // interpret the response
        util::string_truncate(str, '#');
        std::vector<wxString> args = util::string_split(str, [](wchar_t ch) { return ch == ' ' || ch == '\r' || ch == '\n'; });

        // did we get a status reponse
        if (args.size() >= 2 && args[0] == "OK" && args[1] == "STATUS")
        {
            StatusUpdate status_update;
            if (ReadStatusUpdate(input, status_update))
                util::QueueEvent(handler, EVT_STATUS_UPDATE, wxID_ANY, std::move(status_update));
        }
    }

	util::QueueEvent(handler, EVT_RUN_MACHINE_RESULT, wxID_ANY, std::move(result));
}


//-------------------------------------------------
//  Abort
//-------------------------------------------------

void RunMachineTask::Abort()
{
	Post("abort", true);
}


//-------------------------------------------------
//  OnBegin
//-------------------------------------------------

void RunMachineTask::Post(wxString &&command, bool exit)
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
	result.m_paused_specified = false;
	result.m_frameskip_specified = false;
	result.m_speed_text_specified = false;
	result.m_throttled_specified = false;
	result.m_throttle_rate_specified = false;

	XmlParser xml;
	xml.OnElement({ "status" }, [&](const XmlParser::Attributes &attributes)
	{
		result.m_paused_specified = attributes.Get("paused", result.m_paused);
	});
	xml.OnElement({ "status", "video" }, [&](const XmlParser::Attributes &attributes)
	{
		result.m_frameskip_specified		= attributes.Get("frameskip", result.m_frameskip);
		result.m_speed_text_specified		= attributes.Get("speed_text", result.m_speed_text);
		result.m_throttled_specified		= attributes.Get("throttled", result.m_throttled);
		result.m_throttle_rate_specified	= attributes.Get("throttle_rate", result.m_throttle_rate);
	});

	// because XmlParser::Parse() is not smart enough to read until XML ends, we are using this
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
	return xml.Parse(input_buffer);
}
