find_package(Qt6 REQUIRED COMPONENTS Test)

function(dash_add_test name)
    add_executable(${name} ${name}.cpp ${ARGN})
    target_link_libraries(${name} PRIVATE dash_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60)
endfunction()

dash_add_test(tst_main_window)
dash_add_test(tst_obd)
dash_add_test(tst_socketcanbus)
