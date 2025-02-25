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

#ifndef Q_RFC_CGAL_CLASSIFIER_HEADER
#define Q_RFC_CGAL_CLASSIFIER_HEADER

#include "qRFCEvaluationDialog.h"

//qCC_plugins
#include <ccMainAppInterface.h>

//qCC_db
#include <ccPointCloud.h>
#include <ccScalarField.h>
#include <CCTypes.h>

// CGAL
#if defined (_MSC_VER) && !defined (_WIN64)
#pragma warning(disable:4244) // boost::number_distance::distance()
// converts 64 to 32 bits integers
#endif

#include <CGAL/Simple_cartesian.h>
#include <CGAL/Classification.h>
#include <CGAL/Point_set_3.h>
#include <CGAL/Point_set_3/IO.h>
#include <CGAL/Real_timer.h>

typedef CGAL::Simple_cartesian<ScalarType> Kernel;
typedef Kernel::Point_3 Point;
typedef CGAL::Point_set_3<Point> Point_set;
typedef Kernel::Iso_cuboid_3 Iso_cuboid_3;

typedef Point_set::Point_map Pmap;
typedef Point_set::Vector_map Vmap;
typedef Point_set::Property_map<int> Imap;
typedef Point_set::Property_map<float> Fmap;
typedef Point_set::Property_map<CGAL::IO::Color> Cmap;

namespace Classification = CGAL::Classification;
typedef Classification::Label_handle                                            Label_handle;
typedef Classification::Feature_handle                                          Feature_handle;
typedef Classification::Label_set                                               Label_set;
typedef Classification::Feature_set                                             Feature_set;
typedef Classification::Point_set_feature_generator<Kernel, Point_set, Pmap>    Feature_generator;

// Options for regularizing the classifier
namespace Regularization {
	enum Method { NONE = 0, GRAPH_CUT = 1, LOCAL_SMOOTHING = 2 };

	static constexpr size_t AvailableCount() { return 3; }

	static size_t getIndex(Method id)
	{
		return static_cast<size_t>(id);
	}
	static std::string getName(Method id)
	{
		switch (id) {
		case NONE:
			return "None";
		case GRAPH_CUT:
			return "Graph cut";
		case LOCAL_SMOOTHING:
			return "Local smoothing";
		default:
			return "Unknown";
		}
	}
	static Method getID(size_t i)
	{
		if (i >= 0 && i < AvailableCount()) return static_cast<Method>(i);
		else throw std::out_of_range("Index out of range");
	}
	static Method getID(const std::string& name)
	{
		std::unordered_map<std::string, Method> methodMap;
		for (int i = 0; i < AvailableCount(); ++i) {
			methodMap.insert({ getName(getID(i)), getID(i) });
		}
		auto it = methodMap.find(name);
		return (it != methodMap.end()) ? it->second : NONE;
	}

}

// Options for features compatible with the classifier
namespace Features {
	enum Type { NULL_FEATURE = -1, POINTFEATURES = 1, EIGENVALUES = 2, POINTCOLORS = 3, POINTNORMALS = 4, POINTECHO = 5 };

	enum Source { NULL_UNKOWN = -1, CGAL_GENERATED_FEATURE = 1, CC_COLOR_FIELD = 2, CC_NORMALS_FIELD = 3, CC_SCALAR_FIELD = 4 };

	static constexpr size_t AvailableCount() { return 4; }

	static size_t getIndex(Type id)
	{
		return static_cast<size_t>(id);
	}
	static std::string getName(Type id)
	{
		switch (id) {
		case POINTFEATURES:
			return "Point features";
		case EIGENVALUES:
			return "Eigenvalues";
		case POINTCOLORS:
			return "Color data";
		case POINTNORMALS:
			return "Normal data";
		case POINTECHO: // Functionality not implemented
			return "Echo data";
		default:
			return "";
		}
	}
	static Type getID(size_t i)
	{
		if (i >= 0 && i < AvailableCount()) return static_cast<Type>(i);
		else throw std::out_of_range("Index out of range");
	}
	static Source getSource(Type id)
	{
		switch (id) {
		case POINTFEATURES:
			return CGAL_GENERATED_FEATURE;
		case EIGENVALUES:
			return CGAL_GENERATED_FEATURE;
		case POINTCOLORS:
			return CC_COLOR_FIELD;
		case POINTNORMALS:
			return CC_NORMALS_FIELD;
		case POINTECHO:
			return CC_SCALAR_FIELD;
		default:
			return NULL_UNKOWN;
		}
	}
	static bool isGenerated(Type id)
	{
		switch (id) {
		case POINTFEATURES:
			return true;
		case EIGENVALUES:
			return true;
		case POINTCOLORS:
			return false;
		case POINTNORMALS:
			return false;
		case POINTECHO:
			return false;
		default:
			return false;
		}
	}
	static std::vector<std::string> getDefaultPropertyName(Type id)
	{
		switch (id) {
		case POINTFEATURES:
			return {};
		case EIGENVALUES:
			return {};
		case POINTCOLORS:
			return { "red","green","blue" };
		case POINTNORMALS:
			return { "nx","ny","nz" };
		case POINTECHO:
			return { "echos" };
		default:
			return {};
		}
	}
	static Type getID(const std::string& name)
	{
		std::unordered_map<std::string, Type> typeMap;
		for (size_t i = 0; i < AvailableCount(); ++i)
		{
			typeMap.insert({ getName(getID(i)), getID(i) });
		}
		auto it = typeMap.find(name);
		return (it != typeMap.end()) ? it->second : NULL_FEATURE;
	}
}

class Classifier
{
public:
	Classifier(ccMainAppInterface* app = nullptr);
	//! Train parameters
	struct TrainParams
	{
		std::vector<int> classes_list = { 0,1 };
		int nscales = 5;
		double min_scale = -1;
		bool evaluate_params = false;
		int max_core_points = 0;
		size_t num_trees = 25;
		size_t max_depth = 20;
		std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > features = {};
		std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > eval_features = {};
	};
	//! Classify parameters
	struct ClassifyParams
	{
		std::vector<int> classes_list = { 0,1 };
		int nscales = 5;
		double min_scale = -1;
		bool export_features = false;
		Regularization::Method reg_type = Regularization::Method::NONE;
		std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > eval_features = {};
		CCCoreLib::ScalarField* labels = nullptr;
	};
	//! Trains classifier given two point clouds representing class #1 and class #2 (feature collection phase)
	QString train(ccPointCloud* cloud1, ccPointCloud* cloud2, std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > features1, std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > features2, const TrainParams& params = TrainParams());
	//! Trains classifier with multiple classes provided in a scalar field (feature collection phase)
	QString train(ccPointCloud* cloud, ccScalarField* scalarField, const TrainParams& params = TrainParams());
	//! Trains classifier (generic phase)
	QString train(Point_set pts, Feature_set& features, const TrainParams& params = TrainParams(), ccProgressDialog* progressDlg = nullptr, CCCoreLib::NormalizedProgress* nProgress = nullptr);
	//! Performs classification on input point cloud given a trained classifier configuration file
	std::pair<std::vector<int>, std::map<std::string, std::vector<float>>> classify(ccPointCloud* cloud1, QString classifierFilePath, const ClassifyParams& params = ClassifyParams());
	//! Extracts human readable information from trained classifier configuration file
	QStringList readETHZRandomForestClassifierData(QString classifierFilePath);

protected:
	//! Main application interface
	ccMainAppInterface* m_app;
};



#endif //Q_RFC_CGAL_CLASSIFIER_HEADER
