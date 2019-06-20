/***************************************************************************

    dlginputs.h

    Input customization dialog

***************************************************************************/

#pragma once

#ifndef DLGINPUTS_H
#define DLGINPUTS_H

#include <vector>

class wxWindow;
struct Input;

bool show_inputs_dialog(wxWindow &parent, const std::vector<Input> &inputs);

#endif // DLGINPUTS_H
