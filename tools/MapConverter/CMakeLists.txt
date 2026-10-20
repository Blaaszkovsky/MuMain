cmake_minimum_required(VERSION 3.24)
project(MapConverter LANGUAGES CXX)
include(FetchContent)
FetchContent_Declare(cryptopp
    URL https://codeload.github.com/weidai11/cryptopp/tar.gz/refs/tags/CRYPTOPP_8_9_0
    URL_HASH SHA256=ab5174b9b5c6236588e15a1aa1aaecb6658cdbe09501c7981ac8db276a24d9ab
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(cryptopp)
configure_file("${cryptopp_SOURCE_DIR}/License.txt" "${CMAKE_CURRENT_BINARY_DIR}/CryptoPP.License.txt" COPYONLY)
# Build the required ciphers and their support from pinned source, never a bundled DLL/lib.
set(crypto_sources 3way.cpp cast.cpp casts.cpp gost.cpp idea.cpp mars.cpp marss.cpp
    rc5.cpp rc6.cpp tea.cpp cryptlib.cpp misc.cpp allocate.cpp algparam.cpp
    filters.cpp fips140.cpp queue.cpp mqueue.cpp simple.cpp integer.cpp algebra.cpp
    nbtheory.cpp asn.cpp iterhash.cpp sha.cpp pubkey.cpp primetab.cpp strciphr.cpp)
list(TRANSFORM crypto_sources PREPEND "${cryptopp_SOURCE_DIR}/")
add_library(map_crypto STATIC ${crypto_sources} CryptoTemplates.cpp)
target_compile_definitions(map_crypto PUBLIC CRYPTOPP_DISABLE_ASM)
target_include_directories(map_crypto SYSTEM PUBLIC "${cryptopp_SOURCE_DIR}")
add_library(map_envelope STATIC MapEnvelope.cpp)
target_compile_features(map_envelope PUBLIC cxx_std_20)
target_link_libraries(map_envelope PUBLIC map_crypto)
add_executable(map_envelope_decode Main.cpp)
target_link_libraries(map_envelope_decode PRIVATE map_envelope)
include(CTest)
if(BUILD_TESTING)
    add_executable(map_envelope_tests tests/EnvelopeTests.cpp)
    target_include_directories(map_envelope_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(map_envelope_tests PRIVATE map_envelope)
    add_test(NAME map_envelope_tests COMMAND map_envelope_tests)
endif()
