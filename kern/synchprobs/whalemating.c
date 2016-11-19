/*
 * Copyright (c) 2001, 2002, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver code is in kern/tests/synchprobs.c We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

struct semaphore *matchmaker_male;
struct semaphore *matchmaker_female;
struct semaphore *male_matchmaker;
struct semaphore *female_matchmaker;

void whalemating_init(){
	matchmaker_male = sem_create("matchmaker_male",0);
	matchmaker_female = sem_create("matchmaker_female",0);
	male_matchmaker = sem_create("male_matchmaker",0);
	female_matchmaker = sem_create("female_matchmaker",0);
}

void
whalemating_cleanup() {
	sem_destroy(matchmaker_male);
	sem_destroy(matchmaker_female);
	sem_destroy(male_matchmaker);
	sem_destroy(female_matchmaker);
}

void
male(uint32_t index){
	male_start(index);
	V(matchmaker_male);
	P(male_matchmaker);
	male_end(index);
}

void
female(uint32_t index){
	female_start(index);
	V(matchmaker_female);
	P(female_matchmaker);
	female_end(index);
}

void
matchmaker(uint32_t index){
	matchmaker_start(index);
	P(matchmaker_male);
	P(matchmaker_female);
	V(male_matchmaker);
	V(female_matchmaker);
	matchmaker_end(index);
}
