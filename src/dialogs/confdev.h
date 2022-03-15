/***************************************************************************

    dialogs/confdev.h

	Configurable Devices dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_CONFDEV_H
#define DIALOGS_CONFDEV_H

// bletchmame headers
#include "confdevmodel.h"
#include "softwarelist.h"
#include "imagemenu.h"

// dependency headers
#include <observable/observable.hpp>

// Qt headers
#include <QDialog>
#include <QStyle>

// standard headers
#include <vector>
#include <memory>

// pesky Win32 declaration
#ifdef LoadImage
#undef LoadImage
#endif

QT_BEGIN_NAMESPACE
namespace Ui { class ConfigurableDevicesDialog; }
class QFileDialog;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
QT_END_NAMESPACE

class Preferences;

class IConfigurableDevicesDialogHost
{
public:
	virtual IImageMenuHost &imageMenuHost() = 0;
	virtual bool startedWithHashPaths() const = 0;
	virtual void changeSlots(std::map<QString, QString> &&changes) = 0;
};


// ======================> ConfigurableDevicesDialog

class ConfigurableDevicesDialog : public QDialog
{
	Q_OBJECT
public:
	ConfigurableDevicesDialog(QWidget &parent, IConfigurableDevicesDialogHost &host, bool cancellable);
	~ConfigurableDevicesDialog();

public slots:
	virtual void accept() override;

private slots:
	void on_applyChangesButton_clicked();

private:
	class ConfigurableDevicesItemDelegate;

	IConfigurableDevicesDialogHost &				m_host;
	std::unique_ptr<Ui::ConfigurableDevicesDialog>	m_ui;
	observable::unique_subscription					m_imagesEventSubscription;
	observable::unique_subscription					m_slotsEventSubscription;
	bool											m_canChangeSlotOptions;

	ConfigurableDevicesModel &model();
	const ConfigurableDevicesModel &model() const;
	void onModelReset();
	static void setupWarningIcons(std::initializer_list<std::reference_wrapper<QLabel>> iconLabels);

	void updateSlots();
	void updateImages();
	void deviceMenu(QPushButton &button, const QModelIndex &index);
	void buildDeviceMenuSlotItems(QMenu &popupMenu, const QString &tag, const info::slot &slot, const QString &currentSlotOption);
};


#endif // DIALOGS_CONFDEV_H
