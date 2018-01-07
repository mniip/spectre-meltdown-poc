#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

extern char btb_start, btb_branch_end, btb_end;
asm(
".section .data\n"
"btb_start:\n"
"	cmpq $0, (%rdi)\n"
"	jnz 1f\n"
"btb_branch_end:\n"
"	addsd %xmm0, %xmm0\n"
"	addsd %xmm0, %xmm0\n"
"	addsd %xmm0, %xmm0\n"
"	addsd %xmm0, %xmm0\n"
"	retq\n"
"	.skip 100\n"
"1:	addsd %xmm0, %xmm0\n"
"	addsd %xmm0, %xmm0\n"
"	addsd %xmm0, %xmm0\n"
"	addsd %xmm0, %xmm0\n"
"	retq\n"
"btb_end:\n"
);

extern char btb_time_start, btb_time_branch_end, btb_time_end;
asm(
".section .data\n"
"btb_time_start:\n"
"	rdtsc\n"
"	mov %rax, %rcx\n"
"	cmpq $0, (%rdi)\n"
"	jnz 1f\n"
"btb_time_branch_end:\n"
"	rdtsc\n"
"	retq\n"
"	.skip 100+14\n"
"1:	rdtsc\n"
"	retq\n"
"btb_time_end:\n"
);

extern unsigned long run_btb_measure(unsigned long *decider, void *entry);
asm(
".section .text\n"
".global run_btb_measure\n"
"run_btb_measure:\n"
"	lfence\n"
"	clflush (%rdi)\n"
"	lfence\n"
"	callq *%rsi\n"
"	lfence\n"
"	subq %rcx, %rax\n"
"	retq\n"
);

extern unsigned long run_btb_poison(unsigned long *decider, void *entry);
asm(
".section .text\n"
".global run_btb_poison\n"
"run_btb_poison:\n"
"	callq *%rsi\n"
"	retq\n"
);

extern void procrastinate();
asm(
".section .text\n"
".global procrastinate\n"
"procrastinate:\n"
"	lfence\n"
"	addsd %xmm0, %xmm0\n"
"	addsd %xmm0, %xmm0\n"
"	addsd %xmm0, %xmm0\n"
"	addsd %xmm0, %xmm0\n"
"	retq\n"
);

int popcnt(uint32_t x)
{
	int s = 0;
	for(int i = 0; i < 32; i++)
		if(x & (1 << i))
			s++;
	return s;
}

int countlead(uint32_t x)
{
	int s = 0;
	while(s < 32 && (x & (1 << (31 - s))))
		s++;
	return s;
}

uint32_t leadbit(uint32_t x)
{
	uint32_t s = 0;
	for(int i = 0; i < 32; i++)
		if(x & (1 << i))
			s = i;
	return s;
}

uint32_t nextmask(uint32_t p)
{
	int lead = countlead(p);
	if(lead == popcnt(p))
		return (1 << (1 + lead)) - 1;

	p = (p << lead) >> lead;
	int lb = leadbit(p);
	p &= ~(1 << lb);
	p |= ((1 << (1 + lead)) - 1) << (lb + 1);
	return p;
}

int counts[2][2];
int main(int argc, char *argv[])
{
	unsigned long *decider = mmap(NULL, sizeof(unsigned long), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	char *btb = 0x1000 + mmap((void*)(0x200000000000ull - 0x1000), 0x100000000ull + 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	char *btb_time = 0x1000 + mmap((void*)(0x600000000000ull - 0x1000), 0x100000000ull + 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	size_t mask = 0;
	do
	{
		counts[0][0] = counts[0][1] = counts[1][0] = counts[1][1] = 0;

		for(int i = 0; i < 1000; i++)
			for(int tr_val = 0; tr_val <= 1; tr_val++)
				for(int tm_val = 0; tm_val <= 1; tm_val++)
				{
					size_t offt = (rand() & 0xFFFFull) << 16 | (rand() & 0xFFFFull);

					memcpy(btb + (offt ^ mask) - (&btb_branch_end - &btb_start), &btb_start, &btb_end - &btb_start);
					void *entry = btb + (offt ^ mask) - (&btb_branch_end - &btb_start);

					memcpy(btb_time + offt - (&btb_time_branch_end - &btb_time_start), &btb_time_start, &btb_time_end - &btb_time_start);
					void *entry_time = btb_time + offt - (&btb_time_branch_end - &btb_time_start);

					procrastinate();

					*decider = tr_val;
					for(int j = 0; j < 8; j++)
						run_btb_poison(decider, entry);

					procrastinate();

					*decider = tm_val;
					unsigned long t = run_btb_measure(decider, entry_time);
					if(t < 100)
						counts[tr_val][tm_val]++;
				}

		if(counts[0][0] > 4 * counts[1][0] && counts[1][1] > 4 * counts[0][1])
			printf("Likely collision: %08x (%zu+%zu vs %zu+%zu)\n", mask, counts[0][0], counts[1][1], counts[1][0], counts[0][1]);

		mask = nextmask(mask);
		madvise((void*)(0x200000000000ull - 0x1000), 0x100000000ull + 0x2000, MADV_REMOVE);
		madvise((void*)(0x600000000000ull - 0x1000), 0x100000000ull + 0x2000, MADV_REMOVE);
	}
	while(mask);
}
