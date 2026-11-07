# Smoke tests for Mist host and client executables

add_test(NAME smoke_host_version
    COMMAND mist_host --version
)

add_test(NAME smoke_host_self_test
    COMMAND mist_host --self-test
)

add_test(NAME smoke_host_list_displays
    COMMAND mist_host --list-displays
)

add_test(NAME smoke_client_version
    COMMAND mist_client --version
)

add_test(NAME smoke_client_self_test
    COMMAND mist_client --self-test
)

set_tests_properties(
    smoke_host_version
    smoke_host_self_test
    smoke_host_list_displays
    smoke_client_version
    smoke_client_self_test
    PROPERTIES
        TIMEOUT 15
)
