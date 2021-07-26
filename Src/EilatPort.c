#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <time.h>

#define MAX_VESSELS_THREADS 50              // Maximum vessels
#define MIN_VESSELS_THREADS 2               // Minimum vessels
#define MAX_CARGO_WEIGHT 50                 // Maximum weight of vessels cargo in tons
#define MIN_CARGO_WEIGHT 5                  // Minimum weight of vessels cargo in tons
#define MIN_CRANES 2                        // Minimum cranes
#define BUFFER_SIZE 4                       // Buffer size for pipes
#define MAX_SLEEP_MSEC 3000			        // Maximum sleep time in miliseconds
#define MIN_SLEEP_MSEC 5			        // Minimum sleep time in miliseconds
#define MAX_MESSAGE 100		                // Maximum message length
#define MAX_TIMESTAMP_MESSAGE 11		    // Maximum timestamp message length
#define TRUE 1
#define FALSE 0

#define sleepTime() (rand() % (MAX_SLEEP_MSEC - MIN_SLEEP_MSEC + 1) + MIN_SLEEP_MSEC)
#define randNumOfCranes() (rand() % (numOfVessels - MIN_CRANES) + MIN_CRANES)
#define randNumOfWeight() (rand() % (MAX_CARGO_WEIGHT - MIN_CARGO_WEIGHT + 1) + MIN_CARGO_WEIGHT)

HANDLE ReadHandle, WriteHandle2;
DWORD read, written;

HANDLE canalMutex;
HANDLE unloadingQueueMutex;
HANDLE barrierMutex;
HANDLE cranesMutex[MAX_VESSELS_THREADS];
HANDLE vesselsMutex[MAX_VESSELS_THREADS];

typedef struct vesselInfo
{
    int vesselCargoWeight;
    int vesselID;
} vesselInfo;

vesselInfo vesselsQueue[MAX_VESSELS_THREADS];
int in;                                     // Next empty slot in queue for vessel
int out;                                    // Next slot in queue to take vessel
int vesselsQueyIn;                          // How many vessels entered the unloading quay in a single time
int vesselsQueyOut;                         // How many vessels exited the unloading quay in a single time
int numOfCranes;
int numOfVessels;

DWORD WINAPI Vessel(LPVOID Param);
DWORD WINAPI Crane(LPVOID Param);
int initGlobalData(int numOfCranes, int numOfVessels);
void printMessage(char* message);
int checkPrime(int num);
void closeHandles(HANDLE* VesselsThreadHandles, HANDLE* CranesThreadHandles);
int insert_vessel(int);

int main(VOID)
{
    ReadHandle = GetStdHandle(STD_INPUT_HANDLE);
    WriteHandle2 = GetStdHandle(STD_OUTPUT_HANDLE);

    char message[MAX_MESSAGE];
    int buffer, i;
    DWORD ThreadID;
    HANDLE VesselsThreadHandles[MAX_VESSELS_THREADS];
    HANDLE CranesThreadHandles[MAX_VESSELS_THREADS];
    srand(time(NULL));

#pragma region Haifa Validate
    // Read number of vessels from Haifa Port
    if (!ReadFile(ReadHandle, &numOfVessels, BUFFER_SIZE, &read, NULL))
    {
        fprintf(stderr, "Eilat Port: Error - reading from pipe!\n");
        return 1;
    }
    printMessage("Eilat Port: Number of vessels recieved succesfully");

    // Check if the number of vessels is a prime number
    int isPrime = checkPrime(numOfVessels);
    if (isPrime)
        printMessage("Eilat Port: Request denied");
    else
    {
        printMessage("Eilat Port: Request approved");

        // Random number of cranes until legal
        while (numOfVessels % (numOfCranes = randNumOfCranes()) != 0);

        // Initialize global variables
        if (!initGlobalData(numOfCranes, numOfVessels))
        {
            fprintf(stderr, "Eilat Port: Error - semaphores creation failed!\n");
            return 1;
        }

        // Initialize Cranes in Array
        for (i = 0; i < numOfCranes; i++)
        {
            CranesThreadHandles[i] = CreateThread(NULL, 0, Crane, (int*)(i + 1), 0, &ThreadID);
            if (!CranesThreadHandles[i])
            {
                fprintf(stderr, "Eilat Port: Error - in crane id %d creation!\n", i + 1);
                return 1;
            }
            sprintf(message, "Crane %d - was created", i + 1);
            printMessage(message);
        }
    }

    // Write back to Haifa Port the answer
    if (!WriteFile(WriteHandle2, &isPrime, BUFFER_SIZE, &written, NULL))
    {
        fprintf(stderr, "Eilat Port: Error - writing to pipe\n");
        return 1;
    }
    if (isPrime)
    {
        closeHandles(VesselsThreadHandles, CranesThreadHandles);
        CloseHandle(ReadHandle);
        return 1;
    }
#pragma endregion

    // Listen to incoming vessels
    for (i = 0; i < numOfVessels; i++)
    {
        if (ReadFile(ReadHandle, &buffer, BUFFER_SIZE, &read, NULL))
        {
            VesselsThreadHandles[i] = CreateThread(NULL, 0, Vessel, (int*)buffer, 0, &ThreadID);
            if (!VesselsThreadHandles[i])
            {
                fprintf(stderr, "Eilat Port: Error - in vessel id %d creation!\n", i + 1);
                return 1;
            }
        }
        else
            fprintf(stderr, "Eilat Port: Error - reading from pipe!\n");
    }

    // Close the read end of the pipe
    CloseHandle(ReadHandle);

    // Join on both Vessels and Cranes Threads
    WaitForMultipleObjects(numOfCranes, CranesThreadHandles, TRUE, INFINITE);
    printMessage("Eilat Port: All cranes threads are done");
    WaitForMultipleObjects(numOfVessels, VesselsThreadHandles, TRUE, INFINITE);
    printMessage("Eilat Port: All vessel threads are done");

    // Close all Handles
    closeHandles(VesselsThreadHandles, CranesThreadHandles);

    printMessage("Eilat Port: Exiting...");

    return 0;
}

