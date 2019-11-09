/***************************************************************************

    app.cpp

    BletchMAME Front End Application

***************************************************************************/

#include <wx/wxprec.h>
#include <wx/wx.h>

#include "frame.h"
#include "validity.h"


//**************************************************************************
//  RESOURCES
//**************************************************************************

// the application icon (under Windows it is in resources and even
// though we could still include the XPM here it would be unused)
#ifndef wxHAS_IMAGES_IN_RESOURCES
    #include "../sample.xpm"
#endif


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
    // Define a new application type, each program should derive a class from wxApp
    class MameApp : public wxApp
    {
    public:
        // override base class virtuals
        // ----------------------------

        // this one is called on application startup and is a good place for the app
        // initialization (doing it here and not in the ctor allows to have an error
        // return: if OnInit() returns false, the application terminates)
        virtual bool OnInit() override;
    };
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  OnInit
//-------------------------------------------------

bool MameApp::OnInit()
{
	if (!validity_check::run_all())
		return false;

	// call the base class initialization method, currently it only parses a
	// few common command-line options but it could be do more in the future
	if (!wxApp::OnInit())
		return false;

	// add image handlers
	wxImage::AddHandler(new wxICOHandler());

	// create the main application window
	wxFrame *frame = create_mame_frame();

	// and show it (the frames, unlike simple controls, are not shown when
	// created initially)
	frame->Show(true);

	// success: wxApp::OnRun() will be called which will enter the main message
	// loop and the application will run. If we returned false here, the
	// application would exit immediately.
	return true;
}


//-------------------------------------------------
//  wxIMPLEMENT_APP
//-------------------------------------------------

wxIMPLEMENT_APP(MameApp);
