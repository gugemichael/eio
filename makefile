# ==============================================================
# Generic Makefile
# ==============================================================

LIB_NAME = eio

# compile relative
CC = gcc 
PROFILER = #-lprofiler
UB_OPT = -pg -g -O2 ${PROFILER}
C_FLAGS = -std=gnu99 -Wall -Werror ${UB_OPT} 
INCLUDE = 
LIBS = 
LIBS_PATH = 
ENV = ENV
TARGET = lib${LIB_NAME}.a
TARGET_PATH = ./output/

# output relative
TARGET_INCLUDE = $(TARGET_PATH)/${LIB_NAME}
TARGET_LIB = $(TARGET_PATH)/${LIB_NAME}

# scaned files
SRC = ./
SOURCES = $(wildcard *.c)
HEADERS = $(wildcard *.h)
OBJFILES = $(SOURCES:%.c=%.o)

# ctags files
TAGS = ./tags
CSCP = ./cscope.*

# command macro definition
MKDIR = mkdir -p 
RM = rm -rf 
MV = mv 
COPY = cp 
AR = ar -r

####################################################
COMPILER = $(CC) $(INCLUDE) $(LIBS_PATH) $(LIBS)
####################################################

all: TAGS $(TARGET)

TAGS:
	ctags -R --fields=+iaS --extra=+q
	cscope -bkqR


$(TARGET): $(ENV) $(OBJFILES)
	$(AR) $(TARGET_PATH)/$(TARGET) ${SRC}/*.o
	${COPY} ${SRC}/*.h $(TARGET_INCLUDE)
	${RM} ${SRC}/*.o 

$(OBJFILES): %.o: %.c $(HEADERS)
	${COMPILER} $(C_FLAGS) -c -o $@ $<

$(ENV):
	$(MKDIR) $(TARGET_LIB)

clean:
	$(RM) $(TARGET_PATH) ${TAGS} ${CSCP}

check:
	cppcheck --enable=all --platform=unix64 . 

