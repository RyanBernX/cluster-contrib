SLURM_ROOT=/usr/local/slurm
PREFIX=/usr/local
CC=gcc
LIBS=-L${SLURM_ROOT}/lib -lslurm -Wl,-rpath=${SLURM_ROOT}/lib,--enable-new-dtags
CFLAGS=${EXTRA_CFLAGS} -std=c99 -I${SLURM_ROOT}/include

.PHONY: all snode clean

all: snode

snode: snode.c
	${CC} -o $@ $^ ${CFLAGS} ${LIBS}

clean:
	-rm -f snode
