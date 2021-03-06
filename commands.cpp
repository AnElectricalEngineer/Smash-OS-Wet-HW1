//		commands.cpp
//******************************************************************************
#include "commands.h"

#define MAX_HISTORY_SIZE 50

using namespace std;

// Counter to keep track of highest job number
unsigned int totalJobCount = 0;

bool isFgProcess = false;
pid_t lastFgPid;
string lastFgJobName;

static string sigList[] = {
        "INVALID", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
        "SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE", "SIGKILL",
        "SIGUSR1", "SIGSEGV", "SIGUSR2", "SIGPIPE", "SIGALRM",
        "SIGTERM", "SIGSTKFLT", "SIGCHLD", "SIGCONT", "SIGSTOP",
        "SIGTSTP", "SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU",
        "SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH", "SIGIO",
        "SIGPWR", "SIGSYS"
};

//******************************************************************************
// function name: ExeCmd
// Description: interprets and executes built-in commands
// Parameters: pointer to jobs, command string
// Returns: 0 - success,1 - failure
//******************************************************************************
int ExeCmd(map<unsigned int, pJob>* jobs, char* lineSize, char* cmdString)
{
    char* cmd;
    char* args[MAX_ARG];
    char pwd[MAX_LINE_SIZE];
    char delimiters[] = " \t\n";
    int i = 0, num_arg = 0;
    bool illegal_cmd = false; // illegal command
    cmd = strtok(lineSize, delimiters);
    if (cmd == NULL)
        return 0;
    args[0] = cmd;
    for (i=1; i<MAX_ARG; i++)
    {
        args[i] = strtok(NULL, delimiters);
        if (args[i] != NULL)
            num_arg++;
    }

    static queue<string> historyQueue;   // Holds history of commands

    // Enqueue command if command is not history. Holds all prev commands
    // (except 'history', including gibberish.
    if(strcmp(cmd, "history") != 0)
    {
        enqueueNewCmd(&historyQueue, cmdString);
    }

/******************************************************************************/
// Built in Commands PLEASE NOTE NOT ALL REQUIRED ARE IN THIS CHAIN OF IF
// COMMANDS. PLEASE ADD MORE IF STATEMENTS AS REQUIRED
/******************************************************************************/
    if (!strcmp(cmd, "cd") )
    {
        //  Check that correct number of arguments were received (1)
        if(num_arg != 1)
        {
            cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
            return 1;
        }

        //  Here, number of arguments is correct
        static char prevPath[MAX_LINE_SIZE];    // Hold prev working directory
        static bool prevPathValid = false;
        char *path = args[1];

        // Case 1: path equals "-"
        if (!strcmp(path, "-"))
        {
            if(prevPathValid)
            {
                char nextPath[MAX_LINE_SIZE];
                strcpy(nextPath, prevPath);
                char* getCwdSuccess = getcwd(prevPath, MAX_LINE_SIZE);
                if(!getCwdSuccess)   // If getcwd() failed
                {
                    illegal_cmd = true;
                }
                if(illegal_cmd == false)
                {
                    int changeDirSuccess = chdir(nextPath);
                    if (changeDirSuccess == -1)   // changeDir failed
                    {
                        illegal_cmd = true;
                    }
                    else if (changeDirSuccess == 0) //changeDir succeeded
                    {
                        prevPathValid = true;
                    }
                }
            }
            // If no previous path exists, do nothing
        }
        // Case 2: path is not "-"
        else
        {
            char *getCwdSuccess = getcwd(prevPath, MAX_LINE_SIZE);
            if (!getCwdSuccess)   // If getcwd() failed
            {
                illegal_cmd = true;
            }
            if(illegal_cmd == false)
            {
                int changeDirSuccess = chdir(path);
                if (changeDirSuccess == -1)   // changeDir failed
                {
                    illegal_cmd = true;
                }
                else if (changeDirSuccess == 0) //changeDir succeeded
                {
                    prevPathValid = true;
                }
            }
        }
    }

    /*************************************************/

    else if (!strcmp(cmd, "pwd"))
    {
        //  Check that number of arguments is correct
        if(num_arg != 0)
        {
            cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
            return 1;
        }
        // getcwd() returns NULL if buffer size is too small
        if(!getcwd(pwd, MAX_LINE_SIZE))
        {
            illegal_cmd = true;
        }
        else
        {
            printf("%s\n", pwd);
        }
    }

    /*************************************************/

    else if (!strcmp(cmd, "jobs"))
    {
        //  Check that number of parameters is correct (0)
        if(num_arg != 0)
        {
            cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
            return 1;
        }

        // Update jobs
        updateJobs(jobs);

        // Print jobs
        auto it = jobs->begin();
        while(it != jobs->end() && !jobs->empty())
        {
            time_t currentTime = time(NULL);

            cout << "[" << it->first << "] " << it->second->jobName << " : "
                 << it->second->jobPid << " " << currentTime -
                 it->second->jobStartTime << " secs " <<
                 it->second->jobStatus << endl;

            ++it;
        }
    }

    /*************************************************/

    else if (!strcmp(cmd, "kill"))
    {
        //Check if number of parameters is correct (2)
        if(num_arg != 2)
        {
            cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
            return 1;
        }

        //Check if first argument (signum) begins with "-"
        if(args[1][0] != '-')
        {
            cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
            return 1;
        }

        // Convert string arguments to numbers
        int sig;    // signal number
        int jobNum; // jobID
        try
        {
            sig = std::stoi(args[1] + 1);
            jobNum = std::stoi(args[2]);
        }
        catch(std::invalid_argument&)
        {
            cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
            return 1;
        }

        updateJobs(jobs);
        auto jobIt = jobs->find(jobNum);

        // If job does not exist in jobs
        if(jobIt == jobs->end())
        {
            cerr << "smash error: > kill " << jobNum << " - job does not "
                                                        "exist" << endl;
            return 1;
        }
        else
        {
            pid_t jobPid = jobIt->second->jobPid;
            int didKillSucceed = kill(jobPid, sig);

            // If sending signal sending failed
            if(didKillSucceed == -1)
            {
                cerr << "smash error: > kill " << jobNum << " - cannot send "
                                                            "signal" << endl;
                return 1;
            }

            // If sending signal sending succeeded
            else
            {
                cout << "smash > signal " << sigList[sig] << " was sent to "
                                                              "pid " <<
                jobPid
                << endl;
                updateJobs(jobs);
            }
        }
    }

    /*************************************************/

    else if (!strcmp(cmd, "showpid"))
    {
        //  Check that number of parameters is correct (0)
        if(num_arg != 0)
        {
            cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
            return 1;
        }

        // getpid is always successful, no need to check
        pid_t myPID = getpid();
        cout << "smash pid is " << myPID << endl;
    }

    /*************************************************/

    else if (!strcmp(cmd, "fg"))
    {
        //  Check that number of parameters is correct (0 or 1), and that jobs
        //  contains at least one job
        if(num_arg > 1 || jobs->empty())
        {
            cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
            return 1;
        }

        // update jobs list
        updateJobs(jobs);

        if (num_arg == 0)
        {
            pid_t jobPid = (prev(jobs->end()))->second->jobPid;
            string jobName = (prev(jobs->end()))->second->jobName;
            cout << jobName << endl;

            // Start stopped job
            int didKillFail = kill(jobPid, SIGCONT);
            if(didKillFail == -1)
            {
                illegal_cmd = true;
            }
            else
            {
                isFgProcess = true;
                lastFgPid = jobPid;
                lastFgJobName = jobName;
                printf("signal SIGCONT was sent to pid %d\n", jobPid);
                waitpid(jobPid, NULL, 0);
                isFgProcess = false;
            }
        }

        else if (num_arg == 1)
        {
            int job_id;
            try
            {
                job_id = std::stoi(args[1]);
            }
            catch(std::invalid_argument&)
            {
                cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
                return 1;
            }

            if ((jobs->find(job_id) != jobs->end()))
            {
                pJob Job = jobs->find(job_id)->second;
                pid_t jobPid = Job->jobPid;
                string jobName = Job->jobName;
                cout << jobName << endl;

                int didKillFail = kill(jobPid, SIGCONT);
                if(didKillFail == -1)
                {
                    illegal_cmd = true;
                }
                else
                {
                    isFgProcess = true;
                    lastFgPid = jobPid;
                    lastFgJobName = jobName;
                    printf("signal SIGCONT was sent to pid %d\n", jobPid);
                    waitpid(jobPid, NULL, 0);
                    isFgProcess = false;
                }
            }

            // job_id is a number, but no such job exists in jobs
            else
            {
                cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
                return 1;
            }
        }
    }

    /*************************************************/

    else if (!strcmp(cmd, "bg"))
    {
        //  Check that number of parameters is correct (0 or 1), and that jobs
        //  contains at least one job
        if(num_arg > 1 || jobs->empty())
        {
            cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
            return 1;
        }

        // update jobs list
        updateJobs(jobs);

        // If no supplied arguments, run last stopped job
        if (num_arg == 0)
        {
            //  Iterate over jobs in REVERSE order
            auto it = jobs->rbegin();
            bool foundStoppedJob = false;
            for(; it != jobs->rend(); ++it)
            {
                if(it->second->jobStatus == "(Stopped)")
                {
                    foundStoppedJob = true;
                    break;
                }
            }

            // If a stopped job exists in jobs
            if(foundStoppedJob == true)
            {
                pid_t jobPid = it->second->jobPid;
                string jobName = it->second->jobName;
                cout << jobName << endl;

                // Start stopped job
                int didKillFail = kill(jobPid, SIGCONT);
                if(didKillFail == -1)
                {
                    illegal_cmd = true;
                }
                else
                {
                    printf("signal SIGCONT was sent to pid %d\n", jobPid);
                }
            }

            // If no stopped jobs exist in jobs, error
            else
            {
                cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
                return 1;
            }
        }

        // If one argument was supplied, check if that job is stopped, and if
        // so, run it
        else if (num_arg == 1)
        {
            int job_id;
            try
            {
                job_id = std::stoi(args[1]);
            }
            catch(std::invalid_argument&)
            {
                cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
                return 1;
            }

            // Check if the requested job exists in jobs
            if ((jobs->find(job_id) != jobs->end()))
            {
                pJob Job = jobs->find(job_id)->second;

                // Check if the the job is stopped
                if(Job->jobStatus == "(Stopped)")
                {
                    pid_t jobPid = Job->jobPid;
                    string jobName = Job->jobName;
                    cout << jobName << endl;

                    int didKillFail = kill(jobPid, SIGCONT);
                    if(didKillFail == -1)
                    {
                        illegal_cmd = true;
                    }
                    else
                    {
                        printf("signal SIGCONT was sent to pid %d\n", jobPid);
                    }
                }

                // If the chosen job is not stopped.
                else
                {
                    cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
                    return 1;
                }
            }

            // If the requested job does not exist in jobs, error
            else
            {
                cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
                return 1;
            }
        }
    }

    /*************************************************/

    else if (!strcmp(cmd, "quit"))
    {
        //  Check that number of parameters is correct (0 or 1)
        if(num_arg > 1)
        {
            cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
            return 1;
        }

        else if (num_arg == 0)
        {
            exit(0);
        }
        else if (num_arg == 1)
        {
            if(!strcmp(args[1], "kill"))    // If first argument is "kill"
            {
                updateJobs(jobs);

                // Iterate over all jobs in jobs
                auto it = jobs->begin();
                pid_t currentPid;
                while(it != jobs->end() && !jobs->empty())
                {
                    currentPid = it->second->jobPid;
                    int didKillFail = kill(currentPid, SIGTERM);
                    if(didKillFail == -1)
                    {
                        fprintf(stderr, "smash error: > %s\n", strerror(errno));
                    }
                    cout << "[" << it->first << "] " << it->second->jobName
                    << " - Sending SIGTERM... ";

                    sleep(5);
                    int waitRetVal = waitpid(currentPid, NULL, WNOHANG);
                    if(waitRetVal == -1)
                    {
                        fprintf(stderr, "smash error: > %s\n", strerror(errno));
                    }
                    // If process was not terminated
                    else if(waitRetVal == 0)
                    {
                        int didKillFail2 = kill(currentPid, SIGKILL);
                        if(didKillFail2 == -1)
                        {
                            fprintf(stderr, "smash error: > %s\n", strerror(errno));
                        }
                        cout << "(5 sec passed) Sending SIGKILL... Done." <<
                        endl;
                    }
                    else
                    {
                        cout << "Done." << endl;
                    }
                    ++it;
                }
                // Exit from smash
                exit(0);
            }
            else    // If 1 arg, but is not "kill"
            {
                cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
                return 1;
            }
        }
    }

    /*************************************************/

    else if (!strcmp(cmd, "history"))
    {
        //  Check that number of parameters is correct (0)
        if(num_arg != 0)
        {
            cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
            return 1;
        }

        queue<string> tempQueue;

        // Print history of commands
        while(!historyQueue.empty())
        {
            auto currentCommand = historyQueue.front();
            cout << currentCommand << endl;
            historyQueue.pop();
            tempQueue.push(currentCommand);
        }

        while(!tempQueue.empty())
        {
            historyQueue.push(tempQueue.front());
            tempQueue.pop();
        }
    }

    /*************************************************/

    else if (!strcmp(cmd, "cp"))
    {
        //  Check that number of parameters is correct (2)
        if(num_arg != 2)
        {
            cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
            return 1;
        }

        // Paths for two files to copy
        const char* pathName1 = args[1];
        const char* pathName2 = args[2];

        int fd1 = open(pathName1, O_RDONLY);

        // Exit if opening file 1 fails
        if(fd1 == -1)
        {
            illegal_cmd = true;
        }
        else
        {
            int fd2 = open(pathName2, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);

            // Check if opening file 2 fails
            if(fd2 == -1)
            {
                illegal_cmd = true;
                close(fd1);
            }
            if(illegal_cmd == false)
            {
                // Files 1 and 2 are open
                char fileBuff1[BUFF_SIZE];
                ssize_t bytesRead1;
                ssize_t didWriteSucceed;
                do
                {
                    bytesRead1 = read(fd1, fileBuff1, BUFF_SIZE);
                    didWriteSucceed = write(fd2, fileBuff1, bytesRead1);
                    if (didWriteSucceed == -1)
                    {
                        illegal_cmd = true;
                        break;
                    }
                }while(bytesRead1 > 0);
                if(illegal_cmd == false)
                {
                    if (didWriteSucceed >= 0)
                    {
                        cout << pathName1 << " has been copied to "
                        << pathName2 << endl;
                    }
                    //  Close the files
                    int closeFd1Fail = close(fd1);
                    int closeFd2Fail = close(fd2);
                    if(closeFd1Fail || closeFd2Fail)
                    {
                        if(closeFd1Fail)
                        {
                            illegal_cmd = true;
                        }
                        if(closeFd2Fail)
                        {
                            illegal_cmd = true;
                        }
                    }
                }
            }
        }
    }

    /*************************************************/

    else if (!strcmp(cmd, "diff"))
    {
        //  Check that number of parameters is correct (2)
        if(num_arg != 2)
        {
            cerr << "smash error: > " << "\"" << cmdString << "\"" << endl;
            return 1;
        }

        // Paths for two files to diff
        const char* pathName1 = args[1];
        const char* pathName2 = args[2];

        int fd1 = open(pathName1, O_RDONLY);

        // Exit if opening file 1 fails
        if(fd1 == -1)
        {
            illegal_cmd = true;
        }
        else
        {
            int fd2 = open(pathName2, O_RDONLY);

            // Check if opening file 2 fails
            if(fd2 == -1)
            {
                illegal_cmd = true;
                close(fd1);
            }

            // Files 1 and 2 are open
            if(illegal_cmd == false)
            {
                char fileBuff1[BUFF_SIZE], fileBuff2[BUFF_SIZE];
                ssize_t bytesRead1, bytesRead2;
                bool areFilesSame = true;
                do
                {
                    bytesRead1 = read(fd1, fileBuff1, BUFF_SIZE - 1);
                    fileBuff1[bytesRead1] = '\0';
                    bytesRead2 = read(fd2, fileBuff2, BUFF_SIZE - 1);
                    fileBuff2[bytesRead2] = '\0';

                    // files 1 and 2 contain some different characters
                    if(strcmp(fileBuff1, fileBuff2) != 0)
                    {
                        areFilesSame = false;
                        break;
                    }
                }while(bytesRead1 > 0 && bytesRead2 > 0);

                if(areFilesSame == true)
                {
                    cout << "0" << endl;
                }
                else
                {
                    cout << "1" << endl;
                }

                //  Close the files
                int closeFd1Fail = close(fd1);
                int closeFd2Fail = close(fd2);
                if(closeFd1Fail || closeFd2Fail)
                {
                    if(closeFd1Fail)
                    {
                        illegal_cmd = true;
                    }
                    if(closeFd2Fail)
                    {
                        illegal_cmd = true;
                    }
                }
            }
        }
    }

    /*************************************************/

    else // external command
    {
        ExeExternal(args, cmdString, jobs);
        return 0;
    }

    if (illegal_cmd == true)
    {
        fprintf(stderr, "smash error: > %s\n", strerror(errno));
        return 1;
    }
    return 0;
}
//******************************************************************************
// function name: ExeExternal
// Description: executes external command
// Parameters: external command arguments, external command string
// Returns: void
//******************************************************************************
void ExeExternal(char *args[MAX_ARG], char* cmdString, map<unsigned int, pJob>*
jobs)
{
    int pID;
    switch(pID = fork())
    {
        case -1:
            // Add your code here (error)
            fprintf(stderr, "smash error: > %s\n", strerror(errno));

        case 0 :
            // Child Process
            setpgrp();

            // Add your code here (execute an external command)
            execv(args[0], args);

            //If execv returns - error
            fprintf(stderr, "smash error: > %s\n", strerror(errno));
            exit(1); // Lior said correct way to exit process - just dont do
            // exit in main

        default:
            // If command should be run in background
            if (cmdString[strlen(cmdString)-1] == '&')
            {
                cmdString[strlen(cmdString)-1] = '\0';

                // Create new jobs entry
                pJob pNewJob = new Job;
                pNewJob->jobName = args[0];
                pNewJob->jobPid = pID;
                pNewJob->jobStartTime = time(NULL);
                pNewJob->jobStatus = "";

                totalJobCount++;
                unsigned int jobNumber = totalJobCount;
                jobs->insert({jobNumber, pNewJob});
            }
            // If command should be run in foreground
            else
            {
                isFgProcess = true;
                lastFgPid = pID;
                lastFgJobName = args[0];
                waitpid(pID, NULL, WUNTRACED);
                isFgProcess = false;
            }
    }
}

