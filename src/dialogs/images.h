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
#include <QDialog>

#include "status.h"
#include "softwarelist.h"
#include "info.h"


// pesky Win32 declaration
#ifdef LoadImage
#undef LoadImage
#endif

QT_BEGIN_NAMESPACE
namespace Ui { class ImagesDialog; }
class QLabel;
class QLineEdit;
class QPushButton;
QT_END_NAMESPACE

class Preferences;

class IImagesHost
{
public:
	virtual Preferences &GetPreferences() = 0;
	virtual info::machine GetMachine() = 0;
	virtual observable::value<std::vector<status::image>> &GetImages() = 0;
	virtual const QString &GetWorkingDirectory() const = 0;
	virtual void SetWorkingDirectory(QString &&dir) = 0;
	virtual const std::vector<QString> &GetRecentFiles(const QString &tag) const = 0;
	virtual std::vector<QString> GetExtensions(const QString &tag) const = 0;
	virtual void CreateImage(const QString &tag, QString &&path) = 0;
	virtual void LoadImage(const QString &tag, QString &&path) = 0;
	virtual void UnloadImage(const QString &tag) = 0;
};


// ======================> ImagesDialog

class ImagesDialog : public QDialog
{
	Q_OBJECT
public:
	ImagesDialog(QWidget &parent, IImagesHost &host, bool cancellable);
	~ImagesDialog();

private:
	IImagesHost &						m_host;
	std::unique_ptr<Ui::ImagesDialog>	m_ui;
	observable::unique_subscription		m_imagesEventSubscription;

	void UpdateImageGrid();
	const QString &PrettifyImageFileName(const software_list_collection &software_col, const QString &tag, const QString &file_name, QString &buffer, bool full_path);
	const QString *DeviceInterfaceFromTag(const QString &tag);
	software_list_collection BuildSoftwareListCollection() const;
	bool ImageMenu(const QPushButton &button, int row);
	bool CreateImage(const QString &tag);
	bool LoadImage(const QString &tag);
	bool LoadSoftwareListPart(const software_list_collection &software_col, const QString &tag, const QString &dev_interface);
	bool UnloadImage(const QString &tag);

	template<class T>
	T &getOrCreateGridWidget(int row, int column, void (ImagesDialog::*setupFunc)(T &, int) = nullptr);

	void setupGridLineEdit(QLineEdit &lineEdit, int row);
	void setupGridButton(QPushButton &button, int row);
};


#endif // DIALOGS_IMAGES_H
