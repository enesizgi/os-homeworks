#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <algorithm>
#include "fat32.h"
#include "parser.h"

using namespace std;

uint32_t current_dir_cluster_no;
uint32_t beginning_of_clusters;
uint32_t bytes_per_cluster;
uint16_t beginning_of_fat_table;
uint32_t end_of_chain;

string months_str[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

uint32_t get_fat_entry_at (FILE*& imgFile, uint32_t location);
vector<uint32_t>* find_entries (uint32_t& first_cluster, FILE*& imgFile, BPB_struct& BPBstruct);
string lfn_name_extract (FatFileEntry*& file_entry);

vector<FatFileEntry> get_fatFileEntries_in_cluster(uint32_t cluster_no, FILE*& imgFile, BPB_struct& BPBstruct) {
    fseek(imgFile, beginning_of_clusters + (cluster_no - BPBstruct.extended.RootCluster) * bytes_per_cluster, SEEK_SET);
    int size_of_fatFileEntry = sizeof(FatFileEntry);
    vector<FatFileEntry> FatFileEntries;
    for (int j = 0; j < bytes_per_cluster; j += size_of_fatFileEntry) {
        auto *tmp_file = new FatFileEntry;
        fread(tmp_file, size_of_fatFileEntry, 1, imgFile);
        FatFileEntries.push_back(*tmp_file);
        delete tmp_file;
    }
    return FatFileEntries;
}

bool is_path_valid (vector<string>& path, FILE*& imgFile, BPB_struct& BPBstruct, bool is_file = false, FatFileEntry* file_entry_83 = nullptr, bool shouldClusterNoChange = true) {
    if (path.empty()) return false;

    uint32_t nextCluster = BPBstruct.extended.RootCluster;
    size_t pathSize = path.size();
    for (int i = 0; i < pathSize - 1; i++) {
        auto* root_fat_entries_p = find_entries(nextCluster, imgFile, BPBstruct);
        auto& root_fat_entries = *root_fat_entries_p;
        bool is_folder_found = false;
        string FatFileEntryName;
        bool is_deleted = false;
        for (unsigned int root_fat_entrie : root_fat_entries) {
            auto FatFileEntries = get_fatFileEntries_in_cluster(root_fat_entrie, imgFile, BPBstruct);
            for (auto & FatFileEntrie : FatFileEntries) {
                if (FatFileEntrie.lfn.attributes == 15) {
                    FatFileEntry* tmp = &FatFileEntrie;
                    FatFileEntryName.insert(0, lfn_name_extract(tmp));
                    if (FatFileEntrie.lfn.sequence_number == 0xE5) is_deleted = true;
                }
                else {
                    if (pathSize - 2 == i && is_file && path[i+1] == FatFileEntryName) { // It is a file.
                        is_folder_found = true;
                        nextCluster = FatFileEntrie.msdos.firstCluster;
                        if (file_entry_83 != nullptr) {
                            *file_entry_83 = FatFileEntrie;
                        }
                    }
                    else if ((FatFileEntrie.msdos.attributes & 0x10) == 0x10 && path[i+1] == FatFileEntryName) { // It is a directory.
                        is_folder_found = true;
                        nextCluster = FatFileEntrie.msdos.firstCluster;
                    }
                    is_deleted = false;
                    FatFileEntryName.clear();
                }
            }
            if (is_folder_found) break;
        }
        delete root_fat_entries_p;
        if (!is_folder_found) return false;
    }
    if (shouldClusterNoChange) current_dir_cluster_no = nextCluster;
    return true;
}

void split_path(vector<string>& result, string& path, string& current_working_dir) {
    string s = path.substr(0,1) == "/" ? path + "/" : current_working_dir + (current_working_dir != "/" ? "/" : "") + path + "/";
    s = path == "/" ? path : s;
    std::string delimiter = "/";

    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        result.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
}

void clean_path (vector<string>& clean_result, vector<string>& dirty_path) {
    for (auto & i : dirty_path) {
        if (i.empty()) {
            clean_result.push_back(i);
        }
        else if (i == ".") {
            continue;
        }
        else if (i == "..") {
            // TODO: MAYBE CHECK HERE IF CLEAN_RESULT IS EMPTY (IT SHOULDN'T BE EMPTY EVER)
            string tmp = clean_result.back();
            if (tmp.empty()) {
                continue;
            }
            clean_result.pop_back();
        }
        else clean_result.push_back(i);
    }
}

void concat_string (string& str, vector<string>& strings) {
    for (int i = 0; i < strings.size(); i++) {
        if (i == strings.size() - 1 && strings.size() != 1) {
            str += strings[i];
        }
        else str += strings[i] + "/";
    }
}

string resolve_path (string& path, string& current_working_dir, FILE*& imgFile, BPB_struct& BPBstruct) {
    vector<string> result;
    split_path(result, path, current_working_dir);
    if (result.empty()) {
        return current_working_dir;
    }

    vector<string> clean_result;
    clean_path(clean_result, result);

    if (!is_path_valid(clean_result, imgFile, BPBstruct)) {
        return current_working_dir;
    }

    string s_tmp;
    concat_string(s_tmp, clean_result);
    return s_tmp;
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

vector<uint32_t>* find_entries (uint32_t& first_cluster, FILE*& imgFile, BPB_struct& BPBstruct) {
    fseek(imgFile, beginning_of_fat_table+8+(first_cluster - BPBstruct.extended.RootCluster)*4, SEEK_SET);
    auto* fat_entries_p = new vector<uint32_t>;
    auto& fat_entries = *fat_entries_p;
    fat_entries.push_back(first_cluster);
    auto* tmp_entry = new uint32_t; // TODO: NEED TO TEST WITH CHAIN FOLDERS. THIS IS PROBABLY WRONG BECAUSE OF LITTLE ENDIAN. SHOULD USE GET_FAT_ENTRY
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

uint32_t find_last_entry_no (uint32_t& first_cluster, FILE*& imgFile, BPB_struct& BPBstruct) {
    fseek(imgFile, beginning_of_fat_table+8+(first_cluster - BPBstruct.extended.RootCluster)*4, SEEK_SET);
    uint32_t last_entry_no = first_cluster;
    uint32_t tmp_entry; // TODO: NEED TO TEST WITH CHAIN FOLDERS. THIS IS PROBABLY WRONG BECAUSE OF LITTLE ENDIAN. SHOULD USE GET_FAT_ENTRY
    while (true) {
        fread(&tmp_entry, sizeof(uint32_t), 1, imgFile);
        if (tmp_entry == end_of_chain) break;
        else {
            last_entry_no = tmp_entry;
            fseek(imgFile, beginning_of_fat_table+4*tmp_entry, SEEK_SET);
        }
    }
    return last_entry_no;
}

uint32_t find_free_entry (FILE*& imgFile, BPB_struct& BPBstruct) {
    fseek(imgFile, beginning_of_fat_table+8, SEEK_SET);
    uint32_t cluster_no = BPBstruct.extended.RootCluster;
    while (true) {
        uint32_t entry = get_fat_entry(imgFile);
        if (entry == 0) {
            fseek(imgFile, -4, SEEK_CUR);
            fwrite(&end_of_chain, 4, 1, imgFile);
            fseek(imgFile, beginning_of_fat_table+BPBstruct.extended.FATSize*BPBstruct.BytesPerSector + 8+(cluster_no - BPBstruct.extended.RootCluster)*4, SEEK_SET);
            fwrite(&end_of_chain, 4, 1, imgFile);
            return cluster_no;
        };
        cluster_no++;
    }
}

uint32_t add_free_entry (uint32_t& first_cluster, FILE*& imgFile, BPB_struct& BPBstruct) {
    auto* root_fat_entries_p = find_entries(first_cluster, imgFile, BPBstruct);
    auto& root_fat_entries = *root_fat_entries_p;

    uint32_t tmp_cluster_no = root_fat_entries.back();
    fseek(imgFile, beginning_of_fat_table+8+(root_fat_entries.back() - BPBstruct.extended.RootCluster)*4, SEEK_SET);
    uint32_t new_cluster_no = find_free_entry(imgFile, BPBstruct);
    fwrite(&new_cluster_no, 4, 1, imgFile);

    fseek(imgFile, beginning_of_fat_table+8+(new_cluster_no - BPBstruct.extended.RootCluster)*4, SEEK_SET);
    fwrite(&end_of_chain, 4, 1, imgFile);

    return new_cluster_no;
}

void modify_time(FatFileEntry& msdos) {
    time_t now = time(nullptr);
    tm *ltm = localtime(&now);

    msdos.msdos.creationTime = msdos.msdos.modifiedTime = (ltm->tm_hour << 11) + (ltm->tm_min << 5);
    msdos.msdos.creationDate = msdos.msdos.modifiedDate = msdos.msdos.lastAccessTime = ((ltm->tm_year - 80) << 9) + (ltm->tm_mon << 5) + ltm->tm_mday;
}

unsigned char lfn_checksum(const unsigned char *pFCBName)
{
    int i;
    unsigned char sum = 0;

    for (i = 11; i; i--)
        sum = ((sum & 1) << 7) + (sum >> 1) + *pFCBName++;

    return sum;
}

vector<FatFileEntry> create_lfns (string& filename) {
    string tmp_string(filename);
    vector<FatFileEntry> lfns;
    const auto* chr_filename = reinterpret_cast<const unsigned char *>(filename.c_str());
    auto checksum = lfn_checksum(chr_filename);

    uint8_t seq_number = 0;
    while (!tmp_string.empty()) {
        seq_number++;
        FatFileEntry lfn;
        lfn.lfn.attributes = 15;
        lfn.lfn.firstCluster = 0;
        lfn.lfn.reserved = 0;
        lfn.lfn.checksum = checksum;
        for (int i = 0; i < 5; i++) {
            lfn.lfn.name1[i] = 65535;
        }
        for (int i = 0; i < 6; i++) {
            lfn.lfn.name2[i] = 65535;
        }
        for (int i = 0; i < 2; i++) {
            lfn.lfn.name3[i] = 65535;
        }
        for (int i = 0; i < 5; i++) {
            if (tmp_string.empty()) {
                lfn.lfn.name1[i] = 0;
                lfn.lfn.sequence_number = 0x40 + seq_number;
                lfns.push_back(lfn);
                return lfns;
            }
            lfn.lfn.name1[i] = *tmp_string.substr(0,1).c_str();
            tmp_string.erase(0,1);
        }

        for (int i = 0; i < 6; i++) {
            if (tmp_string.empty()) {
                lfn.lfn.name2[i] = 0;
                lfn.lfn.sequence_number = 0x40 + seq_number;
                lfns.push_back(lfn);
                return lfns;
            }
            lfn.lfn.name2[i] = *tmp_string.substr(0,1).c_str();
            tmp_string.erase(0,1);
        }

        for (int i = 0; i < 2; i++) {
            if (tmp_string.empty()) {
                lfn.lfn.name3[i] = 0;
                lfn.lfn.sequence_number = 0x40 + seq_number;
                lfns.push_back(lfn);
                return lfns;
            }
            lfn.lfn.name3[i] = *tmp_string.substr(0,1).c_str();
            tmp_string.erase(0,1);
        }
        lfn.lfn.sequence_number = seq_number;
        lfns.push_back(lfn);
    }
    return lfns;
}

FatFileEntry create_msdos (FatFileEntry& previous_msdos, bool is_file = true, uint16_t free_entry = 0) {
    FatFileEntry f;
    f.msdos.firstCluster = is_file ? 0 : free_entry;
    f.msdos.fileSize = f.msdos.eaIndex = f.msdos.creationTimeMs = f.msdos.reserved = 0;
    f.msdos.attributes = is_file ? 32 : 16;

    modify_time(f);

    for (unsigned char & i : f.msdos.extension) i = 32;

    f.msdos.filename[0] = 126;
    for (int i = 1; i < 8; i++) {
        f.msdos.filename[i] = 32;
    }

    uint8_t index = 0;
    string index_str;
    for (int i = 1; i < 8; i++) {
        if (previous_msdos.msdos.filename[i] == 32) break;
        else {
            char tmp_char = previous_msdos.msdos.filename[i];
            string tmp_string;
            tmp_string.push_back(tmp_char);
            index_str.append(tmp_string);
        }
    }

    index = index_str.empty() ? 0 : stoul(index_str, nullptr);
    index++;
    index_str = to_string(index);
    int size = index_str.size();
    for (int i = 1; i < 8; i++) {
        if (index_str.empty()) break;
        f.msdos.filename[i] = *index_str.substr(0,1).c_str();
        index_str.erase(0,1);
    }

    return f;
}

void cd_command (string& path, FILE*& imgFile, BPB_struct& BPBstruct, string& current_working_dir) {
    if (path.empty()) return;
    current_working_dir = resolve_path(path, current_working_dir, imgFile, BPBstruct);
}

auto ls_command (parsed_input& input, FILE*& imgFile, BPB_struct& BPBstruct, string& current_working_dir) {
    // TODO: Should LS handle this: ls -l dir2file.c or ls file.c ? Right now, this is not printing anything.
    string arg1 = input.arg1 != nullptr ? input.arg1 : "";
    string arg2 = input.arg2 != nullptr ? input.arg2 : "";
    string backup_current_working_dir = current_working_dir;
    bool is_cd_used = false;
    vector<pair<string, vector<FatFileEntry> > > entries;

    if (!arg1.empty() && arg1 != "-l") {
        vector<string> result;
        split_path(result, arg1, current_working_dir);
        if (result.empty()) {
            return entries;
        }

        vector<string> clean_result;
        clean_path(clean_result, result);

        if (!is_path_valid(clean_result, imgFile, BPBstruct)) {
            return entries;
        }
        cd_command(arg1, imgFile, BPBstruct, current_working_dir);
        is_cd_used = true;
    }
    else if (arg1 == "-l" && !arg2.empty()) {
        vector<string> result;
        split_path(result, arg2, current_working_dir);
        if (result.empty()) {
            return entries;
        }

        vector<string> clean_result;
        clean_path(clean_result, result);

        if (!is_path_valid(clean_result, imgFile, BPBstruct)) {
            return entries;
        }

        cd_command(arg2, imgFile, BPBstruct, current_working_dir);
        is_cd_used = true;
    }

    auto* root_fat_entries_p = find_entries(current_dir_cluster_no, imgFile, BPBstruct);
    auto& root_fat_entries = *root_fat_entries_p;
    int print_counter = 0;
    string str1; // TODO: MAYBE SHOULD MOVE THIS TO A LEVEL UP OUTSIDE THIS FOR LOOP. (DONE)
    bool is_deleted = false;

    pair<string, vector<FatFileEntry> > entry;
    for (auto& root_fat_entry : root_fat_entries) {
        fseek(imgFile, beginning_of_clusters + (root_fat_entry - BPBstruct.extended.RootCluster)*bytes_per_cluster, SEEK_SET); // TODO: CHECK IF ROOTCLUSTER EDIT WORKS
        int size_of_fatFileEntry = sizeof(FatFileEntry);
        auto* tmp_file = new FatFileEntry; // TODO: Change this to stack allocation.
        for (int j = 0; j < bytes_per_cluster; j+=size_of_fatFileEntry) {
            fread(tmp_file, size_of_fatFileEntry, 1, imgFile);
            if (tmp_file->msdos.filename[0] == 0x2E) continue;
            else if (tmp_file->lfn.attributes == 15) { // It is LFN.
                entry.second.push_back(*tmp_file);
                str1.insert(0, lfn_name_extract(tmp_file));
                if (tmp_file->lfn.sequence_number == 0xE5) {
                    is_deleted = true;
                }
            }
            else {
                if (is_deleted) {
                    str1.clear();
                    is_deleted = false;
                    entry.first.clear();
                    entry.second.clear();
                }
                else if (str1.length() > 0) {
                    if (arg1 == "-l") {
                        uint8_t hours = (tmp_file->msdos.modifiedTime & 0xf800) >> 11;
                        uint8_t minutes = (tmp_file->msdos.modifiedTime & 0x7e0) >> 5;
                        string month = months_str[(tmp_file->msdos.modifiedDate & 0x1e0) >> 5];
                        uint8_t day = tmp_file->msdos.modifiedDate & 0x1f;
                        if ((tmp_file->msdos.attributes & 0x10) == 0x10) { // It is a directory.
                            if (j != 0 && print_counter != 0) cout << endl;
                            cout << "drwx------ 1 root root 0 " << month << " " << to_string(day) << (hours < 10 ? " 0" : " ");
                            cout << to_string(hours) << ":" << (minutes < 10 ? "0" : "") << to_string(minutes) << " " << str1;
                        }
                        else {
                            if (j != 0 && print_counter != 0) cout << endl;
                            cout << "-rwx------ 1 root root " << tmp_file->msdos.fileSize << " " << month << " " << to_string(day) << (hours < 10 ? " 0" : " ");
                            cout << to_string(hours) << ":" << (minutes < 10 ? "0" : "") << to_string(minutes) << " " << str1;
                        }
                    }
                    else {
                        if (j != 0 && print_counter != 0) cout << " ";
                        cout << str1;
                    }
                    entry.first = str1;
                    entry.second.push_back(*tmp_file);
                    entries.push_back(entry);
                    entry.first.clear();
                    entry.second.clear();
                    print_counter++;
                    str1.clear();
                }
            }
        }
        delete tmp_file;
    }
    if (print_counter > 0) cout << endl;
    root_fat_entries.clear();
    delete root_fat_entries_p;

    if (is_cd_used) {
        cd_command(backup_current_working_dir, imgFile, BPBstruct, current_working_dir);
    }

    return entries;
}

void cat_command(parsed_input& input, FILE*& imgFile, BPB_struct& BPBstruct, string& current_working_dir) {
    if (input.arg1 == nullptr) return;
    string path(input.arg1);

    vector<string> result;
    split_path(result, path, current_working_dir);
    if (result.empty()) {
        return;
    }

    vector<string> clean_result;
    clean_path(clean_result, result);

    FatFileEntry file_entry_83;
    if (!is_path_valid(clean_result, imgFile, BPBstruct, true, &file_entry_83, false)) {
        return;
    }

    uint32_t first_entry = file_entry_83.msdos.firstCluster;
    auto* fat_entries_p = find_entries(first_entry, imgFile, BPBstruct);
    auto& fat_entries = *fat_entries_p;

    for (int i = 0; i < fat_entries.size(); i++) {
        char content[1024] = {0};
        fseek(imgFile, beginning_of_clusters + (fat_entries[i] - BPBstruct.extended.RootCluster) * bytes_per_cluster, SEEK_SET);
        fread(content, sizeof(content), 1, imgFile);
        for (int j = 0; j < 1024; j++) if (content[j] != 0) cout << content[j];
    }
    cout << endl;
}

void touch_command(parsed_input& input, FILE*& imgFile, BPB_struct& BPBstruct, string& current_working_dir, vector<FatFileEntry>* ready_file = nullptr) {
    if (input.arg1 == nullptr) return;
    string path(input.arg1);

    vector<string> result;
    split_path(result, path, current_working_dir);
    if (result.empty()) {
        return;
    }

    vector<string> clean_result;
    clean_path(clean_result, result);

    if (is_path_valid(clean_result, imgFile, BPBstruct, true, nullptr, false)) return; // Checks if file exists, it shouldn't
    if (is_path_valid(clean_result, imgFile, BPBstruct, false, nullptr, false)) return; // Checks if file exists, it shouldn't

    string filename = clean_result.back();
    clean_result.pop_back();
    if (!is_path_valid(clean_result, imgFile, BPBstruct, false, nullptr, false)) return; // Checks if directory exists, it should

    string backup_current_working_dir = current_working_dir;
    auto tmp_arg1 = input.arg1;
    string cd_directory;
    concat_string(cd_directory, clean_result);
    cd_command(cd_directory, imgFile, BPBstruct, current_working_dir);

    auto* fat_entries_p = find_entries(current_dir_cluster_no, imgFile, BPBstruct);
    auto& fat_entries = *fat_entries_p;


    int size_of_fatFileEntry = sizeof(FatFileEntry);
    FatFileEntry tmp_file;
    bool is_lfn = false;

    vector<FatFileEntry> file_entries = ready_file == nullptr ? create_lfns(filename) : *ready_file;
    FatFileEntry msdos;
    FatFileEntry previous_msdos;
    for (int i = 0; i < 8; i++) // TODO: For . and .. entries. It could be unnecessary in touch. Check this in mkdir.
        previous_msdos.msdos.filename[i] = 32;

    // TODO: Should we check for deleted files?
    for (int i = 0; i < fat_entries.size(); i++) {
        if (file_entries.empty()) break;
        fseek(imgFile, beginning_of_clusters + (fat_entries[i] - BPBstruct.extended.RootCluster) * bytes_per_cluster, SEEK_SET);
        for (int j = 0; j < bytes_per_cluster; j+=size_of_fatFileEntry) {
            if (file_entries.empty()) break;
            fread(&tmp_file, size_of_fatFileEntry, 1, imgFile);
            if (tmp_file.msdos.filename[0] == 0x2E) continue;
            else if (tmp_file.lfn.attributes == 15) {
                is_lfn = true;
            }
            else {
                if (is_lfn) {
                    is_lfn = false;
                    previous_msdos = tmp_file;
                }
                else { // We found the empty space for our file.
                    fseek(imgFile, -size_of_fatFileEntry, SEEK_CUR);
                    msdos = create_msdos(previous_msdos);
                    if (ready_file != nullptr) {
                        uint8_t tmp_filename[8];
                        for (int k = 0; k < 8; k++)
                            tmp_filename[k] = msdos.msdos.filename[k];
                        msdos = file_entries.back();
                        for (int k = 0; k < 8; k++)
                            msdos.msdos.filename[k] = tmp_filename[k];
                        file_entries.pop_back();
                        std::reverse(file_entries.begin(), file_entries.end());
                    }
                    file_entries.insert(file_entries.begin(), msdos);
                    for (int k = 0; k < (bytes_per_cluster - j) / size_of_fatFileEntry; k++) {
                        if (file_entries.empty()) break;
                        fwrite(&file_entries.back(), size_of_fatFileEntry, 1, imgFile);
                        file_entries.pop_back();
                    }
                }
            }
        }
    }

    while (!file_entries.empty()) {
        uint32_t new_cluster_no = add_free_entry(current_dir_cluster_no, imgFile, BPBstruct);

        fseek(imgFile, beginning_of_clusters + (new_cluster_no - BPBstruct.extended.RootCluster) * bytes_per_cluster, SEEK_SET);
        for (int j = 0; j < bytes_per_cluster; j+=size_of_fatFileEntry) {
            if (file_entries.empty()) break;
            fwrite(&file_entries.back(), size_of_fatFileEntry, 1, imgFile);
            file_entries.pop_back();
        }
    }

    cd_command(backup_current_working_dir, imgFile, BPBstruct, current_working_dir);

}

FatFileEntry create_dot_entry(uint16_t first_cluster) {
    FatFileEntry f;
    f.msdos.firstCluster = first_cluster;
    f.msdos.fileSize = f.msdos.eaIndex = f.msdos.creationTimeMs = f.msdos.reserved = 0;
    f.msdos.attributes = 16;

    modify_time(f);

    for (unsigned char & i : f.msdos.extension) i = 32;

    f.msdos.filename[0] = 0x2E;
    for (int i = 1; i < 8; i++) {
        f.msdos.filename[i] = 32;
    }

    return f;
}

FatFileEntry create_double_dot_entry(uint16_t first_cluster) {
    FatFileEntry f;
    f.msdos.firstCluster = first_cluster;
    f.msdos.fileSize = f.msdos.eaIndex = f.msdos.creationTimeMs = f.msdos.reserved = 0;
    f.msdos.attributes = 16;

    modify_time(f);

    for (unsigned char & i : f.msdos.extension) i = 32;

    f.msdos.filename[0] = 0x2E;
    f.msdos.filename[1] = 0x2E;
    for (int i = 2; i < 8; i++) {
        f.msdos.filename[i] = 32;
    }

    return f;
}

void mkdir_command(parsed_input& input, FILE*& imgFile, BPB_struct& BPBstruct, string& current_working_dir) {
    if (input.arg1 == nullptr) return;
    string path(input.arg1);

    vector<string> result;
    split_path(result, path, current_working_dir);
    if (result.empty()) {
        return;
    }

    vector<string> clean_result;
    clean_path(clean_result, result);

    if (is_path_valid(clean_result, imgFile, BPBstruct, true, nullptr, false)) return; // Checks if file exists, it shouldn't
    if (is_path_valid(clean_result, imgFile, BPBstruct, false, nullptr, false)) return; // Checks if file exists, it shouldn't

    string filename = clean_result.back();
    clean_result.pop_back();
    if (!is_path_valid(clean_result, imgFile, BPBstruct, false, nullptr, false)) return; // Checks if directory exists, it should

    string backup_current_working_dir = current_working_dir;
    auto tmp_arg1 = input.arg1;
    string cd_directory;
    concat_string(cd_directory, clean_result);
    cd_command(cd_directory, imgFile, BPBstruct, current_working_dir);

    auto* fat_entries_p = find_entries(current_dir_cluster_no, imgFile, BPBstruct);
    auto& fat_entries = *fat_entries_p;

    int size_of_fatFileEntry = sizeof(FatFileEntry);
    FatFileEntry tmp_file;
    bool is_lfn = false;

    vector<FatFileEntry> file_entries = create_lfns(filename);
    FatFileEntry msdos;
    FatFileEntry previous_msdos;
    for (int i = 0; i < 8; i++) // TODO: For . and .. entries. It could be unnecessary in touch. Check this in mkdir.
        previous_msdos.msdos.filename[i] = 32;

    // TODO: Should we check for deleted files?
    uint32_t free_cluster;
    for (int i = 0; i < fat_entries.size(); i++) {
        if (file_entries.empty()) break;
        fseek(imgFile, beginning_of_clusters + (fat_entries[i] - BPBstruct.extended.RootCluster) * bytes_per_cluster, SEEK_SET);
        for (int j = 0; j < bytes_per_cluster; j+=size_of_fatFileEntry) {
            if (file_entries.empty()) break;
            fread(&tmp_file, size_of_fatFileEntry, 1, imgFile);
            if (tmp_file.msdos.filename[0] == 0x2E) continue;
            else if (tmp_file.lfn.attributes == 15) {
                is_lfn = true;
            }
            else {
                if (is_lfn) {
                    is_lfn = false;
                    previous_msdos = tmp_file;
                }
                else { // We found the empty space for our file.
                    fseek(imgFile, -size_of_fatFileEntry, SEEK_CUR);
                    free_cluster = find_free_entry(imgFile, BPBstruct);
                    msdos = create_msdos(previous_msdos, false, free_cluster);
                    file_entries.insert(file_entries.begin(), msdos);
                    fseek(imgFile, beginning_of_clusters + (fat_entries[i] - BPBstruct.extended.RootCluster) * bytes_per_cluster + j, SEEK_SET);
                    for (int k = 0; k < (bytes_per_cluster - j) / size_of_fatFileEntry; k++) {
                        if (file_entries.empty()) break;
                        fwrite(&file_entries.back(), size_of_fatFileEntry, 1, imgFile);
                        file_entries.pop_back();
                    }
                }
            }
        }
    }

    while (!file_entries.empty()) {
        uint32_t new_cluster_no = add_free_entry(current_dir_cluster_no, imgFile, BPBstruct);

        fseek(imgFile, beginning_of_clusters + (new_cluster_no - BPBstruct.extended.RootCluster) * bytes_per_cluster, SEEK_SET);
        for (int j = 0; j < bytes_per_cluster; j+=size_of_fatFileEntry) {
            if (file_entries.empty()) break;
            fwrite(&file_entries.back(), size_of_fatFileEntry, 1, imgFile);
            file_entries.pop_back();
        }
    }

    // TODO: There is a bug here which corrupts the FAT Table. I didn't quite understand it. Fseek not going to location.
    fseek(imgFile, beginning_of_clusters + (free_cluster - BPBstruct.extended.RootCluster) * bytes_per_cluster, SEEK_SET);
    FatFileEntry dot_entry = create_dot_entry(free_cluster);
    fwrite(&dot_entry, sizeof(dot_entry), 1, imgFile);
    FatFileEntry double_dot_entry = create_double_dot_entry(current_dir_cluster_no);
    fwrite(&double_dot_entry, sizeof(double_dot_entry), 1, imgFile);
    cd_command(backup_current_working_dir, imgFile, BPBstruct, current_working_dir);

}

void mv_command(parsed_input& input, FILE*& imgFile, BPB_struct& BPBstruct, string& current_working_dir) {
    if (input.arg1 == nullptr || input.arg2 == nullptr) return;
    string arg1(input.arg1);
    string arg2(input.arg2);

    vector<string> result_1, result_2;
    vector<string> clean_result_1, clean_result_2;
    split_path(result_1, arg1, current_working_dir);
    split_path(result_2, arg2, current_working_dir);

    if (result_1.empty() || result_2.empty()) return;

    clean_path(clean_result_1, result_1);
    clean_path(clean_result_2, result_2);

    if (!is_path_valid(clean_result_1, imgFile, BPBstruct, true, nullptr, false)
        && !is_path_valid(clean_result_1, imgFile, BPBstruct, false, nullptr, false)) return; // Checks if file exists, it shouldn't
    if (!is_path_valid(clean_result_2, imgFile, BPBstruct, false, nullptr, false)) return; // Checks if file exists, it shouldn't

    string filename = clean_result_1.back();
    clean_result_1.pop_back();

    string backup_current_working_dir = current_working_dir;
    string cd_directory;
    concat_string(cd_directory, clean_result_1);
    cd_command(cd_directory, imgFile, BPBstruct, current_working_dir);

    auto* fat_entries_p = find_entries(current_dir_cluster_no, imgFile, BPBstruct);
    auto& fat_entries = *fat_entries_p;

    int size_of_fatFileEntry = sizeof(FatFileEntry);
    FatFileEntry tmp_file;
    bool is_deleted = false;
    string str1;
    vector<pair<string, vector<FatFileEntry> > > entries;

    FatFileEntry msdos;
    FatFileEntry previous_msdos;
    for (int i = 0; i < 8; i++) // TODO: For . and .. entries. It could be unnecessary in touch. Check this in mkdir.
        previous_msdos.msdos.filename[i] = 32;

    // TODO: Should we check for deleted files?
    uint32_t free_cluster;
    pair<string, vector<FatFileEntry> > entry;
    bool is_found = false;
    for (auto& root_fat_entry : fat_entries) {
        if (is_found) break;
        fseek(imgFile, beginning_of_clusters + (root_fat_entry - BPBstruct.extended.RootCluster)*bytes_per_cluster, SEEK_SET); // TODO: CHECK IF ROOTCLUSTER EDIT WORKS
        auto* tmp_file = new FatFileEntry; // TODO: Change this to stack allocation.
        for (int j = 0; j < bytes_per_cluster; j+=size_of_fatFileEntry) {
            fread(tmp_file, size_of_fatFileEntry, 1, imgFile);
            if (tmp_file->msdos.filename[0] == 0x2E) continue;
            else if (tmp_file->lfn.attributes == 15) { // It is LFN.
                entry.second.push_back(*tmp_file);
                str1.insert(0, lfn_name_extract(tmp_file));
                if (tmp_file->lfn.sequence_number == 0xE5) {
                    is_deleted = true;
                }
            }
            else {
                if (is_deleted) {
                    str1.clear();
                    is_deleted = false;
                    entry.first.clear();
                    entry.second.clear();
                }
                else if (str1.length() > 0) {
                    if (str1 == filename) {
                        is_found = true;
                        entry.first = str1;
                        entry.second.push_back(*tmp_file);
                        entries.push_back(entry);

                        bool is_directory = (entry.second.back().msdos.attributes & 0x10) == 0x10;
                        FatFileEntry folder_msdos = entry.second.back();

                        // TODO: Update double dot entries of entries (if it is a directory)

                        cd_command(cd_directory, imgFile, BPBstruct, current_working_dir);
                        // TODO: Mark deleted old directories/files.
                        fseek(imgFile, -(((int)size_of_fatFileEntry)*(int)entry.second.size()), SEEK_CUR);
                       uint8_t deleted = 0xE5;
                       for (int k = 0; k < entry.second.size(); k++) {
                            fwrite(&deleted, 1, 1, imgFile);
                            fseek(imgFile, size_of_fatFileEntry - 1, SEEK_CUR);
                        //     FatFileEntry tmp_entry = entry.second[k];
                        //     tmp_entry.lfn.sequence_number = 0xE5;
                        //    fwrite(&tmp_entry, size_of_fatFileEntry, 1, imgFile);
                        //    fseek(imgFile, size_of_fatFileEntry - 1, SEEK_CUR);
                       }
                    //    fseek(imgFile, size_of_fatFileEntry, SEEK_CUR);
                    //    cd_command(cd_directory, imgFile, BPBstruct, current_working_dir);

                        string backup_current_working_dir_2 = current_working_dir;
                        string cd_directory_2;
                        concat_string(cd_directory_2, clean_result_2);
                        cd_command(cd_directory_2, imgFile, BPBstruct, current_working_dir);


                        // TODO: Put the entries into new directory, if there is not enough space, allocate new cluster.

                            // TODO: Change the msdos filename (ie ~1, ~2)
                            parsed_input touch_input;
                            char* touch_arg1 = new char[filename.size()];

                            for (int l = 0; l < filename.size(); l++)
                                touch_arg1[l] = filename[l];

                            touch_arg1[filename.size()] = '\0';
                            touch_input.arg1 = touch_arg1;
                            touch_command(touch_input, imgFile, BPBstruct, current_working_dir, &entry.second);

                            delete[] touch_arg1;

                        if (is_directory) {
                            fseek(imgFile, beginning_of_clusters+(folder_msdos.msdos.firstCluster - BPBstruct.extended.RootCluster)*bytes_per_cluster, SEEK_SET);
                            fseek(imgFile, size_of_fatFileEntry, SEEK_CUR);
                            FatFileEntry double_dot_entry;
                            fread(&double_dot_entry, size_of_fatFileEntry, 1, imgFile);
                            if (double_dot_entry.msdos.filename[0] == 0x2E && double_dot_entry.msdos.filename[1] == 0x2E) {
                                fseek(imgFile, -size_of_fatFileEntry, SEEK_CUR);
                                if (!clean_result_2.empty()) {
                                    clean_result_2.pop_back();
                                }
                                string clean_result_2_str;
                                concat_string(clean_result_2_str, clean_result_2);
                                cd_command(clean_result_2_str, imgFile, BPBstruct, current_working_dir);
                                double_dot_entry.msdos.firstCluster = current_dir_cluster_no;
                                fwrite(&double_dot_entry, size_of_fatFileEntry, 1, imgFile);
                            }
                        }

                        cd_command(backup_current_working_dir_2, imgFile, BPBstruct, current_working_dir);
                    }
                    str1.clear();
                }
                entry.first.clear();
                entry.second.clear();
            }
            if (is_found) break;
        }
        delete tmp_file;
    }

    cd_command(backup_current_working_dir, imgFile, BPBstruct, current_working_dir);

}

int main(int argc, char *argv[]) {
    string current_working_dir = "/";
    bool is_development = false; // TODO: Change this to false before submission
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

    BPB_struct BPBstruct;
    size_t result = fread(&BPBstruct, sizeof(BPB_struct), 1, imgFile);
    if (result != 1) {
        exit(1);
    }

    current_dir_cluster_no = BPBstruct.extended.RootCluster;
    bytes_per_cluster = BPBstruct.BytesPerSector*BPBstruct.SectorsPerCluster;
    beginning_of_clusters = (BPBstruct.ReservedSectorCount+BPBstruct.extended.FATSize*BPBstruct.NumFATs)*BPBstruct.BytesPerSector;
    beginning_of_fat_table = BPBstruct.ReservedSectorCount*BPBstruct.BytesPerSector;
    end_of_chain = get_fat_entry_at(imgFile, beginning_of_fat_table);

    while (true) {
        cout << current_working_dir << "> ";
        string line;
        std::getline(std::cin, line);

        if (line == "quit") {
            return 0;
        }

        if (line.length() == 0) continue;

        parsed_input input;
        char* input_chr = new char[line.size() + 1];
        strcpy(input_chr, line.c_str());
        parse(&input, input_chr);

        if (input.type == LS) {
            ls_command(input, imgFile, BPBstruct, current_working_dir);
        }
        else if (input.type == CD) {
            if (input.arg1 != nullptr) {
                string path(input.arg1);
                cd_command(path, imgFile, BPBstruct, current_working_dir);
            }
        }
        else if (input.type == CAT) {
            cat_command(input, imgFile, BPBstruct, current_working_dir);
        }
        else if (input.type == TOUCH) {
            touch_command(input, imgFile, BPBstruct, current_working_dir);
        }
        else if (input.type == MKDIR) {
            mkdir_command(input, imgFile, BPBstruct, current_working_dir);
        }
        else if (input.type == MV) {
            mv_command(input, imgFile, BPBstruct, current_working_dir);
        }

        clean_input(&input);
        delete[] input_chr;
    }
}
