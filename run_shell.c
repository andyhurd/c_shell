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

#define DEBUG 0

extern char **my_getline();
int *splitCommands(char ****commands, char **args);
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
  char **args;
  int i; 
  int *nCommands;
  char ***commands;
  

  // Set up the signal handler
  sigset(SIGCHLD, sig_handler);

  // Loop forever
  while(1) {

    // Print out the prompt and get the input
    printf("->");
    args = my_getline();
  
    // No input, continue
    if(args == NULL)
      return -1;

    // Check for command delimiters
    nCommands = splitCommands(&commands, args);

    int j;
    for (i = 0; i < *nCommands; i++) {
      if (DEBUG) {
        printf("\nTESTING: "); debugPrintArgs(commands[i]);
      }
      initCommand(commands[i]);
    }
  }
}


/*
 * Initiate Command
 */
int initCommand(char **args){
	int i;
	int result;
	int block;
	int output;
	int input;
	int pipe;
	char *output_filename;
	char *input_filename;
  
    // No input, continue
    if(args[0] == NULL)
      return -1;

	  // Check for tilde (~)
	  tilde(args);
	  
    // Check for internal shell commands, such as exit
    if(internal_command(args))
      return -1;

    // Check for an ampersand
    block = (ampersand(args) == 0);

    // Check for redirected input
    input = redirect_input(args, &input_filename);

    switch(input) {
    case -1:
      printf("Syntax error!\n");
	  return -1;
      break;
    case 0:
      break;
    case 1:
      if (DEBUG)
        printf("Redirecting input from: %s\n", input_filename);
      break;
    }


	// Check for pipe "|"
	pipe = pipes(args);

  // Check for redirected output
  output = redirect_output(args, &output_filename);

  switch(output) {
  case -1:
      printf("Syntax error!\n");
    return;
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

    switch(pipe) {
    case -1:
        printf("Syntax error!\n");
	  return -1;
      break;
    case 0:
      break;
    case 1:
      if (DEBUG)
        printf("Pipe detected\n");
      break;
    }


    // Do the command
    do_command(args, block, 
			input, input_filename, 
			output, output_filename,
			pipe);
			
  for (i = 0; args[i] != NULL; i++)
    free(args[i]);
	free(args);
}

/*
 * Check for | and returns true if found
 * 	Returns 1 if true, 0 if false
 */
int pipes(char **args){
	int i;
	if (args[0][0] == '|'){
		return -1;
	}
	for (i = 1; args[i] != NULL; i++){
		if (args[i][0] == '|'){
			return 1;
		}
	}
	return 0;
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
  } else if (strcmp(args[0], "cd") == 0) {
    chdir(args[1]);
    return 1;
  }

  return 0;
}

/* 
 * Do the command
 */
int do_command(char **args, int block,
	       int input, char *input_filename,
	       int output, char *output_filename,
		 int pipe) {
  
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

	if (pipe){
		// Pipe Command
		result = runPipes(args);
	} else { //Normal Command
		// Execute the command
		result = execvp(args[0], args);
	}

    exit(-1);
  }

  // Wait for the child process to complete, if necessary
  if(block) {
    if (DEBUG)
      printf("Waiting for child, pid = %d\n", child_id);
    result = waitpid(child_id, &status, 0);
  } else {
	
    if (DEBUG)
      printf("Running child in background, pid = %d\n", child_id);
	if ((strcmp(args[0], "vi") == 0) ||
			(strcmp(args[0], "vim") == 0) ||
			(strcmp(args[0], "nano") == 0) ||
			(strcmp(args[0], "emacs") == 0)) {
		kill(child_id, SIGSTOP);
	}
	void delete_zombies(void);
	struct sigaction sa;
	sigfillset(&sa.sa_mask);
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigaction(SIGCHLD, &sa, NULL);
  }
  

  
}

 
/*
 * Deals with all things pipes!
 */
