#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#define BUFSIZE 256

int interactive = 0; //1 for interactive mode, 0 for script mode
char *arg; //for input redirection, to hold child process file

void  trim(char *line, char **argv, int* argc)
{
	int length = 0;
    while (*line != '\0') // while not the end of line
	{       					
		int hasToken = 0;
        while (isspace(*line)) 
		{
			*line++ = '\0';     // replace white spaces with null char 
			if (hasToken == 0)
			{
				hasToken = 1;
				length++;
			}
		 }
         *argv++ = line;        // maintain next argument's position    
         while (*line != '\0' && !(isspace(*line))) line++;
     }
     *argv = '\0';                 // mark the end of argument list
	 *argc = length;
}

int  executeCmd(char **argv, int background, int input, int output)
{
	pid_t pid;
	int status;
	int inFile;
	int outFile;
	arg = *argv;
	
	if ((pid = fork()) < 0)
	{             
		fprintf(stderr, "Fork unsuccessful\n"); //remove this later?
		exit(1);
    }
	
	//child process
	if (pid == 0) 
	{            
		//remove irrelevant args from command
		int i = 0;
		while (argv[i])
		{
			if ((strcmp(argv[i], ">") == 0) || (strcmp(argv[i], "<") == 0) || (strcmp(argv[i], "&") == 0))
			{
				argv[i] = "";
			}
			i++;
		}
		
		//Open files and remove file args from command
		if (output != -1)
		{
			//printf("output file is %s\n", argv[output]);
			outFile = open(argv[output], O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (outFile < 0 || dup2(outFile, STDOUT_FILENO) < 0) return 1;
			argv[output] = "";
		} 
		if (input != -1)
		{
			//printf("input file is %s\n", argv[input]);
			inFile = open(argv[input], O_RDONLY);
			if (inFile < 0 || dup2(inFile, STDIN_FILENO) < 0) return 1;
			//rebuild argv using stdin now?
			if (fgets(*argv, BUFSIZE, stdin) == NULL)
			{
				return 1;
			}
		}
		
		//execute command
		if (execvp(arg, argv) < 0) //path (file to execute) , args (arguments to file)
		{ 
			fprintf(stderr, "%s: %s\n", *argv, strerror(errno));
            exit(1);
        }
		close(inFile);
		close(outFile);
	}
	else while (wait(&status) != pid)
	{
		if (background) break;
	}
	return 0;
}

/*Each command will appear as a single line of input*/

int processCmd(char *str, char **argv, int argCount, int input, int output, int background)
{
	if (argCount == -1) return 1;
	int validCmd = 0;//used for distinguishing shell vs exec commands (and general valid commands)
	
	char *token = strtok(str, " ");
	// < turn argv[input] into stdin (read from argv[input] as a file for command at index input -2's arguments)
	// > turn argv[output] into stdout (write to argv[output] as a file for command at index output -2's output)
	
	
	if (token != NULL) //add while loops using token = strtok(NULL, " "); as check
	{
		//printf("%d arguments entered.\n", argCount);
		if (strcmp(token, "echo") == 0)
		{
			//printf("echo command entered\n");
			validCmd = 1;
		}
		else if (strcmp(token, "tr") == 0)
		{
			//printf("tr command entered\n");
			validCmd = 1;
		}
		else if (strcmp(token, "ls") == 0)
		{
			//printf("ls command entered\n");
			validCmd = 1;
		}
		else if (strcmp(token, "cat") == 0)
		{
			//printf("cat command entered\n");
			validCmd = 1;
		}
		else if (strcmp(token, "clear") == 0)
		{
			//printf("clear command entered\n");
			if (argCount > 1) 
			{
				fprintf(stderr, "clear: too many arguments\n");
			}
			else validCmd = 1;
		}
		else if (strcmp(token, "cd") == 0)
		{
			//printf("cd command entered\n");
			validCmd = 0;
			if (argCount > 2) 
			{
				fprintf(stderr, "cd: too many arguments\n");
			}
			else if (argCount == 1)
			{
				int ret = chdir(getenv("HOME"));
				if (ret != 0)
				{
					fprintf(stderr, "cd: %s: %s\n", getenv("HOME"), strerror(errno));
				}
			}
			else if (argCount == 2)
			{
				//printf("Changing dir to %s\n", argv[1]);//delete this later!
				int ret = chdir(argv[1]);
				if (ret != 0)
				{
					fprintf(stderr, "cd: %s: %s\n", argv[1], strerror(errno));
				}
			}
		}
		else if (strcmp(token, "pwd") == 0)
		{
			//printf("pwd command entered\n");
			validCmd = 0;
			if (argCount > 1) 
			{
				fprintf(stderr, "pwd: too many arguments\n");
			}
			char dir[256];
			printf("%s\n", getcwd(dir, 256));
		}
		else if (strcmp(token, "exit") == 0)
		{
			if (strcmp(argv[0], "exit") == 0) 
			exit(0); 
		}
		else 
		{
			fprintf(stderr, "Invalid command.\n");
		}
		if (validCmd) executeCmd(argv, background, input, output);
	//printf("Valid command = %d\n", validCmd);
	}
	return validCmd;
}

int processFile(FILE *file)
{
	//Read a single line from a file
	//execute commands from that line
	//until EOF
	char str[256];
	while (fgets(str, BUFSIZE, file) != NULL)
	{
		int *status = 0;
		waitpid(0, status, WNOHANG);
		if (status > 0) fprintf(stderr, "[Process completed with status %d]\n", *status);
		char  *argv[256];
		int argCount = 0;
		int input = -1;
		int output = -1;
		int inputCt = 0;
		int outputCt = 0;
		int background = 0;
		trim(str, argv, &argCount);
		for (int i = 0; i < argCount; i++)
		{
			if (strcmp(argv[i], ">") == 0)
			{
				//printf("Found output redirection\n");
				//printf("Command feeds into file at argv[%d]\n", i+1);
				output = i + 1;
				outputCt++;
				//turn argv[output] into stdout
			}
			if (strcmp(argv[i], "<") == 0)
			{
				//printf("Found input redirection\n");
				//printf("Command reads args from file at argv[%d]\n", i+1);
				input = i + 1;
				inputCt++;
				//turn argv[input] into stdin
			}
		}
		if (strcmp(argv[argCount-1], "&") == 0)
		{
			printf("Running in background\n");
			background = 1;
		}
		if (inputCt + outputCt > 2) 
		{
			fprintf(stderr, "Syntax: invalid redirection\n");//Remove this!
			return 1;
		}
		processCmd(str, argv, argCount, input, output, background);
	}
	return 0;
}


int main(int argc, char** argv)
{
	/* Your code here! */
	if (argc == 1 && isatty(0)) interactive = 1;
	
	/* In non-interactive mode, the input source should be the file named 
	by the first command-line argument if there is one (a script, in essence), 
	or stdin if there isn't a command-line argument.*/
	
	if (argc == 2 && interactive == 0)
	{
		FILE *file;
		if ((file = fopen(argv[1], "r")) != NULL)
		{
			processFile(file);
			exit(0);
		}
		else 
		{
			fprintf(stderr, "Invalid filename for script mode.\n");
			exit(1);
		}
	}
	else
	{
		//Read command lines from stdin
		printf("sqysh$ ");
		char str[256];
		while (fgets(str, BUFSIZE, stdin) != NULL)
		{
			int *status = 0;
			waitpid(0, status, WNOHANG);
			if (status > 0) fprintf(stderr, "[Process completed with status %d]\n", *status);
			//printf("\n");
			char  *argv[256];
			int argCount = 0;
			int input = -1;
			int output = -1;
			int inputCt = 0;
			int outputCt = 0;
			int background = 0;
			trim(str, argv, &argCount);
			for (int i = 0; i < argCount; i++)
			{
				if (strcmp(argv[i], ">") == 0)
				{
					//printf("Found output redirection\n");
					//printf("Command feeds into file at argv[%d]\n", i+1);
					output = i + 1;
					outputCt++;
					//turn argv[output] into stdout
				}
				if (strcmp(argv[i], "<") == 0)
				{
					//printf("Found input redirection\n");
					//printf("Command reads args from file at argv[%d]\n", i+1);
					input = i + 1;
					inputCt++;
					//turn argv[input] into stdin
				}
			}
			if (strcmp(argv[argCount-1], "&") == 0)
			{
				//printf("Running in background\n");
				background = 1;
			}
			if (inputCt + outputCt > 2) 
			{
				fprintf(stderr, "Syntax: invalid redirection\n");//Remove this!
				argCount = -1; //because this is main and I can't use return(1) like in processFile
			}
			processCmd(str, argv, argCount, input, output, background);
			printf("sqysh$ "); 
			waitpid(0, status, WNOHANG);
			if (status > 0) fprintf(stderr, "[Process completed with status %d]\n", *status);
		}		
	}
	
	return 0;
}
