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

#include "qRFCClassifDialog.h"

//local
#include "qRFCFeatureDialog.h"
Q_DECLARE_METATYPE(Features::Source);

//qCC_plugins
#include <ccMainAppInterface.h>
#include <ccQtHelpers.h>

//qCC_db
#include <ccPointCloud.h>

//Qt
#include <QSettings>
#include <QMainWindow>
#include <QComboBox>
#include <QFileInfo>
#include <QFileDialog>
#include <QPushButton>
#include <QApplication>
#include <QThread>

qRFCClassifDialog::qRFCClassifDialog(ccPointCloud* cloud, ccMainAppInterface* app)
	: QDialog(app ? app->getMainWindow() : nullptr)
	, Ui::RFCClassifDialog()
	, m_app(app)
	, m_cloud(cloud)
{
	setupUi(this);

	//add the list of available regularizations
	{
		regTypeComboBox->clear();
		constexpr size_t count = Regularization::AvailableCount();
		for (size_t i = 0; i < count; ++i)
		{
			Regularization::Method type = Regularization::getID((unsigned int) i);
			std::string name = Regularization::getName(type);
			regTypeComboBox->addItem(name.c_str(), type);
		}
	}

	//add the list of available features
	m_featureModel = new QStandardItemModel(featureListView);
	featureListView->setModel(m_featureModel);
	{
		QString name = Features::getName(Features::POINTFEATURES).c_str();
		QStandardItem* item = new QStandardItem(name);
		item->setCheckable(true);
		item->setCheckState(Qt::Checked);
		item->setData(QVariant(Features::Source::CGAL_GENERATED_FEATURE));
		m_featureModel->appendRow(item);
	}
	if (cloud->hasNormals()) {
		QString name = Features::getName(Features::POINTNORMALS).c_str();
		QStandardItem* item = new QStandardItem(name);
		item->setCheckable(true);
		item->setData(QVariant(Features::Source::CC_NORMALS_FIELD));
		m_featureModel->appendRow(item);
	}
	if (cloud->hasColors()) {
		QString name = Features::getName(Features::POINTCOLORS).c_str();
		QStandardItem* item = new QStandardItem(name);
		item->setCheckable(true);
		item->setData(QVariant(Features::Source::CC_COLOR_FIELD));
		m_featureModel->appendRow(item);
	}
	if (cloud->hasScalarFields()) {
		QSettings settings("qRandomForestClassifier");
		settings.beginGroup("Shared");
		qRFCTools::FeatureList features = qRFCTools::variantToFeatureList(settings.value("Features", qRFCTools::featureListToVariant(getFeatureList())).value<QVariantList>());
		settings.endGroup();
		for (const auto& feature : features) {
			if (feature.data.value<Features::Source>() == Features::Source::CC_SCALAR_FIELD)
			{
				// check if exists in point cloud
				QString featureSFName = feature.name;
				if (cloud->getScalarFieldIndexByName(featureSFName.toStdString().c_str()) != -1) {
					QStandardItem* item = new QStandardItem(featureSFName);
					item->setCheckable(true);
					item->setCheckState((feature.checked) ? Qt::Checked : Qt::Unchecked);
					item->setData(feature.data);
					m_featureModel->appendRow(item);
				}
			}
		}
	}

	loadParamsFromPersistentSettings();

	connect(browseToolButton, &QAbstractButton::clicked, this, &qRFCClassifDialog::browseClassifierFile);

	connect(classesLineEdit, &QLineEdit::textChanged, this, &qRFCClassifDialog::onClassIndicesChanged);

	connect(m_featureModel, &QStandardItemModel::dataChanged, this, &qRFCClassifDialog::onFeatureCheckStateChanged);

	connect(addPushButton, &QPushButton::clicked, this, &qRFCClassifDialog::onAddFeatureClicked);
	connect(removePushButton, &QPushButton::clicked, this, &qRFCClassifDialog::onRemoveFeatureClicked);

	connect(evaluateCheckBox, &QCheckBox::toggled, this, &qRFCClassifDialog::onEvaluateCheckStateChanged);
	connect(scalarFieldComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &qRFCClassifDialog::onScalarFieldChanged);

	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters());
}

