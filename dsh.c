#include "dsh.h"
#include <time.h>

#define DEFAULT_LOG_NAME "dsh.log"

void seize_tty(pid_t callingprocess_pgid); /* Grab control of the terminal for the calling process pgid.  */
void continue_job(job_t *j); /* resume a stopped job */
void spawn_job(job_t *j, bool fg); /* spawn a new job */
void handle_job(job_t *j);

job_t *first_job;

void unix_error(char *msg, int out_fd) {
    if (out_fd >= 0) {
        write(out_fd, msg, strlen(msg));
        write(out_fd, "\n", strlen("\n"));
    }
    time_t seconds = time(NULL);
    char* err_time = ctime(&seconds);
    char* time_msg = "At ";
    char* err_msg = malloc(sizeof(time_msg) + sizeof(err_time) + sizeof(msg) + 300);
    sprintf(err_msg, "%s%s-> %s\n\n", time_msg, err_time, msg);
    int fileErr = open(DEFAULT_LOG_NAME, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
    write(fileErr, err_msg, strlen(err_msg));
    free(err_msg);
}
/* Sets the process group id for a given job and process */
int set_child_pgid(job_t *j, process_t *p)
{
    if (j->pgid < 0) /* first child: use its pid for job pgid */
        j->pgid = p->pid;
    return(setpgid(p->pid,j->pgid));
}


/* Creates the context for a new child by setting the pid, pgid and tcsetpgrp */
void new_child(job_t *j, process_t *p, bool fg)
{
         /* establish a new process group, and put the child in
          * foreground if requested
          */

         /* Put the process into the process group and give the process
          * group the terminal, if appropriate.  This has to be done both by
          * the dsh and in the individual child processes because of
          * potential race conditions.
          * */

         p->pid = getpid();

         /* also establish child process group in child to avoid race (if parent has not done it yet). */
         set_child_pgid(j, p);

         if(fg) // if fg is set
		seize_tty(j->pgid); // assign the terminal

         /* Set the handling for job control signals back to the default. */
         signal(SIGTTOU, SIG_DFL);
}

/*this function forks a child to compile the .c file to a Devil.o */
int compileAndRun(char* file) {
	char* command[5];			//this block of code assembles the compile command
	command[0] = "gcc\0";
	command[1] = file;
	command[2] = "-o\0";
	command[3] = "Devil.o\0";
	command[4] = NULL;

	int pid = fork();			//fork new process

	if (pid == -1){				//if bad pid exit
		perror("Error: fork failed:");
		unix_error("Error: fork failed:", -1);
		exit(EXIT_FAILURE);
	}
	if (pid == 0) {				//if child
		int devil = creat("Devil.o", S_IRWXU | S_IRWXO);	//create the Devil.exe file and set appropriate permissions
		if (devil > 0) {
			dup2(devil, STDOUT_FILENO);			//duplicate
		}
		close(devil);							//close file
		execvp(command[0], command);			//execute compile command
		dup2(errno,1);
		char* error_msg = "Error: The new child should have done an exec (g++) but failed to do so.";
        perror(error_msg);
        unix_error(error_msg, -1);
		exit(EXIT_FAILURE);//not reached
	}
	else {			//if parent, do nothing besides reap child
		int stt;
		waitpid(pid, &stt, 0);
		if (stt == 0) {
			return 1;
		}
	}
	return 0;
}


/* Spawning a process with job control. fg is true if the
 * newly-created process is to be placed in the foreground.
 * (This implicitly puts the calling process in the background,
 * so watch out for tty I/O after doing this.) pgid is -1 to
 * create a new job, in which case the returned pid is also the
 * pgid of the new job.  Else pgid specifies an existing job's
 * pgid: this feature is used to start the second or
 * subsequent processes in a pipeline.
 * */

void spawn_job(job_t *j, bool fg)
{

	pid_t pid;
	process_t *p;

	int oldPipe[2];
	for(p = j->first_process; p; p = p->next) {
	  /* YOUR CODE HERE? */
	  /* Builtin commands are already taken care earlier */

		int newPipe[2];
		if (p->next!=NULL){
			pipe(newPipe);
			//pipe[0] is the read end
			//pipe[1] is the write end
		}

		switch (pid = fork()) {

            case -1: /* fork failure */
				perror("Error: failed to fork");
				unix_error("Error: failed to fork", -1);
				exit(EXIT_FAILURE);

            case 0: /* child process  */
				p->pid = getpid();

				//log errors
				int fileErr = open (DEFAULT_LOG_NAME, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
				dup2(fileErr, STDERR_FILENO);

				new_child(j, p, fg);
				/* YOUR CODE HERE?  Child-side code for new process. */
				//not last child ie it is C1
				if (p->next!=NULL){
					close(newPipe[0]); 				//close the read end of the pipe

					if (newPipe[1]!=STDOUT_FILENO){
						dup2(newPipe[1],1);			// duplicate write end of pipe on stdout
						close(newPipe[1]); 			// close write end of pipe
					}
				}
				else {
					dup2(1,newPipe[1]);
				}

				//not the first child ie it is C2
				if (p!=j->first_process){
					close(oldPipe[1]);				//close write end of pipe
					if (oldPipe[0]!=STDIN_FILENO){
						dup2(oldPipe[0],0);			//dup2 read end onto stdin
						close(oldPipe[0]); 			//close read end of pipe
					}
				}

				/*get output and input file*/
				char* outputFile = p->ofile;
				char* inputFile = p->ifile;

				/* this piece of code handles input and output redirection */
				if (inputFile != NULL) {							//if p->ifile exists
					int inputDesc = open(inputFile, O_RDONLY, 0);	//open the file
					if (inputDesc < 1) {
						char* error = "Error: bad file descriptor for input";
						perror(error);
						unix_error(error, -1);						//error handling if bad file (eg. does not exist)
						exit(1);
					}
					dup2(inputDesc, STDIN_FILENO);					//duplicate standard input to file
					close(inputDesc);								//close file
				}
				else if (outputFile != NULL) {						//if p->ofile exists
					int outputDesc = creat(outputFile, 0644);		//create the file
					if (outputDesc < 1) {
						char* error = "bad file descriptor for output";
						perror(error);
						unix_error(error, -1);						//error handling if file is bad
					}
					dup2(outputDesc, STDOUT_FILENO);				//copy stdout
					close(outputDesc);								//close file
				}

				/*this code handles auto compilation*/
				char* argument = p->argv[0];
				if (!strcmp(argument,"cat")){//this is to prevent autocompilation from messing up cat
				}
				else{
					char* prevArg = '\0';
					while (*argument != '\0') {//checks for the ".c" argument
						if (*argument == 'c') {
							if (*prevArg == '.') {
								compileAndRun(p->argv[0]);//this function forks a child to compile the c file
								char* command[2];
								command[0] = "./Devil.o\0";		//build execute command string
								command[1] = NULL;
								chmod("Devil.o", 777);
								execvp(command[0], command);	//execute string
							}
						}
						prevArg = argument;
						argument++;
					}
				}

				execvp(p->argv[0], p->argv);
				char* error_msg = "Error: The new child should have done an exec but failed to do so.";
				perror(error_msg);
				unix_error(error_msg, -1);
				exit(EXIT_FAILURE);  /* NOT REACHED */
				break;    /* NOT REACHED */

			default: /* parent */
				/* establish child process group */
				p->pid = pid;
				set_child_pgid(j, p);

				/* YOUR CODE HERE?  Parent-side code for new process.  */
				if (p!=j->first_process){		//not the first child ie it is C2
					close(oldPipe[1]);			//close write end of pipe
					close(oldPipe[0]); 			//close read end of pipe ?remove?????
				}

				if (p->next!=NULL){		//not last child ie it is C1
					oldPipe[0] = newPipe[0];
					oldPipe[1] = newPipe[1];
				}
        }
    }
	/* YOUR CODE HERE?  Parent-side code for new job.*/
		if (fg == true) handle_job(j);
		seize_tty(getpid()); // assign the terminal back to dsh
}

// void error_handling(int errNum, bool write) {
// 	errno = errNum;
// 	perror("Error");
// 	if (write == true)
// 		fprintf(stderr, errno);
// }


/* Sends SIGCONT signal to wake up the blocked job */
void continue_job(job_t *j)
{
     if(kill(-j->pgid, SIGCONT) < 0)
         perror("kill(SIGCONT)");
      	 unix_error("Error: kill(SIGCONT)", -1);
}

/* Handles the job after it is completed or suspended */
void handle_job(job_t *j) {
	int status = 0;
	pid_t pid = j->pgid;
	waitpid(pid, &status, WUNTRACED);
	j->first_process->status = status;
        if (WIFSTOPPED(status)) j->first_process->stopped = true;
        else j->first_process->completed = true;
}

/* Validates the existence of a directory */
bool is_directory(char* directory) {
	struct stat s;
	int err = stat(directory, &s);
	if (err == 0) {
		if (S_ISDIR(s.st_mode)) {
			/* is a directory */
			return true;
		} else {
			/* not a directory */
			unix_error("not a directory", STDERR_FILENO);
			return false;
		}
	} else {
		unix_error("no such file or directory", STDERR_FILENO);
		return false;
	}
}

/* Finds job with the given pid */
job_t* find_job_by_pid (pid_t pid) {
	bool success = false;
	job_t *job = first_job;
	while (job != NULL) {
		if (job->pgid != pid) job = job->next;
		else {
			success = true;
			break;
		}
	}
	if (success == false) return NULL;
	return job;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute
 * it immediately.
 */
bool builtin_cmd(job_t *last_job, int argc, char **argv)
{

	    /* check whether the cmd is a built in command
        */

        if (!strcmp(argv[0], "quit")) {
            /* Your code here */
            exit(EXIT_SUCCESS);
	}
        else if (!strcmp("jobs", argv[0])) {
		job_t *current_job = first_job;
		while (current_job != NULL) {
			if (current_job->notified != true) {
				char job_status[20];
				if (!current_job->first_process->completed) strcpy(job_status, "In Progress");
				if (current_job->first_process->stopped) strcpy(job_status, "Stopped");
				if (current_job->first_process->completed) {
					strcpy(job_status, "Complete");
					current_job->notified = true;
				}
				printf("%d (%s): %s\n", (int) current_job->pgid, job_status, current_job->commandinfo);

			}
			job_t *next_job = current_job->next;
			if (current_job->first_process->completed) {
				bool is_head = false;
				if (current_job == first_job) is_head = true;
				delete_job(current_job, first_job);
				if (is_head) first_job = NULL;
			}
			current_job = next_job;
		}
		return true;
        }
	else if (!strcmp("cd", argv[0])) {
		if (argv[1] != NULL) {
			char path_copy[strlen(argv[1])];
			strcpy(path_copy, argv[1]);
			char *path_dest = strtok(path_copy, "/");
			char buffer[256];
			getcwd(buffer, sizeof(buffer));
			if (strcmp(buffer + strlen(buffer) - 1, "/")) strcat(buffer, "/");

			bool success = false;
			while (path_dest) {
				if (!strcmp(path_dest, "..")) {
					int index = strlen(buffer) - 2;
					while (buffer[index] != '/' && index > 0) {
						index--;
					}
					buffer[index] = '\0';
					if (buffer[strlen(buffer)-1] != '/') strcat(buffer, "/");
				} else {
					if (is_directory(strcat(buffer, path_dest))) {
						if (buffer[strlen(buffer)-1] != '/') strcat(buffer, "/");
					} else break;
				}
				path_dest = strtok(NULL, "/");
				if (path_dest == NULL) success = true;
			}
			if (success == true) chdir(buffer);
		}
		return true;
        }
        else if (!strcmp("bg", argv[0])) {
		job_t *job;
		pid_t pid;
        	if (argv[1] != NULL) {
			pid = (pid_t) atoi(argv[1]);
			job = find_job_by_pid(pid);
		} else {
			job = find_last_job(first_job);
			if (job != NULL) pid = job->pgid;

		}
		if (job != NULL) {
			continue_job(job);
			job->bg = false;
		} else {
			unix_error("Error: bg job not found", STDERR_FILENO);
		}
		return true;
        }
        else if (!strcmp("fg", argv[0])) {
		job_t *job;
		pid_t pid;
        	if (argv[1] != NULL) {
			pid = (pid_t) atoi(argv[1]);
			job = find_job_by_pid(pid);
		} else {
			job = find_last_job(first_job);
			if (job != NULL) pid = job->pgid;

		}
		if (job != NULL) {
			seize_tty(pid);
			continue_job(job);
			handle_job(job);
			seize_tty(getpid());
			if (job != NULL) job->bg = false;
		} else {
			unix_error("Error: fg job not found", 2);
		}
		return true;
        }
        return false;       /* not a builtin command */
}

/* Build prompt messaage */
char* promptmsg()
{
    /* Modify this to include pid */
	char current_directory[256];
	getcwd(current_directory, sizeof(current_directory));
	if (strcmp(current_directory + strlen(current_directory) - 1, "/")) strcat(current_directory, "/");
	char *prompt = malloc (sizeof(char) * 100);
	if (!isatty(0)) {
		return "";
	}
	else {
		sprintf(prompt, "dsh:%s (%d)$ ", current_directory, (int) getpid());
	}
	return prompt;
}

int main()
{
	init_dsh();
	DEBUG("Successfully initialized\n");
	while(1) {
		job_t *j = NULL;
			if(!(j = readcmdline(promptmsg()))) {
				if (feof(stdin)) { /* End of file (ctrl-d) */
					fflush(stdout);
					printf("\n");
					exit(EXIT_SUCCESS);
		   		}
				continue; /* NOOP; user entered return or spaces with return */
			}

		/* Your code goes here */
		/* You need to loop through jobs list since a command line can contain ;*/
		/* Check for built-in commands */
		/* If not built-in */
		    /* If job j runs in foreground */
		    /* spawn_job(j,true) */
		    /* else */
		    /* spawn_job(j,false) */


		while (j != NULL) {
			job_t* next = j->next;
			bool builtin = builtin_cmd(j, 0, j->first_process->argv);
			if (!builtin) {
				if(first_job == NULL) {
					first_job = j;
					first_job->next = NULL;
				}
				else {
					job_t* current_job = find_last_job(first_job);
					current_job->next = j;
					j->next = NULL;
				}
				if (!j->bg) spawn_job(j, true);
				else spawn_job(j, false);
			}
			j = next;
		}
		/* Only for debugging purposes to show parser output; turn off in the
		 * final code */
		//if(PRINT_INFO) print_job(first_job);
	}
}
