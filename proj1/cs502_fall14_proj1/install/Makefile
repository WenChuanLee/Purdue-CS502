srcdir = /u/data/u3/cs502/Fall12/CS502/gcc-4.7.0/gcc

CC = gcc
AS = as
AR = ar
AR_FLAGS = rc

SHELL = /bin/sh

STAMP = echo timestamp >

AWK = gawk

RUN_GEN = 

build_exeext = 

CFLAGS = -g -O0

OBJ_DIR = /u/data/u3/cs502/Fall12/CS502/install_new/gcc

#INCLUDES = -I. -I$(srcdir) -I$(srcdir)/config \
           -I$(srcdir)/../include -I$(OBJ_DIR) \
           -I$(srcdir)/../libcpp/include \
           -I$(srcdir)/../libgcc/


CPPINC = -I$(srcdir)/../libcpp/include

STDCINC = -I$(srcdir)/../libstdc++-v3/include

LIBPATH = -I$/usr/lib/gcc/x86_64-pc-linux-gnu/4.5.3/include/
 
INCLUDES = -I. -I$(@D) -I$(srcdir) -I$(srcdir)/$(@D) \
           -I$(srcdir)/../include  \
           -I$(OBJ_DIR) \
           $(CPPINC) $(GMPINC) $(DECNUMINC) \
           $(PPLINC) $(CLOOGINC) $(STDCINC) $(LIBPATH)

#WARN_CFLAGS = -W -Wall -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wtraditional -pedantic -Wno-long-long 

WARN_CFLAGS = -w

LINKER = $(CC)
LINKER_FLAGS = $(CFLAGS)


C_TARGET_OBJS = $(OBJ_DIR)/i386-c.o $(OBJ_DIR)/default-c.o

C_COMMON_OBJS = ${OBJ_DIR}/c-family/c-common.o ${OBJ_DIR}/c-family/c-cppbuiltin.o ${OBJ_DIR}/c-family/c-dump.o \
                ${OBJ_DIR}/c-family/c-format.o ${OBJ_DIR}/c-family/c-gimplify.o ${OBJ_DIR}/c-family/c-lex.o \
                ${OBJ_DIR}/c-family/c-omp.o ${OBJ_DIR}/c-family/c-opts.o ${OBJ_DIR}/c-family/c-pch.o \
                ${OBJ_DIR}/c-family/c-ppoutput.o ${OBJ_DIR}/c-family/c-pragma.o ${OBJ_DIR}/c-family/c-pretty-print.o \
                ${OBJ_DIR}/c-family/c-semantics.o ${OBJ_DIR}/c-family/c-ada-spec.o

C_AND_OBJC_OBJS = ${OBJ_DIR}/c-errors.o \
                  ${OBJ_DIR}/c-decl.o \
                  ${OBJ_DIR}/c-typeck.o \
                  ${OBJ_DIR}/c-convert.o \
                  ${OBJ_DIR}/c-aux-info.o \
                  ${OBJ_DIR}/attribs.o ${OBJ_DIR}/c-objc-common.o \
                  ${OBJ_DIR}/tree-mudflap.o \
                  ${OBJ_DIR}/prefix.o \
                  $(C_COMMON_OBJS) \
                  $(C_TARGET_OBJS) 

C_OBJS = ${OBJ_DIR}/c-lang.o \
         ${OBJ_DIR}/c-parser.o \
         ${OBJ_DIR}/c-family/stub-objc.o \
         $(C_AND_OBJC_OBJS)


ALL_CFLAGS = -DIN_GCC $(CFLAGS) $(WARN_CFLAGS) -DHAVE_CONFIG_H
LDFLAGS = 


CPPLIB = ${OBJ_DIR}/../libcpp/libcpp.a

LIBINTL = 
LIBINTL_DEP =

LIBICONV = 
LIBICONV_DEP =

LIBDECNUMBER = $(OBJ_DIR)/../libdecnumber/libdecnumber.a

LIBIBERTY = $(OBJ_DIR)/../libiberty/libiberty.a


GMPLIBS = -L/u/data/u3/cs502/Fall12/CS502/install_new/./gmp/.libs \
          -L/u/data/u3/cs502/Fall12/CS502/install_new/./mpfr/.libs \
          -L/u/data/u3/cs502/Fall12/CS502/install_new/./mpc/src/.libs -lmpc -lmpfr -lgmp

GMPINC = -I/u/data/u3/cs502/Fall12/CS502/install_new/./gmp \
         -I/u/data/u3/cs502/Fall12/CS502/gcc-4.7.0/gmp \
         -I/u/data/u3/cs502/Fall12/CS502/install_new/./mpfr \
         -I/u/data/u3/cs502/Fall12/CS502/gcc-4.7.0/mpfr \
         -I/u/data/u3/cs502/Fall12/CS502/gcc-4.7.0/mpc/src

# How to find PPL

PPLLIBS =
PPLINC =

# How to find CLOOG                                                                                                                
CLOOGLIBS =
CLOOGINC =


PLUGINLIBS = -rdynamic -ldl

enable_plugin = yes

ZLIB = -L../zlib -lz
ZLIBINC = -I$(OBJ_DIR)/../zlib

# Dependencies on the intl and portability libraries.                                                                              
LIBDEPS= $(OBJ_DIR)/libcommon.a $(CPPLIB) $(LIBIBERTY) $(LIBINTL_DEP) $(LIBICONV_DEP) $(LIBDECNUMBER)

HOST_LIBS = 

LIBS =  $(OBJ_DIR)/libcommon.a $(CPPLIB) $(LIBINTL) $(LIBICONV) $(LIBIBERTY) $(LIBDECNUMBER) \
        $(HOST_LIBS)

BACKENDLIBS = $(CLOOGLIBS) $(PPLLIBS) $(YICESLIBS) $(GMPLIBS) $(PLUGINLIBS) $(HOST_LIBS) $(ZLIB) #-lstdc++ 


BACKEND = $(OBJ_DIR)/main.o $(OBJ_DIR)/libcommon-target.a $(OBJ_DIR)/libcommon.a $(OBJ_DIR)/libbackend.a $(CPPLIB) $(LIBDECNUMBER)


MY_FILES = csproj2.o csproj1.o dfa.o hashtable.o



cc1: $(C_OBJS) $(OBJ_DIR)/cc1-checksum.o $(BACKEND) $(LIBDEPS) $(MY_FILES)
	+$(LINKER) $(ALL_LINKERFLAGS) $(LDFLAGS) -o $@ $(C_OBJS) \
          $(OBJ_DIR)/cc1-checksum.o $(BACKEND) $(LIBS) $(MY_FILES) $(BACKENDLIBS)



# write your own rules
csproj2.o : csproj2.c 
	$(CC) -c $(ALL_CFLAGS) $(INCLUDES) csproj2.c -o csproj2.o

csproj1.o : csproj1.c
	$(CC) -c $(ALL_CFLAGS) $(INCLUDES) csproj1.c -o csproj1.o

dfa.o : dfa.c
	$(CC) -c $(ALL_CFLAGS) $(INCLUDES) dfa.c -o dfa.o

hashtable.o : hashtable.c
	$(CC) -c $(ALL_CFLAGS) $(INCLUDES) hashtable.c -o hashtable.o

clean:
	rm -f *.o cc1 *.s output.txt
