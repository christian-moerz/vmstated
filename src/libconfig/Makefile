#
# Makefile for internal configuration library
#

INTERNALLIB=	yes
LIB=		config
SRCS= 		check.c check_core.c check_isabridge.c config_block.c   \
		config_controller.c config_core.c config_hostbridge.c	\
		config_network.c config_vnc.c file_memory.c \
		ident_keeper.c output_bhyve_core.c parser_reader.c \
		parser_offsets.c string_list.c
INCS=		check.h check_core.h check_isabridge.h config_block.h \
		config_controller.h config_core.h config_hostbridge.h \
		config_network.h config_vnc.h file_memory.h \
		output_bhyve_core.h parser_reader.h parser_offsets.h \
		string_list.h

.include <bsd.lib.mk>
