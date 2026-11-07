find_package(Qt6 REQUIRED COMPONENTS Test QuickWidgets)

function(dash_add_test name)
    add_executable(${name} ${name}.cpp ${ARGN})
    target_link_libraries(${name} PRIVATE dash_core Qt6::Test Qt6::QuickWidgets)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 60)
endfunction()

dash_add_test(tst_main_window)
dash_add_test(tst_obd)
dash_add_test(tst_socketcanbus)
dash_add_test(tst_stylesheet)
dash_add_test(tst_stretch_view)
dash_add_test(tst_keypad)
