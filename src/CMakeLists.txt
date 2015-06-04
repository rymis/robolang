
ADD_EXECUTABLE(roborun main.c x_sdl_sprite.c sdl_source.c robot_vm.c)
TARGET_LINK_LIBRARIES(roborun ${GLIB_LIBRARIES} ${SDL_LIBRARIES})

