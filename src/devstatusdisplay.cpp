/***************************************************************************

	devstatusdisplay.cpp

	Shepherds widgets for device status

***************************************************************************/

// bletchmame headers
#include "devstatusdisplay.h"

// Qt headers
#include <QApplication>
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
	state.cassettes().subscribe([this, &state]() { updateCassettes(state.cassettes().get()); });
}


//-------------------------------------------------
//  updateCassettes
//-------------------------------------------------

void DevicesStatusDisplay::updateCassettes(const std::vector<status::cassette> &cassettes)
{
	for (const status::cassette &cassette : cassettes)
	{
		DisplayWidget *displayWidget = getDeviceDisplay(cassette.m_tag, cassette.m_motor_state);
		if (displayWidget)
		{
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

DevicesStatusDisplay::DisplayWidget *DevicesStatusDisplay::getDeviceDisplay(const QString &tag, bool show)
{
	// find the widget in our map
	auto iter = m_displayWidgets.find(tag);

	// if we have to show a widget but it is not present, add it
	if (show && iter == m_displayWidgets.end())
	{
		// we have to create a new widget; add it
		DisplayWidget::ptr widget = std::make_unique<DisplayWidget>(m_cassettePixmap);
		iter = m_displayWidgets.emplace(tag, std::move(widget)).first;

		// and signal that we added it
		emit addWidget(*iter->second);
	}
	else if (!show && iter != m_displayWidgets.end())
	{
		// we have a widget but the display is gone; remove it
		emit removeWidget(*iter->second);
		m_displayWidgets.erase(iter);
		iter = m_displayWidgets.end();
	}

	return iter != m_displayWidgets.end()
		? &*iter->second
		: nullptr;
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
