################################################################################
#
#  Copyright (c) 2017-2024 Belledonne Communications SARL.
# 
#  This file is part of linphone-desktop
#  (see https://www.linphone.org).
# 
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <http://www.gnu.org/licenses/>.
#
################################################################################

cmake_minimum_required(VERSION 3.5)

set(TARGET_NAME Linphone)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")


if (GIT_EXECUTABLE AND NOT(LINPHONEAPP_VERSION))
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --always
		OUTPUT_VARIABLE LINPHONEAPP_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	)
endif()

if(NOT LINPHONEAPP_VERSION)
	bc_compute_full_version(LINPHONEAPP_VERSION)
endif()

if (NOT(LINPHONEAPP_VERSION))
  message(WARNING "Failed to compute Linphone app version from Git!")
  set(LINPHONEAPP_VERSION "6.2.1")
endif ()

include(${CMAKE_SOURCE_DIR}/Linphone/application_info.cmake)

set(LINPHONE_MAJOR_VERSION)
set(LINPHONE_MINOR_VERSION)
set(LINPHONE_MICRO_VERSION)
set(LINPHONE_BRANCH_VERSION)
bc_parse_full_version(${LINPHONEAPP_VERSION} LINPHONE_MAJOR_VERSION LINPHONE_MINOR_VERSION LINPHONE_MICRO_VERSION LINPHONE_BRANCH_VERSION)

set(LINPHONE_VERSION ${LINPHONE_MAJOR_VERSION}.${LINPHONE_MINOR_VERSION}.${LINPHONE_MICRO_VERSION})
set(PACKAGE_VERSION "${LINPHONEAPP_VERSION}")

