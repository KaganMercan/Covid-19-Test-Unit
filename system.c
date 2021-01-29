//***********************A BRIEF INFORMATION ON THE USE OF THE SIMULATION***************************
//THE BEST EFFICIENCY IN USING THE SIMULATION CAN BE ACHIEVED BY ADJUSTING THE SCREEN TO SHOW
//THE REFRESHED PORTION OF THE SIMULATION
//***************************************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
//-----DURATION TIMES AND SIZES OF VARIABLES-------//
#define CREATE_PATIENT_TIME 3 // Creating patients in this time scala
#define COVID19_TEST_TIME 8 // Test duration
#define MAX_PATIENT 72 // Total patient number for simulation
#define MAX_STAFF 8 // Unit healthcare staff number
#define MAX_UNIT 8 // Covid-19 test unit number for hospital
#define MAX_UNITCAPACITY 3 // Available unit capacity for each unit
//------HEALTHCARE STAFF STATES------//
#define OUT_OF_SERVICE -1
#define ANNOUNCING 0
#define VENTILATING_ROOM 1
#define FULL_BUSY 2
//------SEMAPHORES-----//
sem_t patientWait[MAX_UNIT]; // Patients wait for covid-19 test until unit will be full
sem_t testStop; // When unit test are done this sem will be used
sem_t unit[MAX_UNIT]; // Unit sem
sem_t mutex; // Will be used for critical sections
int createdPatients = 0, patientTestDone = 0;
int unit_num = -1;
int unitCapacity[MAX_UNIT] = {0, 0, 0, 0, 0, 0, 0, 0}; // Keep the number of the patients in the each unit
int healthcareStaffStates[MAX_STAFF] = {-1, -1, -1, -1, -1, -1, -1, -1}; // Keep the status of the healthcare staff members
int testNum[MAX_STAFF] = {0, 0, 0, 0, 0, 0, 0, 0}; // Keep the service number of units to prevent starvation
void *staff(void *staffID)
{
    int i = 0;
    int id = (int)staffID; // staff id will be unique
    while (1)
    {
        sem_post(&testStop); // Patient will be wait untill unit will be full
        sem_wait(&mutex);
        healthcareStaffStates[id] = ANNOUNCING;
        sem_post(&mutex);
        sem_wait(&mutex);
        int unitPatientCapacity = 0;
        for (i = 0; i < MAX_UNIT; i++)
        {
            unitPatientCapacity += unitCapacity[i];
        }
        if ((unitCapacity[id]==0 && createdPatients-patientTestDone-unitPatientCapacity==0) || createdPatients==0) // if there no patient in the unit room, staff will be ventilating the room
        {
            healthcareStaffStates[id] = VENTILATING_ROOM; // Unit will be changes to ventilatin because of the "0" patient num.
            simulation(); // calling simulation on each refresh
            sem_post(&mutex);
            sem_wait(unit + id); // unit staff waiting.
        }
        else
        {
            sem_post(&mutex);
        }
        sem_wait(&mutex);
        healthcareStaffStates[id] = ANNOUNCING;
        sem_post(&mutex);
        // The healthcare staff anouncing the how many capacity left in the unit.
        while (1)
        {
            sem_wait(&mutex);
            // If the unit is full unit staff will be start covid-19 test. And the state of the unit will be changed to "FULL_BUSY" state.
            if (unitCapacity[id] == MAX_UNITCAPACITY)
            {
                healthcareStaffStates[id] = FULL_BUSY;
                sem_post(&mutex);
                break;
            }
            simulation(); 
            sem_post(&mutex);
            sleep(1);
        }
        sleep(COVID19_TEST_TIME); // Covid-19 FULL_BUSY duration
        sem_wait(&mutex);
        simulation(); // simulation refreshed and called
        patientTestDone += unitCapacity[id]; // The number of covid-19 tests that ended will be added to the "patientTestDone".
        unitCapacity[id] = 0;
        sem_post(&mutex);
    }
    return NULL;
}
void *patient(void *patientID)
{
    int id = (int)patientID;
    int i = 0;
    int unitID;
    sem_wait(&mutex); // lock mutex
    createdPatients++; // Patient wait is checking 
    sem_post(&mutex); // unlock mutex
    sem_wait(&testStop); // If all units are in full capacity patient will be wait.
    sem_wait(&mutex);
    while (1)
    {
        int minStarvation = testNum[0]; // Starvation control
        int capacityInUnit = 0;
        for(i = 1; i < MAX_UNIT; i++)
        {
            if(testNum[i] < testNum[i - 1])
                minStarvation = testNum[i];
        }
        for(i = 0; i < MAX_UNIT; i++)
        {                            
            if (healthcareStaffStates[i] == FULL_BUSY || healthcareStaffStates[i] == OUT_OF_SERVICE || unitCapacity[i] >= MAX_UNITCAPACITY)
            {
                continue;
            }
            if(unitCapacity[i] > capacityInUnit && unitCapacity[i] < MAX_UNITCAPACITY){
                unit_num = i;  
                capacityInUnit = unitCapacity[i];             
            }
            else if(unitCapacity[i] == capacityInUnit && unitCapacity[i] < MAX_UNITCAPACITY){
                if(testNum[i] <= minStarvation){ 
                    unit_num = i; 
                }
            }
        }
        if (healthcareStaffStates[unit_num] != FULL_BUSY && healthcareStaffStates[unit_num] != OUT_OF_SERVICE && unitCapacity[unit_num] < MAX_UNITCAPACITY )
        {
            unitID = unit_num; // Starvation control
            unitCapacity[unitID]++; // According the current unit room capacity is updated in this line.

            if(unitCapacity[unitID] == MAX_UNITCAPACITY)
                testNum[unitID]++;

            if(unitCapacity[unitID] < MAX_UNITCAPACITY) 
                sem_post(&testStop); // If there are still capacity in the unit room so patients in the waiting room will be warn.

            break;
        }
    }
    if (healthcareStaffStates[unitID] == VENTILATING_ROOM) // If the unit healthcare staff is ventilating the room, then the patient have to warn the staff.
    {
        sem_post(unit + unitID);
        sem_post(&mutex);
    }
    else
    {
        sem_post(&mutex);
    }
    sem_wait(&mutex);
    
    if (unitCapacity[unitID] < MAX_UNITCAPACITY) // patients will be wait in the unit untill it will be reach full capacity.
    {
        sem_post(&mutex);
        sem_wait(patientWait + unitID);
        sleep(1);
    }
    else if (unitCapacity[unitID] == MAX_UNITCAPACITY) // The last patient  will wake the unit staff.
    {
        sem_post(&mutex);
        sem_post(patientWait + unitID);
        sem_post(patientWait + unitID);
        sem_post(patientWait + unitID);
    }
    return NULL;
}
void simulation() // Simulate the program
{
    int i = 0;
    int j = 0;
    printf("Welcome to Dokuz Eylul Hospital Covid-19 Test Unit Simulation\n");
    printf("Thank you for your patience, we wish you healthy days...\n");
    printf("-------------------------------------------------------------\n");
    printf("Patients Test Done: %d\n \n", patientTestDone);
    for(i = 0; i < MAX_UNIT; i++)
    {
        printf("UNIT %d\n",i+1);
        for(j = 0; j < unitCapacity[i]; j++)
        {
            printf("[x]");
        }
        for(j = 0; j < MAX_UNITCAPACITY - unitCapacity[i]; j++)
        {
            printf("[ ]");
        }
        if(healthcareStaffStates[i] == VENTILATING_ROOM)
            printf("  Staff %d is Ventilating The Unit",i+1);
        else if(healthcareStaffStates[i] == FULL_BUSY)
            printf("  Covid-19 Test Started");
        else if(healthcareStaffStates[i] == ANNOUNCING)
            printf("  Staff %d is Announcing: The last %d people, let's start!\nPlease, pay attention to your social distance and hygiene; use a mask.", i+1, MAX_UNITCAPACITY - unitCapacity[i]);
        else if(healthcareStaffStates[i] == OUT_OF_SERVICE)
            printf("Out of Service ");
        printf("\nTotal Covid-19 Test Number: %d\n", testNum[i]);
        printf("\n");
    }
}
int main(int argc, char *argv[])
{
    int i;
    srand(time(NULL));
    pthread_t thread_patient[MAX_PATIENT];
    pthread_t thread_healthcareStaff[MAX_STAFF];
    sem_init(&testStop, 0, 0);
    sem_init(&mutex, 0, 1);
    for (i = 0; i < MAX_UNIT; i++)
    {
        sem_init(unit + i, 0, 0);
    }
    for (i = 0; i < MAX_UNIT; i++)
    {
        sem_init(patientWait + i, 0, 0);
    }
    // Her healthcare staff için thread oluştur.
    for (i = 0; i < MAX_STAFF; i++)
    {
        pthread_create(thread_healthcareStaff + i, NULL, &staff, (void *)i);
    }
    // Her patient için thread oluştur.
    for (i = 0; i < MAX_PATIENT; i++)
    { 
        pthread_create(thread_patient + i, NULL, &patient, (void *)i);
        sleep(rand() % CREATE_PATIENT_TIME); // random patient creation time
    }
    // Staff threadlerini joinle.
    for(i = 0; i < MAX_STAFF; i++) 
    {
        pthread_join(thread_healthcareStaff[i], NULL);
    }
    // Patient threadlerini joinle burada
    for (i = 0; i < MAX_PATIENT; i++) 
    {
        pthread_join(thread_patient[i], NULL);
    }
    return 0;
}