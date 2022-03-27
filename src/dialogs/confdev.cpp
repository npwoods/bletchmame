/***************************************************************************

    dialogs/confdev.cpp

    Configurable Devices dialog

***************************************************************************/

// bletchmame headers
#include "ui_confdev.h"
#include "filedlgs.h"
#include "dialogs/choosesw.h"
#include "dialogs/confdev.h"

// Qt headers
#include <QAbstractItemDelegate>
#include <QAction>
#include <QBitmap>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStringListModel>
#include <QStyledItemDelegate>


//**************************************************************************
//  TYPES
//**************************************************************************

class ConfigurableDevicesDialog::ConfigurableDevicesItemDelegate : public QStyledItemDelegate
{
public:
    ConfigurableDevicesItemDelegate(ConfigurableDevicesDialog &parent)
        : QStyledItemDelegate(&parent)
    {
    }

    virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        // create a button
        QPushButton &button = *new QPushButton("...");
        button.setMaximumSize(30, 17);  // there is probably a better way to do this
        connect(&button, &QPushButton::clicked, this, [this, &button, index]() { buttonClicked(button, index); });

        // and wrap it up into a widget
        QWidget &widget = *new QWidget(parent);
        QHBoxLayout &layout = *new QHBoxLayout(&widget);
        layout.setSpacing(0);
        layout.setContentsMargins(0, 0, 0, 0);
        layout.setAlignment(Qt::AlignRight);
        layout.addWidget(&button);
        layout.setSizeConstraint(QLayout::SetMinimumSize);
        return &widget;
    }

private:
    void buttonClicked(QPushButton &button, const QModelIndex &index) const
    {
        ConfigurableDevicesDialog &dialog = *dynamic_cast<ConfigurableDevicesDialog *>(parent());
        dialog.deviceMenu(button, index);
    }
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

ConfigurableDevicesDialog::ConfigurableDevicesDialog(QWidget &parent, IConfigurableDevicesDialogHost &host, bool cancellable)
    : QDialog(&parent)
    , m_host(host)
    , m_canChangeSlotOptions(false)
{
    // set up UI
    m_ui = std::make_unique<Ui::ConfigurableDevicesDialog>();
    m_ui->setupUi(this);

    // we may or may not be cancellable
    QDialogButtonBox::StandardButtons standardButtons = cancellable
        ? QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel
        : QDialogButtonBox::StandardButton::Ok;
    m_ui->buttonBox->setStandardButtons(standardButtons);

    // set up warning icons
    setupWarningIcons({ *m_ui->warningHashPathIcon, *m_ui->warningDeviceChangesRequireResetIcon });

    // warnings
    m_ui->warningHashPathIcon->setVisible(!host.startedWithHashPaths());
    m_ui->warningHashPathLabel->setVisible(!host.startedWithHashPaths());

    // find a software list collection, if possible
    software_list_collection software_col;
    software_col.load(m_host.imageMenuHost().getPreferences(), m_host.imageMenuHost().getRunningMachine());

    // set up tree view
    ConfigurableDevicesModel &model = *new ConfigurableDevicesModel(this, m_host.imageMenuHost().getRunningMachine(), std::move(software_col));
    m_ui->treeView->setModel(&model);
    m_ui->treeView->setItemDelegateForColumn(1, new ConfigurableDevicesItemDelegate(*this));

    // set up the model reset event
    connect(&model, &QAbstractItemModel::modelReset, [this]() { onModelReset(); });

    // host interactions
	status::state &state = m_host.imageMenuHost().getRunningState();
    m_slotsEventSubscription = state.devslots().subscribe_and_call([this] { updateSlots(); });
    m_imagesEventSubscription = state.images().subscribe_and_call([this] { updateImages(); });
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

ConfigurableDevicesDialog::~ConfigurableDevicesDialog()
{
}


//-------------------------------------------------
//  model
//-------------------------------------------------

ConfigurableDevicesModel &ConfigurableDevicesDialog::model()
{
    return *dynamic_cast<ConfigurableDevicesModel *>(m_ui->treeView->model());
}

const ConfigurableDevicesModel &ConfigurableDevicesDialog::model() const
{
    return *dynamic_cast<ConfigurableDevicesModel *>(m_ui->treeView->model());
}


//-------------------------------------------------
//  onModelReset
//-------------------------------------------------

void ConfigurableDevicesDialog::onModelReset()
{
    // expand all tree items (not really the correct thing to do, but good enough for now)
    m_ui->treeView->expandRecursively(QModelIndex());

    // determine if we have any pending changes
    bool hasPendingDeviceChanges = model().getChanges().size() > 0;

    // and update the UI accordingly
    m_ui->warningDeviceChangesRequireResetIcon->setVisible(hasPendingDeviceChanges);
    m_ui->warningDeviceChangesRequireResetLabel->setVisible(hasPendingDeviceChanges);
    m_ui->applyChangesButton->setEnabled(hasPendingDeviceChanges);
}


//-------------------------------------------------
//  on_applyChangesButton_clicked
//-------------------------------------------------

void ConfigurableDevicesDialog::on_applyChangesButton_clicked()
{
    m_host.changeSlots(model().getChanges());
}


//-------------------------------------------------
//  setupWarningIcons
//-------------------------------------------------

void ConfigurableDevicesDialog::setupWarningIcons(std::initializer_list<std::reference_wrapper<QLabel>> iconLabels)
{
    QSize size(24, 24);
    QPixmap warningIconPixmap = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(size);

    for (QLabel &iconLabel : iconLabels)
    {
        iconLabel.setPixmap(warningIconPixmap);
        iconLabel.setMask(warningIconPixmap.mask());
    }
}


//-------------------------------------------------
//  accept
//-------------------------------------------------

void ConfigurableDevicesDialog::accept()
{
    bool aborting = false;

    // do we have any pending changes?
    std::map<QString, QString> pendingDeviceChanges = model().getChanges();
    if (pendingDeviceChanges.size() > 0)
    {
        // we do - prompt the user
        QMessageBox msgBox(this);
        msgBox.setText("There are pending device configuration changes.  Do you want to apply them?  This will reset the emulation.");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        switch (msgBox.exec())
        {
        case QMessageBox::StandardButton::Yes:
            // apply changes and accept
            m_host.changeSlots(std::move(pendingDeviceChanges));
            break;

        case QMessageBox::StandardButton::No:
            // don't apply changes, just proceed accepting
            break;

        case QMessageBox::StandardButton::Cancel:
            // abort; don't accept
            aborting = true;
            break;

        default:
            throw false;
        }
    }

    // continue up the chain, unless we're cancelling
    if (!aborting)
        QDialog::accept();
}


//-------------------------------------------------
//  updateSlots
//-------------------------------------------------

void ConfigurableDevicesDialog::updateSlots()
{
    // get the slots status
    const std::vector<status::slot> &devslots = m_host.imageMenuHost().getRunningState().devslots().get();

    // if we have act least one slot, we have affirmed that we are on a
    // version of MAME that can handle slot device changes
    if (!devslots.empty())
        m_canChangeSlotOptions = true;

    // update the model
    model().setSlots(devslots);
}


//-------------------------------------------------
//  updateImages
//-------------------------------------------------

void ConfigurableDevicesDialog::updateImages()
{
    // get the image status
    const std::vector<status::image> &images = m_host.imageMenuHost().getRunningState().images().get();

    // update the model
    model().setImages(images);

    // are there any mandatory images missing?
    auto iter = std::find_if(
        images.cbegin(),
        images.cend(),
        [](const status::image &image) { return image.m_details && image.m_details->m_must_be_loaded && image.m_file_name.isEmpty(); });
    bool anyMandatoryImagesMissing = iter != images.cend();

    // set the ok button enabled property
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!anyMandatoryImagesMissing);
}


