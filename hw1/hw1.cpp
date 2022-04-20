#include <iostream>
#include <vector>
#include "parser.h"
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

#define BUFFER_SIZE 80

std::pair<char *, std::vector<char **> *> &findBundle(std::string &s, std::vector<std::pair<char *, std::vector<char **> *> > &bundles)
{
    for (int i = 0; i < bundles.size(); i++)
    {
        std::string name = bundles[i].first;
        if (name == s)
        {
            std::cout << "Found bundle: " << bundles[i].first << std::endl;
            return bundles[i];
        }
    }
    return bundles[0];
}

int main()
{
    // get a single line from stdin
    int is_bundle_creation = 0;

    std::vector<std::pair<char *, std::vector<char **> *> > bundles;
    std::string bundle_name;
    bool is_executing = false;
    // int tty_input = dup(0);
    // close(0);
    // dup2(0,0);
    // std::cout << tty_input << std::endl;
    while (1)
    {
        char line[256];
        fgets(line, 256, stdin);
        if (is_executing)
        {
            // std::cout << "Executing" << std::endl;
            continue;
        }
        // std::cout << "Parsing: " << is_executing << std::endl;
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
                std::cout << "Already executing a bundle" << std::endl;
                continue;
            }
            is_executing = true;
            int bundle_count = parsedInput->command.bundle_count;
            std::vector<std::pair<int, int> > pipe_ids;
            for (int y = 0; y < bundle_count; y++)
            {
                std::cout << "bundle_count: " << bundle_count << std::endl;
                std::cout << "bundle execution y:" << y << std::endl;
                bundle_execution &execution_bundle = parsedInput->command.bundles[y]; // should update for multiple bundles
                char *execution_bundle_out = execution_bundle.output;
                char *execution_bundle_input = execution_bundle.input;
                std::string bundle_name = execution_bundle.name;
                std::pair<char *, std::vector<char **> *> &vec_current_bundle = findBundle(bundle_name, bundles);
                int f_out = -1;
                int f_in = -1;
                std::cout << "bundle file test" << std::endl;
                if ((bundle_count > 1) && (y < bundle_count - 1))
                { // creating repeater, middle elements
                    std::cout << "creating repeater" << std::endl;
                    std::cout << "y: " << y << std::endl;
                    bundle_execution &next_bundle = parsedInput->command.bundles[y + 1];
                    std::string next_bundle_name_str = next_bundle.name;
                    std::pair<char *, std::vector<char **> *> &vec_next_bundle = findBundle(next_bundle_name_str, bundles);
                    // char *next_bundle_out = next_bundle.output; // daha sonra kullan
                    // char *next_bundle_input = next_bundle.input; // daha sonra kullan
                    int stat;
                    std::vector<std::pair<int, int> > pipe_ids_curToRepeater; // it is current pipes, not next. Wrong naming
                    std::vector<std::string> curr_bundle_outputs;
                    int currentBundleSize = vec_current_bundle.second->size();
                    int nextBundleSize = vec_next_bundle.second->size();
                    for (int i = 0; i < currentBundleSize; i++)
                    {
                        int pipe_id[2];
                        pipe(pipe_id);
                        pipe_ids_curToRepeater.push_back(std::make_pair(pipe_id[0], pipe_id[1]));
                    }
                    int pid = fork();
                    std::vector<int> pidList;

                    if (pid == 0)
                    { // Repeater process
                        std::vector<std::pair<int, int> > pipe_ids_repeaterToNext;
                        for (int i = 0; i < nextBundleSize; i++)
                        {
                            int pipe_id[2];
                            pipe(pipe_id);
                            pipe_ids_repeaterToNext.push_back(std::make_pair(pipe_id[0], pipe_id[1]));
                        }
                        std::vector<int> pidList3;
                        for (int k = 0; k < nextBundleSize; k++)
                        {
                            int pid = fork();
                            if (pid == 0)
                            {
                                for (int i = 0; i < nextBundleSize; i++)
                                {
                                    close(pipe_ids_repeaterToNext[i].second);
                                    if (i != k)
                                    {
                                        close(pipe_ids_repeaterToNext[i].first);
                                    }
                                    else
                                    {
                                        dup2(pipe_ids_repeaterToNext[i].first, 1);
                                    }
                                }

                                char **argument_list = vec_current_bundle.second->at(k);

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
                            else
                            {
                                pidList3.push_back(pid);
                                for (int i = 0; i < nextBundleSize; i++)
                                {
                                    close(pipe_ids_repeaterToNext[i].first);
                                }
                                for (int i = 0; i < currentBundleSize; i++)
                                {
                                    close(pipe_ids_curToRepeater[i].second);
                                }
                                for (int i = 0; i < currentBundleSize; i++)
                                {
                                    // read 256 bits from pipe
                                    curr_bundle_outputs[i] = "";
                                    char buffer[BUFFER_SIZE];
                                    int size = read(pipe_ids_curToRepeater[i].first, buffer, BUFFER_SIZE);

                                    while (1)
                                    {
                                        int size = read(pipe_ids_curToRepeater[i].first, buffer, BUFFER_SIZE);
                                        if (size != BUFFER_SIZE)
                                        {
                                            std::cout << "Error reading from pipe" << std::endl;
                                        }
                                        if (size > 0)
                                        {
                                            std::cout << "size: " << size << std::endl;
                                            for (int x = 0; x < nextBundleSize; x++)
                                            {
                                                int write_size = write(pipe_ids_repeaterToNext[x].second, buffer, BUFFER_SIZE);
                                                if (write_size != BUFFER_SIZE)
                                                {
                                                    curr_bundle_outputs[i].append(buffer);
                                                    std::cout << "Error writing to pipe" << std::endl;
                                                }
                                            }
                                        }
                                        if (size <= 0)
                                        {
                                            break;
                                        }
                                    }

                                    // write 256 bits to pipe
                                    // write(pipe_ids[i].second, buffer, 256);
                                }
                            }
                        }
                        for (int i = 0; i < pidList3.size(); i++)
                        {
                            waitpid(pidList3[i], &stat, 0);
                            kill(pidList3[i], SIGKILL);
                        }
                    }
                    else
                    {
                        // parent process
                        std::vector<int> pidList2;
                        pidList.push_back(pid);
                        for (int b = 0; b < currentBundleSize; b++)
                        {
                            int pid = fork();
                            if (pid == 0)
                            {
                                for (int i = 0; i < currentBundleSize; i++)
                                {
                                    close(pipe_ids_curToRepeater[i].first);
                                    if (i != b)
                                    {
                                        close(pipe_ids_curToRepeater[i].second);
                                    }
                                    else
                                    {
                                        dup2(pipe_ids_curToRepeater[i].second, 1);
                                    }
                                }

                                char **argument_list = vec_current_bundle.second->at(b);

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
                            else
                            {
                                for (int i = 0; i < currentBundleSize; i++)
                                {
                                    close(pipe_ids_curToRepeater[i].first);
                                    close(pipe_ids_curToRepeater[i].second);
                                }
                                pidList2.push_back(pid);
                            }
                        }

                        for (int i = 0; i < currentBundleSize; i++)
                        {
                            close(pipe_ids_curToRepeater[i].first);
                            close(pipe_ids_curToRepeater[i].second);
                        }

                        for (int i = 0; i < pidList2.size(); i++)
                        {
                            waitpid(pidList2[i], &stat, 0);
                            kill(pidList2[i], SIGKILL);
                        }
                    }

                    waitpid(pid, &stat, 0);
                    kill(pid, SIGKILL);
                    break;
                }
                // update for multiple bundles
                if (bundle_count == 1)
                {
                    std::cout << " below if y: " << y << std::endl;
                    int process_size = vec_current_bundle.second->size();
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
                            char **argument_list = vec_current_bundle.second->at(j);
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
                            // std::cout << "fork failed" << std::endl;
                            continue;
                        }
                        else
                        {
                            pids.push_back(pid);
                            // std::cout << "child process" << std::endl;
                        }
                    }

                    for (int p = 0; p < pids.size(); p++)
                    {
                        waitpid(pids[p], &stat, 0);
                        kill(pids[p], SIGKILL);
                    }
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