/***************************************************************************

	throughputtracker.h

	Code for measuring throughput (used to measure auditing speeds)

***************************************************************************/

#pragma once

#ifndef THROUGHPUTTRACKER_H
#define THROUGHPUTTRACKER_H

#include <chrono>
#include <ctime>
#include <optional>
#include <queue>

#include <QString>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> ThroughputTracker

class ThroughputTracker
{
public:
	// ctor
	ThroughputTracker(const QString &fileName);
	ThroughputTracker(const ThroughputTracker &) = delete;
	ThroughputTracker(ThroughputTracker &&) = delete;

	// methods
	void mark(double units);

private:
	typedef std::chrono::duration<std::int64_t, std::ratio<1, CLOCKS_PER_SEC>> interval_t;

	struct Entry
	{
		interval_t	m_interval;
		double		m_units;
	};

	// members
	QString						m_fileName;
	double						m_totalUnits;
	std::optional<interval_t>	m_lastDump;
	std::queue<Entry>			m_queue;

	// private methods
	void dump() const;
};


#endif // THROUGHPUTTRACKER_H
