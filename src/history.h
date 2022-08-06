/***************************************************************************

	history.h

	Implements a database for history.xml files

***************************************************************************/

#pragma once

#ifndef HISTORY_H
#define HISTORY_H

// bletchmame headers
#include "identifier.h"

// Qt headers
#include <QIODevice>

// standard headers
#include <string>
#include <unordered_map>
#include <vector>


// ======================> HistoryDatabase

class HistoryDatabase
{
public:
	typedef std::unique_ptr<HistoryDatabase> ptr;
	class Test;

	HistoryDatabase() = default;
	HistoryDatabase(const HistoryDatabase &) = delete;
	HistoryDatabase(HistoryDatabase &&) = default;
	~HistoryDatabase() = default;

	HistoryDatabase &operator=(HistoryDatabase &&) = default;

	bool load(const QString &fileName);
	void clear();
	std::u8string_view get(const Identifier &identifier) const;
	QString getRichText(const Identifier &identifier) const;

private:
	std::vector<std::u8string>					m_texts;
	std::unordered_map<Identifier, std::size_t>	m_lookup;

	bool load(QIODevice &stream);
	static QString toRichText(std::u8string_view text);
};


#endif // HISTORY_H
