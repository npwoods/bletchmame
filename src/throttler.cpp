/***************************************************************************

	throttler.cpp

	Used to throttle events that we want limited over time (e.g. - audit
	progress)

***************************************************************************/

#include "throttler.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

Throttler::Throttler(interval_t period)
	: m_period(period)
	, m_nextTarget(0)
{
}


//-------------------------------------------------
//  check
//-------------------------------------------------

bool Throttler::check(Throttler::interval_t currentInterval)
{
	// check the throttle - has enough time elapsed?
	bool result = m_nextTarget == (interval_t)0 || m_nextTarget < currentInterval;

	// if so, update
	if (result)
		m_nextTarget = currentInterval + m_period;

	return result;
}


//-------------------------------------------------
//  now
//-------------------------------------------------

Throttler::interval_t Throttler::now()
{
	return interval_t(clock());
}
