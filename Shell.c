/* MINI SHELL */

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>

// Formating prompt text
#define ANSI_COLOR_RED   "\x1b[31m"
#define ANSI_COLOR_RESET "\x1b[0m"

#define STYLE_BOLD       "\033[1m"
#define STYLE_RESET      "\033[22m"

// Limits over command arguments and number of characters in them.
#define MAX_COMM 50
#define MAX_CHARS 250

// To print some messages
void print_init(void);
void print_dir(void);
void print_error(void){ printf("\nERROR: ");}

// command number counter
int command_counter;

// This function copies data string into location string.
int copy_string( char**, int, int, char**);

// Functions for environment variables
int process_setenv(char*, char*);
int process_printenv(char*);

// These are the functions for shell built-in commands: exit, cd and history.
int internal_exit(void);
int internal_cd(char** );
int internal_history(void);

// For history, we need to create file, then constantly write to it.
void create_history_file(void);
char* history_file_loc="/tmp/history.txt";
int add_to_history(char*);

// function to handle external commands
int external_execute(char**);

// To avoid code duplication we use this array to refer
//   to the input stream, output steam, and output-append stream.
char* streams[3]={NULL, NULL, NULL};

// These will be used to execute commands.
int execute(char**);
int piped_execute(char*** coms); // for piped commands.



////////////////////////////////////////////////////////////////
/////////////////////     the MAIN function      ///////////////
////////////////////////////////////////////////////////////////


