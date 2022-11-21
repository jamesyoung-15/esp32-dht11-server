# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/opt/esp-idf/components/bootloader/subproject"
  "/home/jamesyoung/Documents/Personal_Projects/Programming/Practice/esp32/practice/temp-sensor/build/bootloader"
  "/home/jamesyoung/Documents/Personal_Projects/Programming/Practice/esp32/practice/temp-sensor/build/bootloader-prefix"
  "/home/jamesyoung/Documents/Personal_Projects/Programming/Practice/esp32/practice/temp-sensor/build/bootloader-prefix/tmp"
  "/home/jamesyoung/Documents/Personal_Projects/Programming/Practice/esp32/practice/temp-sensor/build/bootloader-prefix/src/bootloader-stamp"
  "/home/jamesyoung/Documents/Personal_Projects/Programming/Practice/esp32/practice/temp-sensor/build/bootloader-prefix/src"
  "/home/jamesyoung/Documents/Personal_Projects/Programming/Practice/esp32/practice/temp-sensor/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/jamesyoung/Documents/Personal_Projects/Programming/Practice/esp32/practice/temp-sensor/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
