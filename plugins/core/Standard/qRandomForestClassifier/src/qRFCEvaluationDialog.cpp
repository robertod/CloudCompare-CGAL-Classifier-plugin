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

#include "qRFCEvaluationDialog.h"

EvaluationDialog::EvaluationDialog(std::string html, QWidget* parent)
	: QDialog(parent),
	m_closeButton(new QPushButton(tr("Close"), this)),
	m_textBrowser(new QTextBrowser(this)) {
	QVBoxLayout* layout = new QVBoxLayout(this);

	m_textBrowser->setOpenExternalLinks(true);
	m_textBrowser->setHtml(html.c_str());
	layout->addWidget(m_textBrowser);

	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	buttonLayout->addWidget(m_closeButton);
	layout->addLayout(buttonLayout);

	connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);

	setLayout(layout);
	setWindowFlags(Qt::Window);
	setWindowTitle(tr("Evaluation Dialog"));
	setWindowIcon(QIcon(":/CC/plugin/qRandomForestClassifier/images/icon_classify.png"));
	setMinimumSize(600, 400);
}
