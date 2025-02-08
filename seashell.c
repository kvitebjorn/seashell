#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

// Return status codes
#define EMPTY_ARGS -3
#define EOF_REACHED -2
#define ERROR -1

// Magic numbers and stuff
#define MAX_LINE 1024
#define MAX_ARGS 64
#define DELIM " \t\r\n\a"

typedef struct
{
  char *name;
  char **args;
  char arg_count;
} Command;

ssize_t read_line(char *line);
int parse_line(char *line, Command *cmd);
int execute_command(Command *cmd);
void free_command(Command *cmd);

int main(void)
{
  int status = 1;

  // Ignore SIGINT (Ctrl+C) in the parent shell
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  sigaction(SIGINT, &sa, NULL);

  // Allocate memory for command structure
  Command cmd;
  cmd.args = malloc(MAX_ARGS * sizeof(char *));
  if (!cmd.args)
  {
    perror("Error allocating memory for command args.");
    exit(ERROR);
  }

  cmd.name = malloc(MAX_LINE * sizeof(char));
  if (!cmd.name)
  {
    perror("Error allocating memory for command name.");
    free(cmd.args);
    exit(ERROR);
  }

  while (status)
  {
    printf("seashell> ");

    char line[MAX_LINE];
    ssize_t len = read_line(line);
    if (len == EOF_REACHED)
    {
      printf("EOF reached.\n");
      break;
    }
    if (len == ERROR)
    {
      perror("Error reading input.");
      continue;
    }

    // Reset the command, just in case! Data integrity and all that...
    memset(cmd.args, 0, MAX_ARGS * sizeof(char *));
    memset(cmd.name, 0, MAX_LINE);
    cmd.arg_count = 0;

    // Parse the input into the Command structure
    int parse_status = parse_line(line, &cmd);
    if (parse_status == EMPTY_ARGS)
      continue;
    if (parse_status == ERROR)
    {
      fprintf(stderr, "Error parsing command.\n");
      continue;
    }

    // Handle built-in exit command
    if (strcmp(cmd.name, "exit") == 0)
      break;

    // Execute the command
    status = execute_command(&cmd);
  }

  // Cleanup
  free_command(&cmd);
  return 0;
}

/**
 * Reads a line of input from standard input.
 *
 * @param line A pointer to a fixed-size buffer where the read line will be stored.
 * @return The number of characters read, or a status code indicating an error or EOF.
 */
ssize_t read_line(char *line)
{
  if (!line)
    return ERROR;

  if (fgets(line, MAX_LINE, stdin) != NULL)
  {
    size_t len = strlen(line);

    // Check if the input was truncated and exceeds our buffer size
    if (len == MAX_LINE - 1 && line[len - 1] != '\n')
    {
      fprintf(stderr, "Input too long!\n");
      int ch;
      while ((ch = getchar()) != '\n' && ch != EOF)
        ;
      return ERROR;
    }
    return len;
  }
  return feof(stdin) ? EOF_REACHED : ERROR;
}

/**
 * Parses a given line of input and fills the provided Command structure.
 *
 * @param line A pointer to the input line to be parsed.
 * @param cmd A pointer to the Command to be filled with parsed data.
 * @return 1 if the command was successfully parsed, otherwise an appropriate status code.
 */
int parse_line(char *line, Command *cmd)
{
  if (!line || !cmd || !cmd->args || !cmd->name)
    return ERROR;

  int position = 0;
  char *token = strtok(line, DELIM);

  while (token)
  {
    cmd->args[position] = strdup(token); // Allocate memory for each argument
    if (!cmd->args[position])
    {
      perror("Error allocating memory for argument");
      return ERROR;
    }

    if (++position >= MAX_ARGS)
    {
      fprintf(stderr, "Too many arguments\n");
      return ERROR;
    }
    token = strtok(NULL, DELIM);
  }

  cmd->args[position] = NULL;
  if (position == 0)
    return EMPTY_ARGS;

  strncpy(cmd->name, cmd->args[0], MAX_LINE - 1);
  cmd->name[MAX_LINE - 1] = '\0';
  cmd->arg_count = position;

  return 1;
}

/**
 * Frees the memory allocated for a Command.
 *
 * @param cmd A pointer to the Command to be freed.
 */
void free_command(Command *cmd)
{
  if (!cmd)
    return;
  for (int i = 0; i < cmd->arg_count; i++)
  {
    free(cmd->args[i]);
  }
  free(cmd->args);
  free(cmd->name);
}

/**
 * Executes the given command.
 *
 * @param cmd A pointer to a Command that contains the details of the command to be executed.
 * @return 1 if the command was successfully executed, otherwise ERROR.
 */
int execute_command(Command *cmd)
{
  // Handle built-in `cd` command
  if (strcmp(cmd->name, "cd") == 0)
  {
    if (cmd->arg_count < 2)
      fprintf(stderr, "cd: missing argument\n");
    else if (chdir(cmd->args[1]) != 0)
      perror("cd");
    return 1;
  }

  // Create a child process to execute the given command
  pid_t pid = fork();
  if (pid == -1)
  {
    perror("fork");
    return ERROR;
  }

  if (pid == 0)
  {
    signal(SIGINT, SIG_DFL); // Restore default SIGINT behavior in child
    execvp(cmd->name, cmd->args);
    perror("execvp");
    exit(ERROR);
  }

  // Wait for the child process to finish executing
  int status;
  do
  {
    if (waitpid(pid, &status, WUNTRACED) == -1)
    {
      perror("waitpid");
      return ERROR;
    }
  } while (!WIFEXITED(status) && !WIFSIGNALED(status));

  return 1;
}
