#include "kernel/types.h"      // Basic type definitions for xv6
#include "kernel/stat.h"       // File status definitions
#include "user/user.h"         // User space system call wrappers
#include "kernel/fcntl.h"      // File control options for open()

// ----------- CONFIGURABLE LIMITS AND CONSTANTS -------------
#define MAX_SERVICES 32          // Maximum number of distinct services supported
#define MAX_DEPENDENCIES 8       // Maximum number of dependencies for a service
#define MAX_NAME 32              // Maximum length for a service name
#define MAX_LINE 128             // Maximum length for a line in the config file
#define MAX_CMDS 32              // Maximum number of standalone shell commands in config
#define MAX_CMD_ARGS 8           // Maximum allowed command-line arguments per service

// ----------- STRUCTURE DEFINITIONS -------------------------

// Structure representing a service definition from the config file
struct service {
  char name[MAX_NAME];                          // The unique name of the service (e.g., "S1")
  char command[MAX_LINE];                       // The command to execute for this service
  char deps[MAX_DEPENDENCIES][MAX_NAME];        // Names of services this service depends on
  int dep_count;                                // Number of dependencies this service has
  int started;                                  // Set to 1 if the service has been started
  int pid;                                      // Process ID of the service's running process
};

// Structure for commands in the config file that are not services
struct shellcmd {
  char line[MAX_LINE];                          // Full command line as a string
};

// Arrays to store all parsed services and shell commands
struct service services[MAX_SERVICES];
int service_count = 0;                          // Actual number of services parsed

struct shellcmd shellcmds[MAX_CMDS];
int shellcmd_count = 0;                         // Actual number of shell commands parsed

// ----------- UTILITY FUNCTIONS -----------------------------

// Trims leading/trailing whitespace and removes newline chars from a string
void trim(char *s) {
  int i = 0, j = 0;
  // Skip leading spaces/tabs
  while (s[i] == ' ' || s[i] == '\t') i++;
  // Copy over the buffer, omitting newlines and carriage returns
  while (s[i]) {
    if (s[i] != '\n' && s[i] != '\r')
      s[j++] = s[i];
    i++;
  }
  s[j] = 0;  // Null-terminate the string
}

// Parses a config file line into a service struct if possible
// Returns 1 if the line is a service definition, 0 for other lines (e.g., comments or commands)
int parse_line(char *line, struct service *svc) {
  trim(line);                                  // Clean up whitespace, newlines
  if (line[0] == '#' || line[0] == '\0') return 0; // Skip comment/empty lines

  // Find ':' separator for dependencies
  char *colon = strchr(line, ':');
  if (!colon) return 0;                        // Not a service definition if missing

  // Find '|' separator for command
  char *pipe = strchr(line, '|');
  if (!pipe) return 0;                         // Not a service definition if missing

  // --- Parse service name (left of ':') ---
  *colon = '\0';                               // Temporarily split string at ':'
  safestrcpy(svc->name, line, MAX_NAME);       // Copy name to struct
  char *deps_start = colon + 1;                // Dependencies start after ':'

  // --- Parse dependencies (between ':' and '|') ---
  *pipe = '\0';                                // Temporarily split string at '|'
  svc->dep_count = 0;                          // Reset dependency count
  char *deps = deps_start;
  trim(deps);
  if (deps[0] != '\0') {
    // Tokenize dependencies by spaces
    char *tok = deps;
    while (*tok && svc->dep_count < MAX_DEPENDENCIES) {
      while (*tok == ' ') tok++;
      if (*tok == '\0') break;
      char *end = tok;
      while (*end && *end != ' ') end++;
      char tmp = *end;
      *end = '\0';
      safestrcpy(svc->deps[svc->dep_count++], tok, MAX_NAME); // Add each dependency
      *end = tmp;
      tok = end;
    }
  }

  // --- Parse command (after '|') ---
  char *cmd = pipe + 1;
  trim(cmd);                                   // Remove whitespace/newlines
  safestrcpy(svc->command, cmd, MAX_LINE);     // Save command

  svc->started = 0;                            // Not started yet
  svc->pid = -1;                               // Not running yet

  return 1;                                    // Successfully parsed a service
}

