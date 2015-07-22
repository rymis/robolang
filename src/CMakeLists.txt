ADD_SUBDIRECTORY(lua)
INCLUDE(lua/check.cmake)

ADD_DEFINITIONS(-I${LUA_INCLUDE_DIR})

ADD_LIBRARY(robotvm robot_vm.c robot_obj_file.c)

ADD_EXECUTABLE(robot_run main.c robot_sprite.c sdl_source.c robot_labirinth.c robot_idrawable.c robot_scene.c robot_robot.c robot_xml.c)
TARGET_LINK_LIBRARIES(robot_run ${GLIB_LIBRARIES} ${SDL_LIBRARIES} ${LUA_LIBRARIES} robotvm)

ADD_EXECUTABLE(robot_editor editor.c)
TARGET_LINK_LIBRARIES(robot_editor ${GTK_LIBRARIES})

ADD_EXECUTABLE(robot_as robot_as.c)
TARGET_LINK_LIBRARIES(robot_as ${GLIB_LIBRARIES} robotvm)

ADD_EXECUTABLE(robot_objdump robot_objdump.c)
TARGET_LINK_LIBRARIES(robot_objdump ${GLIB_LIBRARIES} robotvm)

ADD_EXECUTABLE(robot_ld robot_ld.c)
TARGET_LINK_LIBRARIES(robot_ld ${GLIB_LIBRARIES} robotvm)

ADD_EXECUTABLE(robot_vm robot_vm_exec.c)
TARGET_LINK_LIBRARIES(robot_vm ${GLIB_LIBRARIES} robotvm)

ADD_EXECUTABLE(test_xml test_xml.c robot_xml.c)
TARGET_LINK_LIBRARIES(test_xml ${GLIB_LIBRARIES})


ADD_EXECUTABLE(test_linalg test_linalg.c)

