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

pthread_mutex_t SAVE;

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

// QUEUE
typedef struct node
{
	student *stud;
	struct node *next_in_line;
} *node_ptr;

node_ptr front = NULL;
node_ptr rear = NULL;

// readability
int is_empty()
{
	return (front == NULL); // if front is empty, there is nothing in the queue
}

int enqueue(student *value)
{
	node_ptr item = (node_ptr)malloc(sizeof(struct node));

	if (item == NULL) // Check if 
		return 0;

	item->stud = value; // set the new item to hold the input student
	item->next_in_line = (node_ptr)malloc(sizeof(struct node));

	if (rear == NULL) // if rear is null, queue has nothing in it so .. .
	{
		front = rear = item; // set front and rear to new element
		front->next_in_line = rear->next_in_line = NULL;
	}
	else
	{
		node_ptr walker = front; // temporary node for iteration
		while (walker != NULL) // Walk for every list element from the back
		{
			// if the next students priority <= new students priority
			if (walker->stud->priority < item->stud->priority) 
			{
				item->next_in_line = walker->next_in_line; // work new stud right into middle of queue
				walker->next_in_line = item; // reset current node to point for new item
				return 1; // exit out since were done at this point
			}
			walker = walker->next_in_line; // otherwise continue iterating
		}
		// if while loop iterated all the way to the back
		rear->next_in_line = item; // put new item at the back
		rear = item; 
	}
	return 1;
}

// tutor gets student off of queue
student *dequeue()
{
	if (is_empty()) // fail on empty
	{
		printf("\nThe queue is empty!\n");
		return NULL; // return null (fail) if nothing in queue
	}
	node_ptr temp = front; // grab the front of the queue
	front = front->next_in_line; // next in line becomes the front

	return temp->stud; // return dequeued student if successful
}
// END QUEUE

student *student_holder;
int done = 0;
static void *coordinatorRoutine()
{
	int total_requests = 0;
	student_holder = malloc(sizeof(student));
	while (1) // always until job is done
	{
		sleep(TUTORING_TIME);   
		sem_wait(&COORDINATOR);  // wait for coordinator to arrive and be signaled by a student

		if (!done) // if not done
		{
			pthread_mutex_lock(&SAVE);
			if (student_holder != NULL && enqueue(student_holder) < 1) // Enqueue a student if not null
				printf("Failed to enqueue a student\n");
			//student_holder == NULL;  
			sem_post(&QUEUE_SEM); // signal tutors that someone is in the queue
			printf("Co: Student %d with priority %d in the queue. Waiting students now = %d. Total requests = %d.\n",
				student_holder->id,
				student_holder->priority,
				NUM_SEATS - ss_val,
				++total_requests
			);
			pthread_mutex_unlock(&SAVE);
		}
		else
		{
			printf("All done\n");
			pthread_exit(NULL); // Exit once everything is done
		}
	}
}

student *to_tutor; // holding vars for tutor
tutor *helping_tut; // holding vars for tutor
int total_tutoring_sessions = 0;
static void *tutorRoutine(void *id)
{
	tutor tut = tutors[(long) id]; // Keep tutor info handy for lengthy print statement
	sem_post(&TUTOR_ROOM);		   // tutors populate

	sem_wait(&QUEUE_SEM);			// Wait for something in the queue
	to_tutor = dequeue();			// dequeue a student for tutoring
	*helping_tut = tutors[(long)id]; // update global of tutoring info

	sem_post(&TUTORING); 			// Start the tutoring session
	printf("Tu: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d.\n", 
		to_tutor->id, 
		tut.id, 
		++tut.students_tutored, 
		++total_tutoring_sessions
	);

	if (!done) // If jobs not done
		tutorRoutine(id); // Tutor starts over back at tutor room
	else
		pthread_exit(NULL); // Exit since were done
}

static void *studentRoutine(void *id)
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

	if (students[(long)id].priority) // If the priority != 0 . . .
	{
		printf("Student %d will return\n", students[(long)id].id);
		studentRoutine(id); // repeat the cycle
	}
	else
	{
		printf("Student %d done with tutoring\n", students[(long)id].id);
		pthread_exit(NULL); // exit the thread because student has completed all tutoring 
	}
}

void *makeTuts()
{
	long counter;
	for (counter = 0; counter < NUM_TUTORS; counter++)
	{
		// create a new tutor
		tutor tut = {counter, 0};
		tutors[counter] = tut;

		// Declare and create a thread
		pthread_t tut_thread;
		if (pthread_create(&tut_thread, NULL, tutorRoutine, (void *)counter) != 0)
			printf("Failed to create thread for tutor.");
	}
}

void *makeStuds()
{
	long counter;
	for (counter = 0; counter < NUM_TUTORS; counter++)
	{
		// create a new student
		student stud = {counter, MAX_PRIORITY};
		students[counter] = stud;

		// Declare and create a thread 
		pthread_t stud_thread;
		if (pthread_create(&stud_thread, NULL, studentRoutine, (void *)counter) != 0)
			printf("Failed to create thread for student.");
	}
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

	pthread_mutex_init(&SAVE, NULL);
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
	student stu[NUM_STUDENTS];
	tutors = tu;
	students = stu;

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
