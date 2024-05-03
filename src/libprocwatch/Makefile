#
# Makefile for ip proxy library
#

INTERNALLIB=	yes
LIB=		procwatch
SRCS=		bhyve_command.c bhyve_config.c bhyve_config_console.c bhyve_config_object.c \
		bhyve_director.c bhyve_uclparser.c bhyve_uclparser_funcs.c \
		config_generator_object.c daemon_config.c process_def.c process_def_object.c \
		process_state.c state_change.c
INCS=		bhyve_config.h bhyve_config_console.h bhyve_config_object.h bhyve_director.h \
		bhyve_uclparser.h bhyve_uclparser_funcs.h daemon_config.h process_def_object.h \
		config_generator_object.h process_def.h process_state.h

.include <bsd.lib.mk>
