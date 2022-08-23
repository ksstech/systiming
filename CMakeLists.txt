# SYSTIMING
set( srcs "systiming.c" )
set( include_dirs "." )
set( requires hal_esp32 )
idf_component_register(SRCS ${srcs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires} PRIV_REQUIRES ${priv_requires})
set( priv_requires esp_timer printfx rtos-support )
