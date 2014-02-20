/**********************************************
 * Please DO NOT MODIFY the format of this file
 **********************************************/

/*************************
 * Team Info & Time spent
 *************************/
/*
	Edit this to list all group members and time spent.
	Please follow the format or our scripts will break. 
*/

	Name1: Yi Hong Poo
	NetId1: yp27
	Time spent: 20 hours

	Name2: Gang Li
	NetId2: gl56
	Time spent: 10 hours

	Name3: Jerry Li	
	NetId3:   jl344
	Time spent: 15 hours

	Name3: David Le	
	NetId3:   dl103
	Time spent: 20 hours

/******************
 * Files to submit
 ******************/

	dsh.c 	// Header file is not necessary; other *.c files if necessary
	README	// This file filled with the lab implementation details

/************************
 * Implementation details
 *************************/

/* 
 * This section should contain the implementation details and a overview of the
 * results. 

 * You are expected to provide a good README document including the
 * implementation details. In particular, you can use pseudocode to describe your
 * implementation details where necessary. However that does not mean to
 * copy/paste your C code. Specifically, you should summarize details corresponding 
 * to (1) multiple pipelines (2) job control (3) logging (3) .c commands 
 * compilation and execution. We expect the design and implementation details to 
 * be at most 1-2 pages.  A plain textfile is requested. 

 * In case of lab is limited in some functionality, you should provide the
 * details to maximize your partial credit.  
 * */

Multiple pipelines 

	Pipeline takes the following shape for multiple processes.

	A ----- Old Pipe ---- B ------ New Pipe ----- C
		                    B ----- Old Pipe  -----  C ------ New Pipe ----- D

	As we iterate through the list of processes, the parent process will fork a new child. 
	There will be 3 cases in making a fork.
	A) If it fails, we exit.
B) If it is the child process, if it is not the first process, we close write end of pipe, dup2 read end onto stdin and close read end of pipe. If it is not the last process, we close read end of pipe, dup2 write end onto stdout and close write end of pipe. 
In the child process, we also do IO redirection and check for autocompilation of C files. We execute the process at the end.
C) If it is the parent process, if it not the first  child, we just close the pipe. Else if it is the last child, we let the old pipe become the new pipe.

After iterating through all the processes, we call the function for the parent job to handle the job. The parent will wait until child jobs are done.


job control
Job control begins as soon as the parser breaks down the line of input and converts it into a list of jobs. Before we delve into any job work, we must first check if the processed job will require a child process or is just a built-in command. If it’s a built-in command, we will execute the command and not keep track of it. Otherwise, since we have a global variable that will keep track of the shell’s first job, if that pointer is null, we will set it to the first job of the newly received list. Otherwise, we’ll tack the new list of jobs onto the existing job list. This will be used for the “jobs” command, which will be explained further below.

Then, because the parser checks if an “&” is detected, we can use that to spawn a job either in the background or the foreground. If the job is to be spawned in the foreground, we give it control of the terminal and use waitpid to wait for its completion -- we used WUNTRACTED for this so that it returns when the child process is done or interrupted. Once the shell receives control of the terminal again, we will check whether or not the job completed or was interrupted by a signal using WIFSTOPPED and the corresponding status. The job_t will be marked accordingly.

Running a job in the background in our shell is almost the same thing, with the biggest difference being that once the child process is created, the parent will not use waitpid for the child -- rather, it will just continue on as if nothing happened.

We can use fg and bg to continue jobs again. Running this command with a pid will cause the program to search through the list of jobs and find the matching pgid of the pid entered. Once found, the shell sends a signal to continue to that process and will use waitpid depending on whether or not it was in background or foreground.

Once the jobs command is issued, the program will run through the linked list and print out information about the pid, current job status, and the command that was issued for that job. If the job was successfully completed, the program will mark it as so and free it from memory using the delete_job command from the given helper file. Of course, it will readjust pointers as necessary. If the job was not completed, it will be marked as “In Progress” and will not be deleted and will continue to show up if jobs is issued another time.


logging - Gang
	
Logging and error checking is implemented through a helper function called unix_error. We first open a designated error log using open and pass it permission of O_APPEND to make sure that we keep a consistent log for all the devil shell command entries. This open operation is done within the child process of a fork because only non-builtin commands require error logging; built-in commands on the other hand, directly prints their errors onto the shell using “perror”. After getting the file descriptor for the error log, aka “dsh.log”, we dup2 STDERR to that file.

For the actual unix_error command, it takes in two arguments: one the error msg and also the out_fd. out_fd is the file descriptor for the function to write to in addition to the log file (eg. stdout) and if out_fd is less than 0, we only write to log file.
We detect error for fork, input/output, builtin commands, pipeline, exec etc. All with distinct msg passed to the unix_error function.

The last part of logging is detecting whenever a child process changes status, this is done within the “jobs” command when jobs gets deleted upon completion. We add another log entry right after to denote how the processes terminated and its corresponding pid.

	
	
.c commands and file redirection - Jerry

Auto compilation of c commands requires several steps. First we need to detect if the first argument of the command string is a c file. We check p->argv[0] to see if it contains the sequence “.c”. If so we send the argument to a method called compileAndRun. This method builds the compile command “gcc “filename” -o Devil”. We then fork a new process in this method. The child executes the compile command. The parent frees the argument. Finally, after compileAndRun executes, we build the string “./Devil.o” and execute that. 

Flow:
spawn_auto
	if (file is c) {
		compileAndRun  ------>
 build compile command and fork and execute
	?----
build “./Devil.o” string
		execute that string
	}
	}

File redirection was slightly different. We first detected if p->ofile or p->ifile was not null. If it was not null and was occupied, we opened a file using the open(char*name, permission, mode) command in the case of input redirection. We used
 creat (char*name, mode) for output redirection. After we got the file descriptors we used dup2(outputDescr, STDIN_FILENO) and dup2(inputDesc, STDIN_FILEOUT) for output and input respectively. 

Our main issues dealt with file permissions and which ones to use when calling open and creat. We also had to figure out where and when to call them. For building strings, we had to extra precise. 	

	
	


/************************
 * Feedback on the lab
 ************************/

/*
 * Any comments/questions/suggestions/experiences that you would help us to
 * improve the lab.
 * */
Jerry:
	I thought the lab was reasonable in difficulty and amount of work. Perhaps maybe teaching us more about file permissions and what each one does. 

Yi Hong:
	I thought the lab was reasonable in terms of difficulty. I had a lot of questions but I started early and with enough time asking TA, checking out sources on the internet, it was doable. Although I feel that what was taught might not be enough for us to complete this assignment.




/************************
 * References
 ************************/

Jerry: 
	http://codewiki.wikidot.com/c:system-calls:open
	http://publib.boulder.ibm.com/infocenter/zvm/v6r1/index.jsp?topic=/com.ibm.zvm.v610.edclv/rtcre.htm


/*
 * List of collaborators involved including any online references/citations.
 * */