int main(){
    // To print a message at the start
    print_init();
    
    // Now, to create a file for history 
    create_history_file();

    // Initializing the command counter
    command_counter=0;

    // The shell works an infinite loop constantly taking in input and executing it.
    while(1){

        // Printing current directory
        print_dir();

        // Increment the command_counter
        command_counter++;
        
    //.......Reading Input.............
    //.................................
        long unsigned int temp_size=10;
        char* input_string = (char *) malloc (temp_size);
        int size = getline (&input_string, &temp_size, stdin);
        // getline uses dynamic allocation to expand the size of input_string
        // in case the entered command is longer than our expectations.

        //erros in reading input-
        if(size<0){
            perror("getline");

            // Given there was a failure to read input, we reset to start.
            free(input_string);
            continue;
        }

        // Saving to history
        add_to_history(input_string);

    //......Parsing  the Input..........
    //..................................

        // Allocating the array that stores commands & arguments
        
        char*** com_arg=(char***)calloc(2,(sizeof(char**)));

        for(int a=0; a<2; a++){
        
            com_arg[a]=(char**)calloc(MAX_COMM,(sizeof(char*)));
        
            for(int aa=0; aa<MAX_COMM; aa++){
                com_arg[a][aa]=(char*)calloc(MAX_CHARS, (sizeof(char)));
            }
        
        }
        int command_no[2]={0,0};
        
        // in specifying com_arg, the first index specifies which side its on from pipe 
        // then MAX_COMM is the list of commands and MAX_CHARS is number of letters that
        // can be in each command.

        int hvPipes=0; // This will specify which side of pipe we're on
        int args_full=0; // this will be set to true when
        int reset=0;

        for(int i=0; i<size-1 && reset<1; i++){ 
        
            if(input_string[i]==' ') continue; // igonre spaces

            // First.........................................
            // we don't want to mess up with strings in arguments.
            // Like for example, if the input is 
            //      echo "this is a <sample input"
            //  then we don't want "sample" to labeled input stream, since
            //  its a part of a string.
            if(input_string[i]=='\''|| input_string[i]=='\"'){
                if(args_full){ //print error if max limit exceeded
                    print_error(); printf("Max argument lenght exceeded ");
                    break;
                }
                char temp=input_string[i]; int temp_pos=0;
                i++;

                // we copy the contents till we see again ' or "
                while((input_string[i]!=temp ) && i<size){ 
                    if(temp_pos>MAX_CHARS){ break;}
                    com_arg[hvPipes][command_no[hvPipes]][temp_pos]= input_string[i];
                    i++; temp_pos++;
                } 

                if(temp_pos>MAX_CHARS){  // print error if max limit exceeded
                    reset=1; 
                    print_error(); printf("Max character limit exceeded\n");
                    continue;
                }
                
                command_no[hvPipes]++; // increment command number
                if(command_no[hvPipes]>=MAX_COMM) {
                    args_full=1; // so if more input given, we can prompt an error message.
                }
            }

            // .......................................................
            // Second, we mark the pipes.
            else if(input_string[i]=='|'){
                if(hvPipes==1){
                    print_error(); printf("This shell supports single level pipes only.\n");
                    reset=1; // if more than 1 pipe detected, we print an error message and reset.
                }
                hvPipes=1; 
            }
          

            //.........................................................
            // We consider the case of redirecting output-input.
            
            else if(input_string[i]=='<' || input_string[i]=='>'){

                int k; // this k denotes the type of redirection, 
                // 0 is input stream,
                // 1 is output in write mode
                // 2 is output in append mode

                // we calculate the value of k.
                if(input_string[i]=='<'){
                    k=0;
                }
                else if(input_string[i]=='>'){
                    i++;
                    // we need to check next unit for knowing if its > or >>
                    if(input_string[i]=='>'){
                        k=2;
                        if(streams[1]!=NULL){ // can't have multiple output streams
                            print_error(); printf("multiple output streams not supported");
                            reset=1; continue;
                        }
                    } 
                    else {
                        k=1; i--;
                        if(streams[2]!=NULL){ // can't have multiple output streams
                            print_error(); printf("multiple output streams not supported");
                            reset=1; continue;
                        }
                    }
                }
                
                i++; // now we got the type of redirection, we move to getting the name/path of the file

                // we take in only one input-output stream
                if(streams[k]!=NULL){
                    print_error(); printf("multiple streams not supported."); 
                    reset=1; continue;
                }

                // Here we'll first mark the opening position, mark the position where 
                // name ends, ignoring spaces.
                while((input_string[i]==' ') && i<size-1) i++; // stop when you don't see spaces
                int pos_begin=i; //mark the beginning
                if(i>=size-1){ // error handling
                    print_error(); printf("Redirection stream not found\n");
                    continue;
                }
                while((input_string[i]!=' ') && i<size-1) i++; // stop when you see the next space
                i--;
                int pos_end=i; // mark the end
                
                int len = copy_string(&input_string, pos_begin, pos_end, &streams[k]);
                // this copy_string will take out the stream name, put it in desired variable.

                if(len<0){
                    reset=1; continue; // if some error with copying, we reset.
                }
            }

            //Finally, Command argument entered....
            //............................................
            else {
                if(args_full){ // print error if arg limit exceeded
                    print_error(); printf("Max argument lenght exceeded ");
                    break;
                }

                // mark the beginning and end of commands
                int pos_begin=i;
                while( i<size && (input_string[i]!=' ')) i++;
                if(i==size) i--;
                i--;
                int pos_end=i;

                // verify if char limit exceeded
                if(pos_end-pos_begin+1>MAX_CHARS){ 
                    reset=1;
                    print_error();
                    printf("Max character limit exceeded\n");
                    continue;
                }

                // copy the commands from input string to com_arg array 
                for(int j=0; j<pos_end-pos_begin+1; j++){
                    com_arg[hvPipes][command_no[hvPipes]][j]=input_string[pos_begin+j];
                }
                // end with null character
                com_arg[hvPipes][command_no[hvPipes]][pos_end-pos_begin+1]='\0';
                command_no[hvPipes]++; // keeping track of number of commands entered

                if(command_no[hvPipes]>=MAX_COMM) args_full=1;
            }

        }
        if(reset){
            // Reset is set to 1 when some error occurs during parsing.
            // So we free all the memory, and move to next iteration of the loop.
            
            free(input_string);
            for(int a=0; a<3; a++) free(streams[a]);
            for(int a=0; a<3; a++) streams[a]=NULL; // since these are global varibles, we need to 
            // set them to NULL for next iteration of the loop.

            for(int a=0; a<2; a++){
                for(int aa=0; aa<MAX_COMM; aa++){
                    free(com_arg[a][aa]);
                }
                free(com_arg[a]);
            }
            free(com_arg);

            continue;
        }

        // all the commands that will be passed to execvp 
        //  need to have a NULL pointer at the end, so we set that to null below.
        if(args_full==0){
            for(int i=0; i<2; i++){

            free(com_arg[i][command_no[i]]); // gotta free the memory first.
            com_arg[i][command_no[i]]=NULL;
            }
        }


        //.........................................
        //....Now we've done parsing the input.....
        //....Calling the Execute functions .......
        //.........................................

        int return_code; // when this is 0 we break the infinte while loop

        if(hvPipes==0) return_code=execute(com_arg[0]); // if no pipes
        else return_code=piped_execute(com_arg); // if there is a pipe


        //.........................................
        //......Freeeing memory...................

        free(input_string);

        for(int a=0; a<3; a++) free(streams[a]);
        for(int a=0; a<3; a++) streams[a]=NULL; // since these are global varibles, we need to 
        // set them to NULL for next iteration of the loop.

        for(int a=0; a<2; a++){
            for(int aa=0; aa<MAX_COMM; aa++){
                free(com_arg[a][aa]);
            }
            free(com_arg[a]);
        }
        free(com_arg);

        //...........................................................
        //.......Exiting the program.................................
        //... return_code being 0 is the code to exit the program ... 
        //...........................................................

        if(return_code==0){

            // First, delete the history file
            if(remove(history_file_loc)==0) printf("History file deleted successfully...\n");
            else{
                print_error(); printf("history file could not be deleted\n");
            }
            // Using break to exit the loop.
            printf("\nExiting the shell...\n");
            break;
        }
    }
    printf("\n");
}


