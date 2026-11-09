################################################################################
# Nexus: carrega os alvos IMPORTADOS do linphone-sdk pre-compilado (bctoolbox, ortp, belle-sip,
# mediastreamer2, liblinphone, lime... e o liblinphone++ gerado por cmake/sdk-precompilado).
#
# Os Find*.cmake do app (cmake/Modules) usam o alvo se ele ja existir, entao com os alvos carregados aqui
# o Linphone/CMakeLists.txt encontra o SDK sem mudanca. So inclui os arquivos de export gerados pelo
# install(EXPORT) (*Targets.cmake / *-targets.cmake); os Config.cmake chamariam find_dependency de coisas
# que o zip ja traz embutidas.
################################################################################
# macOS (zip so com frameworks) e Linux (bibliotecas do AppImage da Belledonne + headers do fonte) nao tem
# os arquivos de export, entao os alvos sao declarados aqui.
# alvo:nome(framework no macOS, lib<nome>.so.* no Linux):dependencias(separadas por virgula)
set(NEXUS_SDK_FRAMEWORKS
	"bctoolbox:bctoolbox:"
	"ortp:ortp:bctoolbox"
	"belr:belr:bctoolbox"
	"belle-sip:belle-sip:belr,bctoolbox"
	"mediastreamer2:mediastreamer2:ortp,bctoolbox"
	"lime:lime:bctoolbox"
	"liblinphone:linphone:mediastreamer2,ortp,belle-sip,belr,lime,bctoolbox"
)
macro(nexus_sdk_precompilado_frameworks _sdk)
	foreach(_nexus_fw_def IN LISTS NEXUS_SDK_FRAMEWORKS)
		string(REPLACE ":" ";" _nexus_fw_def "${_nexus_fw_def}")
		list(GET _nexus_fw_def 0 _nexus_fw_alvo)
		list(GET _nexus_fw_def 1 _nexus_fw_nome)
		list(LENGTH _nexus_fw_def _nexus_fw_n)
		set(_nexus_fw_deps "")
		if(_nexus_fw_n GREATER 2)
			list(GET _nexus_fw_def 2 _nexus_fw_deps)
			string(REPLACE "," ";" _nexus_fw_deps "${_nexus_fw_deps}")
		endif()
		if(TARGET ${_nexus_fw_alvo})
			continue()
		endif()
		add_library(${_nexus_fw_alvo} SHARED IMPORTED)
		if(APPLE)
			set_target_properties(${_nexus_fw_alvo} PROPERTIES
				FRAMEWORK TRUE
				IMPORTED_LOCATION "${_sdk}/Frameworks/${_nexus_fw_nome}.framework/${_nexus_fw_nome}"
				INTERFACE_COMPILE_OPTIONS "-F${_sdk}/Frameworks"
				INTERFACE_LINK_OPTIONS "-F${_sdk}/Frameworks"
				INTERFACE_LINK_LIBRARIES "${_nexus_fw_deps}"
			)
		else()
			file(GLOB _nexus_fw_so "${_sdk}/lib/lib${_nexus_fw_nome}.so.*")
			if(NOT _nexus_fw_so)
				message(FATAL_ERROR "Nexus: lib${_nexus_fw_nome}.so.* ausente em ${_sdk}/lib")
			endif()
			list(SORT _nexus_fw_so)
			list(GET _nexus_fw_so 0 _nexus_fw_so)
			if(NOT EXISTS "${_nexus_fw_so}")
				message(FATAL_ERROR "Nexus: lib${_nexus_fw_nome}.so.* ausente em ${_sdk}/lib")
			endif()
			get_filename_component(_nexus_fw_soname "${_nexus_fw_so}" NAME)
			set_target_properties(${_nexus_fw_alvo} PROPERTIES
				IMPORTED_LOCATION "${_nexus_fw_so}"
				IMPORTED_SONAME "${_nexus_fw_soname}"
				INTERFACE_INCLUDE_DIRECTORIES "${_sdk}/include"
				# dependencias indiretas (mbedtls, srtp, soci...) ficam no mesmo lib/
				INTERFACE_LINK_OPTIONS "-Wl,-rpath-link,${_sdk}/lib"
				INTERFACE_LINK_LIBRARIES "${_nexus_fw_deps}"
			)
		endif()
	endforeach()
	# O liblinphone++ (dylib em lib/) entra pelo glob de exports abaixo.
