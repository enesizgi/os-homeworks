#include <iostream>
#include <string>
#include "fat32.h"
#include "parser.h"

using namespace std;

void ls_command (parsed_input& input, FILE*& imgFile, BPB_struct& BPBstruct) {
    cout << input.type << endl;
}

int main(int argc, char *argv[]) {
    bool is_development = true; // TODO: Change this to false before submission
    if (argc != 2) {
        exit(1);
    }

    string filename("/home/enes/os-hw3/");
    filename.append(argv[1]);

    FILE* imgFile;
    imgFile = fopen(is_development ? filename.c_str() : argv[1],"r+");
    if (imgFile == nullptr) {
        fclose(imgFile);
        exit(1);
    }

    BPB_struct* BPBstruct = new BPB_struct;
    size_t result = fread(BPBstruct, sizeof(BPB_struct), 1, imgFile);
    if (result != 1) {
        exit(1);
    }

    while (true) {
        string line;
        std::getline(std::cin, line);

        if (line == "quit") {
            return 0;
        }

        auto* input = new parsed_input;
        char* input_chr = new char[line.length() + 1];
        strcpy(input_chr, line.c_str());
        parse(input, input_chr);

        if (input->type == LS) {
            ls_command(*input, imgFile, *BPBstruct);
        }

        clean_input(input);
        delete input;
        delete[] input_chr;
    }
    return 0;
}
