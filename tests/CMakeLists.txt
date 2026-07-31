include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")

add_executable(md2anki_tests
  test_util.cpp
  test_card.cpp
  test_parser.cpp
  test_io.cpp
  test_examples.cpp
)
target_link_libraries(md2anki_tests PRIVATE md2anki_lib Catch2::Catch2WithMain)
target_compile_definitions(md2anki_tests PRIVATE
  MD2ANKI_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../examples")

include(Catch)
catch_discover_tests(md2anki_tests)
