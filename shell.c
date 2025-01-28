// BY: DAVID KRLJANOVIC

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pwd.h>

#define COMMAND_LENGTH 1024
#define NUM_TOKENS (COMMAND_LENGTH / 2 + 1)

// counter and array for history
#define HISTORY_DEPTH 10
char history[HISTORY_DEPTH + 1][COMMAND_LENGTH];
int counter = 0;

// flag for signal
int flag = 0;

// past directory for "ch -"
char pastDir[500];

//function prototypes.
bool check_internal(char* tokens[]);
void external_commands(char* tokens[], _Bool* in_background);

void historyAdd(char* cmd)
{   
    // shift existing history to make room for new cmd.
    for(int i = HISTORY_DEPTH - 1; i >= 0; i--)
    {
        strcpy(history[i+1],history[i]);
    }
    
    // create new history string:
    for(int i = 0; cmd[i] != 0; i++)
    {
        history[0][i] = cmd[i];
    }

    // adding terminating character
    history[0][strlen(cmd)] = 0;

    //increment counter
    counter++;
    
}
// this reverses the top ten history functions if "!!", "!-" or "!n" commands are entered.
void historyReverse()
{
    for(int i = 1; i < 11; i++)
    {
        strcpy(history[i-1],history[i]);
    }

    //decrement counter'
    counter--;
}
void historyPrint()
{
    // we print in the order specified in the assignment instructions.
    // first we turn the counter/line value into a string, then we concat to get
    // our entire history line printed.
    int lineNo;
    char historyLine[1032]; //1032 chars long because COMMAND_LENGTH + 8.
    for(int i = 0; i < HISTORY_DEPTH; i++)
    {   
        lineNo = counter - (i + 1);
        
        // here we just check the edge case that we are not looking into random memory.
        if(lineNo >= 0)
        {
            sprintf(historyLine, "%d        ", lineNo);
            historyLine[8] = 0;
            strcat(historyLine, history[i]);
            write(STDOUT_FILENO, historyLine, strlen(historyLine));
            write(STDOUT_FILENO, "\n", 1);
        }
    }
}

/**
 * Command Input and Processing
 */

/*
 * Tokenize the string in 'buff' into 'tokens'.
 * buff: Character array containing sthtring to tokenize.
 *       Will be modified: all whitespace replaced with '\0'
 * tokens: array of pointers of size at least COMMAND_LENGTH/2 + 1.
 *       Will be modified so tokens[i] points to the i'th token
 *       in the string buff. All returned tokens will be non-empty.
 *       NOTE: pointers in tokens[] will all point into buff!
 *       Ends with a null pointer.
 * returns: number of tokens.
 */
int tokenize_command(char *buff, char *tokens[])
{
	int token_count = 0;
	_Bool in_token = false;
	int num_chars = strnlen(buff, COMMAND_LENGTH);
	for (int i = 0; i < num_chars; i++) {
		switch (buff[i]) {
		// Handle token delimiters (ends):
		case ' ':
		case '\t':
		case '\n':
			buff[i] = '\0';
			in_token = false;
			break;

		// Handle other characters (may be start)
		default:
			if (!in_token) {
				tokens[token_count] = &buff[i];
				token_count++;
				in_token = true;
			}
		}
	}
	tokens[token_count] = NULL;
	return token_count;
}

/**
 * Read a command from the keyboard into the buffer 'buff' and tokenize it
 * such that 'tokens[i]' points into 'buff' to the i'th token in the command.
 * buff: Buffer allocated by the calling code. Must be at least
 *       COMMAND_LENGTH bytes long.
 * tokens[]: Array of character pointers which point into 'buff'. Must be at
 *       least NUM_TOKENS long. Will strip out up to one final '&' token.
 *       tokens will be NULL terminated (a NULL pointer indicates end of tokens).
 * in_background: pointer to a boolean variable. Set to true if user entered
 *       an & as their last token; otherwise set to false.
 */
