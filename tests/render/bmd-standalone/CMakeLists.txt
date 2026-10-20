cmake_minimum_required(VERSION 3.24)
project(BmdLoaderTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT CRYPTOPP_ROOT OR NOT CRYPTOPP_LIBRARY)
    message(FATAL_ERROR "Set CRYPTOPP_ROOT (containing cryptopp headers) and CRYPTOPP_LIBRARY")
endif()
add_library(bmd_loader STATIC
    ${ROOT}/src/source/Render/Models/BmdModelFile.cpp
    ${ROOT}/src/source/Render/Models/BmdModulusCipher.cpp
    ${ROOT}/src/source/Render/Models/LegacyMarsDecryption.cpp)
target_compile_definitions(bmd_loader PUBLIC CRYPTOPP_DISABLE_ASM)
target_include_directories(bmd_loader PUBLIC ${ROOT}/src/source ${CRYPTOPP_ROOT})
target_link_libraries(bmd_loader PUBLIC ${CRYPTOPP_LIBRARY})
add_executable(bmd_loader_tests ${ROOT}/tests/main.cpp
    ${ROOT}/tests/render/test_bmd_model_file.cpp ${ROOT}/tests/render/test_bmd_modulus_cipher.cpp)
target_include_directories(bmd_loader_tests PRIVATE ${ROOT}/tests/third_party/doctest)
target_link_libraries(bmd_loader_tests PRIVATE bmd_loader)
add_executable(bmd_loader_probe probe.cpp)
target_link_libraries(bmd_loader_probe PRIVATE bmd_loader)
enable_testing()
add_test(NAME bmd_loader_tests COMMAND bmd_loader_tests)
