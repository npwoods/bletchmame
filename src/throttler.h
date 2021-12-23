/***************************************************************************

	throttler.h

	Used to throttle events that we want limited over time (e.g. - audit
	progress)

***************************************************************************/

#ifndef THROTTLER_H
#define THROTTLER_H

#include <chrono>

// ======================> Throttler

class Throttler
{
public:
	typedef std::chrono::duration<std::int64_t, std::ratio<1, CLOCKS_PER_SEC>> interval_t;

	// ctor
	Throttler(interval_t period);

	// methods
	bool check(interval_t currentInterval = now());

	// statics
	static interval_t now();

private:
	interval_t	m_period;
	interval_t	m_nextTarget;
};

#endif // THROTTLER_H
