/***************************************************************************

	dialogs/audit.cpp

	Auditing Dialog

***************************************************************************/

#include <QProgressBar>

#include "dialogs/audit.h"
#include "dialogs/audititemmodel.h"
#include "ui_audit.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

AuditDialog::AuditDialog(const Audit &audit, const QString &name, const QString &description, const QPixmap &pixmap, IconLoader &iconLoader, QWidget *parent)
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
	AuditItemModel &model = *new AuditItemModel(audit, iconLoader, this);
	m_ui->tableView->setModel(&model);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

AuditDialog::~AuditDialog()
{
}


//-------------------------------------------------
//  auditProgress
//-------------------------------------------------

void AuditDialog::auditProgress(int entryIndex, std::uint64_t bytesProcessed, std::uint64_t totalBytes, const std::optional<Audit::Verdict> &verdict)
{
	// destroy the existing progress bar if we have to
	if (m_activeProgressBar.has_value() && (verdict.has_value() || m_activeProgressBar.value().entryIndex() != entryIndex))
	{
		QModelIndex index = modelIndexForProgressBar(m_activeProgressBar->entryIndex());
		m_ui->tableView->setIndexWidget(index, nullptr);
		m_activeProgressBar.reset();
	}

	// and set up a progress bar (if we have to)
	if (!verdict.has_value())
	{
		if (!m_activeProgressBar.has_value())
		{
			// there is no active progress bar; we have to set one up
			m_activeProgressBar.emplace(entryIndex);

			// and set it on the table view
			QModelIndex index = modelIndexForProgressBar(entryIndex);
			m_ui->tableView->setIndexWidget(index, &m_activeProgressBar->widget());
		}

		// and update the bar
		m_activeProgressBar->update(bytesProcessed, totalBytes);
	}

	if (verdict.has_value())
	{
		AuditItemModel &model = *dynamic_cast<AuditItemModel *>(m_ui->tableView->model());
		model.singleMediaAudited(entryIndex, verdict.value());
	}
}


//-------------------------------------------------
//  modelIndexForProgressBar
//-------------------------------------------------

QModelIndex AuditDialog::modelIndexForProgressBar(int entryIndex)
{
	const int COLUMN = (int)AuditItemModel::Column::Status;

	return m_ui->tableView->model()->index(
		entryIndex,
		COLUMN);
}


//-------------------------------------------------
//  ActiveProgressBar ctor
//-------------------------------------------------

AuditDialog::ActiveProgressBar::ActiveProgressBar(int entryIndex)
	: m_progressBar(*new QProgressBar)
	, m_entryIndex(entryIndex)
{
	m_progressBar.setMinimum(0);
	m_progressBar.setMaximum(100);
}


//-------------------------------------------------
//  ActiveProgressBar::update
//-------------------------------------------------

void AuditDialog::ActiveProgressBar::update(std::uint64_t bytesProcessed, std::uint64_t totalBytes)
{
	m_progressBar.setValue((int)(bytesProcessed * 100.0 / totalBytes));
}


//-------------------------------------------------
//  ActiveProgressBar::widget
//-------------------------------------------------

QWidget &AuditDialog::ActiveProgressBar::widget()
{
	return m_progressBar;
}


//-------------------------------------------------
//  ActiveProgressBar::entryIndex
//-------------------------------------------------

int AuditDialog::ActiveProgressBar::entryIndex() const
{
	return m_entryIndex;
}
