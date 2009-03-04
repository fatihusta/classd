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
LIBS          = json microhttpd pcap 
#LIBS          += efence
LIBS_FLAGS    = -pthread $(patsubst %,-l%,$(LIBS)) --warn-common
FLAGS        += 
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

## Libmvutil source
mvutil_source=libmvutil/src/debug.c libmvutil/src/errlog.c libmvutil/src/hash.c \
	libmvutil/src/libmvutil.c libmvutil/src/list.c libmvutil/src/lock.c \
	libmvutil/src/mailbox.c libmvutil/src/mvpoll.c libmvutil/src/unet.c \
	libmvutil/src/uthread.c libmvutil/src/utime.c

## faild source
faild_source=src/json/server.c src/json/object_utils.c \
	src/json/serializer.c src/main.c src/functions.c \
	src/faild/config.c src/faild/manager.c \
	${mvutil_source}

faild_objects=$(patsubst %.c,${object_dir}/%.o, ${faild_source})
faild_binary=faild

test_source=src/json/server.c src/json/object_utils.c \
	src/json/serializer.c test/config.c \
	src/faild/config.c src/faild/manager.c \
	${mvutil_source}

test_objects=$(patsubst %.c,${object_dir}/%.o, ${test_source})
test_binary=config


default: ${object_dir} mvutil_includes ${faild_binary}

${faild_binary}: ${faild_objects} ${mvutil_source}
	@echo "==> gcc ${faild_objects} -> $@"
	@${CC} ${CFLAGS} ${LIBS_FLAGS} ${faild_objects} -o $@

${object_dir}/%.o: %.c
	@if [ ! -d `dirname $@` ] ; then echo "==> mkdir `dirname $@`" ; mkdir -p `dirname $@` ; fi
	@echo "==> gcc $@"
	@${CC} -c $< ${CFLAGS} -o $@

${object_dir}:
	@echo "==> Making directory ${object_dir}"
	@mkdir -p ${object_dir}

mvutil_includes:
	@echo "==> cp -a libmvutil/include/ ${build_dir}/include"
	@tar --exclude=.svn -cf - -C libmvutil include | tar -xf - -C ${build_dir}

clean:
	@echo "==> Removing object files in ${object_dir}"
	@rm -rf ${object_dir}
	@rm -f ${faild_binary}

install:
	@echo "Installing"

${test_binary}: ${test_objects} ${mvutil_source}
	@echo "==> gcc ${test_objects} -> $@"
	@${CC} ${CFLAGS} ${LIBS_FLAGS} ${test_objects} -o $@

