#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <fstream>
#include <fcntl.h>
#include <cstring>

using namespace std;

ofstream history_file("history.txt", ios::app);

vector<string> parse_command(const string& input) {
    istringstream iss(input);
    vector<string> args;
    string token;
    while (iss >> token) {
        args.push_back(token);
    }
    return args;
}

string expand_env_variables(const string& input) {
    string result = input;
    size_t pos = 0;

    while ((pos = result.find('$', pos)) != string::npos) {
        size_t end = result.find_first_of(" \t\n", pos + 1);
        string var_name = result.substr(pos + 1, (end == string::npos ? result.length() : end) - pos - 1);
        const char* var_value = getenv(var_name.c_str());
        if (var_value) {
            result.replace(pos, var_name.length() + 1, var_value);
        }
        pos += var_name.length() + (var_value ? strlen(var_value) : 0);
    }
    return result;
}

void executeCommand(vector<string>& args) {
    vector<char*> c_args(args.size() + 1);
    for (size_t i = 0; i < args.size(); ++i) {
        c_args[i] = const_cast<char*>(args[i].c_str());
    }
    c_args[args.size()] = nullptr;


    if (execvp(c_args[0], c_args.data()) == -1) {
        perror("execvp");
        exit(EXIT_FAILURE);
    }
}

void handleRedirection(vector<string>& args) {
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == ">") {
            int fd = open(args[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args.erase(args.begin() + i, args.begin() + i + 2);
            break;
        } else if (args[i] == "<") {
            int fd = open(args[i + 1].c_str(), O_RDONLY);
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            args.erase(args.begin() + i, args.begin() + i + 2);
            break;
        }
    }
}

void handlePipes(vector<string>& commands) {
    int numPipes = commands.size() - 1;
    int pipefds[2 * numPipes];

    for (int i = 0; i < numPipes; ++i) {
        if (pipe(pipefds + i * 2) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i <= numPipes; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            if (i != 0) {
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            }
            if (i != numPipes) {
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            }

            for (int j = 0; j < 2 * numPipes; ++j) {
                close(pipefds[j]);
            }

            vector<string> args = parse_command(commands[i]);
            handleRedirection(args);
            executeCommand(args);
        } else if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < 2 * numPipes; ++i) {
        close(pipefds[i]);
    }

    for (int i = 0; i <= numPipes; ++i) {
        wait(NULL);
    }
}

void execute_command(const vector<string>& args, bool run_in_background) {
    vector<char*> c_args(args.size() + 1);
    for (size_t i = 0; i < args.size(); ++i) {
        c_args[i] = const_cast<char*>(args[i].c_str());
    }
    c_args[args.size()] = nullptr;

    pid_t pid = fork();

    if (pid == 0) {
        execvp(c_args[0], c_args.data());
        exit(EXIT_FAILURE);
    } else if (pid > 0 && !run_in_background) {
        int status;
        waitpid(pid, &status, 0);
    } else if (pid < 0) {
        cerr << "Failed to fork" << endl;
    }
}

int main() {
    string command_line;
    cout << "welcome in my mini-shell>" << endl;
    while (true) {
        cout << "louy-shell> ";
        getline(cin, command_line);
        if (command_line == "exit") break;

        command_line = expand_env_variables(command_line);
        history_file << command_line << endl;

        vector<string> args = parse_command(command_line);
        if (args.empty()) continue;

        bool run_in_background = !args.empty() && args.back() == "&";
        if (run_in_background) {
            args.pop_back();
        }

        vector<string> commands;
        istringstream iss(command_line);
        string command;
        while (getline(iss, command, '|')) {
            commands.push_back(command);
        }

        if (commands.size() > 1) {
            handlePipes(commands);
        } else {
            handleRedirection(args);
            execute_command(args, run_in_background);
        }
    }

    history_file.close();
    return 0;
}
