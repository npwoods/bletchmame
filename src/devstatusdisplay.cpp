/***************************************************************************

	devstatusdisplay.cpp

	Shepherds widgets for device status

***************************************************************************/

// bletchmame headers
#include "devstatusdisplay.h"

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
	DisplayWidget(const QPixmap &icon1Pixmap);

	// methods
	void set(const QPixmap &icon2Pixmap, const QString &text);

private:
	QLabel	m_icon1;
	QLabel	m_icon2;
	QLabel	m_label;
};


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

DevicesStatusDisplay::DevicesStatusDisplay(QObject *parent)
	: QObject(parent)
	, m_cassettePixmap(":/resources/dev_cassette.png")
	, m_cassettePlayPixmap(":/resources/dev_cassette_play.png")
	, m_cassetteRecordPixmap(":/resources/dev_cassette_record.png")
{
	int iconSize = QApplication::font().pointSize() * 2;
	setPixmapDevicePixelRatioToFit(m_cassettePixmap, iconSize);
	setPixmapDevicePixelRatioToFit(m_cassettePlayPixmap, iconSize);
	setPixmapDevicePixelRatioToFit(m_cassetteRecordPixmap, iconSize);
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
	state.cassettes().subscribe([this, &state]() { updateCassettes(state); });
}


//-------------------------------------------------
//  updateCassettes
//-------------------------------------------------

void DevicesStatusDisplay::updateCassettes(const status::state &state)
{
	for (const status::cassette &cassette : state.cassettes().get())
	{
		// get the display widget if appropriate
		DisplayWidget *displayWidget = getDeviceDisplay(state, cassette.m_tag, cassette.m_motor_state, m_cassettePixmap);
		if (displayWidget)
		{
			// update the widget contents
			const QPixmap &pixmap = cassette.m_is_recording
				? m_cassetteRecordPixmap
				: m_cassettePlayPixmap;
			QString text = QString("%1 / %2").arg(
				formatTime(cassette.m_position),
				formatTime(cassette.m_length));
			displayWidget->set(pixmap, text);
		}
	}
}


//-------------------------------------------------
//  getDeviceDisplay
//-------------------------------------------------

DevicesStatusDisplay::DisplayWidget *DevicesStatusDisplay::getDeviceDisplay(const status::state &state, const QString &tag, bool show, const QPixmap &icon1Pixmap)
{
	// find the widget in our map
	auto iter = m_displayWidgets.find(tag);

	// find the image if 'show' is specified
	const status::image *image = show
		? state.find_image(tag)
		: nullptr;
	bool imageHasFile = image && !image->m_file_name.isEmpty();

	// if we have to show a widget but it is not present, add it
	if (imageHasFile && iter == m_displayWidgets.end())
	{
		// we have to create a new widget; add it
		DisplayWidget::ptr widget = std::make_unique<DisplayWidget>(icon1Pixmap);
		iter = m_displayWidgets.emplace(tag, std::move(widget)).first;

		// and signal that we added it
		emit addWidget(*iter->second);
	}
	else if (!imageHasFile && iter != m_displayWidgets.end())
	{
		// we have a widget but the display is gone; remove it
		emit removeWidget(*iter->second);
		m_displayWidgets.erase(iter);
	}
	
	// identify the widget
	DisplayWidget *widget = imageHasFile ? &*iter->second : nullptr;
		
	// update the tooltip, if appropriate
	if (imageHasFile && image && widget)
	{
		// update the tooltip
		QString fileName = QDir::fromNativeSeparators(image->m_file_name);
		widget->setToolTip(QFileInfo(fileName).fileName());
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
//  DisplayWidget ctor
//-------------------------------------------------

DevicesStatusDisplay::DisplayWidget::DisplayWidget(const QPixmap &icon1Pixmap)
{
	// create the layout
	QHBoxLayout &layout = *new QHBoxLayout(this);
	layout.setSpacing(0);
	layout.setContentsMargins(0, 0, 0, 0);

	// add the labels
	layout.addWidget(&m_icon1);
	layout.addWidget(&m_icon2);
	layout.addWidget(&m_label);

	// and set the pixmap
	m_icon1.setPixmap(icon1Pixmap);
}


//-------------------------------------------------
//  DisplayWidget::setLabelText
//-------------------------------------------------

void DevicesStatusDisplay::DisplayWidget::set(const QPixmap &icon2Pixmap, const QString &text)
{
	m_icon2.setPixmap(icon2Pixmap);
	m_icon2.update();

	m_label.setText(text);
	m_label.update();
}
