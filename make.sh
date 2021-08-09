#!/usr/bin/ksh
INCLUDES="-I. -I/usr/include -I../../include"
CC=gcc
UNAME=`uname`
if [ ${UNAME} = "HP-UX" ]; then
    CFLAGS="-g -Wall -DPORTABLE_STRNICMP"
else
    CFLAGS="-g -Wall -m64"
fi

BIN_DIR=./bin
OBJ_DIR=./obj
LIB_DIR=../../libs/c

echo "${CC} ${CFLAGS} -o ${OBJ_DIR}/procsig.o   -c ${LIB_DIR}/procsig.c   ${INCLUDES}"
      ${CC} ${CFLAGS} -o ${OBJ_DIR}/procsig.o   -c ${LIB_DIR}/procsig.c   ${INCLUDES}
echo "${CC} ${CFLAGS} -o ${OBJ_DIR}/strlogutl.o -c ${LIB_DIR}/strlogutl.c ${INCLUDES}"
      ${CC} ${CFLAGS} -o ${OBJ_DIR}/strlogutl.o -c ${LIB_DIR}/strlogutl.c ${INCLUDES}
echo "${CC} ${CFLAGS} -o ${OBJ_DIR}/minIni.o    -c ${LIB_DIR}/minIni.c    ${INCLUDES}"
      ${CC} ${CFLAGS} -o ${OBJ_DIR}/minIni.o    -c ${LIB_DIR}/minIni.c    ${INCLUDES}
echo "${CC} ${CFLAGS} -o ${OBJ_DIR}/nfs_copy.o  -c ./nfs_copy.c           ${INCLUDES}"
      ${CC} ${CFLAGS} -o ${OBJ_DIR}/nfs_copy.o  -c ./nfs_copy.c           ${INCLUDES}
echo "${CC} ${CFLAGS} -o ${BIN_DIR}/nfs_copy.exe ${OBJ_DIR}/minIni.o ${OBJ_DIR}/strlogutl.o ${OBJ_DIR}/procsig.o ${OBJ_DIR}/nfs_copy.o -lm"
      ${CC} ${CFLAGS} -o ${BIN_DIR}/nfs_copy.exe ${OBJ_DIR}/minIni.o ${OBJ_DIR}/strlogutl.o ${OBJ_DIR}/procsig.o ${OBJ_DIR}/nfs_copy.o -lm
echo "rm -f ${OBJ_DIR}/strlogutl.o ${OBJ_DIR}/minIni.o ${OBJ_DIR}/nfs_copy.o ${OBJ_DIR}/procsig.o"
      rm -f ${OBJ_DIR}/strlogutl.o ${OBJ_DIR}/minIni.o ${OBJ_DIR}/nfs_copy.o ${OBJ_DIR}/procsig.o
