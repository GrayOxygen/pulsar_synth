# Install script for directory: /Applications/JUCE

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/objdump")
endif()

set(CMAKE_BINARY_DIR "/Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build")

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/JUCE/modules/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/JUCE/extras/Build/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/JUCE-8.0.6" TYPE FILE FILES
    "/Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/JUCE/JUCEConfigVersion.cmake"
    "/Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/JUCE/JUCEConfig.cmake"
    "/Applications/JUCE/extras/Build/CMake/JUCECheckAtomic.cmake"
    "/Applications/JUCE/extras/Build/CMake/JUCEHelperTargets.cmake"
    "/Applications/JUCE/extras/Build/CMake/JUCEModuleSupport.cmake"
    "/Applications/JUCE/extras/Build/CMake/JUCEUtils.cmake"
    "/Applications/JUCE/extras/Build/CMake/JuceLV2Defines.h.in"
    "/Applications/JUCE/extras/Build/CMake/LaunchScreen.storyboard"
    "/Applications/JUCE/extras/Build/CMake/PIPAudioProcessor.cpp.in"
    "/Applications/JUCE/extras/Build/CMake/PIPAudioProcessorWithARA.cpp.in"
    "/Applications/JUCE/extras/Build/CMake/PIPComponent.cpp.in"
    "/Applications/JUCE/extras/Build/CMake/PIPConsole.cpp.in"
    "/Applications/JUCE/extras/Build/CMake/RecentFilesMenuTemplate.nib"
    "/Applications/JUCE/extras/Build/CMake/UnityPluginGUIScript.cs.in"
    "/Applications/JUCE/extras/Build/CMake/checkBundleSigning.cmake"
    "/Applications/JUCE/extras/Build/CMake/copyDir.cmake"
    "/Applications/JUCE/extras/Build/CMake/juce_runtime_arch_detection.cpp"
    "/Applications/JUCE/extras/Build/CMake/juce_LinuxSubprocessHelper.cpp"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/blueear/Documents/个人空间/coding/synth_dev/pulsar_synth/build/JUCE/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
