cmake_minimum_required(VERSION 3.12)
project(mpi_examples C)

set(CMAKE_C_STANDARD 11)

# Find all .c files in examples/
file(GLOB EXAMPLES_SRC "examples/*.c")

find_package(MPI REQUIRED)

foreach(example_src ${EXAMPLES_SRC})
    get_filename_component(example_name ${example_src} NAME_WE)
    add_executable(${example_name} ${example_src})
    target_link_libraries(${example_name} MPI::MPI_C m)
endforeach()