/////////////// Main ends here.... ///////////////////////////////////
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
////////////// Implementing functions ////////////////////////////////



// To print a message at launch of the shell
void print_init(){
    printf("\n");
    printf(ANSI_COLOR_RED STYLE_BOLD "///////////////////////////////////////\n");
    printf("////  Welcome to this mini-shell.  ////\n");
    printf("///////////////////////////////////////\n" STYLE_RESET ANSI_COLOR_RESET);
}

// To print the current directory prompt.
void print_dir(){
    char cwd[PATH_MAX]; // this will contain path to current working directory
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf(ANSI_COLOR_RED STYLE_BOLD "\n\n[%s]>>$ " STYLE_RESET ANSI_COLOR_RESET, cwd);
    } 
    else {
        perror("getcwd"); // error handling
    }
    return;
}

/////////////////////////////////////////////////////////////////////

// Copy a part of a string to another.
/*  location string is dynamically allocated, and has the content of 
    data string from index start to index end. 
    @return -1 if some error, otherwise size of location string.
*/
int copy_string( char** data, int start, int end, char** location){
    // This function takes in the data string and positions of the areas to be copied,
    // and the location string where the data is to be pasted

    int size=end-start+1;
    if(size<0) return -1;

    // max lenght of any command argument can be MAX_CHARS
    if(size>MAX_CHARS){
        print_error();
        printf("Max character limit for arguments exceeded!\n");
        return -1;
    }

    size++;
    *location=(char*)malloc(size);
    for(int i=0; i<size-1; i++){
        (*location)[i]=(*data)[i+start];
    }
    (*location)[size-1]='\0'; // copied the data and ended with a NULL char.

    // we return the size of location string.
    return size;
}

//////////////////////////////////////////////////////////////////////
/////////////   implementing the internal functions.   ///////////////

/*  exit function.
    returns 0, which is code to exit the program.
*/
int internal_exit(){
    return 0; 
}

/*  Function to implement cd command.
    Changes directory to the specified path, 
    if no path given, changes to home directory.
*/
int internal_cd(char** coms){
    if (coms[1] == NULL) {
        //no argument, then we move to home directory.
        char* home_address= getenv("HOME"); // get the path name
        if(home_address==NULL){
            print_error(); printf("Can't get the home address\n");
            return 1;
        }
        if (chdir(home_address) != 0) perror("chdir"); 
        // chdir changes the current working directory.
    } 
    else {
        if (chdir(coms[1]) != 0) perror("chdir");
        // Called the chdir function which will change the directory, 
        // and if will print value of errno if there is some error.
    } 
    return 1;
}

///////////////////   HISTORY   /////////////////////////

/*  Function that prints history.
    Calls external_execute function with command cat and 
    argument being path location of history.txt
*/
int internal_history(void){

    char* history_com[3]={
        "cat",
        history_file_loc,
        NULL
    };
    return external_execute(history_com);
}

