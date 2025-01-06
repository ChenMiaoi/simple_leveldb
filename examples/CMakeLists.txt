file(GLOB_RECURSE EXAMPLE_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

foreach(example ${EXAMPLE_SRCS})
  get_filename_component(example_name ${example} NAME_WE)
  
  set(target_output_dir ${CMAKE_BINARY_DIR}/bin/example)
  file(MAKE_DIRECTORY ${target_output_dir})
  
  add_executable(${example_name} ${example})
  target_link_libraries(${example_name} PRIVATE simple_leveldb)
  
  set_target_properties(${example_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${target_output_dir}
  )
endforeach()