endmacro()

macro(nexus_sdk_precompilado_alvos _sdk)
	if((APPLE AND EXISTS "${_sdk}/Frameworks/linphone.framework") OR (UNIX AND NOT APPLE))
		nexus_sdk_precompilado_frameworks("${_sdk}")
	endif()
	file(GLOB _nexus_sdk_exports LIST_DIRECTORIES false
		"${_sdk}/share/*/cmake/*Targets.cmake"
		"${_sdk}/share/*/*Targets.cmake"
		"${_sdk}/lib/cmake/*/*Targets.cmake"
		"${_sdk}/lib/cmake/jsoncpp/jsoncpp-targets.cmake"
	)
	# Os -<config>.cmake sao incluidos pelo arquivo principal.
	list(FILTER _nexus_sdk_exports EXCLUDE REGEX "-(release|relwithdebinfo|debug|minsizerel)\\.cmake$")
	# Cada export confere, ao ser incluido, que os alvos de que depende ja existem (bctoolbox antes de
	# belle-sip...). Inclui em passadas: so entra quem ja tem as dependencias; para quando nao ha progresso.
	set(_nexus_sdk_pendentes ${_nexus_sdk_exports})
	set(_nexus_sdk_progresso TRUE)
	while(_nexus_sdk_pendentes AND _nexus_sdk_progresso)
		set(_nexus_sdk_progresso FALSE)
		set(_nexus_sdk_resto "")
		foreach(_nexus_sdk_export IN LISTS _nexus_sdk_pendentes)
			file(STRINGS "${_nexus_sdk_export}" _nexus_sdk_req REGEX "^foreach\\(_target ")
			string(REGEX MATCHALL "\"[^\"]+\"" _nexus_sdk_req "${_nexus_sdk_req}")
			set(_nexus_sdk_ok TRUE)
			foreach(_nexus_sdk_dep IN LISTS _nexus_sdk_req)
				string(REPLACE "\"" "" _nexus_sdk_dep "${_nexus_sdk_dep}")
				if(NOT TARGET ${_nexus_sdk_dep})
					set(_nexus_sdk_ok FALSE)
				endif()
			endforeach()
			if(_nexus_sdk_ok)
				message(STATUS "Nexus SDK pre-compilado: ${_nexus_sdk_export}")
				include("${_nexus_sdk_export}")
				set(_nexus_sdk_progresso TRUE)
			else()
				list(APPEND _nexus_sdk_resto "${_nexus_sdk_export}")
			endif()
		endforeach()
		set(_nexus_sdk_pendentes ${_nexus_sdk_resto})
	endwhile()
	if(_nexus_sdk_pendentes)
		message(WARNING "Nexus: exports do SDK sem as dependencias (ignorados): ${_nexus_sdk_pendentes}")
	endif()
	foreach(_nexus_sdk_alvo bctoolbox ortp belle-sip mediastreamer2 liblinphone)
		if(NOT TARGET ${_nexus_sdk_alvo})
			message(FATAL_ERROR "Nexus: alvo ${_nexus_sdk_alvo} ausente no SDK pre-compilado ${_sdk}")
		endif()
	endforeach()
	# O zip e RelWithDebInfo; o app e Release.
	set(CMAKE_MAP_IMPORTED_CONFIG_RELEASE "Release;RelWithDebInfo;")
	set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL "Release;RelWithDebInfo;")
	# Propriedades que o build completo poe nos alvos e que os Find*.cmake do app leem.
	set_target_properties(liblinphone PROPERTIES LIBLINPHONE_PLUGINS_DIR "${_sdk}/${CMAKE_INSTALL_LIBDIR}/liblinphone/plugins")
	set_target_properties(mediastreamer2 PROPERTIES MS2_PLUGINS_DIR "${_sdk}/${CMAKE_INSTALL_LIBDIR}/mediastreamer/plugins")
	# bc_compute_full_version / bc_parse_full_version (no build completo vem do subprojeto bctoolbox).
	include("${_sdk}/share/BCToolbox/cmake/BCToolboxCMakeUtils.cmake")
endmacro()
