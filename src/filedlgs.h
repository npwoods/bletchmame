/***************************************************************************

	filedlgs.h

	Helper functions for working with Qt file dialogs

***************************************************************************/

#ifndef FILEDLGS_H
#define FILEDLGS_H

// bletchmame headers
#include "prefs.h"
#include "info.h"

// Qt headers
#include <QFileDialog>

// methods
void associateFileDialogWithMachinePrefs(QFileDialog &fileDialog, Preferences &prefs, const info::machine &machine, Preferences::machine_path_type machinePathType);

#endif // FILEDLGS_H
