SRC:=$(wildcard  *.c)
all:
	gcc -I.. -I${RTE_SDK}/${RTE_TARGET}/include  -o pmd_test $(SRC) ../queue.c ../shm_util.c 
clean:
	-rm -rf pmd_test
