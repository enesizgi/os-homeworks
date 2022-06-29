#include <iostream>
#include <string>
#include <vector>
#include "fat32.h"
#include "parser.h"

using namespace std;

string resolve_path (parsed_input& input, string& current_working_dir) {
    return "";
}

string lfn_name_extract (FatFileEntry*& file_entry) {
    string result;
    for (auto& i : file_entry->lfn.name1) {
        if (i != 0) result += (char)i;
        else return result;
    }
    for (auto& i : file_entry->lfn.name2) {
        if (i != 0) result += (char)i;
        else return result;
    }
    for (auto& i : file_entry->lfn.name3) {
        if (i != 0) result += (char)i;
        else return result;
    }
    return result;
}

uint32_t get_fat_entry (FILE*& imgFile) {
    vector<uint8_t> fat_entry;
    auto* tmp = new uint8_t;
    for (int i = 0; i < 4; i++) {
        fread(tmp, sizeof(uint8_t), 1, imgFile);
        fat_entry.push_back(*tmp);
    }
    delete tmp;
    uint32_t tmp2 = (fat_entry[3]<<24) + (fat_entry[2]<<16) + (fat_entry[1]<<8) + fat_entry[0];
    return tmp2;
}

uint32_t get_fat_entry_at (FILE*& imgFile, uint32_t location) {
    fseek(imgFile, location, SEEK_SET);
    return get_fat_entry(imgFile);
}

vector<uint32_t>* find_entries (uint32_t& first_cluster, FILE*& imgFile, BPB_struct& BPBstruct, uint16_t& beginning_of_fat_table, uint32_t& end_of_chain) {
    fseek(imgFile, beginning_of_fat_table+8+(first_cluster - BPBstruct.extended.RootCluster)*4, SEEK_SET);
    auto* fat_entries_p = new vector<uint32_t>;
    auto& fat_entries = *fat_entries_p;
    fat_entries.push_back(BPBstruct.extended.RootCluster);
    auto* tmp_entry = new uint32_t;
    while (true) {
        fread(tmp_entry, sizeof(uint32_t), 1, imgFile);
        if (*tmp_entry == end_of_chain) break;
        else {
            fat_entries.push_back(*tmp_entry);
            fseek(imgFile, beginning_of_fat_table+4*(*tmp_entry), SEEK_SET);
        }
    }
    delete tmp_entry;
    return fat_entries_p;
}

void ls_command (parsed_input& input, FILE*& imgFile, BPB_struct& BPBstruct, string& current_working_dir) {
    string arg1 = input.arg1 != nullptr ? input.arg1 : "";
    string arg2 = input.arg2 != nullptr ? input.arg2 : "";
    uint16_t beginning_of_fat_table = BPBstruct.ReservedSectorCount*BPBstruct.BytesPerSector;
    uint32_t end_of_chain = get_fat_entry_at(imgFile, beginning_of_fat_table);
    auto* root_fat_entries_p = find_entries(BPBstruct.extended.RootCluster, imgFile, BPBstruct, beginning_of_fat_table, end_of_chain);
    auto& root_fat_entries = *root_fat_entries_p;
    auto bytes_per_cluster = BPBstruct.BytesPerSector*BPBstruct.SectorsPerCluster;
    auto beginning_of_clusters = (BPBstruct.ReservedSectorCount+BPBstruct.extended.FATSize*BPBstruct.NumFATs)*BPBstruct.BytesPerSector;
    int print_counter = 0;
//    for (int i = 0; i < root_fat_entries.size(); i++) {
    for (auto& root_fat_entry : root_fat_entries) {
        fseek(imgFile, beginning_of_clusters + (root_fat_entry-2)*bytes_per_cluster, SEEK_SET);
        int size_of_fatFileEntry = sizeof(FatFileEntry);
        auto* tmp_file = new FatFileEntry;
        string str1; // TODO: MAYBE SHOULD MOVE THIS TO A LEVEL UP OUTSIDE THIS FOR LOOP.
        for (int j = 0; j < bytes_per_cluster; j+=size_of_fatFileEntry) {
            fread(tmp_file, size_of_fatFileEntry, 1, imgFile);
            if (tmp_file->lfn.attributes == 15) { // It is LFN.
                str1.insert(0, lfn_name_extract(tmp_file));
            }
            else {
                if (str1.length() > 0) {
                    if (arg1 == "-l") {
                        if ((tmp_file->msdos.attributes & 0x10) == 0x10) { // It is a directory.
                            cout << "drwx------ 1 root root 0 " << "<last_modified_date_and_time>" << " " << str1;
                            if (j != bytes_per_cluster - size_of_fatFileEntry) cout << endl;
                        }
                        else {
                            cout << "-rwx------ 1 root root " << tmp_file->msdos.fileSize << " " << "<last_modified_date_and_time>" << " " << str1;
                            if (j != bytes_per_cluster - size_of_fatFileEntry) cout << endl;
                        }
                    }
                    else {
                        cout << str1;
                        if (j != bytes_per_cluster - size_of_fatFileEntry) cout << " ";
                    }
                    print_counter++;
                    str1.clear();
                }
            }
        }
        delete tmp_file;
    }
    if (print_counter > 0 && arg1 != "-l") cout << endl;
    root_fat_entries.clear();
    delete root_fat_entries_p;
}

int main(int argc, char *argv[]) {
    string current_working_dir = "/";
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

    auto* BPBstruct = new BPB_struct;
    size_t result = fread(BPBstruct, sizeof(BPB_struct), 1, imgFile);
    if (result != 1) {
        exit(1);
    }

    while (true) {
        cout << current_working_dir << "> ";
        string line;
        std::getline(std::cin, line);

        if (line == "quit") {
            return 0;
        }

        if (line.length() == 0) continue;

        auto* input = new parsed_input;
        char* input_chr = new char[line.size() + 1];
        strcpy(input_chr, line.c_str());
        parse(input, input_chr);

        if (input->type == LS) {
            ls_command(*input, imgFile, *BPBstruct, current_working_dir);
        }
        else if (input->type == CD) {
            current_working_dir = resolve_path(*input, current_working_dir);
//            cd_command(*input, imgFile, *BPBstruct, current_working_dir);
        }

        clean_input(input);
        delete input;
        delete[] input_chr;
    }
}
