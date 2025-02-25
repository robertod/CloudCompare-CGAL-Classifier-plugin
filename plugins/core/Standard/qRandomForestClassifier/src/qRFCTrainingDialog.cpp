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

#include "qRFCTrainingDialog.h"

//local
#include "qRFCTools.h"
#include "qRFCFeatureDialog.h"
Q_DECLARE_METATYPE(Features::Source);
Q_DECLARE_METATYPE(qRFCTools::FeatureList);

//qCC_plugins
#include <ccMainAppInterface.h>
#include <ccQtHelpers.h>

//Qt
#include <QSettings>
#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QApplication>
#include <QThread>
#include <QTimer>
#include <QStandardItem>
#include <QInputDialog>

//system
#include <limits>

qRFCTrainingDialog::qRFCTrainingDialog(ccMainAppInterface* app)
	: QDialog(app ? app->getMainWindow() : nullptr)
	, Ui::RFCTrainingDialog()
	, m_app(app)
	, m_featureModel(nullptr)
{
	setupUi(this);

	if (m_app)
	{
		//add list of clouds to the combo-boxes
		ccHObject::Container clouds;
		if (m_app->dbRootObject())
		{
			m_app->dbRootObject()->filterChildren(clouds, true, CC_TYPES::POINT_CLOUD);
		}
		else
		{
			m_app->dispToConsole("dbRootObject does not exist", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			QTimer::singleShot(0, this, &QDialog::reject); // trigger close after construction
			return;
		}

		unsigned cloudCount = 0;
		for (ccHObject* cloud : clouds)
		{
			if (cloud->isA(CC_TYPES::POINT_CLOUD))
			{
				QString name = qRFCTools::GetEntityName(cloud);
				QVariant uniqueID(cloud->getUniqueID());

				ccPointCloud* cloudSFCheck = static_cast<ccPointCloud*>(cloud);
				unsigned nSF = cloudSFCheck->getNumberOfScalarFields();
				for (int i = 0; i < cloudSFCheck->getNumberOfScalarFields(); ++i)
				{
					std::string SFName = cloudSFCheck->getScalarField(i)->getName();
					// ignore common scalar field names that are associated with training labels/classifier results
					if (SFName.rfind("label", 0) == 0 || SFName.rfind("Classification", 0) == 0)
					{
						nSF--;
					}
				}
				std::cout << "# SF = " << nSF << std::endl;

				class1CloudComboBox->addItem(name, uniqueID);
				class2CloudComboBox->addItem(name, uniqueID);
				evaluationCloudComboBox->addItem(name, uniqueID);
				if (cloudSFCheck->getNumberOfScalarFields() > 0)
					cloudComboBox->addItem(name, uniqueID);

				m_clouds.push_back(cloud);
				++cloudCount;
			}
		}

		//if 3 clouds are loaded, then there's chances that the first one is the global cloud!
		class1CloudComboBox->setCurrentIndex(cloudCount > 0 ? (cloudCount > 2 ? 1 : 0) : -1);
		class2CloudComboBox->setCurrentIndex(cloudCount > 1 ? (cloudCount > 2 ? 2 : 1) : -1);
		cloudComboBox->setCurrentIndex(cloudComboBox->count() > 0 ? 0 : -1);
		//scalarFieldComboBox->setCurrentIndex(-1);

		if (cloudCount >= 1 && cloudComboBox->currentIndex() >= 0 && m_clouds.at(cloudComboBox->currentIndex())->hasScalarFields())
		{
			onScalarFieldsEnabled();
			onScalarFieldChanged(scalarFieldComboBox->currentIndex());
		}
		else if (cloudCount >= 2)
		{
			onPointCloudsEnabled();
		}
		else if (m_app)
		{
			m_app->dispToConsole("You need either 1 cloud with an integer scalar field, or 2 loaded clouds to train a classifier (one per class).", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			QTimer::singleShot(0, this, &QDialog::reject); // trigger close after construction
			return;
		}
	}

	// initialize feature list view
	m_featureModel = new QStandardItemModel(featureListView);
	featureListView->setModel(m_featureModel);

	loadParamsFromPersistentSettings();

	onPointCloudChanged(cloudComboBox->currentIndex());

	connect(cloud1ClassSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, &qRFCTrainingDialog::onClassChanged);
	connect(cloud2ClassSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, &qRFCTrainingDialog::onClassChanged);
	
	connect(class1CloudComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &qRFCTrainingDialog::onCloudChanged);
	connect(class2CloudComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &qRFCTrainingDialog::onCloudChanged);

	onClassChanged(0);
	onCloudChanged(0);

	connect(usePointCloudsRadioButton, &QRadioButton::clicked, this, &qRFCTrainingDialog::onPointCloudsEnabled);
	connect(useScalarFieldsRadioButton, &QRadioButton::clicked, this, &qRFCTrainingDialog::onScalarFieldsEnabled);

	connect(cloudComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &qRFCTrainingDialog::onPointCloudChanged);
	connect(scalarFieldComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &qRFCTrainingDialog::onScalarFieldChanged);

	connect(classesLineEdit, &QLineEdit::textChanged, this, &qRFCTrainingDialog::onClassIndicesChanged);

	connect(m_featureModel, &QStandardItemModel::dataChanged, this, &qRFCTrainingDialog::onFeatureCheckStateChanged);

	connect(addPushButton, &QPushButton::clicked, this, &qRFCTrainingDialog::onAddFeatureClicked);
	connect(removePushButton, &QPushButton::clicked, this, &qRFCTrainingDialog::onRemoveFeatureClicked);
}
void qRFCTrainingDialog::onFeatureCheckStateChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
{
	if (roles.contains(Qt::CheckStateRole))
	{
		QStandardItem* item = m_featureModel->itemFromIndex(topLeft);
		if (item)
		{
			buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters());

			const QString text = item->text();

			qRFCTools::FeatureList features = getFeatureList();
			for (auto& feature : features)
			{
				// check if name matches scalarFieldSelected
				const QString featureSFName = feature.name;
				if (text == featureSFName)
					feature.checked = item->checkState();
			}
		}
	}
}
std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > qRFCTrainingDialog::getFeatures(ccPointCloud* cloud) const
{
	std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > features;

	for (int i = 0; i < m_featureModel->rowCount(); ++i)
	{
		QStandardItem* item = m_featureModel->item(i);
		if (item->checkState() == Qt::Unchecked)
			continue;

		QVariant QVSource = item->data();

		Features::Source source = QVSource.value<Features::Source>();
		QString name = item->text();

		if (QVSource == Features::Source::CC_SCALAR_FIELD)
		{
			int sfIdx = cloud->getScalarFieldIndexByName(name.toStdString().c_str());
			CCCoreLib::ScalarField* SF = cloud->getScalarField(sfIdx);
			features.push_back({ source,SF });
		}
		else
		{
			features.push_back({ source, nullptr });
		}
	}

	return features;
}
void qRFCTrainingDialog::onAddFeatureClicked()
{
	ccPointCloud* cloud = qRFCTools::GetCloudFromCombo(cloudComboBox, m_app->dbRootObject());

	QStringList items;

	if (!cloud->hasScalarFields())
	{
		return;
	}

	std::set<QString> existingFeatures;
	for (int i = 0; i < m_featureModel->rowCount(); ++i)
	{
		QStandardItem* item = m_featureModel->item(i);
		existingFeatures.insert(item->text());
	}

	for (int i = 0; i < cloud->getNumberOfScalarFields(); ++i)
	{
		CCCoreLib::ScalarField* SF = cloud->getScalarField(i);
		QString SFName = cloud->getScalarField(i)->getName().c_str();
		if (existingFeatures.find(SFName) == existingFeatures.end()) // only include features that have not yet been added
			items << SFName;
	}

	FeatureDialog dialog(items, this);
	if (dialog.exec() == QDialog::Accepted)
	{
		QStringList selections = dialog.getSelections();
		if (selections.size() > 0)
		{
			// add selected features to ListView
			for (int i = 0; i < selections.size(); ++i)
			{
				const QString name = selections[i];
				QStandardItem* item = new QStandardItem(name);
				item->setCheckable(true);
				item->setCheckState(Qt::Checked);
				item->setData(QVariant(Features::Source::CC_SCALAR_FIELD));
				m_featureModel->appendRow(item);
			}
		}
	}

	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters());
}
void qRFCTrainingDialog::onRemoveFeatureClicked()
{
	const QModelIndex& index = featureListView->currentIndex();

	if (!index.isValid()) return;

	// Get the item from the m_model
	QStandardItem* item = m_featureModel->itemFromIndex(index);
	if (!item) return;

	// Only remove scalar fields
	if (item->data().value<Features::Source>() == Features::Source::CC_SCALAR_FIELD)
		m_featureModel->removeRow(index.row());

	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters());
}
bool qRFCTrainingDialog::validParameters(const bool showAdvice) const
{
	if (usePointCloudsRadioButton->isChecked())
	{
		if (cloud1ClassSpinBox->value() == cloud2ClassSpinBox->value())
			return false;

		int c1 = class1CloudComboBox->currentIndex();
		int c2 = class2CloudComboBox->currentIndex();
		if (c1 < 0 || c2 < 0)
		{
			if (m_app && showAdvice)
				m_app->dispToConsole("No point cloud(s) selected.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return false;
		}
		if (c1 == c2)
		{
			if (m_app && showAdvice)
				m_app->dispToConsole("Same point cloud selected twice.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return false;
		}
	}
	else if (useScalarFieldsRadioButton->isChecked())
	{
		int c1 = cloudComboBox->currentIndex();
		if (c1 < 0)
		{
			if (m_app && showAdvice)
				m_app->dispToConsole("No point cloud selected.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return false;
		}

		if (c1 >= m_clouds.size())
		{
			if (m_app && showAdvice)
				m_app->dispToConsole("Invalid point cloud selected.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return false;
		}
		if (!m_clouds.at(c1)->hasScalarFields())
		{
			if (m_app && showAdvice)
				m_app->dispToConsole("Point cloud has no scalar fields.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return false;
		}

		if (getNClasses() < 2)
		{
			if (m_app && showAdvice)
				m_app->dispToConsole("Not enough classes, at least two class labels must be defined.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return false;
		}

		if (!validClassIndices())
		{
			if (m_app && showAdvice)
				m_app->dispToConsole("Please make sure that the class labels are >= -1, and the labels are sequential (e.g., 0,1,2,3).", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return false;
		}

		// check that the class labels is not a scalar field used as a feature
		ccPointCloud* cloud = qRFCTools::GetCloudFromCombo(cloudComboBox, m_app->dbRootObject());
		if (cloud->hasScalarFields())
		{
			int sfIdx = scalarFieldComboBox->currentIndex();
			QString labelSFName = cloud->getScalarFieldName(sfIdx).c_str();
			qRFCTools::FeatureList features = getFeatureList();
			for (const auto& feature : features)
			{
				if (feature.data.value<Features::Source>() == Features::Source::CC_SCALAR_FIELD)
				{
					// check if name matches scalarFieldSelected
					QString featureSFName = feature.name;
					if (featureSFName == labelSFName && feature.checked == Qt::Checked) {
						if (m_app && showAdvice)
							m_app->dispToConsole("A class label scalar field cannot be also used as a feature.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
						return false;
					}
				}
			}
		}
		else
		{
			if (m_app && showAdvice)
				m_app->dispToConsole("Point cloud has no scalar fields. Pointwise class labels are required.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return false;
		}
	}

	return true;
}
double qRFCTrainingDialog::getMinScale() const
{
	double minScale = minScaleDoubleSpinBox->value();
	if (minScale <= 0)
	{
		minScale = -1.f;
	}
	return minScale;
}
int qRFCTrainingDialog::getNScale() const
{
	int nScales = numberScalesSpinBox->value();
	return nScales;
}
bool qRFCTrainingDialog::getEvaluateParams() const
{
	bool evaluateParams = evaluateParamsCheckBox->isChecked();
	return evaluateParams;
}
int qRFCTrainingDialog::getNClasses() const
{
	int nClasses = numberClassesSpinBox->value();
	return nClasses;
}
bool qRFCTrainingDialog::validClassIndices() const
{
	std::vector<int> classIndices = getClassIndices();
	std::sort(classIndices.begin(), classIndices.end());

	if (classIndices[0] < -1) return false; // -1 is for unlabelled points

	// all labels should sequential, otherwise we will fail the labels.is_valid_ground_truth(...) check in the CGAL classifier
	for (size_t i = 1; i < classIndices.size(); ++i) {
		int diff = classIndices[i] - classIndices[i-1];
		if (diff > 1) return false;
	}
	return true;
}
std::vector<int> qRFCTrainingDialog::getClassIndices() const
{
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
		if (f == -1) continue; // skip -1
		classIndices[i] = f;
	}
	return classIndices;
}
void qRFCTrainingDialog::onClassIndicesChanged()
{
	numberClassesSpinBox->setValue((int) getClassIndices().size());
}
int qRFCTrainingDialog::getMaxCorePoints() const
{
	return maxPointsSpinBox->value();
}
int qRFCTrainingDialog::getNTrees() const
{
	return nTreesSpinBox->value();
}
int qRFCTrainingDialog::getMaxTreeDepth() const
{
	return maxTreeDepthSpinBox->value();
}
bool qRFCTrainingDialog::getInputType() const
{
	Q_ASSERT(usePointCloudsRadioButton->isChecked() != useScalarFieldsRadioButton->isChecked()); // You cannot select both point clouds and scalar fields as inputs
	if (usePointCloudsRadioButton->isChecked())
	{
		return true;
	}
	else
	{
		return false;
	}
}
void qRFCTrainingDialog::onPointCloudsEnabled()
{
	usePointCloudsRadioButton->setChecked(true);
	class1CloudComboBox->setEnabled(true);
	class2CloudComboBox->setEnabled(true);
	useScalarFieldsRadioButton->setChecked(false);
	cloudComboBox->setEnabled(false);
	scalarFieldComboBox->setEnabled(false);
	classesLineEdit->setEnabled(false);

	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters());
}
void qRFCTrainingDialog::onScalarFieldsEnabled()
{
	usePointCloudsRadioButton->setChecked(false);
	class1CloudComboBox->setEnabled(false);
	class2CloudComboBox->setEnabled(false);
	useScalarFieldsRadioButton->setChecked(true);
	cloudComboBox->setEnabled(true);
	scalarFieldComboBox->setEnabled(true);
	classesLineEdit->setEnabled(true);

	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters() && validScalarField());
}
bool qRFCTrainingDialog::validScalarField() const
{
	ccPointCloud* cloud = qRFCTools::GetCloudFromCombo(cloudComboBox, m_app->dbRootObject());
	if (!cloud->hasScalarFields())
	{
		return false;
	}

	int sfIdx = scalarFieldComboBox->currentIndex();
	if (sfIdx == -1)
	{
		return false;
	}

	CCCoreLib::ScalarField* SF = cloud->getScalarField(sfIdx);

	return qRFCTools::validScalarField(SF);
}
void qRFCTrainingDialog::onScalarFieldChanged(int sfIdx)
{
	if (sfIdx == -1)
	{
		return;
	}

	ccPointCloud* cloud = qRFCTools::GetCloudFromCombo(cloudComboBox, m_app->dbRootObject());
	if (!cloud->hasScalarFields())
	{
		return;
	}

	CCCoreLib::ScalarField* SF = cloud->getScalarField(sfIdx);

	std::set<float> uniqueValues;
	for (size_t i = 0; i < SF->size(); ++i)
	{
		uniqueValues.insert(SF->getValue(i));
	}

	auto it = uniqueValues.find(-1);
	if (it != uniqueValues.end())
		uniqueValues.erase(it);

	bool valid = true;
	for (float value : uniqueValues)
	{
		if (value != int(value))
		{
			valid = false;
			break;
		}
	}
	
	if (!valid && m_app)
	{
		m_app->dispToConsole(QString("Scalar field [%1] not compatible with classifier. Scalar fields must be of an integer type.").arg(SF->getName().c_str()), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		numberClassesSpinBox->setValue(0);
		buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
		return;
	}
	if (uniqueValues.size() > 256 && m_app)
	{
		m_app->dispToConsole(QString("Scalar field [%1] contains more than 256 classes. Make sure the scalar field is indeed of an integer type.").arg(SF->getName().c_str()), ccMainAppInterface::WRN_CONSOLE_MESSAGE);
	}

	// ensure values are continuous
	int max_class = *std::max_element(uniqueValues.begin(), uniqueValues.end());
	if (max_class <= 256)
	{
		for (int i = 0; i < max_class; ++i)
		{
			uniqueValues.insert(i);
		}
	}

	SF->computeMinAndMax();
	float sfMin = SF->getMin();
	float sfMax = SF->getMax();
	numberClassesSpinBox->setValue((int) uniqueValues.size());
	std::string classesList;
	for (float index : uniqueValues)
	{
		classesList += " " + std::to_string(static_cast<int>(index));
	}
	classesList.erase(0, 1);
	classesLineEdit->setText(classesList.c_str());

	if (sfMax - sfMin != uniqueValues.size() && m_app)
	{
		m_app->dispToConsole("Labels are not continuous. Please make sure the number of classes is correct.", ccMainAppInterface::WRN_CONSOLE_MESSAGE);
	}

	//buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters(true));
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters());
}
void qRFCTrainingDialog::onPointCloudChanged(int idx)
{
	scalarFieldComboBox->clear();

	// get list of scalar fields of current cloudComboBox point cloud
	ccPointCloud* cloud = qRFCTools::GetCloudFromCombo(cloudComboBox, m_app->dbRootObject());

	if (!cloud->hasScalarFields() && m_app)
	{
		m_app->dispToConsole("Point cloud has no scalar fields associated.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	int nSF = cloud->getNumberOfScalarFields();
	for (int i = 0; i < nSF; ++i)
	{
		scalarFieldComboBox->addItem(cloud->getScalarFieldName(i).c_str(), i);
	}
	//scalarFieldComboBox->setCurrentIndex(-1);

	if (cloud->getCurrentDisplayedScalarField() != nullptr)
	{
		int sfIdx = cloud->getCurrentDisplayedScalarFieldIndex();
		onScalarFieldChanged(sfIdx);
	}
	else
	{
		onScalarFieldChanged(0);
	}

	// Add features to featureListView
	assert(m_featureModel != nullptr); // this should be defined
	m_featureModel->clear();
	{
		QString name = Features::getName(Features::POINTFEATURES).c_str();
		QStandardItem* item = new QStandardItem(name);
		item->setCheckable(true);
		item->setCheckState(Qt::Checked);
		item->setData(QVariant(Features::Source::CGAL_GENERATED_FEATURE));
		m_featureModel->appendRow(item);
	}
	if (cloud->hasNormals())
	{
		QString name = Features::getName(Features::POINTNORMALS).c_str();
		QStandardItem* item = new QStandardItem(name);
		item->setCheckable(true);
		item->setData(QVariant(Features::Source::CC_NORMALS_FIELD));
		m_featureModel->appendRow(item);
	}
	if (cloud->hasColors())
	{
		QString name = Features::getName(Features::POINTCOLORS).c_str();
		QStandardItem* item = new QStandardItem(name);
		item->setCheckable(true);
		item->setData(QVariant(Features::Source::CC_COLOR_FIELD));
		m_featureModel->appendRow(item);
	}
	if (cloud->hasScalarFields())
	{
		QSettings settings("qRandomForestClassifier");
		settings.beginGroup("Shared");
		qRFCTools::FeatureList features = qRFCTools::variantToFeatureList(settings.value("Features", qRFCTools::featureListToVariant(getFeatureList())).value<QVariantList>());
		settings.endGroup();
		for (const auto& feature : features)
		{
			if (feature.data.value<Features::Source>() == Features::Source::CC_SCALAR_FIELD)
			{
				// check if exists in point cloud
				QString featureSFName = feature.name;
				if (cloud->getScalarFieldIndexByName(featureSFName.toStdString().c_str()) != -1)
				{
					QStandardItem* item = new QStandardItem(featureSFName);
					item->setCheckable(true);
					item->setCheckState((feature.checked) ? Qt::Checked : Qt::Unchecked);
					item->setData(feature.data);
					m_featureModel->appendRow(item);
				}
			}
		}
	}
	
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters());
}

void qRFCTrainingDialog::onClassChanged(int dummy)
{
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters());
}

void qRFCTrainingDialog::onCloudChanged(int dummy)
{
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validParameters());
}

ccPointCloud* qRFCTrainingDialog::getClass1Cloud()
{
	return qRFCTools::GetCloudFromCombo(class1CloudComboBox, m_app->dbRootObject());
}

ccPointCloud* qRFCTrainingDialog::getClass2Cloud()
{
	return qRFCTools::GetCloudFromCombo(class2CloudComboBox, m_app->dbRootObject());
}

ccPointCloud* qRFCTrainingDialog::getEvaluationCloud()
{
	//returns a cloud if evaluate parameters is checked
	return evaluateParamsCheckBox->isChecked() ? qRFCTools::GetCloudFromCombo(evaluationCloudComboBox, m_app->dbRootObject()) : nullptr;
}

std::pair<ccPointCloud*, ccScalarField*> qRFCTrainingDialog::getPointCloudAndScalarField()
{
	ccPointCloud* cloud = qRFCTools::GetCloudFromCombo(cloudComboBox, m_app->dbRootObject());

	int sfIdx = scalarFieldComboBox->currentIndex();
	assert(sfIdx != -1);
	ccScalarField* SF = static_cast<ccScalarField*>(cloud->getScalarField(sfIdx));

	//return the cloud and scalar field currently selected in the combox boxes
	return std::make_pair(cloud, SF);
}

qRFCTools::FeatureList qRFCTrainingDialog::getFeatureList() const
{
	qRFCTools::FeatureList features;
	if (m_featureModel == nullptr) return features;
	for (int i = 0; i < m_featureModel->rowCount(); ++i)
	{
		QStandardItem* item = m_featureModel->item(i);
		QString text = item->text();
		QVariant data = item->data();
		Qt::CheckState checked = item->checkState();
		features.push_back({ text,data,checked });
	}
	return features;
}
void qRFCTrainingDialog::setFeatureList(qRFCTools::FeatureList features)
{
	if (m_featureModel == nullptr) return;
	m_featureModel->clear();
	for (const auto& feature : features)
	{
		QStandardItem* item = new QStandardItem(feature.name);
		item->setCheckable(true);
		item->setCheckState(feature.checked);
		item->setData(feature.data);
		m_featureModel->appendRow(item);
	}
}
void qRFCTrainingDialog::loadParamsFromPersistentSettings()
{
	QSettings settings("qRandomForestClassifier");

	//read out parameters
	settings.beginGroup("Shared");
	double minScale = settings.value("MinScale", minScaleDoubleSpinBox->value()).toDouble();
	int nScales = settings.value("NScales", numberScalesSpinBox->value()).toInt();
	int nClasses = settings.value("NClasses", numberClassesSpinBox->value()).toInt();
	QString classesList = settings.value("ClassesList", classesLineEdit->text()).toString();
	qRFCTools::FeatureList features = qRFCTools::variantToFeatureList(settings.value("Features", qRFCTools::featureListToVariant(getFeatureList())).value<QVariantList>());
	settings.endGroup();

	settings.beginGroup("Training");
	unsigned maxPoints = settings.value("MaxPoints", maxPointsSpinBox->value()).toUInt();
	unsigned nTrees = settings.value("NTrees", nTreesSpinBox->value()).toUInt();
	unsigned maxTreeDepth = settings.value("MaxTreeDepth", maxTreeDepthSpinBox->value()).toUInt();
	bool evaluateParams = settings.value("EvaluateParams", evaluateParamsCheckBox->isChecked()).toBool();
	settings.endGroup();

	//apply parameters
	minScaleDoubleSpinBox->setValue(minScale);
	numberScalesSpinBox->setValue(nScales);
	numberClassesSpinBox->setValue(nClasses);
	classesLineEdit->setText(classesList);
	maxPointsSpinBox->setValue(maxPoints);
	nTreesSpinBox->setValue(nTrees);
	maxTreeDepthSpinBox->setValue(maxTreeDepth);
	evaluateParamsCheckBox->setChecked(evaluateParams);
	setFeatureList(features);
}
void qRFCTrainingDialog::saveParamsToPersistentSettings()
{
	QSettings settings("qRandomForestClassifier");

	//save parameters
	settings.beginGroup("Shared");
	settings.setValue("MinScale",minScaleDoubleSpinBox->value());
	settings.setValue("NScales", numberScalesSpinBox->value());
	settings.setValue("NClasses", numberClassesSpinBox->value());
	settings.setValue("ClassesList", classesLineEdit->text());
	settings.setValue("Features", qRFCTools::featureListToVariant(getFeatureList()));
	settings.endGroup();

	settings.beginGroup("Training");
	settings.setValue("MaxPoints",maxPointsSpinBox->value());
	settings.setValue("NTrees", nTreesSpinBox->value());
	settings.setValue("MaxTreeDepth", maxTreeDepthSpinBox->value());
	settings.setValue("EvaluateParams", evaluateParamsCheckBox->isChecked());
	settings.endGroup();
}
