/***************************************************************************

	liveinstancetracker.cpp

	Utility class for tracking live instances

***************************************************************************/

#include <QObject>

#include "liveinstancetracker.h"

class LiveInstanceTrackerBase::Tracking : public QObject
{
public:
	Tracking(LiveInstanceTrackerBase &tracker, QObject &parent);
	~Tracking();

private:
	LiveInstanceTrackerBase &m_tracker;
};


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  internalTrack
//-------------------------------------------------

void LiveInstanceTrackerBase::internalTrack(QObject &instance)
{
	// we're only allowed to start tracking if we're not doing so already
	assert(!m_instance);

	// set the instance
	m_instance = &instance;

	// and create a Tracking object that when destroyed, will cause stopTracking() to be invoked
	(void) new Tracking(*this, instance);
}


//-------------------------------------------------
//  stopTracking
//-------------------------------------------------

void LiveInstanceTrackerBase::stopTracking()
{
	assert(m_instance);

	m_instance = nullptr;
}


//-------------------------------------------------
//  Tracking ctor
//-------------------------------------------------

LiveInstanceTrackerBase::Tracking::Tracking(LiveInstanceTrackerBase &tracker, QObject &parent)
	: QObject(&parent)
	, m_tracker(tracker)
{
}


//-------------------------------------------------
//  Tracking dtor
//-------------------------------------------------

LiveInstanceTrackerBase::Tracking::~Tracking()
{
	m_tracker.stopTracking();
}