// Reads a line from a file descriptor into a buffer (like fgets)
// Returns number of chars read, zero at EOF
int readline(int fd, char *buf, int max) {
  int i = 0;
  char c;
  while(i + 1 < max) {
    int n = read(fd, &c, 1);                   // Read one character at a time
    if (n < 1) break;                          // EOF or error
    buf[i++] = c;
    if (c == '\n' || c == '\r') break;         // End of line
  }
  buf[i] = '\0';                               // Null-terminate
  return i;
}

// Find the index of a service by name in the services[] array
int find_service_idx(char *name) {
  for (int i = 0; i < service_count; i++) {
    if (strncmp(name, services[i].name, MAX_NAME) == 0)
      return i;
  }
  return -1;                                   // Not found
}

// ----------- CIRCULAR DEPENDENCY DETECTION -----------------

// Recursive helper for cycle detection: returns 1 if a cycle is found
// Uses Depth First Search (DFS) to explore the dependency graph.
// visited[]: array marks services already checked to avoid redundant visits.
// stack[]:   array marks services currently in the recursion stack (current path).
// If we revisit a node already on the stack, there is a cycle.
int has_circular_dependency_util(int idx, int *visited, int *stack) {
  if (!visited[idx]) {
    visited[idx] = stack[idx] = 1;             // Mark as visited and on the stack (DFS)
    for (int i = 0; i < services[idx].dep_count; i++) {
      // This is implicit: for each service, svc->deps[] holds the names of dependencies.
      // The functions that use this to build a graph are:
      int dep_idx = find_service_idx(services[idx].deps[i]);
      if (dep_idx < 0) continue;               // Skip missing dependencies
      // Recursively check dependencies
      if (!visited[dep_idx] && has_circular_dependency_util(dep_idx, visited, stack))
        return 1;                              // Found new cycle deeper in graph
      else if (stack[dep_idx])
        return 1;                              // Found back-edge: cycle exists
    }
  }
  stack[idx] = 0;                              // Backtrack: remove from stack
  return 0;
}

// Checks if any circular dependencies exist among all services
// For each service, starts a DFS to look for cycles using has_circular_dependency_util().
// Returns 1 if a cycle exists, 0 otherwise.
int has_circular_dependency() {
  int visited[MAX_SERVICES] = {0};
  int stack[MAX_SERVICES] = {0};
  for (int i = 0; i < service_count; i++) {
    if (has_circular_dependency_util(i, visited, stack)) return 1;
  }
  return 0;
}

// ----------- TOPOLOGICAL SORT (ORDERING SERVICES BY DEPENDENCY) -----------

// Utility for topological sort using DFS
// visited[]: tracks services already sorted
// order[]:   filled with sorted indices; services with no dependencies go first
// pos:       pointer to current position in order[]
void topological_sort_util(int idx, int *visited, int *order, int *pos) {
  visited[idx] = 1;
  // Visit all dependencies before this service
  for (int i = 0; i < services[idx].dep_count; i++) {
    int dep_idx = find_service_idx(services[idx].deps[i]);
    if (dep_idx >= 0 && !visited[dep_idx])
      topological_sort_util(dep_idx, visited, order, pos);
  }
  order[(*pos)++] = idx;                       // Add service after its dependencies
}

// Computes dependency-respecting order of services using topological sort
// Populates the provided order[] array with service indices
// The result is that each service appears after all its dependencies.
void topological_sort(int *order) {
  int visited[MAX_SERVICES] = {0};
  int pos = 0;
  for (int i = 0; i < service_count; i++) {
    if (!visited[i])
      topological_sort_util(i, visited, order, &pos);
  }
}
// Tokenizes a command line into argv array for exec()
// Returns the number of arguments (argc)
int tokenize_cmd(char *cmd, char *argv[MAX_CMD_ARGS]) {
  int argc = 0;
  char *p = cmd;
  while (*p && argc < MAX_CMD_ARGS - 1) {
    while (*p == ' ') p++;                      // Skip spaces
    if (*p == 0) break;
    argv[argc++] = p;                           // Start of argument
    while (*p && *p != ' ') p++;                // Find end of argument
    if (*p) {
      *p = 0;                                   // Null-terminate argument
      p++;
    }
  }
  argv[argc] = 0;                               // Null at end for exec
  return argc;
}

