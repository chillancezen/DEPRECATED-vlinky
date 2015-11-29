
#include "util.h"

unsigned int int_seed=0x12345678;
void uuid_init(int seed)
{
	int_seed=seed;
}
unsigned int uuid_alloc()
{
	return int_seed++;
}
