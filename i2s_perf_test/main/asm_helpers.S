
// 
// uint32_t my_asm_func( int num, int numx2, int numx4 );
//		a0, a1, a2 in; a0 out.

.global my_asm_func

my_asm_func:
	slli	a1,a1,0x1
	add		a0,a0,a1
	slli	a2,a2,0x2
	add		a0,a0,a2
	ret

