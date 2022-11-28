/***************************************************************************

	devstatusdisplay.cpp

	Shepherds widgets for device status

***************************************************************************/

// bletchmame headers
#include "devstatusdisplay.h"
#include "imagemenu.h"

// Qt headers
#include <QApplication>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>


//**************************************************************************
//  TYPES
//**************************************************************************

// ======================> DisplayWidget

class DevicesStatusDisplay::DisplayWidget : public QWidget
{
public:
	typedef std::unique_ptr<DisplayWidget> ptr;

	// ctor
	DisplayWidget(QWidget *parent, const QPixmap &icon1Pixmap);

	// methods
	void set(const QPixmap &icon2Pixmap, const QString &text);

private:
	QLabel					m_icon1Label;
	QLabel					m_icon2Label;
	QLabel					m_textLabel;
};


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

const char *DevicesStatusDisplay::s_cassettePixmapResourceName = ":/resources/dev_cassette.png";
const char *DevicesStatusDisplay::s_cassettePlayPixmapResourceName = ":/resources/dev_cassette_play.png";
const char *DevicesStatusDisplay::s_cassetteRecordPixmapResourceName = ":/resources/dev_cassette_record.png";


//-------------------------------------------------
//  ctor
//-------------------------------------------------

DevicesStatusDisplay::DevicesStatusDisplay(IImageMenuHost &host, QObject *parent)
	: QObject(parent)
	, m_host(host)
	, m_cassettePixmap(s_cassettePixmapResourceName)
	, m_cassettePlayPixmap(s_cassettePlayPixmapResourceName)
	, m_cassetteRecordPixmap(s_cassetteRecordPixmapResourceName)
{
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

DevicesStatusDisplay::~DevicesStatusDisplay()
{
}


//-------------------------------------------------
//  subscribe
//-------------------------------------------------

void DevicesStatusDisplay::subscribe(status::state &state)
{
	state.images().subscribe([this, &state]()		{ updateImages(state); });
	state.cassettes().subscribe([this, &state]()	{ updateCassettes(state); });
}


//-------------------------------------------------
//  updateImages
//-------------------------------------------------

void DevicesStatusDisplay::updateImages(const status::state &state)
{
	updateCassettes(state);
}


//-------------------------------------------------
//  updateCassettes
//-------------------------------------------------

void DevicesStatusDisplay::updateCassettes(const status::state &state)
{
	for (const status::cassette &cassette : state.cassettes().get())
	{
		// get the display widget if appropriate
		DisplayWidget *displayWidget = getDeviceDisplay(state, cassette.m_tag, m_cassettePixmap);
		if (displayWidget)
		{
			// update the widget contents
			const QPixmap &pixmap = cassette.m_motor_state
				? (cassette.m_is_recording ? m_cassetteRecordPixmap : m_cassettePlayPixmap)
				: m_emptyPixmap;
			QString text = cassette.m_motor_state
				? QString("%1 / %2").arg(formatTime(cassette.m_position), formatTime(cassette.m_length))
				: "";
			displayWidget->set(pixmap, text);
		}
	}
}


//-------------------------------------------------
//  getDeviceDisplay
//-------------------------------------------------

DevicesStatusDisplay::DisplayWidget *DevicesStatusDisplay::getDeviceDisplay(const status::state &state, const QString &tag, const QPixmap &icon1Pixmap)
{
	// find the widget in our map
	auto iter = m_displayWidgets.find(tag);

	// find the image and check to see if there is an image
	const status::image *image = state.find_image(tag);
	bool imageHasFile = image && !image->m_file_name.isEmpty();

	// if we have to show a widget but it is not present, add it
	if (imageHasFile && iter == m_displayWidgets.end())
	{
		// we have to create a new widget; add it
		DisplayWidget &widget = *new DisplayWidget(&m_parentWidget, icon1Pixmap);
		iter = m_displayWidgets.emplace(tag, widget).first;

		// set up a context menu
		connect(&widget, &QWidget::customContextMenuRequested, this, [this, &widget, tag](const QPoint &pos)
		{
			contextMenu(tag, widget.mapToGlobal(pos));
		});

		// and signal that we added it
		emit addWidget(widget);
	}
	else if (!imageHasFile && iter != m_displayWidgets.end())
	{
		// we have a widget but the display is gone; remove it from our list
		QWidget &widget = iter->second;
		m_displayWidgets.erase(iter);

		// and now delete it
		delete &widget;
	}
	
	// identify the widget
	DisplayWidget *widget = imageHasFile ? &iter->second : nullptr;
		
	// update the tooltip, if appropriate
	if (imageHasFile && image && widget)
	{
		// update the tooltip
		QString text = imageDisplayText(*image);
		widget->setToolTip(text);
	}

	return widget;
}


//-------------------------------------------------
//  formatTime
//-------------------------------------------------

QString DevicesStatusDisplay::formatTime(float t)
{
	long integerTime = (int)floor(t);
	return QString("%1:%2").arg(
		QString::number(integerTime / 60),
		QString::asprintf("%02ld", integerTime % 60));
}


//-------------------------------------------------
//  imageDisplayText
//-------------------------------------------------

QString DevicesStatusDisplay::imageDisplayText(const status::image &image) const
{
	QString imageFileName = prettifyImageFileName(
		m_host.getRunningMachine(),
		m_host.getRunningSoftwareListCollection(),
		image.m_tag,
		image.m_file_name,
		false);

	return QString("%1: %2").arg(image.m_tag, imageFileName);
}


//-------------------------------------------------
//  contextMenu
//-------------------------------------------------

void DevicesStatusDisplay::contextMenu(const QString &tag, const QPoint &pos)
{
	// get the image
	const status::image *image = m_host.getRunningState().find_image(tag);
	if (!image)
		return;

	// determine the display text for the image
	QString text = imageDisplayText(*image);

	// build the context menu
	QMenu popupMenu;
	popupMenu.addAction(text)->setEnabled(false);
	appendImageMenuItems(m_host, popupMenu, tag);

	// show the popup menu
	popupMenu.exec(pos);
}


//-------------------------------------------------
//  DisplayWidget ctor
//-------------------------------------------------

DevicesStatusDisplay::DisplayWidget::DisplayWidget(QWidget *parent, const QPixmap &icon1Pixmap)
	: QWidget(parent)
{
	// create the layout
	QHBoxLayout &layout = *new QHBoxLayout(this);
	layout.setSpacing(0);
	layout.setContentsMargins(0, 0, 0, 0);

	// set the pixmap
	m_icon1Label.setPixmap(icon1Pixmap);

	// add the labels
	layout.addWidget(&m_icon1Label);
	layout.addWidget(&m_icon2Label);
	layout.addWidget(&m_textLabel);

	// set the maximum sizes - we want these icons to never exceed the height of the caption
	int textLabelHeight = m_textLabel.sizeHint().height();
	m_icon1Label.setMaximumSize(QSize(textLabelHeight, textLabelHeight));
	m_icon2Label.setMaximumSize(QSize(textLabelHeight, textLabelHeight));
	m_icon1Label.setScaledContents(true);
	m_icon2Label.setScaledContents(true);

	// prep a context menu
	setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
}


//-------------------------------------------------
//  DisplayWidget::set
//-------------------------------------------------

void DevicesStatusDisplay::DisplayWidget::set(const QPixmap &icon2Pixmap, const QString &text)
{
	// icon 2
	m_icon2Label.setPixmap(icon2Pixmap);
	m_icon2Label.update();

	// text
	m_textLabel.setText(text);
	m_textLabel.update();
}
