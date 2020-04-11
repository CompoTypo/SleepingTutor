#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

// SLEEP CONSTANTS
#define TUTORING_TIME 0.2
#define PROGRAMMING_TIME 2

// GLOBAL VARS
int NUM_STUDENTS, NUM_TUTORS, NUM_SEATS, MAX_PRIORITY;

int ss_val; // for storing semaphore values of the student

sem_t
	COORDINATOR,  // for student to signal coordinator for queueing
	WAITING_ROOM, // for keeping track of students awaiting tutoring
	QUEUE_SEM,	  // for making tutors aware that a student(s) has been queued
	TUTOR_ROOM,	  // manage tutors available
	TUTORING;	  // for tutor to hold a student for tutoring

// BEGIN Self expanetory typedefs
typedef struct
{
	int id;
	int students_tutored;
} tutor;
tutor *tutors;

typedef struct
{
	int id;
	int priority;

} student;
// END self explanatory typedefs

student *students; // for students array pointer

// PRIORITY QUEUE
int CUR_SIZE = 0;
student *QUEUE;

static void swap(student *a, student *b)
{
	student temp = *a;
	*a = *b;
	*b = temp;
}

void heapify(student chairs[], int n, int i) 
{
	int greatest = i;
	int left = 2 * i + 1;
	int right = 2 * i + 2;
	if (left < n && chairs[left].priority > chairs[greatest].priority)
		greatest = left;
	if (right < n && chairs[right].priority > chairs[greatest].priority)
		greatest = right;
	if (greatest != i) 
	{
		// swap and heapify
		swap(&chairs[i], &chairs[greatest]);
		heapify(chairs, n, greatest);
	}
}

void heapSort(student chairs[], int n)
{
	int i;
	for (i = n / 2 - 1; i >= 0; i--) // for all levels
		heapify(chairs, n, i); // heapify
	for (i = n - 1; i >= 0; i--) // for all unordered data
	{
		// swap misplaced values and heapify
		swap(&chairs[0], &chairs[i]); 
		heapify(chairs, i, 0);
	}
}

void pushHeap(student chairs[], student s)
{
	chairs[CUR_SIZE] = s; // Add to end of the heap
	CUR_SIZE++; // adjust size
	heapSort(chairs, CUR_SIZE); // heap process
}

student *popHeap(student chairs[])
{
	student *ret = &chairs[0]; // grab root of the heap
	CUR_SIZE--; // adjust size
	chairs[0] = chairs[CUR_SIZE]; // Place last at the root
	heapSort(chairs, CUR_SIZE); // heap process
	return ret;

}
// END QUEUE

student *student_holder;
static void *coordinatorRoutine()
{
	int total_requests = 0;
	student_holder = malloc(sizeof(student));
	while (1) // always until job is done
	{
		sleep(TUTORING_TIME);   
		sem_wait(&COORDINATOR);  // wait for coordinator to arrive and be signaled by a student

		if (total_requests != NUM_STUDENTS * MAX_PRIORITY) // while max possible sessions is not reached
		{
			if (student_holder != NULL) // Enqueue a student if not null
				pushHeap(QUEUE, *student_holder);
			student_holder == NULL;  

			sem_post(&QUEUE_SEM); // signal tutors that someone is in the queue
			printf("Co: Student %d with priority %d in the queue. Waiting students now = %d. Total requests = %d.\n",
				student_holder->id,
				student_holder->priority,
				NUM_SEATS - ss_val,
				++total_requests
			);
		}
		else
			return NULL; // Exit once everything is done
	}
}

student *to_tutor; // holding vars for tutor
tutor *helping_tut; // holding vars for tutor
int total_tutoring_sessions = 0;
static void *tutorRoutine(void *id)
{
	while (total_tutoring_sessions != NUM_STUDENTS * MAX_PRIORITY) // while max possible sessions is not reached
	{
		sem_post(&TUTOR_ROOM);		   // tutors populate

		sem_wait(&QUEUE_SEM);			// Wait for something in the queue
		if (total_tutoring_sessions == NUM_STUDENTS * MAX_PRIORITY)
			break;

		to_tutor = popHeap(QUEUE);			// dequeue a student for tutoring
		*helping_tut = tutors[(long)id]; // update global of tutoring info

		sem_post(&TUTORING); 			// Start the tutoring session
		printf("Tu: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d.\n", 
			to_tutor->id, 
			tutors[(long) id].id, 
			++tutors[(long) id].students_tutored, 
			++total_tutoring_sessions
		);
	}
	sem_post(&QUEUE_SEM);
	return NULL; // Exit since were done
}

