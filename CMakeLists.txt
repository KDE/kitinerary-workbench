# SPDX-FileCopyrightText: 2018-2021 Volker Krause <vkrause@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(kitinerary-workbench VERSION 0.0.1)

find_package(ECM 5.245 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMAddTests)
include(ECMQtDeclareLoggingCategory)
include(ECMSetupVersion)
include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)

set(CMAKE_AUTOUIC ON)
ecm_setup_version(PROJECT VARIABLE_PREFIX KITINERARY_WORKBENCH VERSION_HEADER kitinerary_workbench_version.h)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core5Compat)
find_package(KF6 REQUIRED COMPONENTS TextEditor KIO)
find_package(KPim6PkPass REQUIRED)
find_package(KPim6Itinerary 5.24.42 REQUIRED)

add_subdirectory(src)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
