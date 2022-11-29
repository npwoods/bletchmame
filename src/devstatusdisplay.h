/***************************************************************************

	devstatusdisplay.h

	Shepherds widgets for device status

***************************************************************************/

#ifndef DEVSTATUSDISPLAY_H
#define DEVSTATUSDISPLAY_H

// bletchmame headers
#include "imagemenu.h"

// Qt headers
#include <QWidget>

// standard headers
#include <tuple>
#include <unordered_map>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE


// ======================> DevicesStatusDisplay

class DevicesStatusDisplay : public QObject
{
	Q_OBJECT
public:
	class Test;

	// ctor/dtor
	DevicesStatusDisplay(IImageMenuHost &host, QObject *parent = nullptr);
	~DevicesStatusDisplay();

	// methods
	void subscribe(status::state &state);

signals:
	void addWidget(QWidget &widget);

private:
	static const char *s_cassettePixmapResourceName;
	static const char *s_cassettePlayPixmapResourceName;
	static const char *s_cassetteRecordPixmapResourceName;

	class DisplayWidget;
	typedef std::unordered_map<QString, DisplayWidget &> DisplayWidgetMap;

	// member variables
	IImageMenuHost &	m_host;
	DisplayWidgetMap	m_displayWidgets;
	QWidget				m_parentWidget;
	QPixmap				m_emptyPixmap;
	QPixmap				m_cassettePixmap;
	QPixmap				m_cassettePlayPixmap;
	QPixmap				m_cassetteRecordPixmap;

	// private methods
	void updateImages(const status::state &state);
	void updateCassettes(const status::state &state);
	DevicesStatusDisplay::DisplayWidget *getDeviceDisplay(const status::state &state, const QString &tag, const QPixmap &icon1Pixmap);
	static QString formatTime(float t);
	void contextMenu(const QString &tag, const QPoint &pos);
	QString imageDisplayText(const status::image &image) const;
};

#endif // DEVSTATUSDISPLAY_H
