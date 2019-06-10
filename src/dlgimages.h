/***************************************************************************

    dlgimages.h

    Images (File Manager) dialog

***************************************************************************/

#pragma once

#ifndef DLGIMAGES_H
#define DLGIMAGES_H

#include <vector>
#include <memory>

struct Image;
class RunMachineTask;

bool show_images_dialog(const std::vector<Image> &images, std::shared_ptr<RunMachineTask> run_machine_task);

#endif // DLGIMAGES_H
