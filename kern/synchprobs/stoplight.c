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
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define OPP_QUADRANT(qdrant) ((qdrant == 0) ? 3 : qdrant - 1)
#define DIAG_QUADRANT(qdrant) ((qdrant+2)%4)

struct semaphore *sem_qdr_zero;
struct semaphore *sem_qdr_one;
struct semaphore *sem_qdr_two;
struct semaphore *sem_qdr_three;
struct semaphore *sem_deadlock;

void getSem(uint32_t intersection);
void giveSem(uint32_t intersection);

void getSem(uint32_t intersection){
	switch(intersection){
		case 0:
			P(sem_qdr_zero);
			break;
		case 1:
			P(sem_qdr_one);
			break;
		case 2:
			P(sem_qdr_two);
			break;
		case 3:
			P(sem_qdr_three);
			break;
	}
}

void giveSem(uint32_t intersection){
	switch(intersection){
		case 0:
			V(sem_qdr_zero);
			break;
		case 1:
			V(sem_qdr_one);
			break;
		case 2:
			V(sem_qdr_two);
			break;
		case 3:
			V(sem_qdr_three);
			break;
	}
}

/*
 * Called by the driver during initialization.
 */

void
stoplight_init() {
	sem_qdr_zero = sem_create("sem_0",1);
	sem_qdr_one = sem_create("sem_1",1);
	sem_qdr_two = sem_create("sem_2",1);
	sem_qdr_three = sem_create("sem_3",1);
	sem_deadlock = sem_create("sem_deadlock",3);
}

/*
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {
	sem_destroy(sem_qdr_zero);
	sem_destroy(sem_qdr_one);
	sem_destroy(sem_qdr_two);
	sem_destroy(sem_qdr_three);
	sem_destroy(sem_deadlock);
}

void
turnright(uint32_t direction, uint32_t index){
	getSem(direction);
	inQuadrant(direction,index);
	leaveIntersection(index);
	giveSem(direction);
}

void
gostraight(uint32_t direction, uint32_t index){
	uint32_t opposite_intersection = OPP_QUADRANT(direction);
	P(sem_deadlock);
	getSem(direction);
	inQuadrant(direction,index);
	getSem(opposite_intersection);
	inQuadrant(opposite_intersection,index);
	giveSem(direction);
	leaveIntersection(index);
	giveSem(opposite_intersection);
	V(sem_deadlock);
}

void
turnleft(uint32_t direction, uint32_t index){
	uint32_t opposite_intersection = OPP_QUADRANT(direction);
	uint32_t diagonal_intersection = DIAG_QUADRANT(direction);
	P(sem_deadlock);
	getSem(direction);
	inQuadrant(direction,index);
	getSem(opposite_intersection);
	inQuadrant(opposite_intersection,index);
	giveSem(direction);
	getSem(diagonal_intersection);
	inQuadrant(diagonal_intersection,index);
	giveSem(opposite_intersection);
	leaveIntersection(index);
	giveSem(diagonal_intersection);
	V(sem_deadlock);
}
