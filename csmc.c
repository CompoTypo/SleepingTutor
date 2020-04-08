#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>

// ONE coordinator WAITS on incoming students
// N tutors WAITS for coordinator to notify to tutor a specific student

// (N <= NUM_QUEUE) students NOTIFY coordinator with semaphore to be QUEUED upon arrival
// PRIORITY is higher for fewer visits. Same priority pick who arrived first
// If not enough chairs for student, they return to lab

// GLOBAL VARS
int NUM_STUDENTS, NUM_TUTORS, NUM_SEATS, MAX_PRIORITY;

int ts_val, ss_val;
int students_tutored = 0;

sem_t 
COORDINATOR,  // for student to signal coordinator for queueing
WAITING_ROOM, // for keeping track of students awaiting tutoring
QUEUE_SEM,    // for making tutors aware that a student(s) has been queued
TUTOR_ROOM,   // manage tutors available
TUTORING;     // for tutor to hold a student for tutoring

pthread_mutex_t SAVE;

struct tutor
{
	int id;
	int students_tutored;
};
struct tutor *tutors;

struct student 
{
	int id;
	int priority;

};
struct student *students;
struct student *seats;

// QUEUE
struct node
{
	struct student *stud;
	struct node *next_in_line;
}; 

struct node *front;
struct node *rear;

// readability
int is_empty()
{
	return (front == NULL);
}

// returns <= 0 if failed
int enqueue(struct student *value){

	struct node *item;
	item->stud = value;

	if (item->stud->id == NULL)
		return 0;

	if(rear == NULL)
		front = rear = item;
	else{
		rear->next_in_line = item;
		rear = item;
	}

	return 1;
}

// tutor gets student off of queue
struct student *dequeue(){

	if(is_empty()){

		printf("\nThe queue is empty!\n");
		return NULL;
	}

	struct node *temp = front;
	front = front->next_in_line;

	return temp->stud;
}
// END QUEUE

int done = 0;
static void *coordinatorRoutine()
{
	struct student *student_holder;
	int total_requests = 0;
	while (1)
	{
		sem_wait(&COORDINATOR);
		total_requests++;
		sleep(0.2);
		
		if (!done) {
			for (int i = 1; i < NUM_SEATS; i++)
				if (seats[i].priority > student_holder->priority)
					*student_holder = seats[i];
			
			printf("Co: Student %d with priority %d in the queue. Waiting students now = %d. Total requests = %d.\n", 
					student_holder->id, 
					student_holder->priority, 
					NUM_SEATS - ss_val, 
					total_requests);
			
			sem_post(&QUEUE_SEM);
		}
		else
		{
			break;
		}
	}
}

static void *tutorRoutine(void *id)
{
	sem_post(&TUTOR_ROOM);

	sem_wait(&QUEUE_SEM);

	sem_wait(&TUTOR_ROOM);

	sem_post(&TUTORING);
	//printf("Tu: Student %d tutored by Tutor %d. Students tutored now = %d\n", tutorList[i].id);

	if (!done)
		tutorRoutine(id);
}

static void *studentRoutine(void *id) 
{
	pthread_mutex_lock(&SAVE);
	sem_getvalue(&WAITING_ROOM, &ss_val);
	if (!ss_val) 
	{
		printf("St: Student %d found no empty chair. Will try again later\n", students[(long) id].id);
		usleep(random() % 2);
	}
	sem_wait(&WAITING_ROOM);
	sem_getvalue(&WAITING_ROOM, &ss_val);
	printf("St: Student %d takes a seat. Empty chairs = %d\n", students[(long) id].id, ss_val);

	seats[NUM_SEATS - ss_val] = students[(long) id];
	pthread_mutex_unlock(&SAVE);

	sem_post(&COORDINATOR);
	printf("St: Student %d flagged coordinator\n", students[(long) id].id);

	sem_wait(&TUTOR_ROOM);
	printf("St: Student %d recieved help from Tutor y\n", students[(long) id].id);
	students[(long) id].priority--;

	sem_post(&WAITING_ROOM);
	printf("St: Student %d leaving waiting room\n", students[(long) id].id);
	usleep(0.2);

	if (students[(long) id].priority)
		studentRoutine(id);
}

void *makeTuts()
{
  	long counter = 0;
    while(counter < NUM_TUTORS)
    {
		// create a new tutor
		struct tutor tut = {counter, 0};
		tutors[counter] = tut;

        // Declare and create a thread 
        pthread_t tut_thread;
        if (pthread_create(&tut_thread, NULL, tutorRoutine, (void *) counter) != 0)
            printf("Failed to create thread for tutor.");
        counter++;
            
        /* Sleep for 100ms before creating another customer */
        usleep(100000);
    }
}

void *makeStuds()
{
    long counter = 0;
    while(counter < NUM_STUDENTS)
    {
		// create a new student 
		struct student stud = {counter, MAX_PRIORITY};
		students[counter] = stud;

        /* Declare and create a thread */
        pthread_t stud_thread;
        if (pthread_create(&stud_thread, NULL, studentRoutine, (void *) counter) != 0)
            printf("Failed to create thread for student.");
        counter++;
    }
}

void initGlobals(char **argv)
{
	NUM_STUDENTS = atoi(argv[1]);
	NUM_TUTORS = atoi(argv[2]);
	NUM_SEATS = atoi(argv[3]);
	MAX_PRIORITY = atoi(argv[4]);

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
	if (argc != 5) {
		printf("EXEC FORMAT: ./csmc #students #tutors #chairs #help\n");
		exit(1);
	}
	initGlobals(argv);
	struct tutor tu[NUM_TUTORS];
	struct student stu[NUM_STUDENTS], se[NUM_SEATS];
	tutors = tu;
	students = stu;
	seats = se;

	pthread_t cor, tut_maker, stud_maker; // define initial threads

	if (pthread_create(&cor, NULL, coordinatorRoutine, NULL) != 0)
		printf("Faied to create thread for coordinator\n");

	// CREATE TUTOR THREADS
	if (pthread_create(&tut_maker, NULL, makeTuts, NULL) != 0)
		printf("Faied to create thread for making tutors\n");

	// CREATE STUDENT THREADS
	if (pthread_create(&stud_maker, NULL, makeStuds, NULL) != 0)
		printf("Faied to create thread for making students\n");

	pthread_join(stud_maker, NULL);
	pthread_join(tut_maker, NULL);

	done = 1;
	pthread_join(cor, NULL);
}
