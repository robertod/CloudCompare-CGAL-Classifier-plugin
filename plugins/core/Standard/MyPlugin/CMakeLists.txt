# CloudCompare example for standard plugins

# Add an option to CMake to control whether we build this plugin or not
option( MY_PLUGIN_STANDARD "Install my plugin" ON )

if ( MY_PLUGIN_STANDARD )
	project( MyPlugin )
	
	find_package(CGAL REQUIRED)

	AddPlugin( NAME ${PROJECT_NAME} )
		
	add_subdirectory( include )
	add_subdirectory( src )
	
	# set dependencies to necessary libraries
	# target_link_libraries( ${PROJECT_NAME} LIB1 )
endif()
