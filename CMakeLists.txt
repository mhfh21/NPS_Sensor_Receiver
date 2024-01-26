cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test1)

list(APPEND EXTRA_COMPONENT_DIRS "components/mpu6050")
list(APPEND EXTRA_COMPONENT_DIRS "components/mirf")
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/examples/common_components/protocol_examples_common")

idf_build_get_property(project_dir PROJECT_DIR)
idf_build_get_property(openocd_files OPENOCD_FILES)
set(OPENOCD_FILES "${OPENOCD_FILES} ${project_dir}/mpu6050_upload.cfg")
idf_build_set_property(PROJECT_DIR ${project_dir} FORCE)