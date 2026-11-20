add_library(graw STATIC src/reader.cpp)
target_include_directories(graw PUBLIC include)
set_target_properties(graw PROPERTIES POSITION_INDEPENDENT_CODE ON)
option(BUILD_BINDINGS "Build Python module" ON)
if(BUILD_BINDINGS)
	find_package(
		Python
		COMPONENTS Interpreter Development.Module
		REQUIRED)
	find_package(nanobind CONFIG REQUIRED)
	nanobind_add_module(_bindings bindings/module.cpp)
	target_link_libraries(_bindings PRIVATE graw)
	install(TARGETS _bindings LIBRARY DESTINATION cobo_viewer)
endif()
