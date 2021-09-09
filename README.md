# minish 

Minish is a miniature bash shell written in C. 

![minish basics](https://i.imgur.com/ckzMacl.gif)

## How to Compile 'main.c' for minish

Run the following line to properly compile the program:

> gcc --std=gnu99 -o minish main.c

## Running minish

Simply enter the following:

> ./minish

## Using minish

Minish handles most basic commands and can also run background processes. Ctrl-Z, or SIGSTP, will enter foreground-only mode, meaning background processes using the & symbol will be ignored. Only entering Ctrl-Z will exit foreground-only mode. 

## Exiting minish

Because minish does not accept Ctrl-C, or SIGINT, as a command to exit, the user merely needs to enter **"exit"** to escape from the minish shell.
