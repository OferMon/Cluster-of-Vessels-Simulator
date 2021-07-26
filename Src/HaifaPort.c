#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <time.h>

#define MAX_VESSELS_THREADS 50      // Maximum vessels
#define MIN_VESSELS_THREADS 2       // Minimum vessels
#define BUFFER_SIZE 4               // Buffer size for pipes
#define MAX_SLEEP_MSEC 3000			// Maximum sleep time in miliseconds
#define MIN_SLEEP_MSEC 5			// Minimum sleep time in miliseconds
#define MAX_MESSAGE 100		        // Maximum message length
#define MAX_PROCESS_NAME 256        // Maximum process name length
#define MAX_TIMESTAMP_MESSAGE 11    // Maximum timestamp message length
#define TRUE 1
#define FALSE 0

#define sleepTime() (rand() % (MAX_SLEEP_MSEC - MIN_SLEEP_MSEC + 1) + MIN_SLEEP_MSEC)

HANDLE WriteHandle, ReadHandle2;
DWORD written, read;

HANDLE vesselsMutex[MAX_VESSELS_THREADS];
HANDLE mutex;

DWORD WINAPI Vessel(LPVOID Param);
int initGlobalData(int numOfVessels);
void printMessage(char* message);

int main(int argc, char* argv[])
{
    // Check if the number of arguments is legal
    if (argc != 2)
    {
        fprintf(stderr, "Haifa Port: Error - mismatch number of arguments!\n");
        return 1;
    }
    int numOfVessels = atoi(argv[1]);

    // Check if the number of vessels is legal
    if (numOfVessels < MIN_VESSELS_THREADS || numOfVessels > MAX_VESSELS_THREADS)
    {
        fprintf(stderr, "Haifa Port: Error - number of vessels needs to be between 2 and 50!\n");
        return 1;
    }

    char message[MAX_MESSAGE];
    TCHAR ProcessName[MAX_PROCESS_NAME];
    HANDLE ReadHandle, WriteHandle2;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    DWORD ThreadID;
    HANDLE VesselsThreadHandles[MAX_VESSELS_THREADS];
    int buffer, i;
    srand(time(NULL));

    sprintf(message, "Haifa Port: %d vessels are docked at Haifa Port", numOfVessels);
    printMessage(message);

    // Initialize global variables
    if (!initGlobalData(numOfVessels))
    {
        fprintf(stderr, "Haifa Port: Error - semaphores creation failed!\n");
        return 1;
    }

#pragma region Pipes Creation
    /* set up security attributes so that pipe handles are inherited */
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL,TRUE };

    /* allocate memory */
    ZeroMemory(&pi, sizeof(pi));

    /* create the pipe */
    if (!CreatePipe(&ReadHandle, &WriteHandle, &sa, 0)) {
        fprintf(stderr, "Create Pipe 1 Failed\n");
        return 1;
    }

    /* create the pipe */
    if (!CreatePipe(&ReadHandle2, &WriteHandle2, &sa, 0)) {
        fprintf(stderr, "Create Pipe 2 Failed\n");
        return 1;
    }

    /* establish the START_INFO structure for the child process */
    GetStartupInfo(&si);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    /* redirect the standard input to the read end of the pipe */
    si.hStdInput = ReadHandle;
    si.hStdOutput = WriteHandle2;
    si.dwFlags = STARTF_USESTDHANDLES;

    /* we do not want the child to inherit the write end of the pipe */
    SetHandleInformation(WriteHandle, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(ReadHandle2, HANDLE_FLAG_INHERIT, 0);
#pragma endregion

    wcscpy(ProcessName, L".\\Eilat_Port.exe");
    /* create the child process */
    if (!CreateProcess(NULL,
        ProcessName,
        NULL,
        NULL,
        TRUE, /* inherit handles */
        0,
        NULL,
        NULL,
        &si,
        &pi))
    {
        fprintf(stderr, "Haifa Port: Error - process creation failed!\n");
        return -1;
    }

    /* close the unused end of the pipe */
    CloseHandle(ReadHandle);
    CloseHandle(WriteHandle2);

#pragma region Validate Eilat
    printMessage("Haifa Port: Request pass approval from Eilat Port");

    /* the parent now wants to write to the pipe */
    if (!WriteFile(WriteHandle, &numOfVessels, BUFFER_SIZE, &written, NULL))
        fprintf(stderr, "Haifa Port: Error - writing to pipe!\n");

    /* the parent now wants to read from the pipe */
    if (!ReadFile(ReadHandle2, &buffer, BUFFER_SIZE, &read, NULL)) {
        fprintf(stderr, "Haifa Port: Error - reading to pipe!\n");
        return 1;
    }

    // Check Eilat Port pass approval answer
    if (buffer)
    {
        printMessage("Haifa Port: Request denied");
        return 1;
    }
    printMessage("Haifa Port: Request approved");
#pragma endregion

    // Initialize Vessels in Array (according to the number received in arg[1])
    for (i = 0; i < numOfVessels; i++)
    {
        VesselsThreadHandles[i] = CreateThread(NULL, 0, Vessel, (int*)(i + 1), 0, &ThreadID);
        if (!VesselsThreadHandles[i])
        {
            fprintf(stderr, "Haifa Port: Error - in vessel id %d creation!\n", i + 1);
            return 1;
        }
    }

    // Listen to incoming vessels back
    for (i = 0; i < numOfVessels; i++)
    {
        if (ReadFile(ReadHandle2, &buffer, BUFFER_SIZE, &read, NULL))
        {
            if (!ReleaseSemaphore(vesselsMutex[buffer - 1], 1, NULL))
            {
                fprintf(stderr, "Haifa Port: Error - in semaphore release!\n");
                return 1;
            }
        }
    }

    // Close the read end of the pipe
    CloseHandle(ReadHandle2);

    // Join on Vessels Threads
    WaitForMultipleObjects(numOfVessels, VesselsThreadHandles, TRUE, INFINITE);

    printMessage("Haifa Port: All vessel threads are done");

    // Close the write end of the pipe
    CloseHandle(WriteHandle);

    /* wait for the child to exit */
    WaitForSingleObject(pi.hProcess, INFINITE);

    /* close all handles */
    for (int i = 0; i < numOfVessels; i++) {
        CloseHandle(VesselsThreadHandles[i]);
        CloseHandle(vesselsMutex[i]);
    }
    CloseHandle(mutex);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    printMessage("Haifa Port: Exiting...");

    return 0;
}

