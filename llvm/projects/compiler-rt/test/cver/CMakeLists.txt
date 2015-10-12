set(CVER_LIT_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(CVER_LIT_TEST_MODE "Standalone")
configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.in
  ${CMAKE_CURRENT_BINARY_DIR}/CverConfig/lit.site.cfg)
set(CVER_TESTSUITES ${CMAKE_CURRENT_BINARY_DIR}/CverConfig)

set(CVER_TEST_DEPS ${SANITIZER_COMMON_LIT_TEST_DEPS})
if(NOT COMPILER_RT_STANDALONE_BUILD)
  list(APPEND CVER_TEST_DEPS cver asan)
endif()

add_lit_testsuite(check-cver "Running CastVerifier tests"
  ${CVER_TESTSUITES}
  DEPENDS ${CVER_TEST_DEPS})
set_target_properties(check-cver PROPERTIES FOLDER "Cver unittests")
