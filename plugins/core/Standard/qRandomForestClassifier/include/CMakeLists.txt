
target_sources( ${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_LIST_DIR}/qRandomForestClassifier.h
		${CMAKE_CURRENT_LIST_DIR}/qRFCTrainingDialog.h
		${CMAKE_CURRENT_LIST_DIR}/qRFCFeatureDialog.h
		${CMAKE_CURRENT_LIST_DIR}/qRFCEvaluationDialog.h
		${CMAKE_CURRENT_LIST_DIR}/qRFCClassifDialog.h
		${CMAKE_CURRENT_LIST_DIR}/qRFCNoticeDialog.h
		${CMAKE_CURRENT_LIST_DIR}/qRFCTools.h
		${CMAKE_CURRENT_LIST_DIR}/Classifier.h
)

target_include_directories( ${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)
