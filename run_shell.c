/*
 * This code implements a simple shell program
 * It supports the internal shell command "exit", 
 * backgrounding processes with "&", input redirection
 * with "<" and output redirection with ">".
 * However, this is not complete.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#define DEBUG 1

extern char **my_getline();
int append_output(char **args, char **output_filename);
void tilde(char **args);

/*
 * Handle exit signals from child processes
 */
void sig_handler(int signal) {
  int status;
  int result = wait(&status);

  if (DEBUG)
    printf("Wait returned %d\n", result);
}

/*
 * The main shell function
 */ 
main() {
  int i;
  char **args; 
  int result;
  int block;
  int output;
  int input;
  char *output_filename;
  char *input_filename;

  // Set up the signal handler
  sigset(SIGCHLD, sig_handler);

  // Loop forever
  while(1) {

    // Print out the prompt and get the input
    printf("->");
    args = my_getline();

    // No input, continue
    if(args[0] == NULL)
      continue;

    // Check for internal shell commands, such as exit
    if(internal_command(args))
      continue;

    // Check for tilde (~)
    tilde(args);

    // Check for an ampersand
    block = (ampersand(args) == 0);

    // Check for redirected input
    input = redirect_input(args, &input_filename);

    switch(input) {
    case -1:
      printf("Syntax error!\n");
      continue;
      break;
    case 0:
      break;
    case 1:
      if (DEBUG)
        printf("Redirecting input from: %s\n", input_filename);
      break;
    }

    // Check for redirected output
    output = redirect_output(args, &output_filename);

    switch(output) {
    case -1:
        printf("Syntax error!\n");
      continue;
      break;
    case 0:
      break;
    case 1:
      if (DEBUG)
        printf("Redirecting (Overriding) output to: %s\n", output_filename);
      break;
    case 2:
      if (DEBUG)
        printf("Redirecting (Appending) output to: %s\n", output_filename);
      break;
    }

    // Do the command
    do_command(args, block, 
	       input, input_filename, 
	       output, output_filename);

    for (i = 0; args[i] != NULL; i++)
      free(args[i]);
  }
}

/*
 * Check for ampersand as the last argument
 */
int ampersand(char **args) {
  int i;

  for(i = 1; args[i] != NULL; i++) ;

  if(args[i-1][0] == '&') {
    free(args[i-1]);
    args[i-1] = NULL;
    return 1;
  } else {
    return 0;
  }
  
  return 0;
}

/*
 * Check for tilde characters replace with home directory
 */
void tilde(char **args) {
  int i;

  for(i = 0; args[i] != NULL; i++) {
    if(args[i][0] == '~') {
      char *expandedLocation;
      const char *homeDir = getenv("HOME");
      int argLength = strlen(args[i]);
      int length = strlen(homeDir) + argLength;

      expandedLocation = (char *)malloc(sizeof(char) * length);
      strcpy(expandedLocation, homeDir);

      // Remove tilde from start of location
      int j;
      for (j = 0; args[i][j+1] != '\0'; j++) 
        args[i][j] = args[i][j+1];
      args[i][j] = '\0';

      char *tempArg = args[i];
      strcat(expandedLocation, args[i]);
      args[i] = expandedLocation;
      free(tempArg);
    }
  }
}

/* 
 * Check for internal commands
 * Returns true if there is more to do, false otherwise 
 */
int internal_command(char **args) {
  if(strcmp(args[0], "exit") == 0) {
    exit(0);
  }

  return 0;
}

/* 
 * Do the command
 */
int do_command(char **args, int block,
	       int input, char *input_filename,
	       int output, char *output_filename) { 
  int result;
  pid_t child_id;
  int status;

  // Fork the child process
  child_id = fork();

  // Check for errors in fork()
  switch(child_id) {
  case EAGAIN:
    perror("Error EAGAIN: ");
    return;
  case ENOMEM:
    perror("Error ENOMEM: ");
    return;
  }

  if(child_id == 0) {

    // Set up redirection in the child process
    if(input)
      freopen(input_filename, "r", stdin);

    if(output == 1)
      freopen(output_filename, "w+", stdout);

    if(output == 2)
      freopen(output_filename, "a+", stdout);

    // Execute the command
    result = execvp(args[0], args);

    exit(-1);
  }

  // Wait for the child process to complete, if necessary
  if(block) {
    if (DEBUG)
      printf("Waiting for child, pid = %d\n", child_id);
    result = waitpid(child_id, &status, 0);
  }
}

/*
 * Check for input redirection
 */
int redirect_input(char **args, char **input_filename) {
  int i;
  int j;

  for(i = 0; args[i] != NULL; i++) {

    // Look for the <
    if(args[i][0] == '<') {
      free(args[i]);

      // Read the filename
      if(args[i+1] != NULL) {
        *input_filename = args[i+1];
      } else {
        return -1;
      }

      // Adjust the rest of the arguments in the array
      for(j = i; args[j-1] != NULL; j++) {
        args[j] = args[j+2];
      }

      return 1;
    }
  }

  return 0;
}

/*
 * Check for output redirection
 */
int redirect_output(char **args, char **output_filename) {
  int i;
  int j;
  int redirectType = 1;

  for(i = 0; args[i] != NULL; i++) {

    // Look for the >
    if(args[i][0] == '>') {
      if(args[i][1] == '>')
        redirectType = 2;
      free(args[i]);

      // Get the filename 
      if(args[i+1] != NULL) {
	      *output_filename = args[i+1];
      } else {
	      return -1;
      }

      // Adjust the rest of the arguments in the array
      for(j = i; args[j-1] != NULL; j++) {
	      args[j] = args[j+2];
      }

      return redirectType;
    }
  }

  return 0;
}

