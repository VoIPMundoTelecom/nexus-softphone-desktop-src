################################################################################
# Nexus: arquivos de RUNTIME do linphone-sdk pre-compilado que entram no pacote do app.
# No build completo o SDK e um subprojeto e as regras de install dele levam isso para o pacote; com o SDK
# pre-compilado o app so tem as proprias regras, entao estas copiam o que o build completo instalaria
# (sem testers, headers, .lib, docs e o wrapper C#).
################################################################################
set(_nexus_sdk "${NEXUS_SDK_PRECOMPILADO}")

if(APPLE)
	# macOS (sub-build MONO_ARCH do TasksMacos.cmake): CMAKE_INSTALL_LIBDIR = Nexus.app/Contents/Frameworks.
	# Frameworks do SDK (plugins do mediastreamer2 e gramaticas do belle-sip vao dentro deles) + liblinphone++.
	install(DIRECTORY "${_nexus_sdk}/Frameworks/" DESTINATION "${CMAKE_INSTALL_LIBDIR}" USE_SOURCE_PERMISSIONS
		PATTERN "*-tester.framework" EXCLUDE)
	install(DIRECTORY "${_nexus_sdk}/lib/" DESTINATION "${CMAKE_INSTALL_LIBDIR}" USE_SOURCE_PERMISSIONS
		FILES_MATCHING PATTERN "*.dylib")
	return()
endif()

if(WIN32)
	install(DIRECTORY "${_nexus_sdk}/bin/" DESTINATION "${CMAKE_INSTALL_BINDIR}"
		FILES_MATCHING PATTERN "*.dll"
		PATTERN "*tester*" EXCLUDE
		PATTERN "CsWrapper.dll" EXCLUDE
		PATTERN "bcunit.dll" EXCLUDE
	)
	set(_nexus_sdk_plugin "*.dll")
else()
	install(DIRECTORY "${_nexus_sdk}/${CMAKE_INSTALL_LIBDIR}/" DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		USE_SOURCE_PERMISSIONS
		FILES_MATCHING PATTERN "*.so" PATTERN "*.so.*"
		PATTERN "*tester*" EXCLUDE
		PATTERN "cmake" EXCLUDE
		PATTERN "pkgconfig" EXCLUDE
	)
	set(_nexus_sdk_plugin "*.so")
endif()

# Plugins do mediastreamer2 (MSPLUGINS_DIR = lib/mediastreamer/plugins) e do liblinphone.
foreach(_dir mediastreamer liblinphone)
	if(EXISTS "${_nexus_sdk}/${CMAKE_INSTALL_LIBDIR}/${_dir}")
		install(DIRECTORY "${_nexus_sdk}/${CMAKE_INSTALL_LIBDIR}/${_dir}" DESTINATION "${CMAKE_INSTALL_LIBDIR}"
			USE_SOURCE_PERMISSIONS FILES_MATCHING PATTERN "${_nexus_sdk_plugin}")
	endif()
endforeach()

# Dados: gramaticas do belr (o liblinphone nao sobe sem elas), toques, imagens e certificados.
foreach(_dir belr sounds images linphone)
	if(EXISTS "${_nexus_sdk}/${CMAKE_INSTALL_DATAROOTDIR}/${_dir}")
		install(DIRECTORY "${_nexus_sdk}/${CMAKE_INSTALL_DATAROOTDIR}/${_dir}" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}")
	endif()
endforeach()
