/***************************************************************************

	splitterviewtoggler.h

	UI glue to facilitate QAbstractButtons toggling splitter views

***************************************************************************/

#ifndef SPLITTERVIEWTOGGLER_H
#define SPLITTERVIEWTOGGLER_H

// Qt headers
#include <QAbstractButton>
#include <QSplitter>

// standard headers
#include <optional>


// ======================> MachineFolderTreeModel

class SplitterViewToggler : public QObject
{
public:
	SplitterViewToggler(QObject *parent, QAbstractButton &button, QSplitter &splitter, int ownedSplitterNumber, int otherSplitterNumber, std::function<void()> &&changedCallback);

private:
	QAbstractButton &		m_button;
	QSplitter &				m_splitter;
	int						m_ownedSplitterNumber;
	int						m_otherSplitterNumber;
	int						m_lastWidth;
	std::function<void()>	m_changedCallback;

	void buttonClicked(bool checked);
	void updateChecked();
	QList<int> getSplitterSizes() const;
};

#endif // SPLITTERVIEWTOGGLER_H
