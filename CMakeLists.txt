idf_component_register(
    SRCS "src/SoftwareSerial.cpp" "src/esp_idf_compat.cpp"
    INCLUDE_DIRS "." "src"
    REQUIRES "driver" "esp_timer" "freertos"
    PRIV_REQUIRES "esp_rom"
)

# Add ghostl library dependency
idf_component_get_property(ghostl_lib ghostl COMPONENT_LIB)
target_link_libraries(${COMPONENT_LIB} PUBLIC ${ghostl_lib})

# Add compile definitions
target_compile_definitions(${COMPONENT_LIB} PUBLIC ESP_IDF)