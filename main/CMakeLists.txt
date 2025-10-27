set(SOURCE_FILES "")
set(SOURCE_FILES_DISPLAY "")
file(GLOB SOURCE_FILES "*.cpp")
file(GLOB SOURCE_FILES_DISPLAY "display/*.cpp")

idf_component_register(
  SRCS 
    # main.cpp
    # display/first_screen.cpp
    # display/calibrate_screen.cpp
    ${SOURCE_FILES}
    ${SOURCE_FILES_DISPLAY}
  
  PRIV_REQUIRES 
    esp_wifi 
    nvs_flash
    spi_flash
    lvgl__lvgl
    LovyanGFX
  INCLUDE_DIRS
    "."
)
