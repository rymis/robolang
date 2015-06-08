
ADD_LIBRARY(robotvm robot_vm.c robot_obj_file.c)

ADD_EXECUTABLE(robot_run main.c x_sdl_sprite.c sdl_source.c)
TARGET_LINK_LIBRARIES(robot_run ${GLIB_LIBRARIES} ${SDL_LIBRARIES} robotvm)

ADD_EXECUTABLE(robot_as robot_as.c)
TARGET_LINK_LIBRARIES(robot_as ${GLIB_LIBRARIES} robotvm)

ADD_EXECUTABLE(robot_objdump robot_objdump.c)
TARGET_LINK_LIBRARIES(robot_objdump ${GLIB_LIBRARIES} robotvm)

ADD_EXECUTABLE(robot_ld robot_ld.c)
TARGET_LINK_LIBRARIES(robot_ld ${GLIB_LIBRARIES} robotvm)
