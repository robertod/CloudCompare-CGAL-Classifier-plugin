//##########################################################################
//#                                                                        #
//#       CLOUDCOMPARE PLUGIN: Random Forest Classification Plugin         #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#                           COPYRIGHT: Inria                             #
//#                                                                        #
//##########################################################################

#include "qRFCFeatureDialog.h"

FeatureDialog::FeatureDialog(QStringList items, QWidget* parent)
	: QDialog(parent),
	m_listView(new QListView(this)),
	m_model(new QStandardItemModel(this)),
	m_okButton(new QPushButton(tr("OK"), this)),
	m_cancelButton(new QPushButton(tr("Cancel"), this)),
	m_checkAllBox(new QCheckBox(tr("Select All"), this)) {
	QVBoxLayout* layout = new QVBoxLayout(this);

	layout->addWidget(m_checkAllBox);

	// Populate the list view with checkable items
	for (const QString& item : items) {
		QStandardItem* listItem = new QStandardItem(item);
		listItem->setCheckable(true);
		m_model->appendRow(listItem);
	}

	m_listView->setModel(m_model);
	layout->addWidget(m_listView);

	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	buttonLayout->addWidget(m_okButton);
	buttonLayout->addWidget(m_cancelButton);
	layout->addLayout(buttonLayout);

	connect(m_checkAllBox, &QCheckBox::toggled, this, &FeatureDialog::onCheckAllToggled);
	connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
	connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

	setLayout(layout);
	setWindowTitle(tr("Feature Dialog"));
	setWindowIcon(QIcon(":/CC/plugin/qRandomForestClassifier/images/icon.png"));
}

void FeatureDialog::onCheckAllToggled(bool checked) {
	for (int i = 0; i < m_model->rowCount(); ++i) {
		QStandardItem* item = m_model->item(i);
		item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
	}
}

QStringList FeatureDialog::getSelections() const {
	QStringList selections;
	for (int i = 0; i < m_model->rowCount(); ++i) {
		QStandardItem* item = m_model->item(i);
		if (item->checkState() == Qt::Checked) {
			selections << item->text();
		}
	}
	return selections;
}
