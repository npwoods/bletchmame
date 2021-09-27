/***************************************************************************

	dialogs/audit.h

	Auditing Dialog

***************************************************************************/

#ifndef DIALOGS_AUDIT_H
#define DIALOGS_AUDIT_H

#include <QDialog>

#include "info.h"
#include "../audit.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

QT_BEGIN_NAMESPACE
namespace Ui { class AuditDialog; }
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
	void singleMediaAudited(int entryIndex, const Audit::Verdict &verdict);

private:
	std::unique_ptr<Ui::AuditDialog> m_ui;
};


#endif // DIALOGS_AUDIT_H
