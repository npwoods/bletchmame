/***************************************************************************

	imagemenu.cpp

	Implements functionality for image selection from a menu

***************************************************************************/

// bletchmame headers
#include "imagemenu.h"
#include "filedlgs.h"
#include "prefs.h"
#include "softwarelist.h"
#include "dialogs/choosesw.h"

// Qt headers
#include <QFileDialog>


//-------------------------------------------------
//  associateFileDialogWithMachinePrefs
//-------------------------------------------------

static void associateFileDialogWithMachinePrefs(IImageMenuHost &host, QFileDialog &dialog)
{
	::associateFileDialogWithMachinePrefs(
		dialog,
		host.getPreferences(),
		host.getRunningMachine(),
		Preferences::machine_path_type::WORKING_DIRECTORY);
}


//-------------------------------------------------
//  getExtensions
//-------------------------------------------------

static std::vector<QString> getExtensions(IImageMenuHost &host, const QString &tag)
{
	std::vector<QString> result;

	// first try getting the result from the image format
	const status::image *image = host.getRunningState().find_image(tag);
	if (image && image->m_details)
	{
		// we did find it in the image formats
		for (const status::image_format &format : image->m_details->m_formats)
		{
			for (const QString &ext : format.m_extensions)
			{
				if (std::find(result.begin(), result.end(), ext) == result.end())
					result.push_back(ext);
			}
		}
	}
	else
	{
		// find the device declaration
		auto devices = host.getRunningMachine().devices();

		auto iter = std::ranges::find_if(devices, [&tag](info::device dev)
		{
			return dev.tag() == tag;
		});
		assert(iter != devices.end());

		// and return it!
		result = util::string_split(iter->extensions(), [](auto ch) { return ch == ','; });
	}
	return result;
}


//-------------------------------------------------
//  getWildcardString
//-------------------------------------------------

static QString getWildcardString(IImageMenuHost &host, const QString &tag, bool supportZip)
{
	// get the list of extensions
	std::vector<QString> extensions = getExtensions(host, tag);

	// append zip if appropriate
	if (supportZip)
		extensions.push_back("zip");

	// figure out the "general" wildcard part for all devices
	QString all_extensions = util::string_join(QString(";"), extensions, [](QString ext) { return QString("*.%1").arg(ext); });
	QString result = QString("Device files (%1)").arg(all_extensions);

	// now break out each extension
	for (const QString &ext : extensions)
	{
		result += QString(";;%1 files (*.%2s)").arg(
			ext.toUpper(),
			ext);
	}

	// and all files
	result += ";;All files (*.*)";
	return result;
}


//-------------------------------------------------
//  createImage
//-------------------------------------------------

static bool createImage(IImageMenuHost &host, QWidget *parent, const QString &tag)
{
	// show the dialog
	QFileDialog dialog(parent, "Create Image", QString(), getWildcardString(host, tag, false));
	associateFileDialogWithMachinePrefs(host, dialog);
	dialog.setFileMode(QFileDialog::FileMode::AnyFile);
	dialog.exec();
	if (dialog.result() != QDialog::DialogCode::Accepted)
		return false;

	// get the result from the dialog
	QString path = dialog.selectedFiles().first();

	// and load the image
	host.createImage(tag, std::move(path));
	return true;
}


//-------------------------------------------------
//  loadImage
//-------------------------------------------------

static bool loadImage(IImageMenuHost &host, QWidget *parent, const QString &tag)
{
	// show the dialog
	QFileDialog dialog(parent, "Load Image", QString(), getWildcardString(host, tag, true));
	associateFileDialogWithMachinePrefs(host, dialog);
	dialog.setFileMode(QFileDialog::FileMode::ExistingFile);
	dialog.exec();
	if (dialog.result() != QDialog::DialogCode::Accepted)
		return false;

	// get the result from the dialog
	QString path = dialog.selectedFiles().first();

	// and load the image
	host.loadImage(tag, std::move(path));
	return true;
}


//-------------------------------------------------
//  loadSoftwareListPart
//-------------------------------------------------

