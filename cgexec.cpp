#include <cerrno>
#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <linux/sched.h>
#include <sched.h>
#include <string>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#define CGROUP_PATH "/sys/fs/cgroup"

pid_t child_pid = -1;

// Wrapper for clone3 syscall
static int clone3(struct clone_args *args, size_t size) {
  return syscall(SYS_clone3, args, size);
}

void parent_signal_handler(int sig) {
  if (child_pid > 0) {
    std::cerr << "cgexec forwarding signal " << sig << " to child " << child_pid
              << std::endl;
    kill(child_pid, sig);
  }
}

bool execute_with_clone_into_cgroup(int cgroup_fd,
                                    const std::vector<std::string> &command) {
  struct clone_args args = {
      .flags = CLONE_INTO_CGROUP, // The magic flag
      .exit_signal = SIGCHLD,     // Signal to send to parent on exit
      .cgroup = static_cast<__u64>(cgroup_fd), // File descriptor of the cgroup
  };

  child_pid = clone3(&args, sizeof(args));
  if (child_pid == -1) {
    perror("clone3 failed");
    return false;
  }

  if (child_pid == 0) { // Child process
    // Prepare arguments for execvp
    std::vector<char *> argv;
    for (const auto &arg : command) {
      argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);

    execvp(argv[0], argv.data());

    // If we get here, execvp failed
    perror("execvp failed");
    _exit(EXIT_FAILURE);
  } else { // Parent process
    // Set up signal forwarding
    struct sigaction sa{};
		sa.sa_handler = parent_signal_handler;
		sa.sa_flags = SA_RESTART;
    for (int sig = 1; sig < NSIG; ++sig) {
      if (sig == SIGCHLD || sig == SIGKILL || sig == SIGSTOP)
        continue;
      sigaction(sig, &sa, NULL);
    }
    int status;
    waitpid(child_pid, &status, 0);
    return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
  }
}

// Function to create a new cgroup (if it doesn't exist)
bool ensure_cgroup_exists(const std::string &cgroup_path) {
  struct stat st;
  if (stat(cgroup_path.c_str(), &st) == -1) {
    if (mkdir(cgroup_path.c_str(), 0755) == -1) {
      if (errno == EEXIST) {
        // indicate others have created the cgroup, concurrently.
        std::cerr << "Warn: others just created the cgroup " << cgroup_path
                  << ", concurrently" << std::endl;
        return true;
      }
      std::cerr << "Failed to create cgroup directory: " << cgroup_path
                << std::endl;
      return false;
    }
  }
  return true;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <cgroup_path> <command> [args...]\n";
    return EXIT_FAILURE;
  }

  // Open the cgroup directory
  std::string cgroup_path = std::string() + CGROUP_PATH + "/" + argv[1];
  if (!ensure_cgroup_exists(cgroup_path)) {
    _exit(EXIT_FAILURE);
  }
  int cgroup_fd = open(cgroup_path.c_str(), O_RDONLY | O_DIRECTORY);
  if (cgroup_fd == -1) {
    perror("Failed to open cgroup directory");
    return EXIT_FAILURE;
  }

  // Prepare command vector
  std::vector<std::string> command;
  for (int i = 2; i < argc; ++i) {
    command.push_back(argv[i]);
  }

  // Execute in cgroup
  if (!execute_with_clone_into_cgroup(cgroup_fd, command)) {
    close(cgroup_fd);
    return EXIT_FAILURE;
  }

  close(cgroup_fd);
  return EXIT_SUCCESS;
}