void read_command(char *buff, char *tokens[], _Bool *in_background)
{
	*in_background = false;

	// Read input
	int length = read(STDIN_FILENO, buff, COMMAND_LENGTH-1);

	if (length < 0 && (errno != EINTR)) {
		perror("Unable to read command from keyboard. Terminating.\n");
		exit(-1);
	}

    

	// Null terminate and strip \n.
	buff[length] = '\0';
	if (buff[strlen(buff) - 1] == '\n') {
		buff[strlen(buff) - 1] = '\0';
	}

    if(strlen(buff) != 0)
    {   
        //add to history
        if(flag == 0) historyAdd(buff); // so that sigint doesnt add to history twice.
    }

	// Tokenize (saving original command string)
	int token_count = tokenize_command(buff, tokens);
	if (token_count == 0) {
		return;
	}

    

	// Extract if running in background:
	if (token_count > 0 && strcmp(tokens[token_count - 1], "&") == 0) {
		*in_background = true;
		tokens[token_count - 1] = 0;
	}
}

// This function handles everything related to the 'help' command.
void helpCMD(char *tokens[])
{
    
    char *tooManyArgs = "Error: Too many arguments.\n";

    // if our only token is 'help', we return this.
    if(tokens[1] == NULL)
     {
        char* temp[3] = {"help", "help", NULL};
        write(STDOUT_FILENO, "\nhelp: ", 7);
        helpCMD(temp);

        temp[1] = "pwd";
        write(STDOUT_FILENO, "\npwd: ", 6);
        helpCMD(temp);

        temp[1] = "exit";
        write(STDOUT_FILENO, "\nexit: ", 7);
        helpCMD(temp);

        temp[1] = "cd";
        write(STDOUT_FILENO, "\ncd: ", 5);
        helpCMD(temp);

        temp[1] = "history";
        write(STDOUT_FILENO, "\nhistory: ", 10);
        helpCMD(temp);

        write(STDOUT_FILENO, "\n", 1);
    }
    // these else ifs handle help for existing internal commands.
    else if(strcmp("exit", tokens[1]) == 0)
    {
        if(tokens[2] == NULL)
        {
            char* msg = "'exit' is a builtin command for closing and exiting the shell.\n";
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        else
        {
            write(STDOUT_FILENO, tooManyArgs, strlen(tooManyArgs));
        }
    }
    else if(strcmp("pwd", tokens[1]) == 0)
    {
        if(tokens[2] == NULL)
        {
            char* msg = "'pwd' is a builtin command for returning the current working directory.\n";
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        else
        {
            write(STDOUT_FILENO, tooManyArgs, strlen(tooManyArgs));
        }
    }
    else if(strcmp("cd", tokens[1]) == 0)
    {
        if(tokens[2] == NULL)
        {
            char* msg = "'cd' is a builtin command for changing the current working directory.\n";
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        else
        {
            write(STDOUT_FILENO, tooManyArgs, strlen(tooManyArgs));
        }
    }
    else if(strcmp("help", tokens[1]) == 0)
    {
        if(tokens[2] == NULL)
        {
            char* msg = "'help' is a builtin command for displaying useful information about shell commands.\n";
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        else
        {
            write(STDOUT_FILENO, tooManyArgs, strlen(tooManyArgs));
        }
    }
    else if(strcmp("history", tokens[1]) == 0)
    {
        if(tokens[2] == NULL)
        {
            char* msg = "'history' is a builtin command for displaying past commands.\n";
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        else
        {
            write(STDOUT_FILENO, tooManyArgs, strlen(tooManyArgs));
        }
    }
    // If the second token isnt internal:
    else
    {
        if(tokens[2] == NULL)
        {
            char* msg = "' is an external command or application.\n";
            write(STDOUT_FILENO, "'", 1);
            write(STDOUT_FILENO, tokens[1], strlen(tokens[1]));
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        else
        {
            write(STDOUT_FILENO, tooManyArgs, strlen(tooManyArgs));
        }
    }
     




}

// this function handles the sigint signal
void handle_SIGINT()
{
    write(STDOUT_FILENO, "\n", 1);
    char* sigintHelp[2] = {"help", NULL};
	helpCMD(sigintHelp);
    char* help = "help";
    historyAdd(help);
    flag = 1;
    
    return;
}

// this function handles internal cmds
void internal_commands(char *tokens[])
{
    char *tooManyArgs = "Error: Too many arguments.\n";
    // internal commands.
    //
    if(strcmp("exit", tokens[0]) == 0)
    {   
        // if input is correct format, return status 0 so that parent function knows to exit completely.
        if(tokens[1] == NULL) 
        {   
            exit(0);
        }
        else
        {   
            write(STDOUT_FILENO, tooManyArgs, strlen(tooManyArgs));
        }
    }
    else if(strcmp("pwd", tokens[0]) == 0)
    {
        if(tokens[1] == NULL)
        {
            char currDir[500];
            getcwd(currDir, 500);
            write(STDOUT_FILENO, currDir, strlen(currDir));
            write(STDOUT_FILENO, "\n", 1);
            
        }
        else
        {
            write(STDOUT_FILENO, tooManyArgs, strlen(tooManyArgs));
        }

        
    }
    else if(strcmp("cd", tokens[0]) == 0)
    {   

        // here we also pass for tokens[1] so we can get the appropriate error message via errno.
        if(tokens[2] == NULL || tokens[1] == NULL)
        {   
            // so that "cd " has the appropriate error message, and so "cd " works
            // the same as  "cd ~" like in a real terminal.
            // Change directory to home directory. We can either use getenv (risky because maybe the target 
            // PC uses different "HOME" path), or we can use getuid and getpwuid.
            struct passwd *home = getpwuid(getuid());

            // for replacing the past directory
            char currDir[500];
            getcwd(currDir, 500);

            if(tokens[1] == NULL || strcmp(tokens[1],"~") == 0) tokens[1] = home->pw_dir, tokens[2] = NULL;
            else if(strcmp(tokens[1], "-") == 0) tokens[1] = pastDir;
        
            //handling chdir error. If chdir returns a -1, we had an error.
            if((chdir(tokens[1]) == -1))
            {
                char errmsg[200] = "ERROR: ";
                strcat(errmsg, strerror(errno));
                write(STDOUT_FILENO, errmsg, strlen(errmsg));
                write(STDOUT_FILENO, "\n", 1);
                return;
            }

            // copy new past directory.
            strcpy(pastDir, currDir);
        }
        else
        {
            write(STDOUT_FILENO, tooManyArgs, strlen(tooManyArgs));
        }
    }
    else if(strcmp("help", tokens[0]) == 0)
    {   
        // the help command has its own function for simplicity's sake.
        helpCMD(tokens);
    }
    else if(strcmp("history", tokens[0]) == 0)
    {
        if(tokens[1] == NULL)
        {
            historyPrint();
        }
        else
        {
            write(STDOUT_FILENO, tooManyArgs, strlen(tooManyArgs));
        }
    }
    else if(strcmp("!!", tokens[0]) == 0)
    {
         
        char* errmsg = "ERROR: no previous commands.";

        // if we enter !! then we dont want it added to the history.
        historyReverse();

        if(counter != 0)
        {   
            // we need to use strcpy because references jumbles things.
            _Bool dummyVal = false;
            char newcmd[1024];
            char newadd[1024];
            strcpy(newcmd, history[0]);
            strcpy(newadd, newcmd);
            write(STDOUT_FILENO, newcmd, strlen(newcmd));
            write(STDOUT_FILENO, "\n", 1);
            tokenize_command(newcmd, tokens);
            historyAdd(newadd);
            if(!check_internal(tokens)) external_commands(tokens, &dummyVal);
        }
        
        else write(STDOUT_FILENO, errmsg, strlen(errmsg)); // if there are no previous commands to run.
    }
       
    else if(strcmp("!-", tokens[0]) == 0)
    {
        if(tokens[1] == NULL)
        {
            // we dont actually have to clear the history, we just have to reset counter to 0, the functions will do the rest.
            counter = 0;
        }
        else
        {
            write(STDOUT_FILENO, tooManyArgs, strlen(tooManyArgs));
        }
    }
    else
    {   
        // this is the case of !n
        historyReverse();
            // first we check whether n is a number.

            for(int i = 1; tokens[0][i] != 0; i++)
            {
                if(tokens[0][i] < 48 || tokens[0][i] > 57)
                {
                    char* msg = "ERROR: invalid input.\n";
                    write(STDOUT_FILENO, msg, strlen(msg));
                    return;
                }
            }
            
            int numletter = atoi(tokens[0] + 1); //tokens[0] + 1 because we want to start reading the string at the number.

            // checking that we arent trying to access hisotry that doesnt exist.
            if(numletter >= counter || numletter < 0)
            {   
                char* msg = "ERROR: outside of history range.";
                write(STDOUT_FILENO, msg, strlen(msg));
                write(STDOUT_FILENO, "\n", 1);
                return;
            }

            // changing numletter to a range of (ideally) 0 to 9.
            numletter = counter - numletter - 1;

            if(numletter >= 0 && numletter <=9)
            {   
                // we need to use strcpy because it was using the token as referene instead of copying.
                _Bool dummyVal = false;
                char newcmd[1024];
                char newadd[1024];
                strcpy(newcmd, history[numletter]);
                strcpy(newadd, newcmd);
                historyAdd(newadd);
                tokenize_command(newcmd, tokens);
                if(!check_internal(tokens)) external_commands(tokens, &dummyVal);  
            }
            else
            {
                char* msg = "ERROR: outside of history range.";
                write(STDOUT_FILENO, msg, strlen(msg));
                write(STDOUT_FILENO, "\n", 1);
                return;
            }
        
    }
}

// this function handles external_commands.
void external_commands(char* tokens[], _Bool* in_background)
{
    //fork Init
            pid_t  var_pid;
            var_pid = fork();
            
            
            if (var_pid < 0) 
            {
                fprintf(stderr, "fork Failed");
                exit(-1); 
            }
            else if (var_pid == 0) 
            {   
                execvp(tokens[0], tokens);

                //Error handeling
                if(errno != 0)
                {
                    char errmsg[200] = "EXECVP: ";
                    strcat(errmsg, strerror(errno));
                    write(STDOUT_FILENO, errmsg, strlen(errmsg));
                    write(STDOUT_FILENO, "\n", 1);
                    
                }
                
                exit(0);
                
            }
            else
            {
                if(!(*in_background))
                {
                waitpid(var_pid, NULL, 0);
               
                }

            }
}
// This function checks whether a given command is internal or not.
bool check_internal(char *tokens[])
{
    char *internalCMDS[] = {"exit", "pwd", "cd", "help", "history", "!!", "!-"}; // array of strings used to identify internals.
    // We check whether a command is internal or not.
    // if a command is internal, internalFlag is set to true and no fork is created. 
    for(int i = 0; i < 7; i++)
    {
        if(strcmp(tokens[0], internalCMDS[i]) == 0)
        {
            internal_commands(tokens);
            return true;
        }
    }

    char* isIt = tokens[0];
    if (isIt[0] == '!')
        {
            internal_commands(tokens);
            return true;
        }

    return false; // no internal cmd found.
}

/**
 * Main and Execute Commands
 */
int main(int argc, char* argv[])
{
    char currDir[500];
	char input_buffer[COMMAND_LENGTH];
	char *tokens[NUM_TOKENS];
    
    bool internalFlag; // this just makes it so that fork doesnt run.
    
   
    // our default directory upon shell startup will be the home directory!
    char* chdirHome[3] = {"cd", "~", NULL};
    internal_commands(chdirHome); //we use the internal_commands function because it can handle errors.


	while (true) {

        

		// Get command
		// Use write because we need to use read() to work with
		// signals, and read() is incompatible with printf().

        getcwd(currDir, 500);
        strcat(currDir, "$ ");
		write(STDOUT_FILENO, currDir, strlen(currDir));
		_Bool in_background = false;

	// Signal Handling
        struct sigaction handler; 
        handler.sa_handler = handle_SIGINT;
        handler.sa_flags = 0; 
        sigemptyset(&handler.sa_mask); 
        sigaction(SIGINT, &handler, NULL);

		read_command(input_buffer, tokens, &in_background);

        if(flag == 1) tokens[0] = NULL, flag = 0; // fixes a bug where ctrl + c would also make past commands go off.

        // if user enters nothing we just skip the loop.
        if(tokens[0] == NULL) continue;

        // We check whether a command is internal or not.
        // if a command is internal, internalFlag is set to true and no fork is created.
        internalFlag = check_internal(tokens);

        // external commands (if the token was not an internal command and internalFlag is false).
        if(!internalFlag)
        {   
            external_commands(tokens, &in_background);

        }
        
        while (waitpid(-1, NULL, WNOHANG) > 0); // do nothing.
		

        
	}
	return 0;
}
