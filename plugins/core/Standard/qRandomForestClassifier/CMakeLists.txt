option( PLUGIN_STANDARD_QRANDOMFORESTCLASSIFIER "Check to install qRandomForestClassifier plugin" ON )

if ( PLUGIN_STANDARD_QRANDOMFORESTCLASSIFIER )
	project( QRANDOMFORESTCLASSIFIER_PLUGIN )
	 
	AddPlugin( NAME ${PROJECT_NAME} )
		
	add_subdirectory( include )
	add_subdirectory( src )
	add_subdirectory( ui )

	find_package(CGAL REQUIRED)

	if( CCCORELIB_USE_TBB )
	  message("CCCORELIB_USE_TBB")
	  find_package(TBB QUIET)
	  include(CGAL_TBB_support)
	  add_definitions(-DQT_NO_KEYWORDS)
	else()
	  message("NO CCCORELIB_USE_TBB")
	endif()

	# set dependencies to necessary libraries
	target_link_libraries( ${PROJECT_NAME} CGAL::CGAL )

	if( TARGET CGAL::TBB_support )
	  target_link_libraries( ${PROJECT_NAME} 
	  ::TBB_support )
	else()
	  message("NOTICE: This project (optionally) uses TBB, parallel processing will be disabled.")
	endif()
endif()