int runPipes(char **args){
	char **cmd1;
	int fds[2];
	int child;
	int i;
	int k = 0;
	int pipeLocation = -1;

	// Find location of "|"
	while(args[++pipeLocation][0] != '|');

	// Copy first command over
	cmd1 = malloc((pipeLocation + 2) * sizeof(char*));
	for (k = 0; k < pipeLocation; k++){
		cmd1[k] = args[k];
	}
	int j;
	for (j = 0; args[j] != NULL; j++){
		args[j] = args[j+k+1];
	}

	if (DEBUG) {
		printf("cmd1: "); debugPrintArgs(cmd1);
		printf("args: ");debugPrintArgs(args);
	}

	//Create Pipe
	pipe(fds);

	//fork kidas
	if (!fork()){
		close(1); //Close stnd out
		dup(fds[1]); //Reassign fds[1] to be stnd out
		close(fds[0]); // close pipe input;
		initCommand(cmd1); //Run command (left side)
	} else {
		close(0); // Close stnd in
		dup(fds[0]); // Reassign fds[0] to be new stnd in
		close(fds[1]); // close pipe input;
		initCommand(args); // Run right side of pipe
	}
	return 0;
}

/*
 * Print args for debug purpuses
 */
int debugPrintArgs(char **args){
	int i = 0;
	while(args[i] != NULL){
		printf("%s ", args[i++]);
	}
	printf("\n");
	return i;
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
      if(args[i+1] != NULL && args[i+1][0] == '>') {
        free(args[i+1]);
        redirectType = 2;
      }
      free(args[i]);

      // Get the filename 
      if(args[i+redirectType] != NULL) {
	      *output_filename = args[i+redirectType];
      } else {
	      return -1;
      }

      // Adjust the rest of the array
      for(j = i; args[j-1] != NULL; j++) {
	      args[j] = args[j+1+redirectType];
      }

      return redirectType;
    }
  }

  return 0;
}

/*
 *	Splits commands by ';'
 */
int *splitCommands(char ****commands, char **args) {
  int cmdStart = 0;
  int *nCommands = (int *)malloc(sizeof(int));
  *nCommands = 0;
  int *error = (int *)malloc(sizeof(int));
  *error = -1;
  int numArgs = 0;
  int i;
  char *buffer;

  // find number of commands total
  for (i = 0; args[i] != NULL; i++) {
    if (args[i][0] == ';') {
      if(i == cmdStart) {
        printf("SHELL: Syntax error near unexpected token ';'\n");
        free(nCommands);
        return error;
      }
      cmdStart = i + 1;
      (*nCommands)++;
    }
  }

  cmdStart = 0;

  free(error);

  // find number of arguments total
  for (i = 0; args[i] != NULL; i++) {
    numArgs++;
  }

  // allocate memory for array of commands
  *commands = (char ***)malloc(sizeof(char **) * (*nCommands + 1));

  *nCommands = 0;

  // build commands that are terminated with semicolon
  for (i = 0; args[i] != NULL; i++) {
    if (args[i][0] == ';') {
      int argIndex;

      // allocate memory for new command
      (*commands)[*nCommands] = (char **)malloc(sizeof(char *) * (i - cmdStart + 1));

      int j;
      for(j = cmdStart; j < i; j++) {
        // set argIndex
        argIndex = j - cmdStart;

        // copy argument to a buffer char *
        int length = strlen(args[j]);
        buffer = (char *)malloc(sizeof(char) * (length + 1)); 
        strcpy(buffer, args[j]);
        buffer[length] = '\0';

        // point the next arg of current command to buffer
        (*commands)[*nCommands][argIndex] = buffer;

        // free memory 
        free(args[j]);
      }

      // free memory
      free(args[i]);

      // null terminate this command's array of args
      argIndex++;
      (*commands)[*nCommands][argIndex] = NULL;

      cmdStart = i + 1;
      (*nCommands)++;
    }
  }

  // build commands that are terminated at EOF
  if (args[cmdStart] != NULL) {
    int argIndex;

    // allocate memory for new command
    (*commands)[*nCommands] = (char **)malloc(sizeof(char *) * (numArgs - cmdStart + 1));

    for (i = cmdStart; args[i] != NULL; i++) {
      // set argIndex
      argIndex = i - cmdStart;

      // copy argument to a buffer char *
      int length = strlen(args[i]);
      buffer = (char *)malloc(sizeof(char) * (length + 1)); 
      strcpy(buffer, args[i]);
      buffer[length] = '\0';

      // point the next arg of current command to buffer
      (*commands)[*nCommands][argIndex] = buffer;

      // free memory 
      free(args[i]);
    }

    // null terminate this command's array of args
    argIndex = i - cmdStart;
    (*commands)[*nCommands][argIndex] = NULL;

    (*nCommands)++;
  }

  // null terminate array of commands
  (*commands)[*nCommands] = NULL;

  return nCommands;
}
