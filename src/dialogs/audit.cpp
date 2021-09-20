/***************************************************************************

	dialogs/audit.cpp

	Auditing Dialog

***************************************************************************/

#include "dialogs/audit.h"
#include "dialogs/audititemmodel.h"
#include "ui_audit.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

AuditDialog::AuditDialog(const Audit &audit, const QString &name, const QString &description, const QPixmap &pixmap, QWidget *parent)
	: QDialog(parent)
{
	// set up the UI
	m_ui = std::make_unique<Ui::AuditDialog>();
	m_ui->setupUi(this);

	// tweak the pixmap
	QPixmap newPixmap = pixmap.copy();
	setPixmapDevicePixelRatioToFit(newPixmap, 64);

	// set up the labels
	m_ui->nameLabel->setText(QString("\"%1\"").arg(name));
	m_ui->descriptionLabel->setText(description);
	m_ui->iconLabel->setPixmap(newPixmap);

	// set up the model
	AuditItemModel &model = *new AuditItemModel(audit, this);
	m_ui->tableView->setModel(&model);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

AuditDialog::~AuditDialog()
{
}


//-------------------------------------------------
//  singleMediaAudited
//-------------------------------------------------

void AuditDialog::singleMediaAudited(int entryIndex, const Audit::Verdict &verdict)
{
	AuditItemModel &model = *dynamic_cast<AuditItemModel *>(m_ui->tableView->model());
	model.singleMediaAudited(entryIndex, verdict);
}