install(TARGETS ${TARGET_NAME}
    BUNDLE DESTINATION .
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES "${CMAKE_SOURCE_DIR}/Linphone/data/config/linphonerc-factory" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${EXECUTABLE_NAME}")

set(LINPHONE_QML_DIR "${CMAKE_SOURCE_DIR}/Linphone/view")
set(QT_PATH "${Qt6Core_DIR}/../../..")

if(APPLE)
	configure_file("${CMAKE_SOURCE_DIR}/cmake/install/macos/entitlements.xml.in" "${CMAKE_BINARY_DIR}/cmake/install/macos/entitlements.xml" @ONLY)
	configure_file("${CMAKE_SOURCE_DIR}/cmake/install/macos/linphone.icns" "${CMAKE_BINARY_DIR}/cmake/install/macos/${EXECUTABLE_NAME}.icns" COPYONLY)
	set(APP_QT_CONF_PATH "[Paths]\nPlugins = PlugIns\nImports = Resources/qml\nQml2Imports = Resources/qml")

	#configure_file("${CMAKE_SOURCE_DIR}/Linphone/../assets/qt.conf.in" "${CMAKE_BINARY_DIR}/cmake/install/macos/qt.conf" @ONLY)
	#install(FILES "${CMAKE_BINARY_DIR}/cmake/install/macos/qt.conf" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/..")
	#install(FILES "${CMAKE_BINARY_DIR}/cmake/install/macos/Info.plist" DESTINATION "${APPLICATION_NAME}.app/Contents")
	install(FILES "${CMAKE_BINARY_DIR}/cmake/install/macos/${EXECUTABLE_NAME}.icns" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/..")
	file(GLOB SHARED_LIBRARIES "${LINPHONE_OUTPUT_DIR}/${CMAKE_INSTALL_LIBDIR}/lib*.dylib")
	if( ENABLE_OPENH264 )# Remove openH264 lib from the installation. this codec will be download by user
		foreach(item ${SHARED_LIBRARIES})
			get_filename_component(LIBRARY_FILENAME ${item} NAME)
			if("${LIBRARY_FILENAME}" MATCHES "^libopenh264.*.dylib$")
				list(REMOVE_ITEM SHARED_LIBRARIES ${item})
			endif()
		endforeach(item)
	endif()
	install(FILES ${SHARED_LIBRARIES} DESTINATION "${CMAKE_INSTALL_LIBDIR}")
	install(DIRECTORY "${LINPHONE_OUTPUT_DIR}/${CMAKE_INSTALL_DATAROOTDIR}/images" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}" USE_SOURCE_PERMISSIONS OPTIONAL)

	file(GLOB SHARED_LIBRARIES "${CMAKE_BINARY_DIR}/cmake/install/macos/${APPLICATION_NAME}.app/Contents/Frameworks/lib*.dylib")

	foreach (LIBRARY ${SHARED_LIBRARIES})
		get_filename_component(LIBRARY_FILENAME ${LIBRARY} NAME)
		message("Changing RPATH of ${LIBRARY_FILENAME} from '${LINPHONE_OUTPUT_DIR}/${CMAKE_INSTALL_LIBDIR}' to '@executable_path/../Frameworks'")
		execute_process(COMMAND install_name_tool -rpath "${LINPHONE_OUTPUT_DIR}/${CMAKE_INSTALL_LIBDIR}" "@executable_path/../Frameworks" "${LIBRARY}")
	endforeach ()
	install(TARGETS ${APP_PLUGIN}
		ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
	)
	install(CODE "execute_process(COMMAND install_name_tool -add_rpath \"@executable_path/../Frameworks/\" \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/${EXECUTABLE_NAME}\")")
	install(CODE "execute_process(COMMAND install_name_tool -add_rpath \"@executable_path/../lib/\" \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/${EXECUTABLE_NAME}\")")
	get_filename_component(_qt_bin_dir "${qmake_executable}" DIRECTORY)
	find_program(DEPLOYQT_PROGRAM macdeployqt HINTS "${_qt_bin_dir}")
	if (NOT DEPLOYQT_PROGRAM)
		message(FATAL_ERROR "Could not find the macdeployqt program. Make sure it is in the PATH.")
	endif()
	#Packaging is done by CPack in the cleanCPack.cmake file. But on mac, we need Qt files in .app
	if(NOT ENABLE_APP_PACKAGING)
		install(CODE "MESSAGE(\"MacDeploy install: execute_process(COMMAND ${DEPLOYQT_PROGRAM} ${APPLICATION_OUTPUT_DIR}/${APPLICATION_NAME}.app -qmldir=${LINPHONE_QML_DIR} -no-strip -verbose=0 -always-overwrite) \")")
		install(CODE "execute_process(COMMAND ${DEPLOYQT_PROGRAM} ${APPLICATION_OUTPUT_DIR}/${APPLICATION_NAME}.app -qmldir=${LINPHONE_QML_DIR} -no-strip -verbose=0 -always-overwrite)")
	endif()
elseif(WIN32)
	set(BIN_ARCH "win32")
	if( CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(MINGW_PACKAGE_PREFIX "mingw-w64-x86_64-")
		set(MINGW_TYPE "mingw64")
		set(BIN_ARCH "win64")
	else()
		set(MINGW_PACKAGE_PREFIX "mingw-w64-i686-")
		set(MINGW_TYPE "mingw32")
	endif()
	find_program(MSYS2_PROGRAM
		NAMES msys2_shell.cmd
		HINTS "C:/msys64/"
	)
	set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		set(CMAKE_INSTALL_DEBUG_LIBRARIES TRUE)
	endif()
	include(InstallRequiredSystemLibraries)
	find_file(UCRTBASE_LIB "ucrtbase.dll" PATHS "C:/Windows/System32")
	install(FILES ${UCRTBASE_LIB} DESTINATION "${CMAKE_INSTALL_BINDIR}")

	find_program(DEPLOYQT_PROGRAM windeployqt HINTS "${_qt_bin_dir}")
	if (NOT DEPLOYQT_PROGRAM)
		message(FATAL_ERROR "Could not find the windeployqt program. Make sure it is in the PATH.")
	endif ()
endif()

################################################################
#						CRASHPAD
################################################################
if(HAVE_CRASH_HANDLER)
	get_target_property(CRASHPAD_BIN_DIR Crashpad CRASHPAD_BIN_DIR)
	get_target_property(CRASHPAD_EXECUTABLE_NAME Crashpad CRASHPAD_EXECUTABLE_NAME)
	install(FILES "${CRASHPAD_BIN_DIR}/${CRASHPAD_EXECUTABLE_NAME}" DESTINATION ${CMAKE_INSTALL_BINDIR} PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
	if(ENABLE_BUGSPLAT_SYMBOLS_UPLOAD)
		if(NOT LINPHONEAPP_APPLICATION_NAME)
			message(FATAL_ERROR "Missing application details for Bugsplat. Please fill LINPHONEAPP_APPLICATION_NAME (${LINPHONEAPP_APPLICATION_NAME} - ${LINPHONEAPP_VERSION})")
		elseif(NOT BUGSPLAT_DATABASE)
			message(FATAL_ERROR "Missing database application details for Bugsplat. Please fill BUGSPLAT_DATABASE in application_info.cmake")
		elseif(NOT BUGSPLAT_CLIENT_ID OR NOT BUGSPLAT_CLIENT_SECRET)
			message(FATAL_ERROR "Missing identifications for Bugsplat. Please fill BUGSPLAT_CLIENT_ID and BUGSPLAT_CLIENT_SECRET to activate symbol uploading")
		else()
			message(STATUS "Preparing symbol uploader Bugsplat for ${LINPHONEAPP_APPLICATION_NAME} - ${LINPHONEAPP_VERSION}.")
			if(WIN32)
				set(CRASHPAD_SYMBOLS_UPLOADER "${CMAKE_BINARY_DIR}/symbol-upload-windows.exe")
				set(CRASHPAD_SYMBOLS_UPLOADER_URL "https://github.com/BugSplat-Git/symbol-upload/releases/download/v11.0.0/symbol-upload-windows.exe")
			else()
				set(CRASHPAD_SYMBOLS_UPLOADER "${CMAKE_BINARY_DIR}/symbol-upload-linux")
				set(CRASHPAD_SYMBOLS_UPLOADER_URL "https://github.com/BugSplat-Git/symbol-upload/releases/download/v11.0.0/symbol-upload-linux")
			endif()
			if(NOT EXISTS "${CRASHPAD_SYMBOLS_UPLOADER}")
				file(DOWNLOAD ${CRASHPAD_SYMBOLS_UPLOADER_URL} ${CRASHPAD_SYMBOLS_UPLOADER})
				file(CHMOD "${CRASHPAD_SYMBOLS_UPLOADER}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
			endif()
		endif()
	endif()
else()
	message(DEBUG "No Crash handler, do not configure Crashpad")
endif()

# ==============================================================================
#                               CPack.
# ==============================================================================


if(${ENABLE_APP_PACKAGING})
	set(CPACK_BINARY_STGZ OFF)
	set(CPACK_BINARY_TGZ OFF)
	set(CPACK_BINARY_TZ OFF)
	set(CPACK_PACKAGE_NAME "${APPLICATION_NAME}")
	set(CPACK_PACKAGE_VENDOR "${APPLICATION_VENDOR}")
	set(CPACK_PACKAGE_VERSION_MAJOR ${LINPHONE_MAJOR_VERSION})
	set(CPACK_PACKAGE_VERSION_MINOR ${LINPHONE_MINOR_VERSION})
	if (LINPHONE_MICRO_VERSION)
		set(CPACK_PACKAGE_VERSION_PATCH ${LINPHONE_MICRO_VERSION})
	endif ()
	set(CPACK_PACKAGE_EXECUTABLES "${EXECUTABLE_NAME};${APPLICATION_NAME}")
	set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
	set(CPACK_MONOLITHIC_INSTALL TRUE)# Need it since libturbo-jpeg introduce components (lib/include etc.)
	
	if(ENABLE_APP_LICENSE)
		set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
	else()
		unset(CPACK_RESOURCE_FILE_LICENSE)
	endif()
	set(CPACK_RESOURCE_FILE_LICENSE_PROVIDED ${ENABLE_APP_LICENSE})
	set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/Linphone/data/icon.ico")
	set(PERFORM_SIGNING 0)
	
	configure_file("${CMAKE_SOURCE_DIR}/cmake/install/cleanCPack.cmake.in" "${CMAKE_BINARY_DIR}/cmake/install/cleanCPack.cmake" @ONLY)
	set(CPACK_PRE_BUILD_SCRIPTS  "${CMAKE_BINARY_DIR}/cmake/install/cleanCPack.cmake")
	set(CPACK_PACKAGE_INSTALL_DIRECTORY ${APPLICATION_NAME})
	if(APPLE)
##############################################
#	APPLE
##############################################
		set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PACKAGE_VERSION}-mac")
		set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/cmake/install/macos/background_dmg.jpg")
		configure_file("${CMAKE_SOURCE_DIR}/cmake/install/macos/linphone_dmg.scpt.in" "${CMAKE_BINARY_DIR}/cmake/install/macos/linphone_dmg.scpt" @ONLY)
		set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_BINARY_DIR}/cmake/install/macos/linphone_dmg.scpt")
		set(CPACK_BINARY_DRAGNDROP ON)
		set(CPACK_BUNDLE_APPLE_CERT_APP ${LINPHONE_BUILDER_SIGNING_IDENTITY})
		set(PACKAGE_EXT "dmg")

		message(STATUS "Set DragNDrop CPack generator in OUTPUT/Packages")
	elseif(WIN32)
##############################################
#	WINDOWS
##############################################
		set(CPACK_GENERATOR "NSIS")
		set(DO_GENERATOR YES)
		set(PACKAGE_EXT "exe")
		set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PACKAGE_VERSION}-${BIN_ARCH}")
		set(CPACK_SOURCE_IGNORE_FILES "*Qt*.pdb;*.lib;*.cmake")
		set(CPACK_IGNORE_FILES ".*Qt.*\\.pdb$" ".*\\.lib$" ".*\\.cmake$") #Not documented

		find_program(NSIS_PROGRAM makensis)
		if(NOT NSIS_PROGRAM)
			if(ENABLE_WINDOWS_TOOLS_CHECK)
				message(STATUS "Installing windows tools for nsis")
				execute_process(
					COMMAND "${MSYS2_PROGRAM}" "-${MINGW_TYPE}" "-here" "-full-path" "-defterm" "-shell" "sh" "-l" "-c" "pacman -Sy ${MINGW_PACKAGE_PREFIX}nsis --noconfirm  --needed"
				)
			else()
				message(FATAL_ERROR "`makensis` is needed for packaging. Please use pacman -Sy ${MINGW_PACKAGE_PREFIX}nsis to install it.")
			endif()
		endif()
		set(PACKAGE_EXT "exe")
		# Use magic `NSIS.template.in` template from the current source directory to force uninstallation
		# and ensure that linphone is not running before installation.
		set(CPACK_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/install/windows")
		set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}\\\\cmake\\\\install\\\\windows\\\\nsis_banner.bmp")
		set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/Linphone/data/icon.ico")
		set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/Linphone/data/icon.ico")
		set(CPACK_NSIS_DISPLAY_NAME "${APPLICATION_NAME}")	# = The display name string that appears in the Windows Add/Remove Program control panel
		set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "${APPLICATION_NAME}")
		
		if (LINPHONE_MICRO_VERSION)# CPACK_NSIS_PACKAGE_NAME = Title at the top of the installer
			set(CPACK_NSIS_PACKAGE_NAME "${APPLICATION_NAME} ${LINPHONE_MAJOR_VERSION}.${LINPHONE_MINOR_VERSION}.${LINPHONE_MICRO_VERSION}")
		else ()
			set(CPACK_NSIS_PACKAGE_NAME "${APPLICATION_NAME} ${LINPHONE_MAJOR_VERSION}.${LINPHONE_MINOR_VERSION}")
		endif ()
		set(CPACK_NSIS_URL_INFO_ABOUT ${APPLICATION_URL})
		
		file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}" DOS_STYLE_BINARY_DIR)
		string(REPLACE "\\" "\\\\" ESCAPED_DOS_STYLE_BINARY_DIR "${DOS_STYLE_BINARY_DIR}")
		configure_file("${CMAKE_SOURCE_DIR}/cmake/install/windows/install.nsi.in" "${CMAKE_CURRENT_BINARY_DIR}/install.nsi" @ONLY)
		set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "!include \\\"${ESCAPED_DOS_STYLE_BINARY_DIR}\\\\install.nsi\\\"")
		configure_file("${CMAKE_SOURCE_DIR}/cmake/install/windows/uninstall.nsi.in" "${CMAKE_CURRENT_BINARY_DIR}/uninstall.nsi" @ONLY)
		set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "!include \\\"${ESCAPED_DOS_STYLE_BINARY_DIR}\\\\uninstall.nsi\\\"")
		set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
		set(CPACK_NSIS_MUI_FINISHPAGE_RUN "${EXECUTABLE_NAME}.exe")
		set(CPACK_NSIS_INSTALLED_ICON_NAME "${CPACK_NSIS_EXECUTABLES_DIRECTORY}/${CPACK_NSIS_MUI_FINISHPAGE_RUN}")
		message(STATUS "Set NSIS CPack generator in OUTPUT/Packages")

		# Nexus: MSI (CPack WiX 3) gerado depois do NSIS, com o mesmo conteudo e os mesmos registros de protocolo.
		if(ENABLE_APP_MSI)
			set(DO_MSI YES)
			# GUIDs FIXOS: mudar o UpgradeCode faz o Windows tratar como outro produto (sem atualizar o instalado).
			set(CPACK_WIX_UPGRADE_GUID "aba86b5a-66f5-4e30-8874-d8ee4762611e")
			set(NEXUS_WIX_REGISTRO_GUID "bedc04db-3f3a-4e75-a031-12019be3af8d")
			set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/Linphone/data/icon.ico")
			set(CPACK_WIX_PROGRAM_MENU_FOLDER "${APPLICATION_NAME}")
			set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "${APPLICATION_URL}")
			set(CPACK_WIX_PROPERTY_ARPHELPLINK "${APPLICATION_URL}")
			# Instala para a maquina (HKLM, Program Files); exige elevacao, como o NSIS. CMake >= 3.29.
			set(CPACK_WIX_INSTALL_SCOPE "perMachine")
			if(CMAKE_VERSION VERSION_LESS 3.29)
				message(WARNING "Nexus: CPACK_WIX_INSTALL_SCOPE precisa de CMake >= 3.29 (o MSI pode sair por usuario)")
			endif()
			# O MSI so compara os 3 primeiros campos da versao: o N de v<versao>-nexus.<N> entra no terceiro
			# (6.2.3-nexus.2 -> 6.2.302), senao nexus.1 e nexus.2 seriam a "mesma versao" e o upgrade nao acontece.
			# So o MSI (CPACK_PROJECT_CONFIG_FILE); nome e versao do NSIS continuam os de sempre.
			set(NEXUS_MSI_MICRO "${LINPHONE_MICRO_VERSION}")
			if(NOT NEXUS_MSI_MICRO)
				set(NEXUS_MSI_MICRO 0)
			endif()
			set(NEXUS_MSI_VERSION "${LINPHONE_MAJOR_VERSION}.${LINPHONE_MINOR_VERSION}.${NEXUS_MSI_MICRO}")
			if(LINPHONEAPP_VERSION MATCHES "-nexus\\.([0-9]+)")
				if(CMAKE_MATCH_1 GREATER_EQUAL 100)
					message(FATAL_ERROR "Nexus: -nexus.${CMAKE_MATCH_1} nao cabe na versao do MSI (use N < 100)")
				endif()
				math(EXPR NEXUS_MSI_PATCH "${NEXUS_MSI_MICRO} * 100 + ${CMAKE_MATCH_1}")
				set(NEXUS_MSI_VERSION "${LINPHONE_MAJOR_VERSION}.${LINPHONE_MINOR_VERSION}.${NEXUS_MSI_PATCH}")
			endif()
			configure_file("${CMAKE_SOURCE_DIR}/cmake/install/windows/cpack_config.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/nexus_cpack_config.cmake" @ONLY)
			set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/nexus_cpack_config.cmake")
			message(STATUS "Nexus: versao do MSI ${NEXUS_MSI_VERSION} (de ${LINPHONEAPP_VERSION})")

			# Esquemas proprios do Nexus: a chave HKCR\<esquema> e do Nexus e sai na desinstalacao.
			# Esquemas genericos (sip, sips, tel, callto): outros apps usam; a chave HKCR\<esquema> vai num componente
			# PERMANENTE e NeverOverwrite (nao sobrescreve a de outro app, nao some ao desinstalar). A associacao do
			# Nexus fica so no ProgID (Nexus.<esquema>) e em Capabilities\URLAssociations, que saem na desinstalacao.
			set(NEXUS_WIX_PROTOCOLOS "")
			set(NEXUS_WIX_GENERICOS "")
			set(NEXUS_WIX_GENERICOS_REFS "")
			set(NEXUS_WIX_GUID_sip "f00ec35f-f290-43dd-841b-684a4274157e")
			set(NEXUS_WIX_GUID_sips "1603160f-f7f6-400b-9542-0738c59f767f")
			set(NEXUS_WIX_GUID_tel "0bc6ba36-dd2c-4ec9-8e91-42bc70dfcf96")
			set(NEXUS_WIX_GUID_callto "49bddfb0-9f95-4457-a08d-d76505b70394")
			foreach(PROTOCOLO sip "sip-${EXECUTABLE_NAME}" "${EXECUTABLE_NAME}-sip" "${EXECUTABLE_NAME}-config" sips "sips-${EXECUTABLE_NAME}" "${EXECUTABLE_NAME}-sips" tel callto)
				if(DEFINED NEXUS_WIX_GUID_${PROTOCOLO})
					string(APPEND NEXUS_WIX_GENERICOS
						"\t\t\t<Component Id=\"NexusEsquema_${PROTOCOLO}\" Guid=\"${NEXUS_WIX_GUID_${PROTOCOLO}}\" Permanent=\"yes\" NeverOverwrite=\"yes\">\n"
						"\t\t\t\t<RegistryKey Root=\"HKCR\" Key=\"${PROTOCOLO}\"><RegistryValue Name=\"URL Protocol\" Type=\"string\" Value=\"\" KeyPath=\"yes\"/><RegistryValue Type=\"string\" Value=\"URL:${PROTOCOLO} Protocol\"/></RegistryKey>\n"
						"\t\t\t</Component>\n")
					string(APPEND NEXUS_WIX_GENERICOS_REFS "\t\t<ComponentRef Id=\"NexusEsquema_${PROTOCOLO}\"/>\n")
				else()
					string(APPEND NEXUS_WIX_PROTOCOLOS
						"\t\t\t\t<RegistryKey Root=\"HKCR\" Key=\"${PROTOCOLO}\"><RegistryValue Type=\"string\" Value=\"URL:${PROTOCOLO} Protocol\"/><RegistryValue Name=\"URL Protocol\" Type=\"string\" Value=\"\"/></RegistryKey>\n")
				endif()
				string(APPEND NEXUS_WIX_PROTOCOLOS
					"\t\t\t\t<RegistryValue Root=\"HKCR\" Key=\"${APPLICATION_NAME}.${PROTOCOLO}\" Type=\"string\" Value=\"${APPLICATION_NAME} ${PROTOCOLO} Protocol\"/>\n"
					"\t\t\t\t<RegistryValue Root=\"HKCR\" Key=\"${APPLICATION_NAME}.${PROTOCOLO}\\Shell\\Open\\Command\" Type=\"string\" Value=\"&quot;[INSTALL_ROOT]bin\\${EXECUTABLE_NAME}.exe&quot; &quot;%1&quot;\"/>\n"
					"\t\t\t\t<RegistryValue Root=\"HKLM\" Key=\"SOFTWARE\\${APPLICATION_VENDOR}\\${APPLICATION_NAME}\\Capabilities\\URLAssociations\" Name=\"${PROTOCOLO}\" Type=\"string\" Value=\"${APPLICATION_NAME}.${PROTOCOLO}\"/>\n")
			endforeach()
			configure_file("${CMAKE_SOURCE_DIR}/cmake/install/windows/wix_patch.xml.in" "${CMAKE_CURRENT_BINARY_DIR}/wix_patch.xml" @ONLY)
			set(CPACK_WIX_PATCH_FILE "${CMAKE_CURRENT_BINARY_DIR}/wix_patch.xml")
			# Licenca: CPACK_RESOURCE_FILE_LICENSE (LICENSE.txt), que o gerador WiX converte para RTF.
			message(STATUS "Nexus: MSI (CPack WiX) tambem sera gerado em OUTPUT/Packages")
		endif()
		
		
		if(LINPHONE_WINDOWS_SIGN_TOOL AND LINPHONE_WINDOWS_SIGN_TIMESTAMP_URL)
			find_program(SIGNTOOL ${LINPHONE_WINDOWS_SIGN_TOOL})
			set(TIMESTAMP_URL ${LINPHONE_WINDOWS_SIGN_TIMESTAMP_URL})
			set(SIGN_HASH ${LINPHONE_WINDOWS_SIGN_HASH})
			if (SIGNTOOL)
				set(SIGNTOOL_COMMAND ${SIGNTOOL})
				message("Found requested signtool")
				set(PERFORM_SIGNING 1)
			else ()
				message(STATUS "Could not find requested signtool! Code signing disabled (${LINPHONE_WINDOWS_SIGN_TOOL})")
			endif ()
		elseif(LINPHONE_WINDOWS_SIGNING_DIR)
			# Sign the installer.
			set(TIMESTAMP_URL "http://timestamp.digicert.com")
			set(PFX_FILE "${LINPHONE_WINDOWS_SIGNING_DIR}/linphone.pfx")
			set(PASSPHRASE_FILE "${LINPHONE_WINDOWS_SIGNING_DIR}/passphrase.txt")
			get_filename_component(WINSDK_DIR "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows;CurrentInstallFolder]" REALPATH CACHE)
			find_program(SIGNTOOL signtool PATHS ${WINSDK_DIR}/${CMAKE_INSTALL_BINDIR})
			if (EXISTS ${PFX_FILE})
				if (SIGNTOOL)
					set(SIGNTOOL_COMMAND ${SIGNTOOL})
					message("Found signtool and certificate ${PFX_FILE}")
					set(PERFORM_SIGNING 1)
				else ()
					message(STATUS "Could not find signtool! Code signing disabled (${SIGNTOOL})")
				endif ()
			else ()
				message(STATUS "No signtool certificate found; assuming development machine (${PFX_FILE})")
			endif ()
		endif ()
		
	else()