/*  
    Creates(if required) and opens the file
    specified by history_file_loc variable, 
    uses write mode to overwrite and thus delete any existing content.
*/
void create_history_file(void){
    
    FILE* history=fopen(history_file_loc, "w"); //open the file

    if(history==NULL){ // error handling
        perror("create_history_file"); return;
    }
    fprintf(history, "%s", ""); // overwrite an empty string 
    fclose(history); // close the file
}

/*  
    using append mode, prints command string 
    over on history file. 
*/
int add_to_history(char* command){

    FILE* history=fopen(history_file_loc, "a"); //open the file

    if(history==NULL) { // error handling 
        perror("add_to_history");
        return -1;
    }
    fprintf(history, "%3d : %s", command_counter, command); //print the commmand
    fclose(history); //close it
    return 1;
}

///////////// ENVIRONMENT VARS //////////////////////

/* Changes value of the environment variable specified by name_var
    to the value specified by value_var, 
    via calling setenv function from stdlib.h
*/
int process_setenv(char* name_var, char* value_var){

    int return_code= setenv(name_var, value_var, 1);
    // name_var specifies the name of env variable, and value_var 
    // specifies the value. The 1 at the end specifies we are overwriting it.
    if(return_code==0) return 1;
    else{
        perror("setenv");
        return 1;
    }
}

