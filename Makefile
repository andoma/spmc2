#
#  Copyright (C) 2013 Andreas Öman
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

WITH_CURL := yes
WITH_MYSQL := yes
WITH_HTTP_SERVER := yes
WITH_CTRLSOCK := yes

BUILDDIR = ${CURDIR}/build

PROG=${BUILDDIR}/spmcd

CFLAGS  += $(shell mysql_config --cflags)

LDFLAGS += -larchive
LDFLAGS += $(shell mysql_config --libs_r)

SRCS += src/main.c \
	src/cli.c \
	src/showtime.c \
	src/ingest.c \
	src/stash.c \
	src/restapi.c \
	src/events.c \


BUNDLES += sql

install: ${PROG}
	install -D ${PROG} "${prefix}/bin/spmcd"
	install -D -m 755 spmc "${prefix}/bin/spmc"
uninstall:
	rm -f "${prefix}/bin/spmcd" "${prefix}/bin/spmc"

include libsvc/libsvc.mk

-include $(DEPS)
