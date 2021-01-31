/***************************************************************************

	splitterviewtoggler.cpp

	UI glue to facilitate QAbstractButtons toggling splitter views

***************************************************************************/

#include "splitterviewtoggler.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

SplitterViewToggler::SplitterViewToggler(QObject *parent, QAbstractButton &button, QSplitter &splitter, int ownedSplitterNumber, int otherSplitterNumber, std::function<void()> &&changedCallback)
	: QObject(parent)
	, m_button(button)
	, m_splitter(splitter)
	, m_ownedSplitterNumber(ownedSplitterNumber)
	, m_otherSplitterNumber(otherSplitterNumber)
	, m_lastWidth(0)
	, m_changedCallback(std::move(changedCallback))
{
	connect(&m_splitter, &QSplitter::splitterMoved, this, [this](int pos, int index) { updateChecked(); });
	connect(&m_button, &QAbstractButton::clicked, this, &SplitterViewToggler::buttonClicked);
	updateChecked();
}


//-------------------------------------------------
//  buttonClicked
//-------------------------------------------------

void SplitterViewToggler::buttonClicked(bool checked)
{
	QList<int> sizes = getSplitterSizes();
	if (sizes.isEmpty())
		return;

	// determine the new size of the splitter we own
	int newSize = checked
		? (m_lastWidth > 0 ? m_lastWidth : sizes[m_otherSplitterNumber] / 2)
		: 0;

	// update lastWidth, if applicable
	if (newSize == 0 && sizes[m_ownedSplitterNumber] > 0)
		m_lastWidth = sizes[m_ownedSplitterNumber];

	// adjust the sizes
	sizes[m_otherSplitterNumber] += sizes[m_ownedSplitterNumber] - newSize;
	sizes[m_ownedSplitterNumber] = newSize;

	// and set them on the splitter
	m_splitter.setSizes(sizes);

	// and invoke the callback if we have one
	if (m_changedCallback)
		m_changedCallback();
}


//-------------------------------------------------
//  updateChecked
//-------------------------------------------------

void SplitterViewToggler::updateChecked()
{
	QList<int> sizes = getSplitterSizes();
	int size = !sizes.isEmpty() ? sizes[m_ownedSplitterNumber] : 0;
	m_button.setChecked(size > 0);
}


//-------------------------------------------------
//  getSplitterSizes
//-------------------------------------------------

QList<int> SplitterViewToggler::getSplitterSizes() const
{
	QList<int> sizes = m_splitter.sizes();

	// clear the results if they are too small (so we can either return a properly sized splitter, or return empty)
	if (sizes.size() <= m_ownedSplitterNumber || sizes.size() <= m_otherSplitterNumber)
		sizes.clear();

	return sizes;
}
