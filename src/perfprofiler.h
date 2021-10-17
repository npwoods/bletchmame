/***************************************************************************

	perfprofiler.h

	Code for performance profiling BletchMAME itself (not to be confused
	with profiles)

***************************************************************************/

#pragma once

#ifndef PERFPROFILER_H
#define PERFPROFILER_H

#include <cstring>
#include <functional>
#include <stack>
#include <vector>
#include <iterator>
#include <unordered_map>


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#ifndef USE_PROFILER
#define USE_PROFILER	0
#endif // USE_PROFILER

#ifdef _MSC_VER
#define STRINGIFY1(x) #x
#define STRINGIFY2(x) STRINGIFY1(x)
#define CURRENT_FUNCTION	__FILE__ ":" STRINGIFY2(__LINE__)
#elif defined(__GNUC__)
#define CURRENT_FUNCTION	ProfilerLabel::fromPrettyFunction(__PRETTY_FUNCTION__)
#else
#define CURRENT_FUNCTION	"<<cannot identify function>>"
#endif


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> ProfilerLabel

class ProfilerLabel
{
	friend struct std::hash<ProfilerLabel>;

public:
	static const int MAX_LABEL_LENGTH = 64;

	constexpr ProfilerLabel(const char *text)
	{
		bool b = true;
		for (auto i = 0; i < std::size(m_u.m_text); i++)
		{
			m_u.m_text[i] = b ? text[i] : '\0';
			if (text[b] == '\0')
				b = false;
		}
	}
	ProfilerLabel(const ProfilerLabel &) = default;
	ProfilerLabel(ProfilerLabel &&) = default;

	// operators
	bool operator==(const ProfilerLabel &that) const noexcept
	{
		return !memcmp(m_u.m_text, that.m_u.m_text, sizeof(m_u.m_text));
	}

	operator std::u8string_view() const;

	static constexpr ProfilerLabel fromPrettyFunction(const char *text)
	{
		const char *newText = text;
		while (*text != '\0' && *text != '(')
		{
			if (*text == ' ')
				newText = text + 1;
			text++;
		}
		return ProfilerLabel(newText);
	}

private:
	union
	{
		char			m_text[MAX_LABEL_LENGTH];
		std::uint64_t	m_longs[MAX_LABEL_LENGTH / sizeof(std::uint64_t)];
	} m_u;
};


namespace std
{
	template<>
	struct hash<ProfilerLabel>
	{
		constexpr std::size_t operator()(const ProfilerLabel &x) const
		{
			const std::size_t p1 = 7;
			const std::size_t p2 = 31;

			std::size_t result = p1;
			for (int i = 0; i < sizeof(x.m_u.m_longs) / sizeof(x.m_u.m_longs[0]); i++)
				result = result * p2 + x.m_u.m_longs[i];
			return result;
		}
	};
}


// ======================> RealPerformanceProfiler

class RealPerformanceProfiler
{
public:
	const int MAX_LABEL_SIZE = 64;

	static RealPerformanceProfiler &instance()	{ return g_instance; }
	bool isReal() const							{ return true; }
	void dump();

	void startScope(ProfilerLabel label)
	{
		// translate the label to an index and push it on the stack
		std::size_t labelIndex = getLabelIndex(label);
		m_stack.push(labelIndex);

		// record accumulated time
		recordAccumulatedTime(labelIndex);
	}

	void endScope()
	{
		// pop a label off the stack
		m_stack.pop();
		std::size_t labelIndex = m_stack.top();

		// record accumulated time
		recordAccumulatedTime(labelIndex);
	}


private:
	struct Entry
	{
		Entry(QString &&label);

		QString			m_label;
		std::clock_t	m_accumulatedTime;
	};

	std::size_t										m_currentLabelIndex;
	std::clock_t									m_currentStartTime;
	std::unordered_map<ProfilerLabel, std::size_t>	m_labelMap;
	std::stack<std::size_t>							m_stack;
	std::vector<Entry>								m_entries;

	static RealPerformanceProfiler					g_instance;

	// ctor
	RealPerformanceProfiler();
	RealPerformanceProfiler(const RealPerformanceProfiler &) = delete;
	RealPerformanceProfiler(RealPerformanceProfiler &&) = delete;

	std::size_t getLabelIndex(ProfilerLabel label)
	{
		auto iter = m_labelMap.find(label);
		return iter == m_labelMap.end()
			? introduceNewLabel(label)
			: iter->second;
	}

	void recordAccumulatedTime(std::size_t labelIndex)
	{
		// get the current clock
		std::clock_t currentClock = std::clock();

		// record accumulated time for the label we're currently tracking
		std::clock_t accumulatedTime = currentClock - m_currentStartTime;
		m_entries[m_currentLabelIndex].m_accumulatedTime += accumulatedTime;

		// and start tracking this one
		m_currentLabelIndex = labelIndex;
		m_currentStartTime = currentClock;
	}

	std::size_t introduceNewLabel(const ProfilerLabel &label);
	void dump(QIODevice &file);
	void dump(QTextStream &file);
};


// ======================> FakePerformanceProfiler

class FakePerformanceProfiler
{
public:
	static FakePerformanceProfiler instance()	{ return FakePerformanceProfiler(); }
	bool isReal() const							{ return false; }
	void startScope(ProfilerLabel)				{ /* do nothing */ }
	void endScope()								{ /* do nothing */ }
	void dump()									{ /* do nothing */ }

private:
	FakePerformanceProfiler() = default;
	FakePerformanceProfiler(const FakePerformanceProfiler &) = delete;
	FakePerformanceProfiler(FakePerformanceProfiler &&) = delete;
};


// ======================> PerformanceProfiler

#if USE_PROFILER
typedef RealPerformanceProfiler PerformanceProfiler;
#else // !USE_PROFILER
typedef FakePerformanceProfiler PerformanceProfiler;
#endif // USE_PROFILER


// ======================> ProfilerScope

class ProfilerScope
{
public:
	ProfilerScope(ProfilerLabel label)
	{
		PerformanceProfiler::instance().startScope(label);
	}

	~ProfilerScope()
	{
		PerformanceProfiler::instance().endScope();
	}
};


#endif // PERFPROFILER_H
