
include_directories(../..)

add_executable (scm_tutorial scm_tutorial.cpp)
target_link_libraries (scm_tutorial scm)
install (TARGETS scm_tutorial DESTINATION bin)

add_executable (test_state_machine test-StateMachine.cpp)
target_link_libraries (test_state_machine scm)
install (TARGETS test_state_machine DESTINATION bin)

add_executable (test_candy_machine test-CandyMachine.cpp)
target_link_libraries (test_candy_machine scm)
install (TARGETS test_candy_machine DESTINATION bin)

add_executable (test_history_machine test-HistoryMachine.cpp)
target_link_libraries (test_history_machine scm)
install (TARGETS test_history_machine DESTINATION bin)