//-------------------------------------------------
//  deviceMenu
//-------------------------------------------------

void ConfigurableDevicesDialog::deviceMenu(QPushButton &button, const QModelIndex &index)
{
    // get info for the device
    ConfigurableDevicesModel::DeviceInfo devInfo = model().getDeviceInfo(index);

    // we're going to be building a popup menu
    QMenu popupMenu;

    // is this device slotted?
    if (devInfo.slot().has_value())
    {
        // if so, append items for slot devices
        buildDeviceMenuSlotItems(popupMenu, devInfo.tag(), devInfo.slot().value(), devInfo.slotOption());
    }

    // is this device "imaged"?
	appendImageMenuItems(m_host.imageMenuHost(), popupMenu, devInfo.tag());

    // and execute the popup menu
    QPoint popupPos = globalPositionBelowWidget(button);
    popupMenu.exec(popupPos);
}


//-------------------------------------------------
//  buildDeviceMenuSlotItems
//-------------------------------------------------

void ConfigurableDevicesDialog::buildDeviceMenuSlotItems(QMenu &popupMenu, const QString &tag, const info::slot &slot, const QString &currentSlotOption)
{
    struct SlotOptionInfo
    {
        SlotOptionInfo(std::optional<info::slot_option> slotOption = std::nullopt)
            : m_slotOption(slotOption)
        {
        }

        std::optional<info::slot_option>    m_slotOption;
        QString                             m_text;
    };

    // if there are no options, if makes no sense to proceed
    if (slot.options().empty())
        return;

    // if so, prepare a list of options (and the initial item)
    std::vector<SlotOptionInfo> slotOptions;
    slotOptions.emplace_back();

    // and the other options
    for (info::slot_option opt : slot.options())
        slotOptions.emplace_back(opt);

    // and get the human-readable text
    for (SlotOptionInfo &soi : slotOptions)
        soi.m_text = ConfigurableDevicesModel::getSlotOptionText(slot, soi.m_slotOption).value_or(TEXT_NONE);

    // and sort them
    std::sort(
        slotOptions.begin(),
        slotOptions.end(),
        [](const SlotOptionInfo &a, const SlotOptionInfo &b)
        {
            int rc = QString::localeAwareCompare(a.m_text, b.m_text);
            return rc != 0
                ? rc < 0
                : QString::compare(a.m_text, b.m_text) < 0;
        });

    // now put the devices on the popup menu
    for (const SlotOptionInfo &soi : slotOptions)
    {
        // identify the name
        const QString &slotOptionName = soi.m_slotOption.has_value()
            ? soi.m_slotOption->name()
            : util::g_empty_string;

        // create the action
        QAction &action = *popupMenu.addAction(soi.m_text, [this, &tag, slotOptionName]
        {
            model().changeSlotOption(tag, slotOptionName);
        });
        action.setCheckable(true);

        // we only enable the action is we can handle slot device changes
        action.setEnabled(m_canChangeSlotOptions);

        // and check it accordingly
        bool isChecked = currentSlotOption == slotOptionName;
        action.setChecked(isChecked);
    }
}
