#----------------------------------------------------------------
# Generated CMake target import file for configuration "MinSizeRel".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "common::common" for configuration "MinSizeRel"
set_property(TARGET common::common APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(common::common PROPERTIES
  IMPORTED_IMPLIB_MINSIZEREL "${_IMPORT_PREFIX}/lib/common.lib"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/bin/common.dll"
  )

list(APPEND _cmake_import_check_targets common::common )
list(APPEND _cmake_import_check_files_for_common::common "${_IMPORT_PREFIX}/lib/common.lib" "${_IMPORT_PREFIX}/bin/common.dll" )

# Import target "common::common2" for configuration "MinSizeRel"
set_property(TARGET common::common2 APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(common::common2 PROPERTIES
  IMPORTED_IMPLIB_MINSIZEREL "${_IMPORT_PREFIX}/lib/common2.lib"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/bin/common2.dll"
  )

list(APPEND _cmake_import_check_targets common::common2 )
list(APPEND _cmake_import_check_files_for_common::common2 "${_IMPORT_PREFIX}/lib/common2.lib" "${_IMPORT_PREFIX}/bin/common2.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
