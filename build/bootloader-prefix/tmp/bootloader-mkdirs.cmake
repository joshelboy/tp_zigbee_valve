# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/statusC/esp/esp-idf/components/bootloader/subproject"
  "/Users/statusC/esp/z_github/HA_on_off_light/build/bootloader"
  "/Users/statusC/esp/z_github/HA_on_off_light/build/bootloader-prefix"
  "/Users/statusC/esp/z_github/HA_on_off_light/build/bootloader-prefix/tmp"
  "/Users/statusC/esp/z_github/HA_on_off_light/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/statusC/esp/z_github/HA_on_off_light/build/bootloader-prefix/src"
  "/Users/statusC/esp/z_github/HA_on_off_light/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/statusC/esp/z_github/HA_on_off_light/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/statusC/esp/z_github/HA_on_off_light/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