/* Prints the environement variable specified by name_var
    If name_var is a NULL string, we print all 4 environment varibales,
    via calling external_execute with echo command and required arguments.
*/
int process_printenv(char* name_var){

    // The print_com array contains the command that needs to be fed into 
    //  external_execute funciton, calling echo with value of required env var
    //  as its argument.

    // If only input it printenv, we print all 4 env vars.
    if(name_var==NULL){
        char* print_com[]={
            "echo",
            "-e", // so echo accepts backslash escaping
            "USER:",
            getenv("USER"),
            "\nHOME:",
            getenv("HOME"),
            "\nSHELL:",
            getenv("SHELL"),
            "\nTERM:",
            getenv("TERM"),
            NULL
        };
        return external_execute(print_com);
    }
    else {
        char* print_com[]={
            "echo",
            getenv(name_var),
            NULL
        };

        // if getenv returns NULL
        if(print_com[1]==NULL){ //error handling
            print_error(); printf("Environment Var not found\n");
            return 1;
        }
        return external_execute(print_com);
    }
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

// All the commands that are to be executed through external binaries
// will be executed from the below function.

int external_execute(char** coms)
{
    pid_t pid = fork(); 
    //This creates a child process which will run our program.

    if (pid == -1) {
        perror("forking"); //error handling if forking fails.
        exit(EXIT_FAILURE);
    } 
    else if (pid == 0) {
        // Chlid process

        // First we implement redirection.

        // If any of streams contains a non-null string,
        //  we change our input/output streams accordingly.
        if(streams[0]!=NULL){ 
            freopen(streams[0], "r", stdin);
        }
        if(streams[1]!=NULL){
            freopen(streams[1], "w", stdout);
        }
        if(streams[2]!=NULL){
            freopen(streams[2], "a", stdout);
        }
        /* NOTE: both streams[1] and streams[2] will not be opened simultaneously,
            as the parser would have already prompted an error message
            if they both contain a non-null string.
        */
        
        // Now, executing the command.
        if (execvp(coms[0], coms) < 0) {
            perror("external commands"); // error handling
        }
        exit(0);
    } 
    else {
        // waiting for child to terminate
        wait(NULL); 
        return 1;
    }
}

///////////////////////////////////////////////////////////////////////

/* Main calls this function,
    which passes the coms array to the corresponding function
    (internal or external) based on the command entered.
*/

int execute(char** coms){

    // if user entered null string, do nothing.
    if(coms[0]==NULL) return 1;

    // We list down the commands that will not be passed to execvp directly.
    char* internal_coms[]={
        "history",
        "cd",
        "exit",
        "quit",
        "x",
        "printenv",
        "setenv"
    };

    // Now, calling the required function.

    if(strcmp(coms[0], internal_coms[0])==0) return internal_history();
    else if(strcmp(coms[0], internal_coms[1])==0) return internal_cd(coms);
    else if(strcmp(coms[0], internal_coms[2])==0) return internal_exit();
    else if(strcmp(coms[0], internal_coms[3])==0) return internal_exit();
    else if(strcmp(coms[0], internal_coms[4])==0) return internal_exit();
    else if(strcmp(coms[0], internal_coms[5])==0) return process_printenv(coms[1]);
    else if(strcmp(coms[0], internal_coms[6])==0) return process_setenv(coms[1], coms[2]);
    else return external_execute(coms);
    // strcmp() funciton returns 0 when both its arguments are identical.
}

/////////////////////////////////////////////////////////

/* If main detects use of pipe in commands, this function is called. 

    The output of the command on the left of the pipe ( coms[0] ) 
    becomes the input of the command on the right of the pipe ( coms[1] ).
*/
int piped_execute(char*** coms)
{
    // We first create the 2 ends, one to write upon, other to read.
    int pipe_file[2];
    if(pipe(pipe_file)<0) { 
        perror("pipe-redirection"); //error handling
        return 1;
    }
    // Now, 0 is read end, 1 is write end
    pid_t p1, p2;
  
    p1 = fork();
    if (p1 < 0) {
        perror("pipe-fork"); // error handling
        return 1;
    }
    
    // CHILD PROCESS 1.
    //---------------------------------------------
    if (p1 == 0) {
        // duplicate output end of pipe into the output stream. 
        close(pipe_file[0]);
        dup2(pipe_file[1], STDOUT_FILENO);
        close(pipe_file[1]);

        // We first check if the command is history, 
        //  since its an internal command we need to handle it separately
        if(strcmp(coms[0][0], "history")==0){
            // we set the given command to output the history file.
            coms[0][0]="cat";
            coms[0][1]=history_file_loc;
            coms[0][2]=NULL;
        }
        // Now, we check if the command is printenv,
        //   for the same reasons as before, we handle it separately
        if(strcmp(coms[0][0], "printenv")==0){
            coms[0][0]="echo";

            // no argument- we print all 4 env vars.
            if(coms[0][1]==NULL){
                coms[0][1]="-e"; // so echo accepts backslash escaping
                coms[0][2]="USER:";
                coms[0][3]=getenv("USER");
                coms[0][4]="\nHOME:";
                coms[0][5]=getenv("HOME");
                coms[0][6]="\nSHELL:";
                coms[0][7]=getenv("SHELL");
                coms[0][8]="\nTERM:";
                coms[0][9]=getenv("TERM");
                coms[0][10]=NULL;
            }
            else{
                coms[0][1]=getenv(coms[0][1]);
                coms[0][2]=NULL;
            }
        }

        // We now implement redirection to some file, if requested.
        if(streams[0]!=NULL){
            freopen(streams[0], "r", stdin);
        }
        // since its the left of pipe process, we only need to 
        //   redirect input stream if requested.


        // executing the command...
        if (execvp(coms[0][0], coms[0]) < 0) {
            perror("pipe-execution process 1");
            exit(EXIT_FAILURE);
        }
        exit(0);
    } 
    
    // Left of the pipe completed, now we move to right of the pipe.

    p2 = fork();
    if (p2 < 0) {
        perror("pipe-fork"); // error handling
        return 1;
    }
  
    // CHILD PROCESS 2
    // -------------------------------------------------------
    if (p2 == 0) {
        // duplicate input end of the pipe into the input stream.
        close(pipe_file[1]);
        dup2(pipe_file[0], STDIN_FILENO);
        close(pipe_file[0]);

        // We first redirect output streams to some files, if requested.
        if(streams[1]!=NULL){
            freopen(streams[1], "w", stdout);
        }
        if(streams[2]!=NULL){
            freopen(streams[2], "a", stdout);
        }

        // Now executing the commands.
        if (execvp(coms[1][0],coms[1]) < 0) {
            perror("pipe-execution process 2"); // error handling
            exit(EXIT_FAILURE);
        }
        exit(0);
    } 

    // The parent process has still both ends of the pipe left open.
    // We first close that.
    close(pipe_file[0]);
    close(pipe_file[1]);

    // Waiting for child process to terminate
    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);
    return 1;
}
