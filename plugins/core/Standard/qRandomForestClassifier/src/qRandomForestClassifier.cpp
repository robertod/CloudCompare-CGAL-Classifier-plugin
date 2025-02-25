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

#include <QtGui>

#include "qRandomForestClassifier.h"

//local
#include "qRFCTrainingDialog.h"
#include "qRFCClassifDialog.h"
#include "qRFCNoticeDialog.h"
#include "Classifier.h"

//CCCoreLib
#include <CloudSamplingTools.h>

//qCC_db
#include <ccProgressDialog.h>
#include <ccScalarField.h>

// Constructor
qRFC::qRFC( QObject *parent )
	: QObject( parent )
	, ccStdPluginInterface( ":/CC/plugin/qRandomForestClassifier/info.json" )
	//, m_action( nullptr )
	, m_trainAction(nullptr)
	, m_classifyAction(nullptr)
{
}

void qRFC::onNewSelection(const ccHObject::Container& selectedEntities)
{
	if (m_classifyAction)
	{
		//classification: only one point cloud
		m_classifyAction->setEnabled(selectedEntities.size() == 1 && selectedEntities[0]->isA(CC_TYPES::POINT_CLOUD));
	}

	if (m_trainAction)
	{
		m_trainAction->setEnabled(m_app && m_app->dbRootObject() && m_app->dbRootObject()->getChildrenNumber() != 0); //need some loaded entities to train the classifier!
	}

	m_selectedEntities = selectedEntities;
}
// This method returns all the 'actions' your plugin can perform.
// getActions() will be called only once, when plugin is loaded.
QList<QAction *> qRFC::getActions()
{
	QList<QAction*> group;

	if (!m_trainAction)
	{
		m_trainAction = new QAction("Train classifier", this);
		m_trainAction->setToolTip("Train random forest classifier");
		m_trainAction->setIcon(QIcon(QString::fromUtf8(":/CC/plugin/qRandomForestClassifier/images/icon_train.png")));
		connect(m_trainAction, &QAction::triggered, this, &qRFC::doTrainAction);
	}
	group.push_back(m_trainAction);

	if (!m_classifyAction)
	{
		m_classifyAction = new QAction("Classify", this);
		m_classifyAction->setToolTip("Classify cloud using random forest classifier");
		m_classifyAction->setIcon(QIcon(QString::fromUtf8(":/CC/plugin/qRandomForestClassifier/images/icon_classify.png")));
		connect(m_classifyAction, &QAction::triggered, this, &qRFC::doClassifyAction);
	}
	group.push_back(m_classifyAction);

	return group;
}
void qRFC::doClassifyAction()
{
	if (!ShowNoticeDialog(m_app))
	{
		return;
	}

	if (m_selectedEntities.empty() || !m_selectedEntities.front()->isA(CC_TYPES::POINT_CLOUD))
	{
		m_app->dispToConsole("Please select a point cloud", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	ccPointCloud* cloud = static_cast<ccPointCloud*>(m_selectedEntities.front());

	//display dialog
	qRFCClassifDialog ctDlg(cloud, m_app);
	if (!ctDlg.exec())
		return;

	ctDlg.saveParamsToPersistentSettings();

	// get parameters
	QString classifFilename = ctDlg.getClassifFilePath();
	Regularization::Method regType = Regularization::getID(ctDlg.getRegType());
	double minScale = ctDlg.getMinScale();
	int nScales = ctDlg.getNScale();
	std::vector<int> classesList = ctDlg.getClassIndices();
	if (classesList.size() < 2)
	{
		if (m_app)
			m_app->dispToConsole("Classes list is invalid, not enough classes were defined.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}
	std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > features = ctDlg.getFeatures();
	bool exportFeatures = ctDlg.getExportFeatures();
	CCCoreLib::ScalarField* labels = ctDlg.getEvaluationLabels();

	Classifier::ClassifyParams params;
	params.reg_type = regType;
	params.nscales = nScales;
	params.min_scale = minScale;
	params.classes_list = classesList;
	params.eval_features = features;
	params.export_features = exportFeatures;
	params.labels = labels;

	Classifier classifier(m_app);
	std::vector<int> label_indices;
	std::map<std::string, std::vector<float>> exported_features;
	std::tie(label_indices, exported_features) = classifier.classify(cloud, classifFilename, params);

	if (label_indices.empty() && exported_features.empty())
		return;

	std::string SFName = "Classification";
	if (cloud->getScalarFieldIndexByName(SFName.c_str()) != -1)
	{
		bool valid_name = false;
		for (size_t i = 0; i < 256; ++i)
			if (cloud->getScalarFieldIndexByName((SFName + " " + std::to_string(i)).c_str()) == -1)
			{
				SFName += " " + std::to_string(i);
				valid_name = true;
				break;
			}
		if (m_app && !valid_name)
			m_app->dispToConsole("Name 'Classification' scalar field already exists.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		if (!valid_name)
			return;
	}

	ccScalarField* SF = new ccScalarField(SFName.c_str());
	if (!SF->resizeSafe(cloud->size(), true, CCCoreLib::NAN_VALUE))
	{
		if (m_app)
			m_app->dispToConsole("Not enough memory to store classification output.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	for (size_t i = 0; i < cloud->size(); ++i)
	{
		SF->setValue(i, label_indices.at(i));
	}

	SF->computeMinAndMax();
	int idx = cloud->addScalarField(SF);
	cloud->setCurrentScalarField(idx);
	cloud->setCurrentDisplayedScalarField(idx);
	cloud->showSF(true);

	if (params.export_features)
	{
		std::cout << "Adding features to view" << std::endl;
		for (auto& feature : exported_features)
		{
			if (feature.first.empty()) continue;
			ccScalarField* eSF = new ccScalarField(feature.first.c_str());
			eSF->resizeSafe(cloud->size(), true, CCCoreLib::NAN_VALUE);
			for (size_t i = 0; i < cloud->size(); ++i)
			{
				eSF->setValue(i, feature.second.at(i));
			}
			eSF->computeMinAndMax();
			cloud->addScalarField(eSF);
		}
	}

	cloud->prepareDisplayForRefresh();
	m_app->refreshAll();
	m_app->updateUI();
}

void qRFC::doTrainAction()
{
	if (!ShowNoticeDialog(m_app))
	{
		return;
	}

	//display dialog
	qRFCTrainingDialog ctDlg(m_app);
	if (!ctDlg.exec())
		return;

	ctDlg.saveParamsToPersistentSettings();

	// get parameters
	double minScale = ctDlg.getMinScale();
	int nScales = ctDlg.getNScale();
	bool evaluateParams = ctDlg.getEvaluateParams();
	int maxCorePoints = ctDlg.getMaxCorePoints();
	int nTrees = ctDlg.getNTrees();
	int maxTreeDepth = ctDlg.getMaxTreeDepth();

	Classifier::TrainParams params;
	params.nscales = nScales;
	params.min_scale = minScale;
	params.evaluate_params = evaluateParams;
	params.max_core_points = maxCorePoints;
	params.num_trees = nTrees;
	params.max_depth = maxTreeDepth;

	QString classifier_path = "";
	if (ctDlg.getInputType())
	{
		params.classes_list = { 0,1 };

		ccPointCloud* cloud1 = ctDlg.getClass1Cloud();
		ccPointCloud* cloud2 = ctDlg.getClass2Cloud();

		if (!cloud1 || !cloud2)
		{
			if (m_app)
				m_app->dispToConsole("At least one cloud (class #1 or #2) was not defined!", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return;
		}

		assert(cloud1 != cloud2);

		std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > features1 = ctDlg.getFeatures(cloud1);
		std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > features2 = ctDlg.getFeatures(cloud2);
		assert(features1.size() == features2.size());

		ccPointCloud* evaluationCloud = ctDlg.getEvaluationCloud();

		//progress dialog
		//ccProgressDialog pDlg(true, m_app->getMainWindow());

		assert(ctDlg.maxPointsSpinBox->value() > 0);

		Classifier classifier(m_app);
		classifier_path = classifier.train(cloud1, cloud2, features1, features2, params);

		/*
		if (params.export_features) {
			std::cout << "Adding params to view" << std::endl;
			for (auto& feature : exported_features) {
				ccScalarField* eSF1 = new ccScalarField(feature.first.c_str());
				eSF1->resizeSafe(cloud1->size(), true, CCCoreLib::NAN_VALUE);
				for (size_t i = 0; i < cloud1->size(); ++i) {
					eSF1->setValue(i, feature.second.at(i));
				}
				eSF1->computeMinAndMax();
				cloud1->addScalarField(eSF1);

				ccScalarField* eSF2 = new ccScalarField(feature.first.c_str());
				eSF1->resizeSafe(cloud2->size(), true, CCCoreLib::NAN_VALUE);
				for (size_t i = 0; i < cloud2->size(); ++i) {
					eSF2->setValue(i, feature.second.at(cloud1->size() + i));
				}
				eSF2->computeMinAndMax();
				cloud2->addScalarField(eSF2);
			}
			cloud1->prepareDisplayForRefresh();
			cloud2->prepareDisplayForRefresh();
			m_app->refreshAll();
			m_app->updateUI();
		}
		*/
	}
	else
	{
		std::vector<int> classesList = ctDlg.getClassIndices();
		params.classes_list = classesList;
		if (classesList.size() < 2)
		{
			if (m_app)
				m_app->dispToConsole("Classes list is invalid, not enough classes were defined.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return;
		}

		ccPointCloud* cloud;
		ccScalarField* SF;
		std::tie(cloud,SF) = ctDlg.getPointCloudAndScalarField();

		if (!cloud)
		{
			if (m_app)
				m_app->dispToConsole("Point cloud was not defined!", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return;
		}

		params.features = ctDlg.getFeatures(cloud);

		Classifier classifier(m_app);
		classifier_path = classifier.train(cloud, SF, params);
	}

	if (classifier_path.isEmpty() || classifier_path.isNull())
	{
		if (m_app)
			m_app->dispToConsole("Classifier either not created or saved.", ccMainAppInterface::WRN_CONSOLE_MESSAGE);
		return;
	}

	if (params.evaluate_params)
	{
		ccPointCloud* evaluationCloud = ctDlg.getEvaluationCloud();
		if (!evaluationCloud)
		{
			if (m_app)
				m_app->dispToConsole("Evaluation point cloud was not defined!", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return;
		}

		std::cout << "classifier_path = " << classifier_path.toStdString() << std::endl;

		Classifier::ClassifyParams classify_params;
		classify_params.nscales = params.nscales;
		classify_params.classes_list = params.classes_list;
		classify_params.min_scale = params.min_scale;
		classify_params.eval_features = ctDlg.getFeatures(evaluationCloud);
		//classify_params.export_features = params.export_features;
		Classifier classifier(m_app);
		std::vector<int> label_indices;
		std::map<std::string, std::vector<float>> exported_features;
		std::tie(label_indices, exported_features) = classifier.classify(evaluationCloud, classifier_path, classify_params);

		std::string SFName = "Classification";
		if (evaluationCloud->getScalarFieldIndexByName(SFName.c_str()) != -1)
		{
			bool valid_name = false;
			for (size_t i = 0; i < 256; ++i)
			{
				if (evaluationCloud->getScalarFieldIndexByName((SFName + " " + std::to_string(i)).c_str()) == -1)
				{
					SFName += " " + std::to_string(i);
					valid_name = true;
					break;
				}
			}
			if (m_app && !valid_name)
				m_app->dispToConsole("Name 'Classification' scalar field already exists.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			if (!valid_name)
				return;
		}

		ccScalarField* SF = new ccScalarField(SFName.c_str());
		if (!SF->resizeSafe(evaluationCloud->size(), true, CCCoreLib::NAN_VALUE))
		{
			if (m_app)
				m_app->dispToConsole("Not enough memory to store classification output.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return;
		}

		for (size_t i = 0; i < evaluationCloud->size(); ++i)
		{
			SF->setValue(i, label_indices.at(i));
		}

		SF->computeMinAndMax();
		int idx = evaluationCloud->addScalarField(SF);
		evaluationCloud->setCurrentScalarField(idx);
		evaluationCloud->setCurrentDisplayedScalarField(idx);
		evaluationCloud->showSF(true);

		evaluationCloud->prepareDisplayForRefresh();
		m_app->refreshAll();
		m_app->updateUI();
		/*
		if (params.export_features) {
			std::cout << "Adding params to view" << std::endl;
			for (auto& feature : exported_features) {
				if (feature.first.empty()) continue;
				ccScalarField* eSF = new ccScalarField(feature.first.c_str());
				eSF->resizeSafe(cloudAndSF.first->size(), true, CCCoreLib::NAN_VALUE);
				for (size_t i = 0; i < cloudAndSF.first->size(); ++i) {
					eSF->setValue(i, feature.second.at(i));
				}
				eSF->computeMinAndMax();
				cloudAndSF.first->addScalarField(eSF);
			}
			cloudAndSF.first->prepareDisplayForRefresh();
			m_app->refreshAll();
			m_app->updateUI();
		}
		*/
	}
}