static void *studentRoutine(void *id)
{
	while (students[(long)id].priority) // while student priority is not 0
	{
		sem_getvalue(&WAITING_ROOM, &ss_val);
		while (!ss_val)	// if student sem_val == 0. . .  
		{
			// Student goes back to programming
			printf("St: Student %d found no empty chair. Will try again later\n", students[(long)id].id);
			usleep(random() % PROGRAMMING_TIME);
		}
		sem_wait(&WAITING_ROOM); // wait for students to arrive

		sem_getvalue(&WAITING_ROOM, &ss_val); // update sem_val holder
		printf("St: Student %d takes a seat. Empty chairs = %d\n", students[(long)id].id, ss_val);

		*student_holder = students[(long)id]; // hold student	
		sem_post(&COORDINATOR); //  and send them to coordinator.

		sem_wait(&TUTOR_ROOM); // Make sure tutors are present at the csmc

		sem_wait(&TUTORING); // Wait to be tutored
		printf("St: Student %d recieved help from Tutor %d.\n", students[(long)id].id, helping_tut->id);
		usleep(TUTORING_TIME); 	// tutoring time
		students[(long)id].priority--; // Decrease students priority after tutoring

		sem_post(&WAITING_ROOM); // Leave the waiting room
	}
	return NULL; // exit when thread is done
}

void *makeTuts()
{
	pthread_t tut_thread[NUM_TUTORS];
	long counter;
	for (counter = 0; counter < NUM_TUTORS; counter++)
	{
		// create a new tutor
		tutor tut = {counter, 0};
		tutors[counter] = tut;

		// Declare and create a thread
		if (pthread_create(&tut_thread[counter], NULL, tutorRoutine, (void *)counter) != 0)
			printf("Failed to create thread for tutor.\n");
	}

	// waiting for all threads to join 
	for (counter = 0; counter < NUM_TUTORS; counter++)
		pthread_join(tut_thread[counter], NULL);

	sem_post(&COORDINATOR); // leave with the coordinator
	return NULL;
}

void *makeStuds()
{
	pthread_t stud_thread[NUM_STUDENTS];
	long counter;
	for (counter = 0; counter < NUM_STUDENTS; counter++)
	{
		// create a new student
		student stud = {counter, MAX_PRIORITY};
		students[counter] = stud;

		// Declare and create a thread 
		if (pthread_create(&stud_thread[counter], NULL, studentRoutine, (void *)counter) != 0)
			printf("Failed to create thread for student.\n");
	}

	// waiting for all threads to join 
	for (counter = 0; counter < NUM_STUDENTS; counter++)
		pthread_join(stud_thread[counter], NULL);

	return NULL;
}

// Factored out initializations from main if program started successfully
void initGlobals(char **argv)
{
	NUM_STUDENTS = atoi(argv[1]);
	NUM_TUTORS = atoi(argv[2]);
	NUM_SEATS = atoi(argv[3]);
	MAX_PRIORITY = atoi(argv[4]);

	student_holder = malloc(sizeof(student));
	to_tutor = malloc(sizeof(student));
	helping_tut = malloc(sizeof(tutor));

	sem_init(&COORDINATOR, 0, 0);
	sem_init(&WAITING_ROOM, 0, NUM_SEATS);
	sem_init(&QUEUE_SEM, 0, 0);
	sem_init(&TUTOR_ROOM, 0, 0);
	sem_init(&TUTORING, 0, 0);
}

void main(int argc, char **argv)
{
	// INPUT VALIDATION
	if (argc != 5)
	{
		printf("EXEC FORMAT: ./csmc #students #tutors #chairs #help\n");
		exit(1);
	}
	initGlobals(argv);

	// init these here to make sure they persist
	tutor tu[NUM_TUTORS];
	student stu[NUM_STUDENTS], seats[NUM_SEATS];
	tutors = tu;
	students = stu; QUEUE = seats;

	pthread_t cor, tut_maker, stud_maker; // define initial threads

	if (pthread_create(&cor, NULL, coordinatorRoutine, NULL) != 0)
		printf("Faied to create thread for coordinator\n");

	// CREATE TUTOR THREADS
	if (pthread_create(&tut_maker, NULL, makeTuts, NULL) != 0)
		printf("Faied to create thread for making tutors\n");

	// CREATE STUDENT THREADS
	if (pthread_create(&stud_maker, NULL, makeStuds, NULL) != 0)
		printf("Faied to create thread for making students\n");

	// join generator threads and coordinator thread
	pthread_join(stud_maker, NULL); 
	pthread_join(tut_maker, NULL);
	pthread_join(cor, NULL);
}
