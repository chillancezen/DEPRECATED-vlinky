#ifndef _UTIL_FUN
#define _UTIL_FUN

#define mac_address_equal(a,b) (((a)[0]==(b)[0])&&((a)[1]==(b)[1])&&((a)[2]==(b)[2])&&((a)[3]==(b)[3])&&((a)[4]==(b)[4])&&((a)[5]==(b)[5]))
#define copy_mac_address(dst,src) do { \
	dst[0]=src[0]; \
	dst[1]=src[1]; \
	dst[2]=src[2]; \
	dst[3]=src[3]; \
	dst[4]=src[4]; \
	dst[5]=src[5]; \
}while(0)
	
void uuid_init(int seed);
#endif