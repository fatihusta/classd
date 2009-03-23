build_dir     = build

USE_DEBUG     = true

CC            = gcc
DEFINES       = -D_GNU_SOURCE -D_REENTRANT -DDEBUG_ON -DDEBUG_PKG=100
WARNINGS      = -Wall
OPTIMIZATIONS =
INC           = ${build_dir}/include src
INC_FLAGS     = $(patsubst %,-I%,$(INC))
LDDIR         = lib $(build_lib_path)  
LDDIR_FLAGS   = $(patsubst %,-L%,$(LDDIR))
LIBS          = json microhttpd dl sysfs rt
#LIBS          += efence
LIBS_FLAGS    = -pthread $(patsubst %,-l%,$(LIBS)) --warn-common
FLAGS        += -rdynamic -std=c99
CFLAGS        =  $(FLAGS) $(DEFINES) $(WARNINGS) $(OPTIMIZATIONS) $(INC_FLAGS) $(LDDIR_FLAGS)

ifeq ($(strip $(USE_DEBUG)),true)
    DEFINES  +=  -DDEBUG_ON
    FLAGS    += -g -ggdb
    STRIPCMD  = /bin/true -Not_stripping_DEBUG_MODE
else
    OPTIMIZATIONS += -funroll-loops -fomit-frame-pointer
    FLAGS    += $(WARNINGS) $(OPTIMIZATIONS)
    LDFLAGS  += -s
    STRIPCMD  = echo "Stripping debug symbols..." ; $(STRIP) --strip-debug --remove-section=.note --remove-section=.comment -x
endif

object_dir=${build_dir}/objects

library_dir=${build_dir}/libs

## Libmvutil source
mvutil_source=libmvutil/src/debug.c libmvutil/src/errlog.c libmvutil/src/hash.c \
	libmvutil/src/libmvutil.c libmvutil/src/list.c libmvutil/src/lock.c \
	libmvutil/src/mailbox.c libmvutil/src/mvpoll.c libmvutil/src/unet.c \
	libmvutil/src/uthread.c libmvutil/src/utime.c

## splitd source
splitd_source=src/json/server.c src/json/object_utils.c \
	src/json/serializer.c src/main.c src/functions.c \
	src/splitd/config.c src/splitd/manager.c src/splitd/libs.c \
	src/splitd/uplink.c src/splitd/splitter_instance.c \
	src/splitd/splitter_config.c ${mvutil_source}

splitd_objects=$(patsubst %.c,${object_dir}/%.o, ${splitd_source})
splitd_binary=splitd

## Base Uplink Library
library_source=src/split_libs/base/lib.c src/split_libs/base/online.c \
	src/split_libs/base/basic.c 
library_objects=$(patsubst %.c,${object_dir}/%.o, ${library_source})
library_binary=${library_dir}/base.so

test_source=src/json/server.c src/json/object_utils.c \
	src/json/serializer.c test/config.c \
	src/splitd/config.c src/splitd/manager.c src/splitd/libs.c \
	${mvutil_source}

test_objects=$(patsubst %.c,${object_dir}/%.o, ${test_source})
test_binary=config

default: ${object_dir} mvutil_includes ${splitd_binary}

${splitd_binary}: ${splitd_objects} ${mvutil_source} ${library_binary}
	@echo "==> gcc ${splitd_objects} -> $@"
	@${CC} ${CFLAGS} ${LIBS_FLAGS} ${splitd_objects} -o $@

${object_dir}/src/split_libs/%.o: src/split_libs/%.c
	@if [ ! -d `dirname $@` ] ; then echo "==> mkdir `dirname $@`" ; mkdir -p `dirname $@` ; fi
	@echo "==> gcc fpic $@"
	@${CC} -c $< ${CFLAGS} -fPIC -o $@

${object_dir}/%.o: %.c
	@if [ ! -d `dirname $@` ] ; then echo "==> mkdir `dirname $@`" ; mkdir -p `dirname $@` ; fi
	@echo "==> gcc $@"
	@${CC} -c $< ${CFLAGS} -o $@

${object_dir}:
	@echo "==> Making directory ${object_dir}"
	@mkdir -p ${object_dir}

${library_dir}:
	@echo "==> Making directory ${library_dir}"
	@mkdir -p ${library_dir}

${library_binary}: ${library_dir} ${library_objects} 
	@echo "==> gcc ${library_objects} -> $@"
	@${CC} -shared ${CFLAGS} ${LIBS_FLAGS} ${library_objects} -o $@


mvutil_includes:
	@echo "==> cp -a libmvutil/include/ ${build_dir}/include"
	@tar --exclude=.svn -cf - -C libmvutil include | tar -xf - -C ${build_dir}

clean:
	@echo "==> Removing object files in ${object_dir}"
	@rm -rf ${object_dir}
	@rm -f ${splitd_binary}

install:
	@echo "Installing"

${test_binary}: ${test_objects} ${mvutil_source}
	@echo "==> gcc ${test_objects} -> $@"
	@${CC} ${CFLAGS} ${LIBS_FLAGS} ${test_objects} -o $@