bool qRFCClassifDialog::validParameters(const bool showAdvice) const {
	std::cout << "validating" << std::endl;
	QString filePath = getClassifFilePath();
	if (filePath.isEmpty()) {
		if (m_app && showAdvice)
			m_app->dispToConsole("Classifier file was not specified.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return false;
	}

	QFileInfo fileinfo(filePath);
	if (!fileinfo.exists()) {
		if (m_app && showAdvice)
			m_app->dispToConsole("Classifier file specified does not exist.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return false;
	}
	if (!fileinfo.isFile()) {
		if (m_app && showAdvice)
			m_app->dispToConsole("Object specified is not a file.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return false;
	}

	auto features = getFeatures();
	if (features.size() == 0) {
		if (m_app && showAdvice)
			m_app->dispToConsole("No features selected.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return false;
	}

	for (const auto& feature : features) {
		if (feature.first != Features::Source::CC_SCALAR_FIELD) continue;
		if (feature.second != nullptr)
			std::cout << "feature: " << feature.second->getName() << std::endl;
		else std::cout << "feature: null" << std::endl;
		if (feature.second == nullptr) {
			if (m_app && showAdvice)
				m_app->dispToConsole("One or more selected features does not exist in this point cloud.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return false;
		}
	}

	if (!validClassIndices()) {
		if (m_app && showAdvice)
			m_app->dispToConsole("Invalid class labels specified.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return false;
	}

	if (evaluateCheckBox->isChecked()) {
		std::cout << "scalarFieldComboBox->currentIndex() = " << scalarFieldComboBox->currentIndex() << std::endl;
		int sfIdx = scalarFieldComboBox->currentIndex();
		if (sfIdx == -1) {
			if (m_app && showAdvice)
				m_app->dispToConsole("Label scalar field not specified.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return false;
		}
		CCCoreLib::ScalarField* SF = m_cloud->getScalarField(sfIdx);

		if (!qRFCTools::validScalarField(SF)) {
			if (m_app && showAdvice)
				m_app->dispToConsole("Invalid label scalar field specified. Detected non-integer values.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return false;
		}
		std::vector<int> classIndices = getClassIndices();
		int maxClassIndex = *std::max_element(classIndices.begin(), classIndices.end());
		SF->computeMinAndMax();
		int maxLabelValue = SF->getMax();
		if (maxLabelValue > maxClassIndex) {
			if (m_app && showAdvice)
				m_app->dispToConsole("Invalid label scalar field specified. Detected a label value that is larger than the largest class index.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return false;
		}
	}

	return true;
}

void qRFCClassifDialog::onEvaluateCheckStateChanged(bool checked)
{
	scalarFieldComboBox->clear();

	if (checked) {
		// get list of scalar fields of current point cloud
		if (!m_cloud->hasScalarFields() && m_app) {
			m_app->dispToConsole("Point cloud has no scalar fields associated.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return;
		}

		int nSF = m_cloud->getNumberOfScalarFields();
		for (int i = 0; i < nSF; ++i) {
			scalarFieldComboBox->addItem(m_cloud->getScalarFieldName(i).c_str(), i);
		}

		scalarFieldComboBox->setCurrentIndex(-1);
		scalarFieldComboBox->setEnabled(true);
	}
	else {
		scalarFieldComboBox->setEnabled(false);
	}
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters());
}

void qRFCClassifDialog::onScalarFieldChanged(int idx) {
	if (idx >= 0 && scalarFieldComboBox->isEnabled()) {
		buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters(true));
	}
}

CCCoreLib::ScalarField* qRFCClassifDialog::getEvaluationLabels() const {
	if (!evaluateCheckBox->isChecked()) return nullptr;
	int sfIdx = scalarFieldComboBox->currentIndex();
	if (sfIdx == -1) {
		if (m_app)
			m_app->dispToConsole("Label scalar field not specified.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return nullptr;
	}
	CCCoreLib::ScalarField* SF = m_cloud->getScalarField(sfIdx);
	return SF;
}

void qRFCClassifDialog::onFeatureCheckStateChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles) {
	if (roles.contains(Qt::CheckStateRole)) {
		QStandardItem* item = m_featureModel->itemFromIndex(topLeft);
		if (item)
			buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters(true));
	}
}

void qRFCClassifDialog::onAddFeatureClicked() {
	QStringList items;

	if (!m_cloud->hasScalarFields()) {
		return;
	}

	std::set<QString> existingFeatures;
	for (int i = 0; i < m_featureModel->rowCount(); ++i) {
		QStandardItem* item = m_featureModel->item(i);
		existingFeatures.insert(item->text());
	}

	for (int i = 0; i < m_cloud->getNumberOfScalarFields(); ++i) {
		CCCoreLib::ScalarField* SF = m_cloud->getScalarField(i);
		QString SFName = m_cloud->getScalarField(i)->getName().c_str();
		if (existingFeatures.find(SFName) == existingFeatures.end()) // only include features that have not yet been added
			items << SFName;
	}

	FeatureDialog dialog(items, this);
	if (dialog.exec() == QDialog::Accepted) {
		QStringList selections = dialog.getSelections();
		if (selections.size() > 0) {
			// add selected features to ListView
			for (int i = 0; i < selections.size(); ++i) {
				const QString name = selections[i];
				QStandardItem* item = new QStandardItem(name);
				item->setCheckable(true);
				item->setCheckState(Qt::Checked);
				item->setData(QVariant(Features::Source::CC_SCALAR_FIELD));
				m_featureModel->appendRow(item);
			}
		}
	}
}
void qRFCClassifDialog::onRemoveFeatureClicked() {
	const QModelIndex& index = featureListView->currentIndex();

	if (!index.isValid()) return;

	// Get the item from the m_model
	QStandardItem* item = m_featureModel->itemFromIndex(index);
	if (!item) return;

	if (item->data().value<Features::Source>() == Features::Source::CC_SCALAR_FIELD)
		m_featureModel->removeRow(index.row());
}

double qRFCClassifDialog::getMinScale() const
{
	double minScale = minScaleDoubleSpinBox->value();
	if (minScale <= 0) {
		minScale = -1.f;
	}
	return minScale;

}
int qRFCClassifDialog::getNScale() const
{
	int nScales = numberScalesSpinBox->value();
	return nScales;
}
int qRFCClassifDialog::getNClasses() const
{
	int nClasses = numberClassesSpinBox->value();
	return nClasses;
}
bool qRFCClassifDialog::validClassIndices() const {
	std::vector<int> classIndices = getClassIndices();
	std::sort(classIndices.begin(), classIndices.end());

	if (classIndices[0] < -1) return false; // -1 is for unlabelled points

	// all labels should sequential, otherwise we will fail the labels.is_valid_ground_truth(...) check in the CGAL classifier
	for (size_t i = 1; i < classIndices.size(); ++i) {
		int diff = classIndices[i] - classIndices[i - 1];
		if (diff > 1) return false;
	}
	return true;
}
std::vector<int> qRFCClassifDialog::getClassIndices() const {
	std::vector<int> classIndices;
	QStringList classesStringList = classesLineEdit->text().split(' ', QString::SkipEmptyParts);

	int listSize = classesStringList.size();
	classIndices.resize(listSize);
	for (int i = 0; i < listSize; ++i)
	{
		bool ok = false;
		int f;
		f = classesStringList[i].toInt(&ok);
		if (!ok)
			return {};
		classIndices[i] = f;
	}
	return classIndices;
}
void qRFCClassifDialog::onClassIndicesChanged() {
	numberClassesSpinBox->setValue((int) getClassIndices().size());
}
bool qRFCClassifDialog::getExportFeatures() const {
	bool exportFeatures = exportFeaturesCheckBox->isChecked();
	return exportFeatures;
}
QString qRFCClassifDialog::getClassifFilePath() const
{
	return classifFileLineEdit->text();
}
int qRFCClassifDialog::getRegType() const
{
	return regTypeComboBox->currentIndex();
}
std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > qRFCClassifDialog::getFeatures() const {
	std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > features;

	for (int i = 0; i < m_featureModel->rowCount(); ++i) {
		QStandardItem* item = m_featureModel->item(i);
		if (item->checkState() == Qt::Unchecked)
			continue;

		QVariant QVSource = item->data();

		Features::Source source = QVSource.value<Features::Source>();
		QString name = item->text();

		if (source == Features::Source::CC_SCALAR_FIELD) {
			int sfIdx = m_cloud->getScalarFieldIndexByName(name.toStdString().c_str());
			CCCoreLib::ScalarField* SF = m_cloud->getScalarField(sfIdx);
			features.push_back({ source,SF });
		}
		else {
			features.push_back({ source, nullptr });
		}
	}

	return features;
}
qRFCTools::FeatureList qRFCClassifDialog::getFeatureList() const {
	qRFCTools::FeatureList features;
	if (m_featureModel == nullptr) return features;
	for (int i = 0; i < m_featureModel->rowCount(); ++i) {
		QStandardItem* item = m_featureModel->item(i);
		QString text = item->text();
		QVariant data = item->data();
		Qt::CheckState checked = item->checkState();
		features.push_back({ text,data,checked });
	}
	return features;
}
void qRFCClassifDialog::setFeatureList(qRFCTools::FeatureList features) {
	if (m_featureModel == nullptr) return;
	m_featureModel->clear();
	for (const auto& feature : features) {
		QStandardItem* item = new QStandardItem(feature.name);
		item->setCheckable(true);
		item->setCheckState(feature.checked);
		item->setData(feature.data);
		m_featureModel->appendRow(item);
	}
}

void qRFCClassifDialog::loadParamsFromPersistentSettings()
{
	QSettings settings("qRandomForestClassifier");

	settings.beginGroup("Shared");
	double minScale = settings.value("MinScale", minScaleDoubleSpinBox->value()).toDouble();
	int nScales = settings.value("NScales", numberScalesSpinBox->value()).toInt();
	int nClasses = settings.value("NClasses", numberClassesSpinBox->value()).toInt();
	QString classesList = settings.value("ClassesList", classesLineEdit->text()).toString();
	qRFCTools::FeatureList features = qRFCTools::variantToFeatureList(settings.value("Features", qRFCTools::featureListToVariant(getFeatureList())).value<QVariantList>());
	settings.endGroup();

	settings.beginGroup("Classif");
	QString currentPath = settings.value("CurrentFilePath", QApplication::applicationDirPath()).toString();
	bool useConfThreshold = settings.value("UseConfThreshold", useConfThresholdGroupBox->isChecked()).toBool();
	int regParam = settings.value("RegParam", regTypeComboBox->currentIndex()).toInt();
	bool exportFeatures = settings.value("ExportFeatures", exportFeaturesCheckBox->isChecked()).toBool();
	settings.endGroup();

	//apply parameters
	minScaleDoubleSpinBox->setValue(minScale);
	numberScalesSpinBox->setValue(nScales);
	numberClassesSpinBox->setValue(nClasses);
	classesLineEdit->setText(classesList);
	classifFileLineEdit->setText(currentPath);
	useConfThresholdGroupBox->setChecked(useConfThreshold);
	regTypeComboBox->setCurrentIndex(regParam);
	setFeatureList(features);
	exportFeaturesCheckBox->setChecked(exportFeatures);

	loadClassifierFile(classifFileLineEdit->text());
}

void qRFCClassifDialog::saveParamsToPersistentSettings()
{
	QSettings settings("qRandomForestClassifier");

	//save parameters
	settings.beginGroup("Shared");
	settings.setValue("MinScale", minScaleDoubleSpinBox->value());
	settings.setValue("NScales", numberScalesSpinBox->value());
	settings.setValue("NClasses", numberClassesSpinBox->value());
	settings.setValue("ClassesList", classesLineEdit->text());
	settings.setValue("Features", qRFCTools::featureListToVariant(getFeatureList()));
	settings.endGroup();

	settings.beginGroup("Classif");
	QString currentFilePath = QFileInfo(classifFileLineEdit->text()).absoluteFilePath();
	settings.setValue("CurrentFilePath", currentFilePath);
	settings.setValue("UseConfThreshold", useConfThresholdGroupBox->isChecked());
	settings.setValue("RegParam", regTypeComboBox->currentIndex());
	settings.setValue("ExportFeatures", exportFeaturesCheckBox->isChecked());
	settings.endGroup();
}

void qRFCClassifDialog::browseClassifierFile()
{
	//select file to open
	QString filename;
	QSettings settings("qRandomForestClassifier");
	settings.beginGroup("Classif");
	QString currentPath = settings.value("CurrentFilePath", classifFileLineEdit->text()).toString();

	filename = QFileDialog::getOpenFileName(this, "Load classifier file", currentPath, "ETHZ Random Forest Classifier (*.bin);;All Files (*)");
	if (filename.isEmpty())
		return;

	loadClassifierFile(filename);
}
void qRFCClassifDialog::loadClassifierFile(QString filename) {
	if (filename.isEmpty())
		return;

	QFileInfo fileinfo(filename);
	if (!fileinfo.exists())
		return;

	QSettings settings("qRandomForestClassifier");
	settings.beginGroup("Classif");

	//we update current file path
	classifFileLineEdit->setText(filename);
	QString currentFilePath = QFileInfo(filename).absoluteFilePath();
	settings.setValue("CurrentFilePath", currentFilePath);
	Classifier classifier;
	QStringList fileDesc = classifier.readETHZRandomForestClassifierData(currentFilePath);//(filename);
	classifInfoTextEdit->setText(fileDesc.join("\n"));

	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters());
}
