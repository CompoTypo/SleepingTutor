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
int NUM_STUDENTS;
int NUM_TUTORS;
int NUM_SEATS;
int MAX_PRIORITY;

int ts_val, ss_val;
int students_tutored = 0;

sem_t COORDINATOR; // for student to signal coordinator for queueing
sem_t WAITING_ROOM; // for keeping track of students awaiting tutoring
sem_t QUEUE_SEM; // for making tutors aware that a student(s) has been queued
sem_t TUTOR_ROOM; // manage tutors available
sem_t TUTORING; // for tutor to hold a student for tutoring

pthread_mutex_t mut_lock;

int * tutorList;

struct student 
{
	int id;
	int priority;
};
struct student *studentList;
int stuListCurSize;


/* PRIORITY QUEUE 
 * MAX-HEAP of waiting students
 */
int CUR_SIZE = 0;
struct student *QUEUE;

static void swap(struct student *a, struct student *b)
{
	struct student temp = *a;
	*a = *b;
	*b = temp;
}

static void heapify(struct student chairs[], int n, int i)
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
		swap(&chairs[i], &chairs[greatest]);
		heapify(chairs, n, greatest);
	}
}

static void heapSort(struct student chairs[], int n)
{
	int i;
	for (i = n / 2 - 1; i >= 0; i--)
		heapify(chairs, n, i);
	for (i = n - 1; i >= 0; i--) 
	{
		swap(&chairs[0], &chairs[i]);
		heapify(chairs, i, 0);
	}
}

static void pushHeap(struct student chairs[], struct student s)
{
	chairs[CUR_SIZE] = s;
	CUR_SIZE++;
	heapSort(chairs, CUR_SIZE);
}

static struct student popHeap(struct student chairs[])
{
	struct student ret = chairs[0];
	CUR_SIZE--;
	chairs[0] = chairs[CUR_SIZE];
	heapSort(chairs, CUR_SIZE);
	return ret;

}
// END MAX_HEAP

int done = 0;
static void *coordinatorRoutine()
{
	int total_requests = 0;
	while (1)
	{
		sem_wait(&COORDINATOR);
		total_requests++;
		sleep(0.2);
		
		
		if (!done) {
			pushHeap(QUEUE, studentList[0]);
			printf("Co: Student %d with priority %d in the queue. Waiting students now = %d. Total requests = %d.\n", 
					studentList[0].id, 
					studentList[0].priority, 
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
	long i = (long) id;

	sem_post(&TUTOR_ROOM);

	sem_wait(&QUEUE_SEM);
	struct student curStu = popHeap(QUEUE);

	sem_wait(&TUTOR_ROOM);

	sem_post(&TUTORING);
	printf("Tu: Student %d tutored by Tutor %d. Students tutored now = %d\n", curStu.id, tutorList[i]);

	tutorRoutine((void *) i);
}

static void *studentRoutine(void *id) 
{
	long i = (long) id;

	sem_getvalue(&WAITING_ROOM, &ss_val);
	if (!ss_val) 
	{
		printf("St: Student %d found no empty chair. Will try again later\n", studentList[i].id);
		sleep(random() % 2);
	}
	sem_wait(&WAITING_ROOM);

	sem_getvalue(&WAITING_ROOM, &ss_val);
	printf("St: Student %d takes a seat. Empty chairs = %d\n", studentList[i].id, ss_val);
	sleep(0.2);

	sem_post(&COORDINATOR);
	printf("St: Student %d flagged coordinator\n", studentList[i].id);
	sleep(0.2);

	sem_wait(&TUTOR_ROOM);
	printf("St: Student %d recieved help from Tutor y\n", studentList[i].id);
	studentList[i].priority--;

	sem_post(&WAITING_ROOM);
	printf("St: Student %d leaving waiting room\n", studentList[i].id);
	sleep(0.2);

	if (studentList[i].priority)
		studentRoutine((void *) i);
	else
		return;
	
}

void main(int argc, char **argv)
{
	// INPUT VALIDATION
	if (argc != 5) {
		printf("EXEC FORMAT: ./csmc #students #tutors #chairs #help\n");
		exit(1);
	}
	NUM_STUDENTS = atoi(argv[1]);
	NUM_TUTORS = atoi(argv[2]);
	NUM_SEATS = atoi(argv[3]);
	MAX_PRIORITY = atoi(argv[4]);

	// ORGANIZE DATA COLLECTIONS
	int tutAlloc[NUM_TUTORS];
	struct student studentAlloc[NUM_STUDENTS];
	struct student queueAlloc[NUM_SEATS];

	tutorList = tutAlloc;
	QUEUE = queueAlloc;
	studentList = studentAlloc;

	pthread_t cor;
	pthread_t tuts[NUM_TUTORS];
	pthread_t stu[NUM_STUDENTS];

	sem_init(&COORDINATOR, 0, 0);
	sem_init(&WAITING_ROOM, 0, NUM_SEATS);
	sem_init(&QUEUE_SEM, 0, 0);
	sem_init(&TUTOR_ROOM, 0, 0);
	sem_init(&TUTORING, 0, 0);

	pthread_mutex_init(&mut_lock, NULL);
	
	pthread_create(&cor, NULL, coordinatorRoutine, NULL);
	long i;

	// CREATE TUTOR THREADS
	for (i = 0; i < NUM_TUTORS; i++)
	{
		tutorList[i] = i+1;
		pthread_create(&tuts[i], NULL, tutorRoutine, (void *) i);
	}

	// CREATE STUDENT THREADS
	stuListCurSize = 0;
	for (i = 0; i < NUM_STUDENTS; i++)
	{
		struct student newStud = {i+1, MAX_PRIORITY};
		studentList[i] = newStud;
		pthread_create(&stu[i], NULL, studentRoutine, (void *) i);
	}

	for (i = 0; i < NUM_STUDENTS; i++)
	{
		pthread_join(stu[i], NULL);
	}

	for (i = 0; i < NUM_TUTORS; i++)
	{
		pthread_join(tuts[i], NULL);
	}

	done = 1;
	pthread_join(cor, NULL);
}
