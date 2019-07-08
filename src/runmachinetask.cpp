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

#include "listxmltask.h"
#include "runmachinetask.h"
#include "utility.h"
#include "prefs.h"
#include "validity.h"
#include "xmlparser.h"


//**************************************************************************
//  VARIABLES
//**************************************************************************

wxDEFINE_EVENT(EVT_RUN_MACHINE_RESULT, PayloadEvent<RunMachineResult>);
wxDEFINE_EVENT(EVT_STATUS_UPDATE, PayloadEvent<StatusUpdate>);
wxDEFINE_EVENT(EVT_CHATTER, PayloadEvent<Chatter>);

static const util::enum_parser<Input::input_class> s_input_class_parser =
{
	{ "controller", Input::input_class::CONTROLLER, },
	{ "misc", Input::input_class::MISC, },
	{ "keyboard", Input::input_class::KEYBOARD, },
	{ "config", Input::input_class::CONFIG, },
	{ "dipswitch", Input::input_class::DIPSWITCH, },
};

static const util::enum_parser<Input::input_type> s_input_type_parser =
{
	{ "analog", Input::input_type::ANALOG, },
	{ "digital", Input::input_type::DIGITAL }
};

static const util::enum_parser<InputSeq::inputseq_type> s_inputseq_type_parser =
{
	{ "standard", InputSeq::inputseq_type::STANDARD },
	{ "increment", InputSeq::inputseq_type::INCREMENT },
	{ "decrement", InputSeq::inputseq_type::DECREMENT }
};


//**************************************************************************
//  UTILITY
//**************************************************************************

//-------------------------------------------------
//  BuildCommand
//-------------------------------------------------

wxString BuildCommand(const std::vector<wxString> &args)
{
	wxString command;
	for (const wxString &arg : args)
	{
		if (!command.IsEmpty())
			command += " ";

		bool has_space = arg.find(' ') != wxString::npos;
		if (has_space)
			command += "\"";

		command += arg;

		if (has_space)
			command += "\"";
	}
	command += "\r\n";
	return command;
}


//**************************************************************************
//  MAIN THREAD OPERATIONS
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

RunMachineTask::RunMachineTask(info::machine machine, wxWindow &target_window)
    : m_machine(machine)
    , m_target_window((std::uintptr_t)target_window.GetHWND())
	, m_chatter_enabled(false)
{
}


//-------------------------------------------------
//  GetArguments
//-------------------------------------------------

std::vector<wxString> RunMachineTask::GetArguments(const Preferences &prefs) const
{
	return
	{
		GetMachine().name(),
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
		"-worker_ui",
		std::to_string(m_target_window),
		"-skip_gameinfo"
	};
}


//-------------------------------------------------
//  Abort
//-------------------------------------------------

void RunMachineTask::Abort()
{
	Issue({ "exit" });
}


//-------------------------------------------------
//  OnChildProcessCompleted
//-------------------------------------------------

void RunMachineTask::OnChildProcessCompleted(emu_error status)
{
	InternalPost(Message::type::TERMINATED, "", status);
}


//-------------------------------------------------
//  OnChildProcessKilled
//-------------------------------------------------

void RunMachineTask::OnChildProcessKilled()
{
	InternalPost(Message::type::TERMINATED, "", emu_error::KILLED);
}


//-------------------------------------------------
//  Issue
//-------------------------------------------------

void RunMachineTask::Issue(const std::vector<wxString> &args)
{
	wxString command = BuildCommand(args);
	InternalPost(Message::type::COMMAND, std::move(command));
}


//-------------------------------------------------
//  IssueFullCommandLine
//-------------------------------------------------

void RunMachineTask::IssueFullCommandLine(const wxString &full_command)
{
	InternalPost(Message::type::COMMAND, full_command + "\r\n");
}


//-------------------------------------------------
//  InternalPost
//-------------------------------------------------

void RunMachineTask::InternalPost(Message::type type, wxString &&command, emu_error status)
{
	Message message;
	message.m_type = type;
	message.m_command = std::move(command);
	message.m_status = status;
	m_message_queue.Post(message);
}


//**************************************************************************
//  CLIENT THREAD OPERATIONS
//**************************************************************************

//-------------------------------------------------
//  Process
//-------------------------------------------------

void RunMachineTask::Process(wxProcess &process, wxEvtHandler &handler)
{
	RunMachineResult result;

	// set up streams
	wxTextInputStream input(*process.GetInputStream());
	wxTextOutputStream output(*process.GetOutputStream());
	wxTextInputStream error(*process.GetErrorStream());

	// receive the inaugural response from MAME
	ReceiveResponse(handler, input);

	// loop until the process terminates
	bool done = false;
	emu_error status = emu_error::NONE;
	while (!done)
	{
		// await a message from the queue
		Message message;
		m_message_queue.Receive(message);

		switch (message.m_type)
		{
		case Message::type::COMMAND:
			// emit this command to MAME
			wxLogDebug("MAME <== [%s]", message.m_command);
			output.WriteString(message.m_command);

			if (m_chatter_enabled)
				PostChatter(handler, Chatter::chatter_type::COMMAND_LINE, std::move(message.m_command));

			// and receive a response from MAME
			ReceiveResponse(handler, input);
			break;

		case Message::type::TERMINATED:
			wxLogDebug("MAME --- TERMINATED (status=%d)", message.m_status);
			done = true;
			status = message.m_status;
			break;

		default:
			throw false;
		}
	}

	// was there an error?  if so capture it
	wxString error_message;
	if (status != emu_error::NONE)
	{
		// read until the end of the stream
		wxString s;
		while ((s = error.ReadLine()), !s.IsEmpty())
		{
			error_message += s;
			error_message += "\r\n";
		}

		// if we were not able to collect any info, show something
		if (error_message.IsEmpty())
			error_message = wxString::Format("Error %d running MAME", (int)status);
	}
	result.m_error_message = std::move(error_message);

	util::QueueEvent(handler, EVT_RUN_MACHINE_RESULT, wxID_ANY, std::move(result));
}