//******************************************************************************
// function name: enqueueNewCmd
// Description: adds a new command to the history of commands
// Parameters: pointer to history queue, command string
// Returns: void
//******************************************************************************
void enqueueNewCmd(queue<string>* historyPtr, char* cmdString)
{
    unsigned long currentSize = historyPtr->size();
    if(currentSize == MAX_HISTORY_SIZE)
    {
        historyPtr->pop();
    }
    historyPtr->push(cmdString);
}
//******************************************************************************
// function name: updateJobs
// Description: iterates over jobs and updates process statuses
// Parameters: pointer to jobs
// Returns: void
//******************************************************************************
void updateJobs(map<unsigned int, pJob>* jobs)
{
    // Check for processes that have finished and remove them
    auto it = jobs->begin();
    while(it != jobs->end() && !jobs->empty())
    {
        pid_t currentJobPid = it->second->jobPid;
        int processStatus;
        pid_t waitPid = waitpid(currentJobPid, &processStatus,
                                WNOHANG | WUNTRACED | WCONTINUED);

        //Sys call error - if process was stopped and then killed with signal
        // other than SIGKILL, clean its entry from jobs
        if(waitPid == -1)
        {
            delete it->second;
            it = jobs->erase(it);
        }

        // if job HAS changed status
        else if(waitPid == currentJobPid)
        {
            // If process was terminated normally or by signal
            if(WIFEXITED(processStatus) || WIFSIGNALED(processStatus))
            {
                delete it->second;
                it = jobs->erase(it);   // Automatically increments iterator
            }

            // If process was stopped
            else if(WIFSTOPPED(processStatus))
            {
                it->second->jobStatus = "(Stopped)";
                ++it;
            }

            // If processes was continued by means of SIGCONT
            else if(WIFCONTINUED(processStatus))
            {
                it->second->jobStatus = "";
                ++it;
            }
        }

        // if job did NOT change status, do not modify its entry in jobs
        else
        {
            ++it;
        }
    }
}
