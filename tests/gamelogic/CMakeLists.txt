mu_add_test(
    NAME baski_prototype_profile_tests
    SOURCES test_baski_prototype_profile.cpp
)

mu_add_test(
    NAME test_boid_steering
    SOURCES
        test_boid_steering.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_head_movement
    SOURCES
        test_head_movement.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_earthquake_falloff
    SOURCES
        test_earthquake_falloff.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_reference_frame_steps
    SOURCES
        test_reference_frame_steps.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_direction_turning
    SOURCES
        test_direction_turning.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_personal_shop_currency
    SOURCES
        test_personal_shop_currency.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME ferea_monster_profile_tests
    SOURCES test_ferea_monster_profile.cpp
)
