/***************************************************************************

	profilelistitemmodel.h

	QAbstractItemModel implementation for the profile list

***************************************************************************/

#ifndef PROFILELISTITEMMODEL_H
#define PROFILELISTITEMMODEL_H

// bletchmame headers
#include "info.h"
#include "profile.h"

// Qt headers
#include <QAbstractItemModel>


QT_BEGIN_NAMESPACE
class QFileSystemWatcher;
QT_END_NAMESPACE

class IconLoader;
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
		
		Max = Path
	};

	// ctor
	ProfileListItemModel(QObject *parent, Preferences &prefs, info::database &infoDb, IconLoader &iconLoader);

	// methods
	void refresh(bool updateProfileList, bool updateFileSystemWatcher);
	void setOneTimeFswCallback(std::function<void()> &&fswCallback);
	std::shared_ptr<profiles::profile> getProfileByIndex(int index);
	QModelIndex findProfileIndex(const QString &path) const;

	// virtuals
	virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override;
	virtual QModelIndex parent(const QModelIndex &child) const override;
	virtual int rowCount(const QModelIndex &parent) const override;
	virtual int columnCount(const QModelIndex &parent) const override;
	virtual QVariant data(const QModelIndex &index, int role) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
	virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;

private:
	Preferences &					m_prefs;
	info::database &				m_infoDb;
	IconLoader &					m_iconLoader;
	QFileSystemWatcher &			m_fileSystemWatcher;
	std::vector<std::shared_ptr<profiles::profile>>	m_profiles;
	std::function<void()>			m_fswCallback;

	static bool setProfileName(const profiles::profile &profile, const QString &newName);

};

#endif // PROFILELISTITEMMODEL_H
