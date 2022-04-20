#include <iostream>
#include <vector>
#include "parser.h"
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

int main()
{
    // get a single line from stdin
    int is_bundle_creation = 0;

    std::vector<std::pair<char *, std::vector<char **> *> > bundles;
    std::string bundle_name;
    bool is_executing = false;
    int tty_input = dup(0);
    std::cout << tty_input << std::endl;
    while (1)
    {
        char line[256];
        fgets(line, 256, stdin);
        if (is_executing)
        {
            // std::cout << "Executing" << std::endl;
            continue;
        }
        parsed_input *parsedInput = new parsed_input();
        parse(line, is_bundle_creation, parsedInput);

        // check if parsedinput->command.type is equal to command_type QUIT enum type
        if (parsedInput->command.type == QUIT)
        {
            exit(0);
        }

        // check if parsedinput->command.type is equal to command_type PROCESS_BUNDLE_START enum type
        else if (parsedInput->command.type == PROCESS_BUNDLE_CREATE)
        {
            std::pair<char *, std::vector<char **> *> bundle;
            is_bundle_creation = 1;
            bundle.first = parsedInput->command.bundle_name;
            // bundle_name = bundle.first;
            bundle_name = bundle.first;
            // std::cout << bundle_name << std::endl;
            bundle.second = new std::vector<char **>();
            bundles.push_back(bundle);
            // std::cout << bundle.first << std::endl;
        }

        else if (parsedInput->command.type == PROCESS_BUNDLE_STOP)
        {
            // std::cout << "bundle stop" << std::endl;
            is_bundle_creation = 0;
            bundle_name = "";
        }

        else if (parsedInput->command.type == PROCESS_BUNDLE_EXECUTION)
        {
            if (is_executing)
            {
                // std::cout << "Already executing a bundle" << std::endl;
                continue;
            }
            is_executing = true;
            // close(0);
            std::cout << parsedInput->command.bundle_count << std::endl;
            // for (int i = 0; i < parsedInput->command.bundle_count; i++)
            // {
            //     std::cout << parsedInput->command.bundles[i].name << std::endl;
            //     std::cout << (parsedInput->command.bundles[i].output == NULL) << std::endl;
            //     std::cout << parsedInput->command.bundles[i].input << std::endl;
            // }
            std::cout << "bundle execution" << std::endl;
            // continue;
            if (parsedInput->command.bundle_count == 1)
            {
                int size = bundles.size();
                for (int i = 0; i < size; i++) // searching for correct bundle
                {
                    std::string vector_bundle_name = bundles[i].first;
                    bundle_execution &execution_bundle = parsedInput->command.bundles[0]; // should update for multiple bundles
                    char *execution_bundle_out = execution_bundle.output;
                    char *execution_bundle_input = execution_bundle.input;
                    std::string bundle_name = execution_bundle.name;
                    if (vector_bundle_name != bundle_name)
                    {
                        continue;
                    }
                    // update for multiple bundles
                    int process_size = bundles[i].second->size();
                    std::vector<int> pids;
                    int stat;
                    for (int j = 0; j < process_size; j++)
                    {
                        int f_out = -1;
                        int f_in = -1;
                        if (execution_bundle_out != NULL)
                        {
                            f_out = open(execution_bundle_out, O_WRONLY | O_CREAT | O_APPEND, 0644);
                        }
                        int pid = fork();
                        if (pid == 0)
                        { // child
                            if (execution_bundle_out != NULL)
                            {
                                dup2(f_out, 1);
                            }
                            if (execution_bundle_input != NULL)
                            {
                                f_in = open(execution_bundle_input, O_RDONLY);
                                dup2(f_in, 0);
                            }
                            char **argument_list = bundles[i].second->at(j);
                            std::vector<char *> arguments;
                            for (int x = 0; argument_list[x] != NULL; x++)
                            {
                                arguments.push_back(argument_list[x]);
                            }
                            arguments.push_back(NULL);
                            // convert arguments vector to char**
                            char **argument_list_char = new char *[arguments.size()];
                            for (int i = 0; i < arguments.size(); i++)
                            {
                                argument_list_char[i] = arguments[i];
                            }
                            execvp(arguments[0], argument_list_char);
                        }
                        else if (pid < 0)
                        {
                            std::cout << "fork failed" << std::endl;
                            continue;
                        }
                        else
                        {
                            pids.push_back(pid);
                            // std::cout << "child process" << std::endl;
                        }
                    }

                    for (int j = 0; j < pids.size(); j++)
                    {
                        waitpid(pids[j], &stat, 0);
                        kill(pids[j], SIGKILL);
                    }

                    break;
                }
            }
            is_executing = false;
        }

        else
        {
            int size = bundles.size();
            for (int i = 0; i < size; i++)
            {
                if (bundle_name == bundles[i].first)
                {
                    bundles[i].second->push_back(parsedInput->argv);
                    break;
                }
            }
        }
    }
    return 0;
}