//-------------------------------------------------
//  ReceiveResponse
//-------------------------------------------------

void RunMachineTask::ReceiveResponse(wxEvtHandler &handler, wxTextInputStream &input)
{
	wxString str = input.ReadLine();
	wxLogDebug("MAME ==> %s", str);

	// chatter
	if (m_chatter_enabled)
		PostChatter(handler, Chatter::chatter_type::RESPONSE, wxString(str));

	// interpret the response
	util::string_truncate(str, '#');
	std::vector<wxString> args = util::string_split(str, [](wchar_t ch) { return ch == ' ' || ch == '\r' || ch == '\n'; });

	// did we get a status reponse
	if (args.size() >= 2 && args[0] == "OK" && args[1] == "STATUS")
	{
		StatusUpdate status_update = ReadStatusUpdate(input);
		util::QueueEvent(handler, EVT_STATUS_UPDATE, wxID_ANY, std::move(status_update));
	}
}


//-------------------------------------------------
//  ReadStatusUpdate
//-------------------------------------------------

StatusUpdate RunMachineTask::ReadStatusUpdate(wxTextInputStream &input)
{
	StatusUpdate result;

	XmlParser xml;
	xml.OnElementBegin({ "status" }, [&](const XmlParser::Attributes &attributes)
	{
		attributes.Get("paused",			result.m_paused);
		attributes.Get("polling_input_seq",	result.m_polling_input_seq);
		attributes.Get("startup_text",		result.m_startup_text);
	});
	xml.OnElementBegin({ "status", "video" }, [&](const XmlParser::Attributes &attributes)
	{
		attributes.Get("frameskip",			result.m_frameskip);
		attributes.Get("speed_text",		result.m_speed_text);
		attributes.Get("throttled",			result.m_throttled);
		attributes.Get("throttle_rate",		result.m_throttle_rate);
	});
	xml.OnElementBegin({ "status", "sound" }, [&](const XmlParser::Attributes &attributes)
	{
		attributes.Get("attenuation",		result.m_sound_attenuation);
	});
	xml.OnElementBegin({ "status", "images" }, [&](const XmlParser::Attributes &)
	{
		result.m_images = std::vector<Image>();
	});
	xml.OnElementBegin({ "status", "images", "image" }, [&](const XmlParser::Attributes &attributes)
	{
		Image &image = result.m_images.value().emplace_back();
		attributes.Get("tag",				image.m_tag);
		attributes.Get("instance_name",		image.m_instance_name);
		attributes.Get("is_readable",		image.m_is_readable, false);
		attributes.Get("is_writeable",		image.m_is_writeable, false);
		attributes.Get("is_createable",		image.m_is_createable, false);
		attributes.Get("must_be_loaded",	image.m_must_be_loaded, false);
		attributes.Get("filename",			image.m_file_name);
		attributes.Get("display",			image.m_display);
	});
	xml.OnElementBegin({ "status", "inputs" }, [&](const XmlParser::Attributes &)
	{
		result.m_inputs = std::vector<Input>();
	});
	xml.OnElementBegin({ "status", "inputs", "input" }, [&](const XmlParser::Attributes &attributes)
	{
		Input &input = result.m_inputs.value().emplace_back();
		attributes.Get("port_tag",			input.m_port_tag);
		attributes.Get("mask",				input.m_mask);
		attributes.Get("class",				input.m_class, s_input_class_parser);
		attributes.Get("type",				input.m_type, s_input_type_parser);
		attributes.Get("name",				input.m_name);
		attributes.Get("value",				input.m_value);
	});
	xml.OnElementBegin({ "status", "inputs", "input", "seq" }, [&](const XmlParser::Attributes &attributes)
	{
		InputSeq &seq = util::last(result.m_inputs.value()).m_seqs.emplace_back();
		attributes.Get("type",				seq.m_type, s_inputseq_type_parser);
		attributes.Get("text",				seq.m_text);
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

	// parse the XML
	wxStringInputStream input_buffer(buffer);
	result.m_success = xml.Parse(input_buffer);

	// this should not happen unless there is a bug
	if (!result.m_success)
		result.m_parse_error = xml.ErrorMessage();

	// and return it
	return result;
}


//-------------------------------------------------
//  PostChatter
//-------------------------------------------------

void RunMachineTask::PostChatter(wxEvtHandler &handler, Chatter::chatter_type type, wxString &&text)
{
	Chatter chatter;
	chatter.m_type = type;
	chatter.m_text = std::move(text);
	util::QueueEvent(handler, EVT_CHATTER, wxID_ANY, std::move(chatter));
}


//**************************************************************************
//  VALIDITY CHECKS
//**************************************************************************

//-------------------------------------------------
//  test
//-------------------------------------------------

static void test()
{
	wxString command = BuildCommand({ "alpha", "bravo", "charlie" });
	assert(command == "alpha bravo charlie\r\n");
}


//-------------------------------------------------
//  checks
//-------------------------------------------------

static const validity_check checks[] =
{
	test
};