##############################################
#	LINUX
##############################################
		set(DO_APPIMAGE YES)
		message("Packaging AppImage as ${CPACK_PACKAGE_NAME}-${PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
		set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
		set(PACKAGE_EXT "AppImage")
		configure_file("${CMAKE_SOURCE_DIR}/cmake/install/linux/linphone.desktop.cmake" "${CMAKE_BINARY_DIR}/cmake/install/linux/${EXECUTABLE_NAME}.desktop" @ONLY)
		install(FILES "${CMAKE_BINARY_DIR}/cmake/install/linux/${EXECUTABLE_NAME}.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
		install(FILES "${CMAKE_SOURCE_DIR}/Linphone/data/image/logo.svg" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps/" RENAME "${EXECUTABLE_NAME}.svg")
		set(ICON_DIRS 16x16 22x22 24x24 32x32 64x64 128x128 256x256)
		foreach (DIR ${ICON_DIRS})
			install(FILES "${CMAKE_SOURCE_DIR}/Linphone/data/icon/hicolor/${DIR}/apps/icon.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${DIR}/apps/" RENAME "${EXECUTABLE_NAME}.png")
		endforeach ()
		message(STATUS "Set AppImage CPack generator in OUTPUT/Packages")
	endif()
##############################################
	configure_file("${CMAKE_SOURCE_DIR}/cmake/install/packaging.cmake.in" "${CMAKE_BINARY_DIR}/cmake/install/packaging.cmake" @ONLY)
	install(SCRIPT "${CMAKE_BINARY_DIR}/cmake/install/packaging.cmake")
	if(LINPHONE_SDK_MAKE_RELEASE_FILE_URL)
		bc_make_release_file("${LINPHONEAPP_VERSION}" "${LINPHONE_SDK_MAKE_RELEASE_FILE_URL}/${CPACK_PACKAGE_FILE_NAME}.${PACKAGE_EXT}")
		file(COPY "${CMAKE_INSTALL_PREFIX}/RELEASE" DESTINATION "${CMAKE_INSTALL_PREFIX}/Packages/")
	endif()
	include(CPack)
endif()
