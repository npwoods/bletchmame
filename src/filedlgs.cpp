/***************************************************************************

	filedlgs.cpp

	Helper functions for working with Qt file dialogs

***************************************************************************/

// bletchmame headers
#include "filedlgs.h"

enum class PrefPathType
{
	File,
	Directory
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  getPrefPathType
//-------------------------------------------------

static PrefPathType getPrefPathType(Preferences::machine_path_type machinePathType)
{
	PrefPathType result;
	switch (machinePathType)
	{
	case Preferences::machine_path_type::WORKING_DIRECTORY:
		result = PrefPathType::Directory;
		break;
	case Preferences::machine_path_type::LAST_SAVE_STATE:
		result = PrefPathType::File;
		break;
	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  associateFileDialogWithMachinePrefs
//-------------------------------------------------

void associateFileDialogWithMachinePrefs(QFileDialog &fileDialog, Preferences &prefs, const info::machine &machine, Preferences::machine_path_type pathType)
{
	const QString &machineName = machine.name();
	const QString &prefsPath = prefs.getMachinePath(machineName, pathType);
	if (!prefsPath.isEmpty())
	{
		QFileInfo fi(prefsPath);

		// set the directory
		fileDialog.setDirectory(fi.dir());

		// try to set the file, if appropriate
		if (getPrefPathType(pathType) == PrefPathType::File)
			fileDialog.selectFile(fi.fileName());
	}

	// monitor dialog acceptance
	fileDialog.connect(&fileDialog, &QFileDialog::accepted, &fileDialog, [&fileDialog, &prefs, machineName, pathType]()
	{
		// get the result out of the dialog
		QStringList selectedFiles = fileDialog.selectedFiles();
		const QString &result = selectedFiles.first();

		// figure out the path we need to persist
		QString newPrefsPath;
		QString absolutePath;
		switch (getPrefPathType(pathType))
		{
		case PrefPathType::File:
			newPrefsPath = result;
			break;

		case PrefPathType::Directory:
			absolutePath = QFileInfo(result).dir().absolutePath();
			if (!absolutePath.endsWith("/"))
				absolutePath += "/";
			newPrefsPath = std::move(absolutePath);
			break;

		default:
			throw false;
		}

		// and finally persist it
		prefs.setMachinePath(machineName, pathType, std::move(newPrefsPath));
	});
}
