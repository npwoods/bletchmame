/***************************************************************************

	dialogs/audit.h

	Auditing Dialog

***************************************************************************/

#ifndef DIALOGS_AUDIT_H
#define DIALOGS_AUDIT_H

// bletchmame headers
#include "info.h"
#include "../audit.h"

// Qt headers
#include <QDialog>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

QT_BEGIN_NAMESPACE
namespace Ui { class AuditDialog; }
class QProgressBar;
QT_END_NAMESPACE

class IconLoader;

// ======================> AuditDialog

class AuditDialog : public QDialog
{
	Q_OBJECT

public:
	// ctor/dtor
	AuditDialog(const Audit &audit, const QString &name, const QString &description, const QPixmap &pixmap, IconLoader &iconLoader, QWidget *parent = nullptr);
	~AuditDialog();

	// methods
	void auditProgress(int entryIndex, std::uint64_t bytesProcessed, std::uint64_t totalBytes, const std::optional<Audit::Verdict> &verdict);

private:
	class ActiveProgressBar
	{
	public:
		ActiveProgressBar(int entryIndex);

		// methods
		QWidget &widget();
		int entryIndex() const;
		void update(std::uint64_t bytesProcessed, std::uint64_t totalBytes);

	private:
		QProgressBar &	m_progressBar;
		int				m_entryIndex;
	};

	std::unique_ptr<Ui::AuditDialog>	m_ui;
	std::optional<ActiveProgressBar>	m_activeProgressBar;

	QModelIndex modelIndexForProgressBar(int entryIndex);
};


#endif // DIALOGS_AUDIT_H
