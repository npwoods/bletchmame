/***************************************************************************

	liveinstancetracker.h

	Utility class for tracking live instances

***************************************************************************/

#ifndef LIVEINSTANCETRACKER_H
#define LIVEINSTANCETRACKER_H

class QObject;

// ======================> LiveInstanceTrackerBase

class LiveInstanceTrackerBase
{
public:
	// operators
	operator bool() const { return m_instance; }

protected:
	// ctor
	LiveInstanceTrackerBase()
		: m_instance(nullptr)
	{
	}

	LiveInstanceTrackerBase(const LiveInstanceTrackerBase &) = delete;
	LiveInstanceTrackerBase(LiveInstanceTrackerBase &&) = delete;

	// accessor
	QObject *instance() { return m_instance; }

	// methods
	void internalTrack(QObject &instance);

private:
	class Tracking;

	// fields
	QObject *	m_instance;

	// methods
	void stopTracking();
};


// ======================> LiveInstanceTracker

template<class T>
class LiveInstanceTracker : public LiveInstanceTrackerBase
{
public:
	LiveInstanceTracker()
		: m_instance(nullptr)
	{
	}

	void track(T &instance)
	{
		internalTrack(instance);
	}

	// operators
	T &operator*()	{ return dynamic_cast<T &>(*instance()); }
	T *operator->() { return dynamic_cast<T *>(instance()); }

private:
	T *m_instance;
};

#endif // LIVEINSTANCETRACKER_H
