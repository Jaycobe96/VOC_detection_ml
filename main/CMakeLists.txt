# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS main.c bluetooth_wrapper.c hw.c sensordata_wrapper.c i2c2_bme688.c spi2.c i2c2.c sampler.c sampler_ctrl.c led_strip_rmt_ws2812.c led_ctrl.c ml_process.c ml_sampler.c ml_tools.c matrix.c ml_vector.c heapsort.c  # list the source files of this component
    INCLUDE_DIRS        # optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES            # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
)
