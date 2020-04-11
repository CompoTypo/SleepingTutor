#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#define MAX_SLEEP 2
#define TUTOR_TIME 0.2

typedef struct 
{
    int id;
    pthread_t thread;
    int visits;
    sem_t notifyStudent;
} Student;

typedef struct 
{
    int id;
    pthread_t thread;
} Tutor;

typedef struct 
{
    bool taken; 
    int studentId;
    int studentVisits;
    int arrivedAt;
    int priority;
    sem_t *notifyStudent;
    int tutoredBy;
} Chair;

Chair *chairs;
int chairsTaken = 0, arrivedStudentId = -1, arrivedStudentVisits = -1;
sem_t *arrivedStudentNotifier,
tutorNeeded,
room_mutex,
chairs_mutex,
notifyCoordinator;

int coordinatorRequests = 0, tutoringCompleted = 0, activeTutoring = 0;

int NUM_CHAIRS = 0, NUM_HELP = 0;

void *StudentThread(void *data)
{
    Student self = *(Student*) data;

    while (self.visits < NUM_HELP)
    {
        sleep(rand() % MAX_SLEEP);

        sem_wait(&room_mutex);
        if (chairsTaken < NUM_CHAIRS)
        {
            chairsTaken++;
            printf("St: Student %d takes a seat. Empty chairs is %d.\n", self.id, NUM_CHAIRS - chairsTaken);
            arrivedStudentId = self.id;
            arrivedStudentVisits = self.visits;
            arrivedStudentNotifier = &self.notifyStudent;

            sem_post(&notifyCoordinator);
            sem_post(&room_mutex);

            sem_wait(&self.notifyStudent);

            int chairIndex = 0;
            for (int i = 0; i < NUM_CHAIRS; i++)
            {
                if (chairs[i].taken && (chairs[i].studentId == self.id))
                {
                    chairIndex = i;
                    break;
                }
            }
            if (chairIndex == -1)
            {
                exit(-1);
            }
            int tutoredBy = chairs[chairIndex].tutoredBy;

            sleep(TUTOR_TIME);
            printf("St: Student %d received help from Tutor %d. \n", self.id, tutoredBy);
            chairs[chairIndex].taken = false;
            chairsTaken--;
            self.visits++;
        }
        else
        {
            printf("St: Student %d found no empty chair. Will try again later.\n", self.id);
            sem_post(&room_mutex);
            continue;
        }
    }
    pthread_exit(NULL);
}

void * TutorThread(void *data)
{
    Tutor self = *(Tutor*)data;
    while (1)
    {
        sem_wait(&tutorNeeded);

        int highestPriority = 0;
        for (int i = 1; i < NUM_CHAIRS; i++)
        {
            if (chairs[i].priority > chairs[highestPriority].priority)
            {
                highestPriority = i;
            }
        }
        
        chairs[highestPriority].tutoredBy = self.id;
        activeTutoring++;
        sem_post(chairs[highestPriority].notifyStudent);

        sleep(TUTOR_TIME);
        tutoringCompleted++;
        printf("Tu: Student %d tutored by Tutor %d. Student tutored now = %d. Total sessions tutored = %d\n", chairs[highestPriority].studentId, self.id, activeTutoring, tutoringCompleted);
        activeTutoring--;
    }
}

void *CoordinatorThread(void *data)
{
    while (1)
    {
        sem_wait(&notifyCoordinator);

        int studentId = arrivedStudentId;
        arrivedStudentId = -1;
        int studentVisits = arrivedStudentVisits;
        arrivedStudentVisits = -1;
        sem_t *studentNotifier = arrivedStudentNotifier;
        arrivedStudentNotifier = NULL;

        coordinatorRequests++;

        int currentChair = -1;
        for (int i = 0; i < NUM_CHAIRS; i++)
        {
            if (!chairs[i].taken)
            {
                currentChair = 1; 
                break;
            }
        }
            
        if (currentChair == -1)
        {
            exit(-1);
        }

        chairs[currentChair].taken = true;
        chairs[currentChair].studentId = studentId;
        chairs[currentChair].arrivedAt = coordinatorRequests;
        chairs[currentChair].studentVisits = studentVisits;
        chairs[currentChair].notifyStudent = studentNotifier;

        int priority = -1;
        int priorityLower = 0;
        int priorityHigher = 0;

        for (int i = 0; i < NUM_CHAIRS; i++)
        {
            if (i == currentChair)
                continue;
            if (chairs[i].taken)
            {
                if (chairs[i].studentVisits < studentVisits || (chairs[i].studentVisits == studentVisits && chairs[i].arrivedAt < chairs[currentChair].arrivedAt))
                {
                    priorityHigher++;
                }
                else if (chairs[i].studentVisits > studentVisits || (chairs[i].studentVisits == studentVisits && chairs[i].arrivedAt > chairs[currentChair].arrivedAt))
                {
                    priorityLower++;
                    chairs[i].priority++;
                }
            }
            else
            {
                continue;
            }
        }
        priority = priorityLower;
        chairs[currentChair].priority = priority;
        printf("Co: Student %d woth priority %d in the queue. Waiting students now = %d. Total requests = %d\n", studentId, priority, chairsTaken, coordinatorRequests);
        sem_post(&tutorNeeded);
    }
}

int main(int argc, char const *argv[])
{
    int NUM_STUDENTS, NUM_TUTORS;
    if (argc == 5)
    {
        NUM_STUDENTS = atoi(argv[1]);
        NUM_TUTORS = atoi(argv[2]);
        NUM_CHAIRS = atoi(argv[3]);
        NUM_HELP = atoi(argv[4]);
    }
    else
    {
        printf("ERROR: IMPROPER ARGEMENTS\n");
        exit(-1);
    }
    
    sem_init(&room_mutex, 0, 1);
    sem_init(&notifyCoordinator, 0, 0);
    sem_init(&tutorNeeded, 0, 0);

    Student *students = (Student*) malloc(NUM_STUDENTS * sizeof(Student));
    Tutor *tutors = (Tutor*) malloc(NUM_TUTORS * sizeof(Tutor));
    chairs = (Chair*) malloc(NUM_CHAIRS * sizeof(Chair));

    pthread_t coordinator;
    pthread_create(&coordinator, NULL, CoordinatorThread, (void*)&NUM_CHAIRS);

    int rc; long t;
    for (t = 0; t < NUM_STUDENTS; t++)
    {
        students[t].id = t;
        students[t].visits = 0;
        sem_init(&students[t].notifyStudent, 0, 0);
        rc = pthread_create(&students[t].thread, NULL, StudentThread, (void*)&students[t]);
        if (rc)
        {
            exit(-1);
        }
    }
    for (t = 0; t < NUM_TUTORS; t++)
    {
        tutors[t].id = t;
        rc = pthread_create(&tutors[t].thread, NULL, TutorThread, (void*)&tutors[t]);
        if (rc)
        {
            exit(-1);
        }
    }
    
    for (t = 0; t < NUM_STUDENTS; t++)
    {
        pthread_join(students[t].thread, NULL);
    }
    
    printf("Exiting Main\n");
    return 0;
}
