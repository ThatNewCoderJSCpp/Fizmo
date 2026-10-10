include(GNUInstallDirs)
add_executable(fizmo-mathtext mathtext.cpp)
target_link_libraries(fizmo-mathtext PRIVATE fizmo::fizmo)
install(TARGETS fizmo-mathtext RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
