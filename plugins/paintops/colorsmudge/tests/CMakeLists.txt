kis_add_test(
    KisColorsmudgeOpTest.cpp
     $<TARGET_PROPERTY:kritatestsdk,SOURCE_DIR>/stroke_testing_utils.cpp
    TEST_NAME KisColorsmudgeOpTest
    LINK_LIBRARIES kritalibpaintop kritaimage kritatestsdk
    )

# krita-spectral
kis_add_test(
    KisColorSmudgeSpectralTest.cpp
     $<TARGET_PROPERTY:kritatestsdk,SOURCE_DIR>/stroke_testing_utils.cpp
    TEST_NAME KisColorSmudgeSpectralTest
    LINK_LIBRARIES kritalibpaintop kritaimage kritatestsdk
    )
