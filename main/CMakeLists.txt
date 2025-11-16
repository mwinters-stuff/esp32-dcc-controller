set(SOURCE_FILES "")
file(GLOB_RECURSE SOURCE_FILES 
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/display/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/utilities/*.cpp"
)

idf_component_register(
  SRCS 
    ${SOURCE_FILES}
  
  PRIV_REQUIRES 
    esp_event 
    lwip
    esp_wifi 
    nvs_flash
    spi_flash
    lvgl__lvgl
    LovyanGFX_dev
  INCLUDE_DIRS
    "."
)
