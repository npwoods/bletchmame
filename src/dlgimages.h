/***************************************************************************

    dlgimages.h

    Images (File Manager) dialog

***************************************************************************/

#pragma once

#ifndef DLGIMAGES_H
#define DLGIMAGES_H

#include <vector>
#include <memory>

// pesky Win32 declaration
#ifdef LoadImage
#undef LoadImage
#endif

struct Image;
class IImagesHost
{
public:
	virtual const std::vector<Image> GetImages() = 0;
	virtual void SetOnImagesChanged(std::function<void()> &&func) = 0;
	virtual void LoadImage(const wxString &tag, wxString &&path) = 0;
	virtual void UnloadImage(const wxString &tag) = 0;
};


bool show_images_dialog(IImagesHost &host);

#endif // DLGIMAGES_H
