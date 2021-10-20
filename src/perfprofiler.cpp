/***************************************************************************

	perfprofiler.cpp

	Code for performance profiling BletchMAME itself (not to be confused
	with profiles)

***************************************************************************/

#include <QCoreApplication>
#include <QFile>
#include <numeric>

#include "perfprofiler.h"

RealPerformanceProfiler RealPerformanceProfiler::g_instance;


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

RealPerformanceProfiler::RealPerformanceProfiler()
{
	const int RESERVE_LABEL_COUNT = 256;
	m_labelMap.reserve(RESERVE_LABEL_COUNT);
	m_entries.reserve(RESERVE_LABEL_COUNT);

	// initial setup
	m_currentLabelIndex = getLabelIndex("<<none>>");
	m_currentStartTime = std::clock();
	m_stack.push(m_currentLabelIndex);
}


//-------------------------------------------------
//  introduceNewLabel
//-------------------------------------------------

std::size_t RealPerformanceProfiler::introduceNewLabel(const ProfilerLabel &label)
{
	QString labelText = label.toQString().leftJustified(70);

	std::size_t result = m_labelMap.size();
	m_labelMap.emplace(label, result);
	m_entries.emplace_back(std::move(labelText));
	return result;
}


//-------------------------------------------------
//  dump
//-------------------------------------------------

void RealPerformanceProfiler::dump()
{
	QString profileDataFileName = QCoreApplication::applicationDirPath() + "/profiledata.txt";
	QFile file(profileDataFileName);
	if (file.open(QIODeviceBase::WriteOnly))
		dump(file);
}


//-------------------------------------------------
//  dump(QIODevice &)
//-------------------------------------------------

void RealPerformanceProfiler::dump(QIODevice &file)
{
	QTextStream stream(&file);
	dump(stream);
}


//-------------------------------------------------
//  dump(QTextStream &)
//-------------------------------------------------

void RealPerformanceProfiler::dump(QTextStream &stream)
{
	// accumulate time one last time
	recordAccumulatedTime(m_currentLabelIndex);

	// get the total time
	std::clock_t totalTime = 0;
	for (const Entry &entry : m_entries)
		totalTime += entry.m_accumulatedTime;

	if (totalTime > 0)
	{
		for (const Entry &entry : m_entries)
		{
			QString percent = QString::number((double)entry.m_accumulatedTime / totalTime * 100.0);
			stream << QString("%1\t%2 / %3%\n").arg(
				entry.m_label,
				QString::number(entry.m_count),
				percent);
		}
	}
	else
	{
		stream << "(no actively profiled code found)\n";
	}

	// clear out times
	for (Entry &entry : m_entries)
	{
		entry.m_accumulatedTime = 0;
		entry.m_count = 0;
	}
}


//-------------------------------------------------
//  Entry ctor
//-------------------------------------------------

RealPerformanceProfiler::Entry::Entry(QString &&label)
	: m_label(std::move(label))
	, m_accumulatedTime(0)
	, m_count(0)
{
}


//-------------------------------------------------
//  ProfilerLabel::toQString()
//-------------------------------------------------

QString ProfilerLabel::toQString() const
{
	std::size_t len = 0;
	while (len < std::size(m_text) && m_text[len])
		len++;
	std::u8string_view sv((const char8_t *) &m_text[0], len);
	return util::toQString(sv);
}
