/***************************************************************************

	devstatusdisplay.h

	Shepherds widgets for device status

***************************************************************************/

#ifndef DEVSTATUSDISPLAY_H
#define DEVSTATUSDISPLAY_H

// bletchmame headers
#include "status.h"

// Qt headers
#include <QWidget>

// standard headers
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
	DevicesStatusDisplay(QObject *parent = nullptr);
	~DevicesStatusDisplay();

	// methods
	void subscribe(status::state &state);

signals:
	void addWidget(QWidget &widget);
	void removeWidget(QWidget &widget);

private:
	class DisplayWidget;
	typedef std::unordered_map<QString, std::unique_ptr<DisplayWidget>> DisplayWidgetMap;

	// member variables
	DisplayWidgetMap	m_displayWidgets;
	QPixmap				m_cassettePixmap;
	QPixmap				m_cassettePlayPixmap;
	QPixmap				m_cassetteRecordPixmap;

	// private methods
	void updateCassettes(const std::vector<status::cassette> &cassettes);
	DisplayWidget *getDeviceDisplay(const QString &tag, bool show);
	static QString formatTime(float t);
};

#endif // DEVSTATUSDISPLAY_H
