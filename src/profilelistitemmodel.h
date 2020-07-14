/***************************************************************************

	profilelistitemmodel.h

	QAbstractItemModel implementation for the profile list

***************************************************************************/

#ifndef PROFILELISTITEMMODEL_H
#define PROFILELISTITEMMODEL_H

#include <QAbstractItemModel>

#include "info.h"
#include "profile.h"

QT_BEGIN_NAMESPACE
class QFileSystemWatcher;
QT_END_NAMESPACE

class Preferences;


// ======================> MachineListItemModel

class ProfileListItemModel : public QAbstractItemModel
{
public:
	enum class Column
	{
		Name,
		Machine,
		Path,
		Count
	};

	// ctor
	ProfileListItemModel(QObject *parent, Preferences &prefs);

	// methods
	void refresh(bool updateProfileList, bool updateFileSystemWatcher);
	void setOneTimeFswCallback(std::function<void()> &&fswCallback);
	const profiles::profile &getProfileByIndex(int index) const;
	QModelIndex findProfileIndex(const QString &path) const;

	// virtuals
	virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override;
	virtual QModelIndex parent(const QModelIndex &child) const override;
	virtual int rowCount(const QModelIndex &parent) const override;
	virtual int columnCount(const QModelIndex &parent) const override;
	virtual QVariant data(const QModelIndex &index, int role) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
	Preferences &					m_prefs;
	QFileSystemWatcher &			m_fileSystemWatcher;
	std::vector<profiles::profile>	m_profiles;
	std::function<void()>			m_fswCallback;

};

#endif // PROFILELISTITEMMODEL_H
