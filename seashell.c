#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// Return status codes
#define EMPTY_ARGS -3
#define EOF_REACHED -2
#define ERROR -1

// Control flow status codes
#define RED 0
#define GREEN 1

// Magic numbers and stuff
#define MAX_LINE 1024
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
  int status = GREEN;

  Command cmd;
  cmd.args = malloc(MAX_LINE * sizeof(char *));
  if (!cmd.args)
  {
    perror("Error allocating memory for the command args.");
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
    switch (len)
    {
    case EOF_REACHED:
      printf("EOF reached.\n");
      status = RED;
      break;
    case ERROR:
      perror("Error reading input.");
      status = RED;
      break;
    default:
      status = GREEN;
      break;
    }

    if (status != GREEN)
    {
      // Early exit
      break;
    }

    // Reset the command, just in case! Data integrity and all that...
    memset(cmd.args, 0, MAX_LINE * sizeof(char *));
    memset(cmd.name, 0, MAX_LINE * sizeof(char));
    cmd.arg_count = 0;

    // Fill the Command structure
    int parse_status = parse_line(line, &cmd);

    switch (parse_status)
    {
    case GREEN:
      // Execute the Command, fulfilling our duty as a shell!
      int execute_status = execute_command(&cmd);
      if (execute_status == ERROR)
      {
        fprintf(stderr, "Error executing command: %s\n", cmd.name);
        status = RED;
      }
      else if (execute_status == RED)
      {
        status = RED;
      }
      else
      {
        status = GREEN;
      }
      break;
    case ERROR:
      // Print an error message and continue the loop
      fprintf(stderr, "Error parsing command.\n");
    case EMPTY_ARGS:
      // No command entered, just continue the loop
    default:
      status = GREEN;
      break;
    }
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
ssize_t
read_line(char *line)
{
  if (!line)
  {
    // Invalid argument
    return ERROR;
  }

  if (fgets(line, MAX_LINE, stdin) != NULL)
  {
    size_t len = strlen(line);

    // Check if the input was truncated and exceeds our buffer size
    if (len == MAX_LINE - 1 && line[len - 1] != '\n')
    {
      // Clear the remaining input from stdin
      int ch;
      while ((ch = getchar()) != '\n' && ch != EOF)
        ;

      // Return an error code to indicate the input was too long
      return ERROR;
    }

    // Success! Return the number of characters read
    return len;
  }

  if (feof(stdin))
  {
    // End of file reached (Ctrl-D)
    return EOF_REACHED;
  }

  // fgets failed
  return ERROR;
}

/**
 * @brief Parses a given line of input and fills the provided Command.
 *
 * This function splits the input line into tokens and stores them in the Command.
 * The first token is assumed to be the command name, and the rest are its arguments.
 *
 * No memory shall be allocated here, so the Command structure must be pre-allocated.
 *
 * @param line A pointer to the input line to be parsed.
 * @param cmd A pointer to the Command to be filled with parsed data.
 * @return A status code indicating success or failure.
 */
int parse_line(char *line, Command *cmd)
{
  if (!line || !cmd || !cmd->args || !cmd->name)
  {
    // Invalid arguments
    return ERROR;
  }

  int position = 0;
  char *token = strtok(line, DELIM);

  // Tokenize the input line and store the tokens in the command
  while (token != NULL)
  {
    cmd->args[position++] = token;

    if (position >= MAX_LINE)
    {
      fprintf(stderr, "Too many arguments\n");
      return ERROR;
    }

    token = strtok(NULL, DELIM);
  }

  // Null-terminate the arguments array
  cmd->args[position] = NULL;

  // Check for no arguments
  if (position == 0)
  {
    return EMPTY_ARGS;
  }

  // Copy the command name and argument count
  strncpy(cmd->name, cmd->args[0], MAX_LINE - 1);
  cmd->name[MAX_LINE - 1] = '\0';
  cmd->arg_count = position;

  return GREEN;
}

/**
 * @brief Frees the memory allocated for a Command.
 *
 * This function releases all resources associated with the given Command,
 * including any dynamically allocated memory within the structure.
 *
 * @param cmd A pointer to the Command to be freed.
 */
void free_command(Command *cmd)
{
  free(cmd->name);
  free(cmd->args);
}

/**
 * @brief Executes the given command.
 *
 * This function takes a pointer to a Command structure and executes the
 * specified command. It creates a child process to run the command and
 * waits for the child to finish before returning.
 *
 * @param cmd A pointer to a Command that contains the details
 *            of the command to be executed.
 * @return A status code indicating the success or failure of the command.
 */
int execute_command(Command *cmd)
{
  pid_t pid;

  if (strcmp(cmd->name, "exit") == 0)
    return RED; // Exit our shell, as requested

  // Create a child process to execute the given command
  pid = fork();
  if (pid == -1)
  {
    perror("fork");
    return ERROR;
  }

  // We are in the child process
  if (pid == 0)
  {
    execvp(cmd->name, cmd->args);

    // If `execvp` returns, it must have failed
    perror("execvp");
    exit(ERROR);
  }

  // We are back in the parent process.
  // Wait for the child to finish executing.
  int status;
  do
  {
    if (waitpid(pid, &status, WUNTRACED) == -1)
    {
      perror("waitpid");
      return ERROR;
    }
  } while (!WIFEXITED(status) && !WIFSIGNALED(status));

  return GREEN;
}
