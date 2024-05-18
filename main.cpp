#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <fstream>
using namespace std ;

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
        size_t end = result.find_first_of(" \t\n", pos+1);
        string var_name = result.substr(pos+1, (end == string::npos ? result.length() : end) - pos - 1);
        const char* var_value = getenv(var_name.c_str());
        if (var_value) {
            result.replace(pos, var_name.length() + 1, var_value);
        }
        pos += var_name.length() + (var_value ? strlen(var_value) : 0);
    }
    return result;
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
        std::cerr << "Failed to fork\n" << endl;
    }
}








int main() {
    string command_line;
    cout << "welcome in my mini-shell>\n";
    while (true) {
        cout << "louy-shell>";
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

        execute_command(args, run_in_background);
    }

    history_file.close();
    return 0;
}
