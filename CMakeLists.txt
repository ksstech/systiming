idf_component_register(
	SRCS "systiming.c"
	INCLUDE_DIRS "."
	PRIV_INCLUDE_DIRS
	REQUIRES common
	PRIV_REQUIRES printf rtos-support
)