int initGlobalData(int numOfCranes, int numOfVessels)
{
    int i;
    // Initialize cranes semaphore array
    for (i = 0; i < numOfCranes; i++)
    {
        cranesMutex[i] = CreateSemaphore(NULL, 0, 1, NULL); // cranes are not working
        if (!cranesMutex[i])
        {
            printf("initGlobalData:: Unexpected error in semaphore creation\n");
            return FALSE;
        }
    }

    // Initialize vessels semaphore array
    for (i = 0; i < numOfVessels; i++)
    {
        vesselsMutex[i] = CreateSemaphore(NULL, 0, 1, NULL); // vessel is not unloading
        if (!vesselsMutex[i])
        {
            printf("initGlobalData:: Unexpected error in semaphore creation\n");
            return FALSE;
        }
    }

    // Initialize canal mutex
    canalMutex = CreateMutex(NULL, FALSE, NULL);
    if (!canalMutex)
    {
        fprintf(stderr, "Main:: Unexpected error in semaphore creation\n");
        return FALSE;
    }

    // Initialize queue mutex
    unloadingQueueMutex = CreateMutex(NULL, FALSE, NULL);
    if (!unloadingQueueMutex)
    {
        fprintf(stderr, "Main:: Unexpected error in semaphore creation\n");
        return FALSE;
    }
    
    barrierMutex = CreateMutex(NULL, FALSE, NULL);
    if (!barrierMutex)
    {
        fprintf(stderr, "Main:: Unexpected error in semaphore creation\n");
        return FALSE;
    }

    return TRUE;
}

