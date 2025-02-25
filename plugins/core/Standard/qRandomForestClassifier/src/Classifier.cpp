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

#include "Classifier.h"

//qCC_db
#include <ccProgressDialog.h>

//Qt
#include <QSettings>
#include <QFileDialog>
#include <QThread>

//system
#include <random>

Classifier::Classifier(ccMainAppInterface* app) : m_app(app) {}

QStringList Classifier::readETHZRandomForestClassifierData(QString classifierFilePath) {
	assert(classifierFilePath.size() > 0);
	assert(QFileInfo(classifierFilePath).exists());

	CGAL::internal::liblearning::RandomForest::ForestParams forestParams;
	typedef CGAL::internal::liblearning::RandomForest::RandomForest
		< CGAL::internal::liblearning::RandomForest::NodeGini
		< CGAL::internal::liblearning::RandomForest::AxisAlignedSplitter> > Forest;
	std::shared_ptr<Forest> rfc = std::make_shared<Forest>(forestParams);

	QStringList fileDesc;

	std::ifstream input(classifierFilePath.toStdString(), std::ios_base::binary);
	if (!input) {
		if (m_app)
			m_app->dispToConsole(QString("Failed to open file [%1]").arg(classifierFilePath), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return fileDesc;
	}
	rfc->read(input);
	input.close();

	fileDesc << QString("No. of classes: %1").arg(rfc->params.n_classes);
	fileDesc << QString("No. of features: %1").arg(rfc->params.n_features);
	fileDesc <<			"====================";
	fileDesc << QString("No. of samples: %1").arg(rfc->params.n_samples);
	fileDesc << QString("No. of trees: %1").arg(rfc->params.n_trees);
	fileDesc << QString("No. of in-bag-samples: %1").arg(rfc->params.n_in_bag_samples);
	fileDesc << QString("Min. no. of samples per node: %1").arg(rfc->params.min_samples_per_node);
	fileDesc << QString("Max depth: %1").arg(rfc->params.max_depth);
	fileDesc << QString("Sample reduction: %1").arg(rfc->params.sample_reduction);

	return fileDesc;
}

std::pair<std::vector<int>, std::map<std::string, std::vector<float>>> Classifier::classify(ccPointCloud* cloud, QString classifierFilePath, const ClassifyParams& params) {
	assert(classifierFilePath.size() > 0);
	assert(QFileInfo(classifierFilePath).exists());

	ccProgressDialog* progressDlg = nullptr;
	CCCoreLib::NormalizedProgress* nProgress = nullptr;
	if (m_app) {
		progressDlg = new ccProgressDialog(true, (QWidget*) m_app->getMainWindow());
		progressDlg->setWindowIcon(QIcon(":/CC/plugin/qRandomForestClassifier/images/icon_classify.png"));
		progressDlg->reset();
		progressDlg->setInfo("Converting point clouds");
		progressDlg->setMethodTitle("Classification");
		if (nProgress == nullptr)
			nProgress = new CCCoreLib::NormalizedProgress(&(*progressDlg), 6);
		progressDlg->start();
	}

	std::vector<Point> points;
	points.reserve(cloud->size());
	for (unsigned i = 0; i < cloud->size(); ++i) {
		const auto pt_cc = cloud->getPoint(i)->toDouble();
		double x = pt_cc[0];
		double y = pt_cc[1];
		double z = pt_cc[2];
		points.push_back(Point(x, y, z));
	}

	Point_set pts;
	std::copy(points.begin(), points.end(), pts.point_back_inserter());
	std::cout << "number of points = " << pts.number_of_points() << std::endl;

	if (nProgress && !nProgress->oneStep()) {
		return {};
	}
	if (progressDlg)
		progressDlg->setInfo("Computing features");
	Feature_set features;

	std::cerr << "Generating features" << std::endl;
	CGAL::Real_timer t;
	t.start();
	Feature_generator generator(pts, pts.point_map(), params.nscales);
	features.begin_parallel_additions();
	for (const auto& feature : params.eval_features) {
		Features::Source source = feature.first;
		if (source == Features::Source::CGAL_GENERATED_FEATURE) {
			//if (type == Features::Type::POINTFEATURES)
			generator.generate_point_based_features(features);

			// bool point_features_generated = false;
			//for (const auto& feature : params.features) {
			//	Features::Type type = feature.third;
			//  if (type== Features::Type::POINTFEATURES) point_features_generated = true;
			//}
			//if (type == Features::Type::EIGENVALUES && !point_features_generated)
			//	generator.generate_eigen_features(features);
		}
		if (source == Features::Source::CC_COLOR_FIELD) {
			if (!cloud->hasColors()) {
				if (m_app)
					m_app->dispToConsole("No color property found", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			}
			else {
				Cmap colorMap;
				bool cmAdded;
				std::tie(colorMap, cmAdded) = pts.add_property_map<Cmap::value_type>("colors");

				if (!cmAdded) {
					if (m_app)
						m_app->dispToConsole("Failed to add \"colors\" field.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
				}
				else {
					for (size_t i = 0; i < cloud->size(); ++i) {
						const ccColor::Rgba& cc_color = cloud->getPointColor(i);
						colorMap[i] = { cc_color.r,cc_color.g,cc_color.b };
					}
					generator.generate_color_based_features(features, colorMap);
					std::cout << "Colors successfully added" << std::endl;
				}
			}
		}
		if (source == Features::Source::CC_NORMALS_FIELD) {
			if (!cloud->hasNormals()) {
				if (m_app)
					m_app->dispToConsole("No normals property found", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			}
			else {
				Vmap normalMap;
				bool nmAdded;
				std::tie(normalMap, nmAdded) = pts.add_normal_map();
				if (!nmAdded) {
					if (m_app)
						m_app->dispToConsole("Failed to add \"normal\" field.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
				}
				else {
					for (size_t i = 0; i < cloud->size(); ++i) {
						const CCVector3& cc_normal = cloud->getPointNormal(i);
						normalMap[i] = { cc_normal.x,cc_normal.y,cc_normal.z };
					}
					generator.generate_normal_based_features(features, normalMap);
					std::cout << "Normals successfully added" << std::endl;
				}
			}
		}
		if (source == Features::Source::CC_SCALAR_FIELD) {
			CCCoreLib::ScalarField* SF = feature.second;
			if (SF == nullptr) {
				if (m_app) {
					m_app->dispToConsole("Invalid pointer to scalar field.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
				}
				if (progressDlg) {
					progressDlg->stop();
				}
				return {};
			}
			std::string name = SF->getName();
			std::replace(name.begin(), name.end(), '_', ' '); // replace underscores with spaces
			if (SF->size() != cloud->size() || SF->size() != pts.number_of_points()) {
				std::cout << "[classify] scalar field (" << name << ") size does not match point cloud size (CC cloud size: " << cloud->size() << ", CGAL cloud size: " << pts.number_of_points() << ", CC scalar size: " << SF->size() << ")" << std::endl;
				if (m_app)
					m_app->dispToConsole("Scalar field size does not match point cloud size.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
				if (progressDlg)
					progressDlg->stop();
				return {};
			}
			Fmap featureMap;
			bool fmAdded;
			std::tie(featureMap, fmAdded) = pts.add_property_map<float>(name);
			if (!fmAdded) {
				std::cout << "\tproperty map couldn't be added [" << name << "]" << std::endl;
				continue;
			}
			std::cout << "[classify] scalar name: " << name << std::endl;
			for (size_t j = 0; j < SF->size(); ++j) {
				featureMap[j] = SF->getValue(j);
			}
			features.add<CGAL::Classification::Feature::Simple_feature<Point_set, Fmap> >(pts, featureMap, name);
			std::cout << "[classify] feature [" << name << "] added" << std::endl;
		}
	}
	features.end_parallel_additions();
	t.stop();
	std::cerr << "Done in " << t.time() << " second(s)" << std::endl;

	if (nProgress && !nProgress->oneStep()) {
		return {};
	}

	// Add labels
	Label_set labels;
	//for (size_t i = 0; i < params.nclasses; ++i) labels.add(("class #" + std::to_string(i)).c_str());
	for (int index : params.classes_list) {
		if (index == -1) // unlabelled class
			continue;
		labels.add(("class #" + std::to_string(index)).c_str(), CGAL::IO::Color(0, 0, 0), index);
	}

	std::vector<int> label_indices(pts.size(), -1);

	if (nProgress && !nProgress->oneStep()) {
		return {};
	}
	if (progressDlg)
		progressDlg->setInfo("Initializing classifier");

	std::cout << "[classify] Using ETHZ Random Forest Classifier" << std::endl;

	std::cout << "[classify] features:" << std::endl;
	for (const auto& feature : features) {
		std::cout << "      name: " << feature->name() << std::endl;
	}
	std::cout << "[classify] pts size: " << pts.number_of_points() << std::endl;
	std::cout << "[classify] pts props:" << std::endl;
	for (const auto& props : pts.properties()) {
		std::cout << "      name: " << props << std::endl;
	}
	std::cout << "[classify] labels size: " << labels.size() << std::endl;
	
	Classification::ETHZ::Random_forest_classifier classifier(labels, features);

	std::cout << "[classify] classifier built" << std::endl;
	std::cout << "[classify] loading configuration" << std::endl;

	std::ifstream fin(classifierFilePath.toStdString(), std::ios_base::binary);
	if (!fin) {
		std::cerr << "Failed to open file [" << classifierFilePath.toStdString() << "]" << std::endl;
		if (progressDlg)
			progressDlg->stop();
		return {};
	}
	classifier.load_configuration(fin);
	fin.close();
	std::cout << "[classify] classifier configured" << std::endl;

	if (nProgress && !nProgress->oneStep()) {
		return {};
	}
	if (progressDlg)
		progressDlg->setInfo("Performing classification");

	t.reset();
	t.start();
	switch (params.reg_type) {
	case Regularization::Method::LOCAL_SMOOTHING:
		Classification::classify_with_local_smoothing<CGAL::Parallel_if_available_tag>
			(pts, pts.point_map(), labels, classifier,
				generator.neighborhood().k_neighbor_query(12),
				label_indices);
		break;
	case Regularization::Method::GRAPH_CUT:
		Classification::classify_with_graphcut<CGAL::Parallel_if_available_tag>
			(pts, pts.point_map(), labels, classifier,
				generator.neighborhood().k_neighbor_query(12),
				0.2f, QThread::idealThreadCount(), label_indices);
		break;
	default:
		Classification::classify<CGAL::Parallel_if_available_tag>(pts, labels, classifier, label_indices);
	}
	t.stop();
	std::cerr << "Classification done in " << t.time() << " second(s)" << std::endl;

	std::map<std::string, std::vector<float>> exportedFeatures;

	if (nProgress && !nProgress->oneStep()) {
		return {};
	}
	if (params.export_features) {
		if (progressDlg)
			progressDlg->setInfo("Exporting features");
		std::cout << "[classify] exporting features:" << std::endl;
		for (auto& feature : features) {
			std::string featname = feature->name();
			std::cout << "\tname: " << featname << std::endl;
			std::replace(featname.begin(), featname.end(), '_', ' '); // replace underscores with spaces
			std::vector<float> indicies;
			indicies.reserve(pts.number_of_points());
			for (size_t i = 0; i < pts.number_of_points(); ++i) {
				indicies.push_back(feature->value(i));
			}

			exportedFeatures.insert({ featname, indicies });

			if (!progressDlg || progressDlg->wasCanceled()) {
				return {};
			}
		}
	}

	if (progressDlg)
		progressDlg->stop();

	if (params.labels != nullptr) {
		// set labels
		Imap label_map;
		bool lm_found;
		std::tie(label_map, lm_found) = pts.add_property_map<int>("label");
		for (size_t i = 0; i < cloud->size(); ++i) {
			label_map[i] = (int) params.labels->getValue(i);
		}

		// evaluate results
		Classification::Evaluation evaluation(labels, pts.range(label_map), label_indices);

		// convert to html (useful as the user can easily copy-paste the tabulated results elsewhere)
		std::stringstream ss;
		evaluation.output_to_html(ss, evaluation);
		EvaluationDialog ctDlg(ss.str(), (QWidget*)m_app->getMainWindow());
		ctDlg.exec();
	}

	std::cout << "Returning classification results (label_indices size: " << label_indices.size() << ")" << std::endl;

	return { label_indices, exportedFeatures };
}

QString Classifier::train(ccPointCloud* cloud1, ccPointCloud* cloud2, std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > features1, std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > features2, const TrainParams& params) {
	ccProgressDialog* progressDlg = nullptr;
	CCCoreLib::NormalizedProgress* nProgress = nullptr;

	if (m_app) {
		progressDlg = new ccProgressDialog(true, (QWidget*)m_app->getMainWindow());
		progressDlg->setWindowIcon(QIcon(":/CC/plugin/qRandomForestClassifier/images/icon_train.png"));
		progressDlg->reset();
		progressDlg->setInfo("Converting point clouds");
		progressDlg->setMethodTitle("Classification");
		if (nProgress == nullptr)
			nProgress = new CCCoreLib::NormalizedProgress(&(*progressDlg), 4);
		progressDlg->start();
	}

	Point_set pts;

	std::vector<Point> points;
	points.reserve(cloud1->size() + cloud2->size());
	for (unsigned i = 0; i < cloud1->size(); ++i) {
		const auto pt_cc = cloud1->getPoint(i)->toDouble();
		double x = pt_cc[0];
		double y = pt_cc[1];
		double z = pt_cc[2];
		points.push_back(Point(x, y, z));
	}
	for (unsigned i = 0; i < cloud2->size(); ++i) {
		const auto pt_cc = cloud2->getPoint(i)->toDouble();
		double x = pt_cc[0];
		double y = pt_cc[1];
		double z = pt_cc[2];
		points.push_back(Point(x, y, z));
	}
	std::copy(points.begin(), points.end(), pts.point_back_inserter());

	Imap label_map;
	bool lm_found;
	std::tie(label_map,lm_found) = pts.add_property_map<int>("label");
	for (size_t i = 0; i < cloud1->size(); ++i) {
		label_map[i] = 0;
	}
	for (size_t i = 0; i < cloud2->size(); ++i) {
		label_map[cloud1->size() + i] = 1;
	}

	if (nProgress && !nProgress->oneStep()) {
		return "";
	}
	if (progressDlg)
		progressDlg->setInfo("Computing features");
	Feature_set features;

	std::cerr << "Generating features" << std::endl;
	CGAL::Real_timer t;
	t.start();
	Feature_generator generator(pts, pts.point_map(), params.nscales);
	features.begin_parallel_additions();
	//for (const auto& feature : params.features) {
	for (int it=0; it < features1.size(); it++) {
		const auto& feature1 = features1[it];
		const auto& feature2 = features2[it];
		if (feature1.first != feature2.first) {
			if (m_app)
				m_app->dispToConsole("Scalar field mismatch", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			if (progressDlg)
				progressDlg->stop();
			return "";
		}
		Features::Source source = feature1.first;
		if (source == Features::Source::CGAL_GENERATED_FEATURE) {
			//if (type == Features::Type::POINTFEATURES)
			generator.generate_point_based_features(features);

			// bool point_features_generated = false;
			//for (const auto& feature : params.features) {
			//	Features::Type type = feature.third;
			//  if (type== Features::Type::POINTFEATURES) point_features_generated = true;
			//}
			//if (type == Features::Type::EIGENVALUES && !point_features_generated)
			//	generator.generate_eigen_features(features);
		}
		if (source == Features::Source::CC_COLOR_FIELD) {
			if (!cloud1->hasColors() || !cloud2->hasColors()) {
				if (m_app)
					m_app->dispToConsole("No color property found", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			}
			else {
				Cmap color_map;
				bool cm_added;
				std::tie(color_map, cm_added) = pts.add_property_map<Cmap::value_type>("colors");

				if (!cm_added) {
					if (m_app)
						m_app->dispToConsole("Failed to add \"colors\" field.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
				}
				else {
					for (size_t i = 0; i < cloud1->size(); ++i) {
						const ccColor::Rgba& cc_color = cloud1->getPointColor(i);
						color_map[i] = { cc_color.r,cc_color.g,cc_color.b };
					}
					for (size_t i = 0; i < cloud2->size(); ++i) {
						const ccColor::Rgba& cc_color = cloud2->getPointColor(i);
						color_map[cloud1->size() + i] = { cc_color.r,cc_color.g,cc_color.b };
					}
					generator.generate_color_based_features(features, color_map);
					std::cout << "Colors successfully added" << std::endl;
				}
			}
		}
		if (source == Features::Source::CC_NORMALS_FIELD) {
			if (!cloud1->hasNormals() || !cloud2->hasNormals()) {
				if (m_app)
					m_app->dispToConsole("No normals property found", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			}
			else {
				Vmap normal_map;
				bool nm_added;
				std::tie(normal_map, nm_added) = pts.add_normal_map();
				if (!nm_added) {
					if (m_app)
						m_app->dispToConsole("Failed to add \"normal\" field.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
				}
				else {
					for (size_t i = 0; i < cloud1->size(); ++i) {
						const CCVector3& cc_normal = cloud1->getPointNormal(i);
						normal_map[i] = { cc_normal.x,cc_normal.y,cc_normal.z };
					}
					for (size_t i = 0; i < cloud2->size(); ++i) {
						const CCVector3& cc_normal = cloud2->getPointNormal(i);
						normal_map[cloud1->size() + i] = { cc_normal.x,cc_normal.y,cc_normal.z };
					}
					generator.generate_normal_based_features(features, normal_map);
					std::cout << "Normals successfully added" << std::endl;
				}
			}
		}
		if (source == Features::Source::CC_SCALAR_FIELD) {
			CCCoreLib::ScalarField* SF1 = feature1.second;
			CCCoreLib::ScalarField* SF2 = feature2.second;
			if (SF1 == nullptr || SF2 == nullptr) {
				std::cout << "[train two clouds] invalid pointer to scalar field (cloud1: " << ((SF1 == nullptr) ? "nullptr" : SF1->getName()) << ", cloud2: " << ((SF2 == nullptr) ? "nullptr" : SF2->getName()) << ")" << std::endl;
				if (m_app)
					m_app->dispToConsole("Invalid pointer to scalar field.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
				if (progressDlg)
					progressDlg->stop();
				return "";
			}
			if (strcmp(SF1->getName().c_str(), SF2->getName().c_str()) != 0) {
				if (m_app)
					m_app->dispToConsole("Scalar field name mismatch.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
				if (progressDlg)
					progressDlg->stop();
				return "";
			}
			std::string name = SF1->getName();
			std::replace(name.begin(), name.end(), '_', ' '); // replace underscores with spaces
			Fmap feature_map;
			bool fm_added;
			std::tie(feature_map, fm_added) = pts.add_property_map<float>(name);
			if (!fm_added) {
				std::cout << "\tproperty map couldn't be added [" << name << "]" << std::endl;
				continue;
			}
			for (size_t j = 0; j < cloud1->size(); ++j) {
				feature_map[j] = SF1->getValue(j);
			}
			for (size_t j = 0; j < cloud2->size(); ++j) {
				feature_map[cloud1->size() + j] = SF2->getValue(j);
			}
			features.add<CGAL::Classification::Feature::Simple_feature<Point_set, Fmap> >(pts, feature_map, name);
			std::cout << "[train two clouds] feature [" << name << "] added." << std::endl;
		}
	}
	features.end_parallel_additions();
	t.stop();
	std::cerr << "Done in " << t.time() << " second(s)" << std::endl;

	return train(pts, features, params, progressDlg, nProgress);
}

QString Classifier::train(ccPointCloud* cloud, ccScalarField* scalarField, const TrainParams& params) {
	assert(cloud->size() == scalarField->size()); // Point cloud and scalar field size mismatch

	ccProgressDialog* progressDlg = nullptr;
	CCCoreLib::NormalizedProgress* nProgress = nullptr;
	if (m_app) {
		progressDlg = new ccProgressDialog(true, (QWidget*)m_app->getMainWindow());
		progressDlg->setWindowIcon(QIcon(":/CC/plugin/qRandomForestClassifier/images/icon_train.png"));
		progressDlg->reset();
		progressDlg->setInfo("Convering point cloud");
		progressDlg->setMethodTitle("Classification");
		if (nProgress == nullptr)
			nProgress = new CCCoreLib::NormalizedProgress(&(*progressDlg), 4);
		progressDlg->start();
	}

	Point_set pts;

	std::vector<Point> points;
	points.reserve(cloud->size());
	for (unsigned i = 0; i < cloud->size(); ++i) {
		const auto pt_cc = cloud->getPoint(i)->toDouble();
		double x = pt_cc[0];
		double y = pt_cc[1];
		double z = pt_cc[2];
		points.push_back(Point(x, y, z));
	}
	std::copy(points.begin(), points.end(), pts.point_back_inserter());

	Imap label_map;
	bool lm_found;
	std::tie(label_map, lm_found) = pts.add_property_map<int>("label");
	for (size_t i = 0; i < cloud->size(); ++i) {
		label_map[i] = (int) scalarField->getValue(i);
	}

	if (nProgress && !nProgress->oneStep()) {
		return "";
	}
	if (progressDlg)
		progressDlg->setInfo("Computing features");
	Feature_set features;

	std::cerr << "Generating features" << std::endl;
	CGAL::Real_timer t;
	t.start();
	Feature_generator generator(pts, pts.point_map(), params.nscales);
	features.begin_parallel_additions();
	for (const auto& feature : params.features) {
		Features::Source source = feature.first;
		if (source == Features::Source::CGAL_GENERATED_FEATURE) {
			//if (type == Features::Type::POINTFEATURES)
			generator.generate_point_based_features(features);

			// bool point_features_generated = false;
			//for (const auto& feature : params.features) {
			//	Features::Type type = feature.third;
			//  if (type== Features::Type::POINTFEATURES) point_features_generated = true;
			//}
			//if (type == Features::Type::EIGENVALUES && !point_features_generated)
			//	generator.generate_eigen_features(features);
		}
		if (source == Features::Source::CC_COLOR_FIELD) {
			if (!cloud->hasColors()) {
				if (m_app)
					m_app->dispToConsole("No color property found", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			}
			else {
				Cmap color_map;
				bool cm_added;
				std::tie(color_map, cm_added) = pts.add_property_map<Cmap::value_type>("colors");

				if (!cm_added) {
					if (m_app)
						m_app->dispToConsole("Failed to add \"colors\" field.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
				}
				else {
					for (size_t i = 0; i < cloud->size(); ++i) {
						const ccColor::Rgba& cc_color = cloud->getPointColor(i);
						color_map[i] = { cc_color.r,cc_color.g,cc_color.b };
					}
					generator.generate_color_based_features(features, color_map);
					std::cout << "Colors successfully added" << std::endl;
				}
			}
		}
		if (source == Features::Source::CC_NORMALS_FIELD) {
			if (!cloud->hasNormals()) {
				if (m_app)
					m_app->dispToConsole("No normals property found", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			}
			else {
				Vmap normal_map;
				bool nm_added;
				std::tie(normal_map, nm_added) = pts.add_normal_map();
				if (!nm_added) {
					if (m_app)
						m_app->dispToConsole("Failed to add \"normal\" field.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
				}
				else {
					for (size_t i = 0; i < cloud->size(); ++i) {
						const CCVector3& cc_normal = cloud->getPointNormal(i);
						normal_map[i] = { cc_normal.x,cc_normal.y,cc_normal.z };
					}
					generator.generate_normal_based_features(features, normal_map);
					std::cout << "Normals successfully added" << std::endl;
				}
			}
		}
		if (source == Features::Source::CC_SCALAR_FIELD) {
			CCCoreLib::ScalarField* SF = feature.second;
			if (SF == nullptr) {
				if (m_app)
					m_app->dispToConsole("Invalid pointer to scalar field.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
				if (progressDlg)
					progressDlg->stop();
				return "";
			}
			std::string name = SF->getName();
			std::replace(name.begin(), name.end(), '_', ' '); // replace underscores with spaces
			if (SF->size() != cloud->size() || SF->size() != pts.number_of_points()) {
				std::cout << "[train] scalar field (" << name << ") size does not match point cloud size (CC cloud size: " << cloud->size() << ", CGAL cloud size: " << pts.number_of_points() << ", CC scalar size: " << SF->size() << ")" << std::endl;
				if (m_app)
					m_app->dispToConsole("Scalar field size does not match point cloud size.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
				if (progressDlg)
					progressDlg->stop();
				return "";
			}
			Fmap feature_map;
			bool fm_added;
			std::cout << "[train] pts.add_property_map SF name: " << name << std::endl;
			std::tie(feature_map, fm_added) = pts.add_property_map<float>(name);
			if (!fm_added) {
				std::cout << "\tproperty map couldn't be added [" << name << "]" << std::endl;
				continue;
			}
			for (size_t j = 0; j < SF->size(); ++j) {
				feature_map[j] = SF->getValue(j);
			}
			features.add<CGAL::Classification::Feature::Simple_feature<Point_set, Fmap> >(pts, feature_map, name);
			std::cout << "[train] feature [" << name << "] added" << std::endl;
		}
	}
	//std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > features = {};
	features.end_parallel_additions();
	t.stop();
	std::cerr << "Done in " << t.time() << " second(s)" << std::endl;

	std::cout << "[train add features] features:" << std::endl;
	for (const auto& feature : features) {
		std::cout << "      name: " << feature->name() << std::endl;
	}
	std::cout << "[train add features] pts props:" << std::endl;
	for (const auto& props : pts.properties()) {
		std::cout << "      name: " << props << std::endl;
	}

	return train(pts, features, params, progressDlg, nProgress);
}

QString Classifier::train(Point_set pts, Feature_set& features, const TrainParams& params, ccProgressDialog* progressDlg, CCCoreLib::NormalizedProgress* nProgress) {
	assert(features.size() > 0); // No features passed

	// get labels
	Imap label_map;
	bool lm_found;
	std::tie(label_map, lm_found) = pts.property_map<int>("label");
	assert(lm_found); // Label map not found

	if (m_app && progressDlg == nullptr) {
		progressDlg = new ccProgressDialog(true, (QWidget*)m_app->getMainWindow());
		progressDlg->setWindowIcon(QIcon(":/CC/plugin/qRandomForestClassifier/images/icon_train.png"));
		progressDlg->reset();
		progressDlg->setInfo("Training classifier");
		progressDlg->setMethodTitle("Classification");
		if (nProgress == nullptr)
			nProgress = new CCCoreLib::NormalizedProgress(&(*progressDlg), 3);
		progressDlg->start();
	}
	if (nProgress == nullptr)
		nProgress = new CCCoreLib::NormalizedProgress(&(*progressDlg), 3);

	// subsample points for training
	size_t n_unassigned = 0;
	for (size_t i = 0; i < pts.number_of_points(); ++i) {
		n_unassigned += (label_map[i] == -1) ? 1 : 0;
	}
	size_t n_assigned = pts.number_of_points() - n_unassigned;

	if (nProgress && !nProgress->oneStep()) {
		return "";
	}
	if (n_assigned > params.max_core_points && params.max_core_points > 0) {
		if (progressDlg)
			progressDlg->setInfo("Subsampling labelled points for training");

		size_t n = pts.number_of_points();
		std::vector<size_t> assigned_points;
		for (size_t i = 0; i < pts.number_of_points(); ++i) {
			if (label_map[i] != -1) {
				assigned_points.push_back(i);
			}
		}
		std::random_device rd;
		std::default_random_engine rng{ rd() };
		std::shuffle(std::begin(assigned_points), std::end(assigned_points), rng);
		for (size_t i = params.max_core_points; i < assigned_points.size(); ++i) {
			label_map[assigned_points[i]] = -1;
		}

		n_unassigned = 0;
		for (size_t i = 0; i < pts.number_of_points(); ++i) {
			n_unassigned += (label_map[i] == -1) ? 1 : 0;
		}
		n_assigned = pts.number_of_points() - n_unassigned;
	}


	CGAL::Real_timer t;

	// Create class labels
	Label_set labels;
	//for (size_t i = 0; i < params.nclasses; ++i) labels.add(("class #" + std::to_string(i)).c_str());
	for (int index : params.classes_list) {
		if (index == -1) // unlabelled class
			continue;
		labels.add(("class #" + std::to_string(index)).c_str(), CGAL::IO::Color(0, 0, 0), index);
	}

	// Check if ground truth is valid for this label set
	if (nProgress && !nProgress->oneStep()) {
		return "";
	}
	if (progressDlg)
		progressDlg->setInfo("Validating ground truths");
	if (!labels.is_valid_ground_truth(pts.range(label_map), true)) {
		if (m_app)
			m_app->dispToConsole("Ground truths are invalid", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		if (progressDlg)
			progressDlg->stop();
		return "";
	}

	std::vector<int> label_indices(pts.size(), -1);

	std::cerr << "[train] Using ETHZ Random Forest Classifier" << std::endl;
	Classification::ETHZ::Random_forest_classifier classifier(labels, features);

	if (nProgress && !nProgress->oneStep()) {
		return "";
	}
	if (progressDlg)
		progressDlg->setInfo("Training classifier");

	std::cout << "[train] features:" << std::endl;
	for (const auto& feature : features) {
		std::cout << "      name: " << feature->name() << std::endl;
	}
	std::cout << "[train] pts size: " << pts.number_of_points() << std::endl;
	std::cout << "[train] pts props:" << std::endl;
	for (const auto& props : pts.properties()) {
		std::cout << "      name: " << props << std::endl;
	}
	std::cout << "[train] label_map size: " << std::distance(label_map.begin(), label_map.end()) << std::endl;
	std::cerr << "Training" << std::endl;
	t.reset();
	t.start();
	classifier.train(pts.range(label_map), true, params.num_trees, params.max_depth);
	t.stop();
	std::cerr << "Done in " << t.time() << " second(s)" << std::endl;


	if (progressDlg)
		progressDlg->stop();
	
	// Save configuration for later use
	QString fname = QFileDialog::getSaveFileName(nullptr,
		QString("Save classifier"),
		"ethz_random_forest.bin",
		QString("ETHZ Random Forest Classifier (*.bin);;All Files (*)"));

	if (!fname.isNull() && !fname.isEmpty()) {
		std::ofstream fconfig(fname.toStdString(), std::ios_base::binary);
		classifier.save_configuration(fconfig);

		// save parameter
		QSettings settings("qRandomForestClassifier");
		settings.beginGroup("Classif");
		QString currentFilePath = QFileInfo(fname).absoluteFilePath();
		settings.setValue("CurrentFilePath", currentFilePath);
		return currentFilePath;
	}
	

	// Doesn't make sense for training, but for validation
	/*
	t.reset();
	t.start();
	Classification::classify_with_graphcut<CGAL::Parallel_if_available_tag>
		(pts, pts.point_map(), labels, classifier,
			generator.neighborhood().k_neighbor_query(12),
			0.2f, 1, label_indices);
	t.stop();
	std::cerr << "Classification with graphcut done in " << t.time() << " second(s)" << std::endl;

	std::cerr << "Precision, recall, F1 scores and IoU:" << std::endl;
	Classification::Evaluation evaluation(labels, pts.range(label_map), label_indices);
	for (Label_handle l : labels)
	{
		std::cerr << " * " << l->name() << ": "
			<< evaluation.precision(l) << " ; "
			<< evaluation.recall(l) << " ; "
			<< evaluation.f1_score(l) << " ; "
			<< evaluation.intersection_over_union(l) << std::endl;
	}
	if (params.export_features) {
		t.reset();
		t.start();
		for (auto& feature : features) {
			std::string featname = feature->name();
			std::vector<float> indicies;
			indicies.reserve(pts.number_of_points());
			for (size_t i = 0; i < pts.number_of_points(); ++i) {
				indicies.push_back(feature->value(i));
			}

			exportedFeatures.insert({ featname, indicies });
		}
		std::cout << "Features exported:" << std::endl;
		for (auto feat : exportedFeatures) {
			std::cout << "\t" << feat.first << std::endl;
		}
		t.stop();
		std::cerr << "Exported features in " << t.time() << " second(s)" << std::endl;
	}
	*/


	return "";
}
