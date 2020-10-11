/***************************************************************************

    dialogs/images.h

    Images (File Manager) dialog

***************************************************************************/

#include <QAction>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>

#include "images.h"
#include "ui_images.h"
#include "dialogs/choosesw.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

ImagesDialog::ImagesDialog(QWidget &parent, IImagesHost &host, bool cancellable)
    : QDialog(&parent)
    , m_host(host)
{
    // set up UI
    m_ui = std::make_unique<Ui::ImagesDialog>();
    m_ui->setupUi(this);

    // we may or may not be cancellable
    QDialogButtonBox::StandardButtons standardButtons = cancellable
        ? QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel
        : QDialogButtonBox::StandardButton::Ok;
    m_ui->buttonBox->setStandardButtons(standardButtons);

    // host interactions
    m_imagesEventSubscription = m_host.GetImages().subscribe([this] { UpdateImageGrid(); });

    // initial update of image grid
    UpdateImageGrid();
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

ImagesDialog::~ImagesDialog()
{
}


//-------------------------------------------------
//  UpdateImageGrid
//-------------------------------------------------

void ImagesDialog::UpdateImageGrid()
{
    bool okEnabled = true;
    QString pretty_buffer;

    // get the software list collection
    software_list_collection software_col = BuildSoftwareListCollection();

    // get the list of images
    const std::vector<status::image> &images(m_host.GetImages().get());

    // iterate through the vector of images, and update the grid
    for (int i = 0; i < (int)images.size(); i++)
    {
        // update the label
        QLabel &label = getOrCreateGridWidget<QLabel>(i, 0);
        label.setText(images[i].m_instance_name);
        label.setToolTip(images[i].m_tag);
        
        // if a required image is missing, turn the label red
        bool isRequiredImageMissing = images[i].m_must_be_loaded && images[i].m_file_name.isEmpty();
        label.setStyleSheet(isRequiredImageMissing ? "QLabel { color : #FF0000 }" : "");
        okEnabled = okEnabled && !isRequiredImageMissing;

        // update the text
        QLineEdit &lineEdit = getOrCreateGridWidget<QLineEdit>(i, 1, &ImagesDialog::setupGridLineEdit);
        const QString &prettyName = PrettifyImageFileName(software_col, images[i].m_tag, images[i].m_file_name, pretty_buffer, true);
        lineEdit.setText(prettyName);

        // update the button
        getOrCreateGridWidget<QPushButton>(i, 2, &ImagesDialog::setupGridButton);
    }

    // clear out the rest
    for (int row = m_ui->gridLayout->rowCount() - 1; row >= 0 && row >= images.size(); row--)
    {
        for (int col = 0; col < m_ui->gridLayout->columnCount(); col++)
        {
            QLayoutItem *layoutItem = m_ui->gridLayout->itemAtPosition(row, col);
            if (layoutItem && layoutItem->widget())
                delete layoutItem->widget();
        }
    }

    // set the ok button enabled property
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(okEnabled);
}


//-------------------------------------------------
//  getOrCreateGridWidget
//-------------------------------------------------

template<class T>
T &ImagesDialog::getOrCreateGridWidget(int row, int column, void (ImagesDialog::*setupFunc)(T &, int))
{
    // get the layout item and the widget, if present
    QLayoutItem *layoutItem = m_ui->gridLayout->itemAtPosition(row, column);   
    QWidget *widget = layoutItem
        ? layoutItem->widget()
        : nullptr;

    // did we find the widget?
    T *result;
    if (widget)
    {
        // we did - cast it
        result = dynamic_cast<T *>(widget);
    }
    else
    {
        // we did not - we have to create it
        result = new T(this);
        if (setupFunc)
            ((*this).*setupFunc)(*result, row);
        m_ui->gridLayout->addWidget(result, row, column);
    }
    return *result;
}


//-------------------------------------------------
//  setupGridLineEdit
//-------------------------------------------------

void ImagesDialog::setupGridLineEdit(QLineEdit &lineEdit, int row)
{
    lineEdit.setReadOnly(true);
}


//-------------------------------------------------
//  setupGridButton
//-------------------------------------------------

void ImagesDialog::setupGridButton(QPushButton &button, int row)
{
    // set up the button
    button.setText("...");
    button.setFixedWidth(30);
    connect(&button, &QPushButton::clicked, this, [this, row, &button]() { ImageMenu(button, row); });
}


//-------------------------------------------------
//  PrettifyImageFileName
//-------------------------------------------------

const QString &ImagesDialog::PrettifyImageFileName(const software_list_collection &software_col, const QString &tag, const QString &file_name, QString &buffer, bool full_path)
{
    const QString *result = nullptr;

    // find the software
    const QString *dev_interface = DeviceInterfaceFromTag(tag);
    const software_list::software *software = dev_interface
        ? software_col.find_software_by_name(file_name, *dev_interface)
        : nullptr;

    // did we find the software?
    if (software)
    {
        // if so, the pretty name is the description
        result = &software->m_description;
    }
    else if (!full_path && !file_name.isEmpty())
    {
        // we want to show the base name plus the extension
        QString normalizedFileName = QDir::fromNativeSeparators(file_name);
        QFileInfo fileInfo(normalizedFileName);
        buffer = fileInfo.baseName();
        if (!fileInfo.suffix().isEmpty())
        {
            buffer += ".";
            buffer += fileInfo.suffix();
        }
        result = &buffer;
    }
    else
    {
        // we want to show the full filename; our job is easy
        result = &file_name;
    }
    return *result;
}


//-------------------------------------------------
//  DeviceInterfaceFromTag
//-------------------------------------------------

const QString *ImagesDialog::DeviceInterfaceFromTag(const QString &tag)
{
    // find the device
    auto iter = std::find_if(
        m_host.GetMachine().devices().cbegin(),
        m_host.GetMachine().devices().end(),
        [&tag](info::device dev) { return tag == dev.tag(); });

    // if we found a device, return the interface
    return iter != m_host.GetMachine().devices().end()
        ? &(*iter).devinterface()
        : nullptr;
}


//-------------------------------------------------
//  BuildSoftwareListCollection
//-------------------------------------------------

software_list_collection ImagesDialog::BuildSoftwareListCollection() const
{
    software_list_collection software_col;
    software_col.load(m_host.GetPreferences(), m_host.GetMachine());
    return software_col;
}


//-------------------------------------------------
//  ImageMenu
//-------------------------------------------------

bool ImagesDialog::ImageMenu(const QPushButton &button, int row)
{
    // get info about the image
    const status::image &image = m_host.GetImages().get()[row];

    software_list_collection software_col = BuildSoftwareListCollection();
    const QString *dev_interface = DeviceInterfaceFromTag(image.m_tag);

    // setup popup menu - first the create/load items
    QMenu popupMenu(this);
    if (image.m_is_creatable)
        popupMenu.addAction("Create...",    [this, &image]() { CreateImage(image.m_tag); });
    popupMenu.addAction("Load Image...",    [this, &image]() { LoadImage(image.m_tag); });

    // if we have a software list part, put that on there too
    if (!software_col.software_lists().empty() && dev_interface)
    {
        popupMenu.addAction("Load Software List Part...", [this, &software_col, &image, dev_interface]()
        {
            LoadSoftwareListPart(software_col, image.m_tag, *dev_interface);
        });
    }

    // unload
    QAction &unloadAction = *popupMenu.addAction("Unload");
    unloadAction.setEnabled(!image.m_file_name.isEmpty());

    // recent files
    const std::vector<QString> &recent_files = m_host.GetRecentFiles(image.m_tag);
    if (!recent_files.empty())
    {
        QString pretty_buffer;
        popupMenu.addSeparator();
        for (const QString &recent_file : recent_files)
        {
            const QString &pretty_recent_file = PrettifyImageFileName(software_col, image.m_tag, recent_file, pretty_buffer, false);
            popupMenu.addAction(pretty_recent_file, [this, &image, &recent_file]() { m_host.LoadImage(image.m_tag, QString(recent_file)); });
        }
    }

    // execute!
    QPoint popupPos = globalPositionBelowWidget(button);
    return popupMenu.exec(popupPos) ? true : false;
}


//-------------------------------------------------
//  CreateImage
//-------------------------------------------------

bool ImagesDialog::CreateImage(const QString &tag)
{
    // show the fialog
    QFileDialog dialog(
        this,
        "Create Image",
        m_host.GetWorkingDirectory(),
        GetWildcardString(tag, false));
    dialog.setFileMode(QFileDialog::FileMode::AnyFile);
    dialog.exec();
    if (dialog.result() != QDialog::DialogCode::Accepted)
        return false;

    // get the result from the dialog
    QString path = QDir::toNativeSeparators(dialog.selectedFiles().first());

    // update our host's working directory
    UpdateWorkingDirectory(path);

    // and load the image
    m_host.CreateImage(tag, std::move(path));
    return true;
}


//-------------------------------------------------
//  LoadImage
//-------------------------------------------------

bool ImagesDialog::LoadImage(const QString &tag)
{
    // show the fialog
    QFileDialog dialog(
        this,
        "Load Image",
        m_host.GetWorkingDirectory(),
        GetWildcardString(tag, true));
    dialog.setFileMode(QFileDialog::FileMode::ExistingFile);
    dialog.exec();
    if (dialog.result() != QDialog::DialogCode::Accepted)
        return false;

    // get the result from the dialog
    QString path = QDir::toNativeSeparators(dialog.selectedFiles().first());

    // update our host's working directory
    UpdateWorkingDirectory(path);

    // and load the image
    m_host.LoadImage(tag, std::move(path));
    return true;
}


//-------------------------------------------------
//  LoadSoftwareListPart
//-------------------------------------------------

bool ImagesDialog::LoadSoftwareListPart(const software_list_collection &software_col, const QString &tag, const QString &dev_interface)
{
    ChooseSoftlistPartDialog dialog(this, m_host.GetPreferences(), software_col, dev_interface);
    dialog.exec();
    if (dialog.result() != QDialog::DialogCode::Accepted)
        return false;

    m_host.LoadImage(tag, dialog.selection());
    return true;
}


//-------------------------------------------------
//  UnloadImage
//-------------------------------------------------

bool ImagesDialog::UnloadImage(const QString &tag)
{
    m_host.UnloadImage(tag);
    return false;
}


//-------------------------------------------------
//  GetWildcardString
//-------------------------------------------------

QString ImagesDialog::GetWildcardString(const QString &tag, bool support_zip) const
{
    // get the list of extensions
    std::vector<QString> extensions = m_host.GetExtensions(tag);

    // append zip if appropriate
    if (support_zip)
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
//  UpdateWorkingDirectory
//-------------------------------------------------

void ImagesDialog::UpdateWorkingDirectory(const QString &path)
{
    QString dir;
    wxFileName::SplitPath(path, &dir, nullptr, nullptr);
    m_host.SetWorkingDirectory(std::move(dir));
}