DWORD WINAPI Vessel(LPVOID Param)
{
    char message[MAX_MESSAGE];
    int thrId = (int)Param;
    int vesselIndxInQueue;
    int cargoWeight;
    int crainIndx;

    // Arrived to Eilat Port
    Sleep(sleepTime());

    // Enter barrier
    vesselIndxInQueue = insert_vessel(thrId);

    while (TRUE)
    {
        // Enter critical section of removing the queue
        WaitForSingleObject(unloadingQueueMutex, INFINITE);

        /* If current vessel is the first vessel in queue
           AND
           number of vessels in queue is >= number of cranes OR the unloading quay is in entering state (vessels enter the quay)
           AND
           the unloading quay is not full */
        if (vesselIndxInQueue == out && (in - out >= numOfCranes || vesselsQueyIn > 0) && vesselsQueyIn != numOfCranes) break;

        if (!ReleaseMutex(unloadingQueueMutex))
            fprintf(stderr, "Eilat Port: Error - in mutex release!\n");

        Yield();
    }
    out++;
    crainIndx = vesselsQueyIn++;
    if (!ReleaseMutex(unloadingQueueMutex))
        fprintf(stderr, "Eilat Port: Error - in mutex release!\n");

    // Move to crane in unloading Quay
    sprintf(message, "Vessel %d - entering Unloading Quay", thrId);
    printMessage(message);
    Sleep(sleepTime());

    sprintf(message, "Vessel %d - anchored in front of crane %d at the Unloading Quay", thrId, crainIndx + 1);
    printMessage(message);

    // Random number of vessel cargo in tons
    cargoWeight = randNumOfWeight();
    sprintf(message, "Vessel %d - has a cargo weight of %d", thrId, cargoWeight);
    printMessage(message);
    vesselsQueue[vesselIndxInQueue].vesselCargoWeight = cargoWeight;
    if (!ReleaseSemaphore(cranesMutex[crainIndx], 1, NULL))
    {
        fprintf(stderr, "Eilat Port: Error - in semaphore release!\n");
        return 1;
    }
    WaitForSingleObject(vesselsMutex[thrId - 1], INFINITE);
    Sleep(sleepTime());

    sprintf(message, "Vessel %d - exiting Unloading Quay", thrId);
    printMessage(message);

    WaitForSingleObject(unloadingQueueMutex, INFINITE);
    vesselsQueyOut++;
    if (vesselsQueyOut == numOfCranes)
    {
        vesselsQueyIn = 0;
        vesselsQueyOut = 0;
    }
    if (!ReleaseMutex(unloadingQueueMutex))
        fprintf(stderr, "Eilat Port: Error - in mutex release!\n");

    // Entering Red Sea ==> Med. Sea Canal
    WaitForSingleObject(canalMutex, INFINITE);
    sprintf(message, "Vessel %d - entering Canal: Red Sea ==> Med. Sea", thrId);
    printMessage(message);
    Sleep(sleepTime());

    if (!WriteFile(WriteHandle2, &thrId, BUFFER_SIZE, &written, NULL))
        fprintf(stderr, "Eilat Port: Error - writing to pipe\n");
    sprintf(message, "Vessel %d - exiting Canal: Red Sea ==> Med. Sea", thrId);
    printMessage(message);
    if (!ReleaseMutex(canalMutex))
    {
        fprintf(stderr, "Eilat Port: Error - in mutex release!\n");
        return 1;
    }

    return 0;
}

int insert_vessel(int vesselId)
{
    char message[MAX_MESSAGE];
    int freeIndx;

    // Enter critical section of updating the queue
    WaitForSingleObject(barrierMutex, INFINITE);

    sprintf(message, "Vessel %d - entered Barrier", vesselId);
    printMessage(message);

    freeIndx = in;
    vesselsQueue[in++].vesselID = vesselId;

    // Exit critical section of updating the queue
    if (!ReleaseMutex(barrierMutex))
        fprintf(stderr, "Eilat Port: Error - in mutex release!\n");

    return freeIndx;
}

DWORD WINAPI Crane(LPVOID Param)
{
    char message[MAX_MESSAGE];
    int thrId = (int)Param;
    int vesselId;
    int cargoWeight;
    int index;
    int numOfIterations = numOfVessels / numOfCranes;
    int i;

    for (i = 0; i < numOfIterations; i++)
    {
        WaitForSingleObject(cranesMutex[thrId - 1], INFINITE);
        index = i * numOfCranes + thrId - 1;
        vesselId = vesselsQueue[index].vesselID;
        cargoWeight = vesselsQueue[index].vesselCargoWeight;
        Sleep(sleepTime());

        sprintf(message, "Crain %d - unloaded %d weight from vessel %d", thrId, cargoWeight, vesselId);
        printMessage(message);
        if (!ReleaseSemaphore(vesselsMutex[vesselId - 1], 1, NULL))
        {
            fprintf(stderr, "Eilat Port: Error - in semaphore release!\n");
            return 1;
        }
    }
    sprintf(message, "Crain %d - finished his job", thrId);
    printMessage(message);

    return 0;
}

void printMessage(char* message) {
    time_t current_time;
    struct tm* time_info;
    char timeString[MAX_TIMESTAMP_MESSAGE];

    time(&current_time);
    time_info = localtime(&current_time);

    strftime(timeString, sizeof(timeString), "[%H:%M:%S]", time_info);
    fprintf(stderr, "%s %s\n", timeString, message);
}

int checkPrime(int num)
{
    for (int i = 2; i <= num / 2; i++)
    {
        if (num % i == 0)
            return FALSE;
    }
    return TRUE;
}

void closeHandles(HANDLE* VesselsThreadHandles, HANDLE* CranesThreadHandles)
{
    int i;
    for (i = 0; i < numOfVessels; i++) {
        CloseHandle(VesselsThreadHandles[i]);
        CloseHandle(vesselsMutex[i]);
    }
    for (i = 0; i < numOfCranes; i++) {
        CloseHandle(CranesThreadHandles[i]);
        CloseHandle(cranesMutex[i]);
    }
    CloseHandle(WriteHandle2);
    CloseHandle(canalMutex);
    CloseHandle(unloadingQueueMutex);
    CloseHandle(barrierMutex);
}