DWORD WINAPI Vessel(LPVOID Param)
{
    char message[MAX_MESSAGE];
    int thrId = (int)Param;

    // Start sailing
    sprintf(message, "Vessel %d - starts sailing @ Haifa Port", thrId);
    printMessage(message);
    Sleep(sleepTime());

    // Entering Med. Sea ==> Red Sea Canal
    WaitForSingleObject(mutex, INFINITE);
    sprintf(message, "Vessel %d - entering Canal: Med. Sea ==> Red Sea", thrId);
    printMessage(message);
    Sleep(sleepTime());

    if (!WriteFile(WriteHandle, &thrId, BUFFER_SIZE, &written, NULL))
        fprintf(stderr, "Haifa Port: Error - writing to pipe\n");
    sprintf(message, "Vessel %d - arrived @ Eilat Port", thrId);
    printMessage(message);
    if (!ReleaseMutex(mutex))
    {
        fprintf(stderr, "Haifa Port: Error - in mutex release!\n");
        return 1;
    }

    // Wait for vessel to return Red Sea ==> Med. Sea Canal
    WaitForSingleObject(vesselsMutex[thrId - 1], INFINITE);

    // Wait for vessel to return to Haifa Port
    Sleep(sleepTime());

    sprintf(message, "Vessel %d - done sailing @ Haifa Port", thrId);
    printMessage(message);
    Sleep(sleepTime());

    return 0;
}

int initGlobalData(int numOfVessels)
{
    int i;
    // Initialize vessels semaphore array
    for (i = 0; i < numOfVessels; i++)
    {
        vesselsMutex[i] = CreateSemaphore(NULL, 0, 1, NULL);
        if (!vesselsMutex[i])
        {
            fprintf(stderr, "Haifa Port: Error - in semaphore creation!\n");
            return FALSE;
        }
    }

    // Initialize Canal mutex
    mutex = CreateMutex(NULL, FALSE, NULL);
    if (!mutex)
    {
        fprintf(stderr, "Haifa Port: Error - in semaphore creation!\n");
        return FALSE;
    }
    return TRUE;
}

void printMessage(char* message) {
    time_t current_time;
    struct tm* time_info;
    char timeString[MAX_TIMESTAMP_MESSAGE];

    time(&current_time);
    time_info = localtime(&current_time);

    strftime(timeString, sizeof(timeString), "[%H:%M:%S]", time_info);
    printf("%s %s\n", timeString, message);
}