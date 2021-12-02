# MiniShell

This is a mini shell made in C, with linux like syntax. 
Note: This program creates a history file in /temp directory.


## Basic Syntax

In a single command, there can be a total of atmost 50 arguments with atmost 250 letters.
Strings entered in input (e.g. echo "hello world") are treated as a single argument, but 
backslash escaping (e.g. \\ , \" , etc.) is NOT supported. 

Spaces separate commands, but the exact number of them doesn't matter, 
unless they are part of a string. 
example: below three commands will get you exact same output.
    `echo hello world`
    `echo  hello  world`
    `echo "hello world"`

---

## Commands supported:

Any commands whose binaries are avaiable can be execute with this program. 
The following lists the internal commands of this shell-

- cd - changes current directory. If no input given, then it changes to home directory.
    Syntax is same as for bash, but ~ is not recognized.

- history - prints list of commands with their 1-based index in order in which they were executed 
    in the current run of the shell. history file is created at the start of the shell and 
    deleted when user exits the shell with exit command.

- exit / quit / x - (any one of these three) exits the shell. 

- printenv and setenv are explained in the last section of this readme file.

---

## Piping and Redirection

Only single level piping supported, and atmost one input (<) and 
one output (> or >>) redirection supported in a single command.
Pipes can be used along with redirection. 

###  Syntax for pipes: 
Space separate the pipe character (|) on both sides.
Other than this, syntax is similar to bash. 

###  Syntax for redirection:
space separate them, necesarily on left side,but not needed on right side.

The exact position of redirections does not matter, so wherever 
there is < character in the command, the word next to it will be marked
as input stream for the given command.
Similarly for output redirections too (> or >>). 
The only exception when < or > or >> is a part of a string.
e.g. echo " hello < world"
This will NOT mark "world" as the input file, just print the content of string.

### When pipes and redirection are used together:
input redirection (<) will become input of the first command (on the left of pipe)
and output redirection (> or >> ) will become output stream of the 
second command (on the rigth of the pipe).

### example commands:
Both commands below are equivalent.

```
   cat <input.txt | grep 'h' >> input.txt

   cat | grep 'h' <input.txt >>input.txt
```
Here cat taked input from input.txt, then its output is fed to grep, which then appends input.txt duplicating the lines that contains 'h' character.

---

##  Environment Variables.

### printenv
To get value of some environment variable, use command printenv followed by name of variable, and if no name entered, then the shell prints values of USER, HOME, SHELL, and TERM variables.

example commands:
```
printenv HOME
printenv
```
NOTE: using echo to print environment variables (e.g. echo $HOME)
    is NOT supported by this shell. User has to use printenv for that.

### setenv
To change values of any environment variables, use command setenv
followed by the name of variable whose value is to be changed, then
the new value, space separated.

example commands:
```
setenv HOME "/home"
```
( then use printenv HOME to verify the value is changed.)
