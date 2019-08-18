/***************************************************************************

    wxhelpers.h

    Miscellaneous helper code to make wxWidgets friendlier to modern C++

***************************************************************************/

#pragma once

#ifndef WXHELPERS_H
#define WXHELPERS_H

#include <wx/menu.h>
#include <wx/sizer.h>
#include <wx/window.h>

#include <memory>


//**************************************************************************
//  PAYLOAD EVENT - a more C++-ish way to implement wxEvent
//**************************************************************************

// ======================> PayloadEvent

template<class T>
class PayloadEvent : public wxEvent
{
public:
	PayloadEvent(wxEventType event_type, int winid, T &&payload)
		: PayloadEvent(event_type, winid, std::make_shared<T>(std::move(payload)))
	{
	}

	T &Payload() { return *m_ptr; }

protected:
	virtual wxEvent *Clone() const override
	{
		return new PayloadEvent(GetEventType(), GetId(), m_ptr);
	}

private:
	std::shared_ptr<T>  m_ptr;

	PayloadEvent(wxEventType event_type, int winid, std::shared_ptr<T> ptr)
		: wxEvent(winid, event_type)
		, m_ptr(std::move(ptr))
	{
	}
};


//**************************************************************************
//  SIZER HELPERS
//**************************************************************************

enum class boxsizer_orientation
{
	HORIZONTAL,
	VERTICAL
};

class SizerBuilder
{
public:
	class Member
	{
	public:
		Member(int proportion, int flag, wxWindow &window)
			: m_proportion(proportion)
			, m_flag(flag)
			, m_window(&window)
		{
		}

		Member(int proportion, int flag, boxsizer_orientation orientation, int border, std::initializer_list<Member> &&members)
			: m_proportion(proportion)
			, m_flag(flag)
			, m_window(nullptr)
		{
			m_sub_sizer = CreateBoxSizer(orientation, border, std::move(members));
		}

		Member(int proportion, int flag, wxSizer *sizer)
			: m_proportion(proportion)
			, m_flag(flag)
			, m_window(nullptr)
			, m_sub_sizer(sizer)
		{
		}

		void AddToSizer(wxSizer &sizer, int border) const
		{
			if (m_window)
				sizer.Add(m_window, m_proportion, m_flag, border);
			else if (m_sub_sizer)
				sizer.Add(m_sub_sizer.release(), m_proportion, m_flag, border);
		}

	private:
		wxWindow *							m_window;
		int									m_proportion;
		int									m_flag;
		mutable std::unique_ptr<wxSizer>	m_sub_sizer;
	};

	SizerBuilder(boxsizer_orientation orientation, int border, std::initializer_list<Member> &&members)
		: m_sizer(CreateBoxSizer(orientation, border, std::move(members)))
	{
	}

	wxSizer *GetSizer()
	{
		return m_sizer.release();
	}

private:
	std::unique_ptr<wxSizer>	m_sizer;

	static std::unique_ptr<wxSizer> CreateBoxSizer(boxsizer_orientation orientation, int border, std::initializer_list<Member> &&members)
	{
		// map the orientation value
		int flags;
		switch (orientation)
		{
		case boxsizer_orientation::HORIZONTAL:
			flags = wxHORIZONTAL;
			break;
		case boxsizer_orientation::VERTICAL:
			flags = wxVERTICAL;
			break;
		default:
			throw false;
		}

		// create the sizer itself
		std::unique_ptr<wxSizer> sizer = std::make_unique<wxBoxSizer>(flags);

		// and build the members
		for (const Member &member : members)
			member.AddToSizer(*sizer, border);

		return sizer;
	}
};

//-------------------------------------------------
//  SpecifySizer
//-------------------------------------------------

inline void SpecifySizer(wxWindow &window, SizerBuilder &&sb)
{
	window.SetSizer(sb.GetSizer());
}


//-------------------------------------------------
//  SpecifySizerAndFit
//-------------------------------------------------

inline void SpecifySizerAndFit(wxWindow &window, SizerBuilder &&sb)
{
	window.SetSizerAndFit(sb.GetSizer());
}


//**************************************************************************
//  POSTEVENT & QUEUEEVENT
//**************************************************************************

template<typename TEvent, typename... TArgs>
void PostEvent(wxEvtHandler &dest, const wxEventTypeTag<TEvent> &event_type, TArgs &&... args)
{
	wxEvent *event = new TEvent(event_type, std::forward<TArgs>(args)...);
	wxPostEvent(&dest, *event);
}


template<typename TEvent, typename... TArgs>
void QueueEvent(wxEvtHandler &dest, const wxEventTypeTag<TEvent> &event_type, TArgs &&... args)
{
	wxEvent *event = new TEvent(event_type, std::forward<TArgs>(args)...);
	dest.QueueEvent(event);
}


//**************************************************************************
//  MENU WITH RESULT - makes popup menus simpler
//**************************************************************************

class MenuWithResult : public wxMenu
{
public:
	MenuWithResult();
	int Result() const { return m_result; }

private:
	int m_result;
};

#endif // WXHELPERS_H
