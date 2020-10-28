idf_component_register(
	SRCS "systiming.c"
	INCLUDE_DIRS "."
	REQUIRES common
	PRIV_REQUIRES printfx rtos-support
)
