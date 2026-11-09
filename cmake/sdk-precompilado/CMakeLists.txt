################################################################################
# Nexus: liblinphone++ (wrapper C++ do liblinphone) sobre o linphone-sdk PRE-COMPILADO.
#
# O zip publico do SDK (download.linphone.org/releases/<so>/sdk) traz a API C do liblinphone e todas as
# dependencias ja compiladas, mas nao o wrapper C++ que o app usa. Este projeto gera o wrapper do mesmo
# jeito que o liblinphone gera (doxygen sobre os headers -> XML -> genwrapper.py) a partir do fonte do
# liblinphone NO MESMO COMMIT do SDK (linphone-sdk no GitHub da Belledonne), compila e instala no
# diretorio do SDK como LinphoneCxx, com o layout que o build completo deixaria.
#
# Uso (scripts/sdk-precompilado.sh prepara as duas pastas):
#   cmake -S cmake/sdk-precompilado -B <build> -DLIBLINPHONE_SOURCE_DIR=<linphone-sdk>/liblinphone \
#         -DNEXUS_SDK_PRECOMPILADO=<sdk> -DCMAKE_INSTALL_PREFIX=<sdk>
#   cmake --build <build> --target install
################################################################################
cmake_minimum_required(VERSION 3.22)
project(NexusLinphoneCxx LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

if(NOT LIBLINPHONE_SOURCE_DIR OR NOT EXISTS "${LIBLINPHONE_SOURCE_DIR}/wrappers/cpp/genwrapper.py")
	message(FATAL_ERROR "LIBLINPHONE_SOURCE_DIR deve apontar para linphone-sdk/liblinphone (no commit do SDK)")
endif()
if(NOT NEXUS_SDK_PRECOMPILADO OR NOT (EXISTS "${NEXUS_SDK_PRECOMPILADO}/include/linphone/core.h"
		OR EXISTS "${NEXUS_SDK_PRECOMPILADO}/Frameworks/linphone.framework/Headers/core.h"))
	message(FATAL_ERROR "NEXUS_SDK_PRECOMPILADO deve apontar para o SDK extraido (include/linphone/core.h ou, no macOS, Frameworks/linphone.framework)")
endif()
set(LL "${LIBLINPHONE_SOURCE_DIR}")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Doxygen REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/alvos.cmake")
nexus_sdk_precompilado_alvos("${NEXUS_SDK_PRECOMPILADO}")

# Versao do liblinphone no fonte (project(LibLinphone VERSION x.y.z ...)).
file(STRINGS "${LL}/CMakeLists.txt" _ll_project REGEX "^project\\(LibLinphone VERSION")
string(REGEX MATCH "VERSION ([0-9]+\\.[0-9]+\\.[0-9]+)" _ll_versao "${_ll_project}")
set(LINPHONE_VERSION "${CMAKE_MATCH_1}")
set(LINPHONE_SO_VERSION "10") # o mesmo de liblinphone/CMakeLists.txt
message(STATUS "Nexus: liblinphone++ ${LINPHONE_VERSION} sobre ${NEXUS_SDK_PRECOMPILADO}")

# ---- 1. XML da API C (doxygen), como em liblinphone/coreapi/help/doc/doxygen ----
# include/CMakeLists.txt do liblinphone define LINPHONE_HEADER_FILES e LINPHONE_HEADER_ROOT_DIR.
add_subdirectory("${LL}/include" "${CMAKE_BINARY_DIR}/liblinphone-include" EXCLUDE_FROM_ALL)
set(top_srcdir "${LL}")
set(DOXYGEN_INPUT "")
foreach(_h ${LINPHONE_HEADER_FILES})
	string(APPEND DOXYGEN_INPUT " \"${_h}\"")
endforeach()
set(_dox_dir "${CMAKE_BINARY_DIR}/doxygen")
file(MAKE_DIRECTORY "${_dox_dir}")
configure_file("${LL}/coreapi/help/doc/doxygen/Doxyfile.in" "${_dox_dir}/Doxyfile")
# So o XML interessa (o HTML da doc C nao e instalado).
file(APPEND "${_dox_dir}/Doxyfile" "\nGENERATE_HTML = NO\n")
add_custom_command(OUTPUT "${_dox_dir}/xml/index.xml"
	COMMAND ${CMAKE_COMMAND} -E remove_directory "${_dox_dir}/xml"
	COMMAND ${DOXYGEN_EXECUTABLE} "${_dox_dir}/Doxyfile"
	WORKING_DIRECTORY "${_dox_dir}"
	DEPENDS "${_dox_dir}/Doxyfile" ${LINPHONE_HEADER_FILES}
	COMMENT "doxygen: XML da API C do liblinphone"
)

# ---- 2. Fontes do wrapper (genwrapper.py), como em liblinphone/wrappers/cpp ----
set(_cxx_dir "${CMAKE_BINARY_DIR}/cxx")
file(MAKE_DIRECTORY "${_cxx_dir}")
add_custom_command(OUTPUT "${_cxx_dir}/include/linphone++/linphone.hh" "${_cxx_dir}/src/linphone++.cc"
	COMMAND ${CMAKE_COMMAND} -E remove_directory "${_cxx_dir}/include" "${_cxx_dir}/src"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${_cxx_dir}/include/linphone++" "${_cxx_dir}/src"
	# Sem -o: o argparse do genwrapper.py declara '-o --output' como um nome so; gera no diretorio atual.
	COMMAND ${Python3_EXECUTABLE} "${LL}/wrappers/cpp/genwrapper.py" "${_dox_dir}/xml"
	WORKING_DIRECTORY "${_cxx_dir}"
	DEPENDS "${_dox_dir}/xml/index.xml" "${LL}/wrappers/cpp/genwrapper.py"
	COMMENT "genwrapper.py: fontes do liblinphone++"
)

# ---- 3. Biblioteca ----
add_library(liblinphone++ SHARED
	"${LL}/wrappers/cpp/object.cc"
	"${LL}/wrappers/cpp/tools.cc"
	"${_cxx_dir}/src/linphone++.cc"
)
if(APPLE)
	set_target_properties(liblinphone++ PROPERTIES OUTPUT_NAME linphone++)
else()
	set_target_properties(liblinphone++ PROPERTIES OUTPUT_NAME liblinphone++)
endif()
set_target_properties(liblinphone++ PROPERTIES PREFIX "" SOVERSION ${LINPHONE_SO_VERSION})
if(MSVC)
	set_target_properties(liblinphone++ PROPERTIES PDB_NAME liblinphone++)
	target_compile_options(liblinphone++ PRIVATE /bigobj)
endif()
target_compile_definitions(liblinphone++ PRIVATE "LINPHONECXX_EXPORTS")
target_link_libraries(liblinphone++ PRIVATE bctoolbox belle-sip liblinphone)
target_include_directories(liblinphone++
	PUBLIC
		$<BUILD_INTERFACE:${_cxx_dir}/include>
		$<BUILD_INTERFACE:${LL}/wrappers/cpp>
		$<INSTALL_INTERFACE:include>
)

install(TARGETS liblinphone++ EXPORT LinphoneCxxTargets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES "${LL}/wrappers/cpp/object.hh" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/linphone++)
install(DIRECTORY "${_cxx_dir}/include/linphone++" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

include(CMakePackageConfigHelpers)
write_basic_package_version_file("${CMAKE_BINARY_DIR}/LinphoneCxxVersion.cmake"
	VERSION ${LINPHONE_VERSION} COMPATIBILITY AnyNewerVersion)
configure_file("${LL}/wrappers/cpp/LinphoneCxxConfig.cmake.in" "${CMAKE_BINARY_DIR}/LinphoneCxxConfig.cmake" @ONLY)
install(EXPORT LinphoneCxxTargets
	FILE LinphoneCxxTargets.cmake
	DESTINATION "${CMAKE_INSTALL_DATADIR}/LinphoneCxx/cmake"
)
install(FILES "${CMAKE_BINARY_DIR}/LinphoneCxxConfig.cmake" "${CMAKE_BINARY_DIR}/LinphoneCxxVersion.cmake"
	DESTINATION "${CMAKE_INSTALL_DATADIR}/LinphoneCxx/cmake"
)
