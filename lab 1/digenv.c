
/* 
 * NAME:
 *   digenv  - a program for studying your environment variables.
 * 
 * SYNTAX:
 *   digenv [optional GREP arguments]
 *
 * DESCRIPTION:
 *   There are two ways to execute digenv - with or without arguments.
 *	 Without arguments is equal to doing the following
 *
 *		- printenv | sort | pager
 *
 *	 With arguments is equal to doing the following
 *
 *		- printenv | grep [arguments] | sort | pager
 *
 *	 Digenv looks at the enviroment variable PAGER to determine which 
 *	 pager to use. If variable is not set, digenv tries to use less(1).
 *	 If less(1) does not exist, digenv falls back to more(1).
 *
 * OPTIONS:
 *	 See GREP(1). Every parameter to digenv is passed directly to GREP.
 *   
 * EXAMPLES:
 *	 List all enviroment variables:
 *   $ digenv
 *	 List all environment variables containing 'PATH'
 *	 $ digenv PATH
 *
 * ENVIRONMENT:
 *  PAGER 	Used to determine which pager to use.
 *
 * DIAGNOSTICS:
 *	The exit status is 0 if everything worked, 1 if a system call failed, 
 *  or 2 if a child was terminated by a signal. If a child process exited
 *	with a non zero status, that status will be the exit status of digenv.
 *
 * SEE ALSO:
 *   printenv(1), grep(1), sort(1), less(1), more(1)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Pipe file descriptor type */
typedef int pipe_def[2];

/* 
 * Define read and write side of a pipe.
 * Used together with 'pipe_def' 
 */
#define PIPE_IN 0
#define PIPE_OUT 1


/* check_sys_return
 *
 * Called after every system call. Check return value from a system call,
 * if value indicates a failiure, print an error message and exit program.
 */
void check_sys_return(int sys_ret, char * error_string)
{
	if (-1 == sys_ret)
	{
		perror(error_string);
		exit(1);
	}
}

/* main
 *
 * create all necessary pipes and handle all children.
 */
