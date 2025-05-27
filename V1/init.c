/*
 * init.c - enhanced init system for xv6-riscv
 * Features:
 * 1. Starts services from init.conf during boot.
 * 2. Supports dependency-aware service ordering.
 * 3. Detects circular dependencies.
 */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"

#define MAX_SERVICES 32
#define MAX_DEPENDENCIES 8
#define MAX_NAME 32
#define MAX_LINE 128

struct service {
  char name[MAX_NAME];
  char path[MAX_NAME];
  char deps[MAX_DEPENDENCIES][MAX_NAME];
  int dep_count;
  int started;
  int pid;
};

struct service services[MAX_SERVICES];
int service_count = 0;

void
trim(char *s) {
  int i = 0, j = 0;
  // Skip leading whitespace
  while (s[i] == ' ' || s[i] == '\t') i++;
  while (s[i]) {
    if (s[i] != '\n' && s[i] != '\r')
      s[j++] = s[i];
    i++;
  }
  s[j] = 0;
}

int
parse_line(char *line, struct service *svc) {
  trim(line);
  if (line[0] == '#' || line[0] == '\0') return 0;

  char *tok = strtok(line, ":");
  if (!tok) return 0;
  safestrcpy(svc->name, tok, MAX_NAME);

  tok = strtok(0, ":");
  if (!tok) return 0;
  safestrcpy(svc->path, tok, MAX_NAME);

  svc->dep_count = 0;
  while ((tok = strtok(0, ",")) && svc->dep_count < MAX_DEPENDENCIES) {
    safestrcpy(svc->deps[svc->dep_count++], tok, MAX_NAME);
  }

  svc->started = 0;
  return 1;
}

int
find_service_idx(char *name) {
  for (int i = 0; i < service_count; i++) {
    if (strncmp(name, services[i].name, MAX_NAME) == 0)
      return i;
  }
  return -1;
}

int
has_circular_dependency_util(int idx, int *visited, int *stack) {
  if (!visited[idx]) {
    visited[idx] = stack[idx] = 1;
    for (int i = 0; i < services[idx].dep_count; i++) {
      int dep_idx = find_service_idx(services[idx].deps[i]);
      if (dep_idx < 0) continue;
      if (!visited[dep_idx] && has_circular_dependency_util(dep_idx, visited, stack))
        return 1;
      else if (stack[dep_idx])
        return 1;
    }
  }
  stack[idx] = 0;
  return 0;
}

int
has_circular_dependency() {
  int visited[MAX_SERVICES] = {0};
  int stack[MAX_SERVICES] = {0};
  for (int i = 0; i < service_count; i++) {
    if (has_circular_dependency_util(i, visited, stack)) return 1;
  }
  return 0;
}

void
topological_sort_util(int idx, int *visited, int *order, int *pos) {
  visited[idx] = 1;
  for (int i = 0; i < services[idx].dep_count; i++) {
    int dep_idx = find_service_idx(services[idx].deps[i]);
    if (dep_idx >= 0 && !visited[dep_idx])
      topological_sort_util(dep_idx, visited, order, pos);
  }
  order[(*pos)++] = idx;
}

void
topological_sort(int *order) {
  int visited[MAX_SERVICES] = {0};
  int pos = 0;
  for (int i = 0; i < service_count; i++) {
    if (!visited[i])
      topological_sort_util(i, visited, order, &pos);
  }
}

void
start_service(int idx) {
  int pid = fork();
  if (pid == 0) {
    char *argv[] = { services[idx].path, 0 };
    exec(services[idx].path, argv);
    exit(1);
  } else if (pid > 0) {
    services[idx].pid = pid;
    services[idx].started = 1;
    printf("[init] Started %s (PID %d)\n", services[idx].name, pid);
  } else {
    printf("[init] Failed to fork %s\n", services[idx].name);
  }
}

void
boot_services() {
  int fd = open("init.conf", O_RDONLY);
  if (fd < 0) {
    printf("[init] Could not open init.conf\n");
    return;
  }

  char buf[MAX_LINE];
  int n;
  while ((n = readline(fd, buf, sizeof(buf))) > 0 && service_count < MAX_SERVICES) {
    if (parse_line(buf, &services[service_count])) {
      service_count++;
    }
  }
  close(fd);

  if (has_circular_dependency()) {
    printf("[init] Error: Circular dependency detected.\n");
    exit(1);
  }

  int order[MAX_SERVICES];
  topological_sort(order);
  for (int i = service_count - 1; i >= 0; i--) {
    int idx = order[i];
    start_service(idx);
  }
}

int
main(void) {
  printf("[init] Starting system...\n");
  boot_services();

  // Keep init alive
  while (1) {
    wait(0);
  }
  return 0;
}
