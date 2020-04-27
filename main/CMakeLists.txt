file(GLOB_RECURSE SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS .
                    EMBED_FILES server/error.html server/favicon.ico server/test.css server/test.js server/root.html)

#target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_CONF_INCLUDE_SIMPLE=1)