bool loadSoftwareListPart(IImageMenuHost &host, QWidget *parent, const software_list_collection &software_col, const QString &tag, const QString &dev_interface)
{
	ChooseSoftlistPartDialog dialog(parent, host.getPreferences(), software_col, dev_interface);
	dialog.exec();
	if (dialog.result() != QDialog::DialogCode::Accepted)
		return false;

	host.loadImage(tag, dialog.selection());
	return true;
}


//-------------------------------------------------
//  prettifyImageFileName
//-------------------------------------------------

QString prettifyImageFileName(const info::machine &machine, const software_list_collection &software_col, const QString &deviceTag, const QString &fileName, bool fullPath)
{
	QString result;

	// try to find the software
	std::optional<info::device> device = machine.find_device(deviceTag);
	const software_list::software *software = device.has_value()
		? software_col.find_software_by_name(fileName, device->devinterface())
		: nullptr;

	// did we find the software?
	if (software)
	{
		// if so, the pretty name is the description
		result = software->description();
	}
	else if (!fullPath && !fileName.isEmpty())
	{
		// we want to show the base name plus the extension
		QString normalizedFileName = QDir::fromNativeSeparators(fileName);
		QFileInfo fileInfo(normalizedFileName);
		result = fileInfo.baseName();
		if (!fileInfo.suffix().isEmpty())
		{
			result += ".";
			result += fileInfo.suffix();
		}
	}
	else if (!fileName.isEmpty())
	{
		// we want to show the full filename; our job is easy
		result = fileName;
	}
	else
	{
		// nothing...
		result = TEXT_NONE;
	}
	return result;
}


//-------------------------------------------------
//  appendImageMenuItems
//-------------------------------------------------

void appendImageMenuItems(IImageMenuHost &host, QMenu &popupMenu, const QString &tag)
{
	const status::image *image = host.getRunningState().find_image(tag);
	if (image)
	{
		// get critical information
		info::machine machine = host.getRunningMachine();
		const software_list_collection &softwareListCollection = host.getRunningSoftwareListCollection();

		// add a separator if necessary
		if (!popupMenu.isEmpty())
			popupMenu.addSeparator();

		if (image->m_details && image->m_details->m_is_creatable)
			popupMenu.addAction("Create Image...", [&host, &popupMenu, &tag]() { createImage(host, &popupMenu, tag); });

		// load image
		popupMenu.addAction("Load Image...", [&host, &popupMenu, &tag]() { loadImage(host, &popupMenu, tag); });

		// load software list part
		if (!softwareListCollection.software_lists().empty())
		{
			std::optional<info::device> device = machine.find_device(tag);
			const QString *devInterface = device.has_value()
				? &device->devinterface()
				: nullptr;
			if (devInterface && !devInterface->isEmpty())
			{
				popupMenu.addAction("Load Software List Part...", [&host, &popupMenu, &softwareListCollection, &tag, devInterface]()
				{
					loadSoftwareListPart(host, &popupMenu, softwareListCollection, tag, *devInterface);
				});
			}
		}

		// unload
		QAction &unloadAction = *popupMenu.addAction("Unload", [&host, &tag]() { host.unloadImage(tag); });
		unloadAction.setEnabled(!image->m_file_name.isEmpty());

		// recent files
		auto deviceIter = std::ranges::find_if(
			machine.devices(),
			[&tag](const info::device device)
			{
				return device.tag() == tag;
			});
		if (deviceIter != machine.devices().end())
		{
			const std::vector<QString> &recentFiles = host.getPreferences().getRecentDeviceFiles(machine.name(), deviceIter->type());
			if (!recentFiles.empty())
			{
				QString pretty_buffer;
				popupMenu.addSeparator();
				for (const QString &recentFile : recentFiles)
				{
					QString prettyRecentFile = prettifyImageFileName(machine, softwareListCollection, tag, recentFile, false);
					popupMenu.addAction(prettyRecentFile, [&host, &tag, &recentFile]() { host.loadImage(tag, QString(recentFile)); });
				}
			}
		}
	}
}