int main( 	
			int argc /* Number of arguments */,
			char ** argv /* List of arguments going to GREP(1) */
		)
{
	int grep = argc > 1 ? 1 : 0; /* Determine if arguments are given, if they are, we should use GREP(1) */
	pid_t pid; /* Stores the process id returned from fork() */
	int sys_return; /* Return value from a system call */
	int i; /* A loop variable */
	pipe_def pipes[3]; /* Three pipes we can use */
	char* pager; /* storage for the pager name, if it exist */
	int return_status; /* Return status for a child */
	int exit_value=0; /* What exit status to use. */
	int cur=0; /* Which pipe we are working on */

	for (i = 0; i < 3; ++i)
	{
		sys_return = pipe(pipes[i]); /* Create a pipe */
		check_sys_return(sys_return, "Error when creating pipe"); /* Check return value of system call */
	}
	
	pid = fork(); /* Create another process */
	check_sys_return(pid, "Error when forking"); /* Check return value of system call */

	if (0 == pid)
	{
		/*
		 * What to do in here?
		 * Child process in charge of call to PRINTENV(1).
		 * Duplicate write side of pipe and overwrite STDOUT with it.
		 * Close 
		 * Call printenv(1)
		 */
		sys_return = dup2(pipes[cur][PIPE_OUT], STDOUT_FILENO);
		check_sys_return(sys_return, "@printenv: Error when duplicating pipe");
		
		sys_return = close(pipes[cur][PIPE_IN]);
		check_sys_return(sys_return, "@printenv: Error when closing read side of pipe");

		sys_return = close(pipes[cur][PIPE_OUT]);
		check_sys_return(sys_return, "@printenv: Error when closing write side of pipe");

		execlp("printenv", "printenv", (char *) 0);
		/* If we get here, something went wrong with PRINTENV(1) */
		perror("Problem with call to printenv");
		exit(1);
	}

	++cur; /* We are done with the first step, cur can have no other value than 1 */

	/* Now we are done with printenv */
	if (grep)
	{
		/* We want to do GREP(1) */
		pid = fork();
		check_sys_return(pid, "Error when forking"); /* Check return value of system call */

		if (0 == pid)
		{
			/* 
			 * What to do here?
			 * Duplicate read side of pipe and duplicate STDIN with it.
			 * Duplicate write side of pipe and duplicate STDOUT with it.
			 * Close pipe ends we don't need anymore.
			 * Call grep(1).
			 */
			sys_return = dup2(pipes[cur-1][PIPE_IN], STDIN_FILENO);
			check_sys_return(sys_return, "@grep: Error when duplicating read side of pipe");

			sys_return = close(pipes[cur-1][PIPE_OUT]);
			check_sys_return(sys_return, "@grep: Error when closing read side of pipe");

			sys_return = dup2(pipes[cur][PIPE_OUT], STDOUT_FILENO);
			check_sys_return(sys_return, "@grep: Error when duplicating read side of pipe");
			
			sys_return = close(pipes[cur][PIPE_IN]);
			check_sys_return(sys_return, "@grep: Error when closing read side of pipe");

			execvp("grep", argv);
			perror("Problem with call to grep");
			exit(1);
		}

		++cur; /* Mark second step as done */

		/* 
		 * Now we have completed 2 steps.
		 * Try to close pipe ends in parent, they are not needed anymore.
		 */
		sys_return = close(pipes[cur-2][PIPE_IN]);
		check_sys_return(sys_return, "Error when closing read side of pipe");
		sys_return = close(pipes[cur-2][PIPE_OUT]);
		check_sys_return(sys_return, "Error when closing read side of pipe");
	}

	/* Let's do some sorting */
	pid = fork();
	check_sys_return(pid, "Error when forking");

	if (0 == pid)
	{
		/* 
		 * What to do here?
		 * Duplicate read side of pipe and duplicate STDIN with it.
		 * Duplicate write side of pipe and duplicate STDOUT with it.
		 * Close pipe ends we don't need anymore.
		 * Call sort(1)
		 */
		sys_return = dup2(pipes[cur-1][PIPE_IN], STDIN_FILENO);
		check_sys_return(sys_return, "@sort: Error when duplicating read side of pipe");

		sys_return = close(pipes[cur-1][PIPE_OUT]);
		check_sys_return(sys_return, "@sort: Error when closing write side of pipe");

		sys_return = dup2(pipes[cur][PIPE_OUT], STDOUT_FILENO);
		check_sys_return(sys_return, "@sort: Error when duplicating read side of pipe");

		sys_return = close(pipes[cur][PIPE_IN]);
		check_sys_return(sys_return, "@sort: Error when closing write side of pipe");


		execlp("sort", "sort", (char *)0);
		/* If we get here, something went wrong with call to sort(1) */
		perror("Problem with call to sort");
		exit(1);
	}

	++cur; /* Another step is complete. */
	/* 
	 * Now we have completed at least 2 steps.
	 * Try to close pipe ends in parent, they are not needed anymore.
	 */
	sys_return = close(pipes[cur-2][PIPE_IN]);
	check_sys_return(sys_return, "Error when closing read side of pipe");
	sys_return = close(pipes[cur-2][PIPE_OUT]);
	check_sys_return(sys_return, "Error when closing read side of pipe");


	/* Last step, check PAGER */
	pid = fork();
	check_sys_return(pid, "Error when forking");

	if (0 == pid)
	{
		/* 
		 * What to do here?
		 * Duplicate read side of pipe and duplicate STDIN with it.
		 * Close pipe ends we don't need anymore.
		 * Check which pager to use.
		 *		If variable pager exist: use that pager
		 *		else if pager less exist: use that
		 *		else if pager more exist: use that
		 *		else terminate
		 * call correct pager
		 */
		sys_return = dup2(pipes[cur-1][PIPE_IN], STDIN_FILENO);
		check_sys_return(sys_return, "@pager: Error when duplicating read side of pipe");
		
		sys_return = close(pipes[cur-1][PIPE_OUT]);
		check_sys_return(sys_return, "@pager: Error when closing write side of pipe");

		/* Check pager */
		pager = getenv("PAGER"); /* Returns the value of the environment variable as a string, or NULL if it does not exist */
		if (NULL != pager)
		{
			printf("Found PAGER: %s", pager);
			execlp(pager, pager, (char *)0);
		}

		execlp("less", "less", (char *)0); /* If variable PAGER did not exist */
		execlp("more", "more", (char *)0); /* If call to less(1) did not work */
		perror("Problem with call to pager"); /* If no previous call worked */
		exit(1);
	}

	++cur; /* Another step is complete. */
	/* 
	 * Now we have completed at least 2 steps.
	 * Try to close pipe ends in parent, they are not needed anymore.
	 */
	sys_return = close(pipes[cur-2][PIPE_IN]);
	check_sys_return(sys_return, "Error when closing read side of pipe");
	sys_return = close(pipes[cur-2][PIPE_OUT]);
	check_sys_return(sys_return, "Error when closing read side of pipe");

	/*
	 * All that is left is to wait for all children to finish and then check their exit statuses.
	 */

	for (i = 0; i < cur; ++i)
	{
		pid = wait(&return_status); /* Wait for a child. 'status' will be given a value of how the child exited */
		check_sys_return(pid, "@digenv: wait() returned unexpectedly");

		if (WIFEXITED(return_status)) /* Child process is done */
  		{
    		int child_status = WEXITSTATUS(return_status);
    		if (0 == exit_value && 0 != child_status) /* child had problems */
    		{
    			printf("A child had problem and failed with exit code %d.\nChild pid: %d\n", child_status, pid);
    			exit_value = child_status;
    		}
  		}
  		else
  		{
			if (0 == exit_value && WIFSIGNALED(return_status))/* child was terminated by a signal */
			{
      			int child_status = WTERMSIG(return_status);
      			printf("A child had problem and failed with signal %d.\nChild pid: %d\n", child_status, pid);
      			exit_value == child_status;
    		}
  		}
	}

	exit(exit_value);
}