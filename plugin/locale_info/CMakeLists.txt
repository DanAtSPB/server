INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/sql ${CMAKE_SOURCE_DIR}/regex)

MYSQL_ADD_PLUGIN(LOCALES locale_info.cc RECOMPILE_FOR_EMBEDDED)

