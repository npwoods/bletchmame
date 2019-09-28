/***************************************************************************

    dialogs/images.h

    Images (File Manager) dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_IMAGES_H
#define DIALOGS_IMAGES_H

#include <vector>
#include <memory>
#include <observable/observable.hpp>

#include "status.h"
#include "softwarelist.h"
#include "info.h"


// pesky Win32 declaration
#ifdef LoadImage
#undef LoadImage
#endif

class IImagesHost
{
public:
	virtual info::machine GetMachine() = 0;
	virtual observable::value<std::vector<status::image>> &GetImages() = 0;
	virtual const wxString &GetWorkingDirectory() const = 0;
	virtual void SetWorkingDirectory(wxString &&dir) = 0;
	virtual const std::vector<software_list> &GetSoftwareLists() = 0;
	virtual const std::vector<wxString> &GetRecentFiles(const wxString &tag) const = 0;
	virtual std::vector<wxString> GetExtensions(const wxString &tag) const = 0;
	virtual void CreateImage(const wxString &tag, wxString &&path) = 0;
	virtual void LoadImage(const wxString &tag, wxString &&path) = 0;
	virtual void UnloadImage(const wxString &tag) = 0;
};


void show_images_dialog(IImagesHost &host);
bool show_images_dialog_cancellable(IImagesHost &host);

#endif // DIALOGS_IMAGES_H