// Forks and starts a service in a child process, executes its command
// Records the PID in the service struct and prints status
void start_service(int idx) {
  int pid = fork();
  if (pid == 0) {
    // --- Child process --- //
    char *argv[MAX_CMD_ARGS];
    char cmd_copy[MAX_LINE];
    safestrcpy(cmd_copy, services[idx].command, MAX_LINE);
    tokenize_cmd(cmd_copy, argv);               // Build argv array

    if (argv[0] == 0) exit(0);                  // Empty command, do nothing
    exec(argv[0], argv);                        // Replace with service program
    printf("[init] exec %s failed\n", argv[0]); // Should not reach here
    exit(1);
  } else if (pid > 0) {
    // --- Parent process --- //
    services[idx].pid = pid;                    // Save child pid
    services[idx].started = 1;                  // Mark as started
    printf("[init] Started %s (PID %d)\n", services[idx].name, pid);
  } else {
    // Fork failure
    printf("[init] Failed to fork %s\n", services[idx].name);
  }
}

// Runs all shell commands parsed from the conf file (not tied to services)
void run_shellcmds() {
  for (int c = 0; c < shellcmd_count; c++) {
    char *line = shellcmds[c].line;
    if (line[0] == '#' || line[0] == '\0')
      continue;                                // Skip comments/blank lines

    int pid = fork();
    if (pid < 0) {
      printf("init: fork failed\n");
      continue;
    }
    if (pid == 0) {
      // Child: tokenize and exec the command
      char *argv[MAX_CMD_ARGS];
      char linecopy[MAX_LINE];
      safestrcpy(linecopy, line, MAX_LINE);
      tokenize_cmd(linecopy, argv);

      exec(argv[0], argv);                      // Execute the shell command
      printf("init: exec %s failed\n", argv[0]);
      exit(1);
    } else {
      // Parent: wait for this shell command to finish before next
      wait(0);
    }
  }
}

// Reads and parses init.conf, launches all services and shell commands in order
void boot_services_and_commands() {
  int fd = open("init.conf", O_RDONLY);         // Open configuration file
  if (fd < 0) {
    printf("[init] Could not open init.conf\n");
    return;
  }

  char buf[MAX_LINE];
  // Parse the config file line by line
  while (readline(fd, buf, sizeof(buf)) > 0) {
    if (buf[0] == '#' || buf[0] == '\0')
      continue;                                // Skip comments and blanks
    struct service svc;
    if (parse_line(buf, &svc)) {
      // Add parsed service to array
      if (service_count < MAX_SERVICES)
        services[service_count++] = svc;
    } else {
      // Otherwise treat as a shell command
      if (shellcmd_count < MAX_CMDS)
        safestrcpy(shellcmds[shellcmd_count++].line, buf, MAX_LINE);
    }
  }
  close(fd);                                   // Close config file

  // Check and report error if there are any circular dependencies
  if (has_circular_dependency()) {
    printf("[init] Error: Circular dependency detected.\n");
    exit(1);
  }
  int order[MAX_SERVICES];
  topological_sort(order);                     // Determine run order

  // Start each service in dependency order, waiting for each to finish
  for (int i = 0; i < service_count; i++) {
    int idx = order[i];
    start_service(idx);
    int wpid;
    // Wait for the specific child process (service) to finish before next
    while ((wpid = wait(0)) != services[idx].pid && wpid != -1)
      ;
  }

  // After all services, run extra shell commands (if any)
  run_shellcmds();
}

// Static argv for launching interactive shell
char *argvsh[] = { "sh", 0 };

// Main: performs system setup, runs services, then launches a shell forever
int main(void) {
  printf("[init] Starting system...\n");

  // Prepare the console device for input/output if not present
  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 0);                    // Create console if missing
    open("console", O_RDWR);
  }
  dup(0);  // Duplicate stdin to stdout
  dup(0);  // Duplicate stdin to stderr

  // --- Start all services and shell commands from config file ---
  boot_services_and_commands();

  // --- Keep init process alive: launch an interactive shell in a loop ---
  for(;;) {
    printf("init: starting sh\n");
    int pid = fork();
    if(pid < 0){
      printf("init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      exec("sh", argvsh);                      // Replace with shell
      printf("init: exec sh failed\n");
      exit(1);
    }
    while(wait(0) != pid)
      ;                                        // Wait for shell process to exit
  }
}
