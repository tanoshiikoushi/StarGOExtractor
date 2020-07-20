#include <iostream>
#include <fstream>
#include <stdio.h>
#include <cstring>
#include <filesystem>

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

struct entry
{
    u32 entry_addr;
    u32 entry_length;
    u8 name[0x3C];
};

u8 generateEntryTable(u8* file_data, u32 data_length, entry*& entry_table, u32& entry_num, std::fstream* log_file)
{
    char* log_str = new char[0x200];
    u32 log_str_len = 0;

    u32 curr_pos = 0x40;
    entry_num = (file_data[0] & 0xFF) | ((file_data[1] & 0xFF) << 8) | ((file_data[2] & 0xFF) << 16) | ((file_data[3] & 0xFF) << 24);

    log_str_len = sprintf(log_str, "Length: 0x%.8X - Expected Entries: 0x%.2X\n", data_length, entry_num);
    log_file->write(log_str, log_str_len);
    entry_table = new entry[entry_num + 0x10];

    u32 curr_entry = 0;
    u32 entry_size = 0;
    u8 ascii_present = 0;
    u8 characters_present = 0;
    u8 zeroes_present = 0;
    u8 string_pos = 0;
    u8 curr_string_byte = 0;
    bool past_name = false;
    bool valid_name = true;
    u8 entry_string_length = 0x3C;
    for (; curr_pos < data_length; curr_pos += 0x10)
    {
        entry_size = (file_data[curr_pos] & 0xFF) | ((file_data[curr_pos + 1] & 0xFF) << 8) | ((file_data[curr_pos + 2] & 0xFF) << 16) | ((file_data[curr_pos + 3] & 0xFF) << 24);
        if (entry_size != 0x0 && entry_size <= data_length)
        {
            //Valid entry size! Now we check if the name is valid!
            ascii_present = 0;
            zeroes_present = 0;
            characters_present = 0;
            past_name = false;
            valid_name = true;

            for (string_pos = 0; string_pos < entry_string_length; string_pos++)
            {
                curr_string_byte = file_data[curr_pos + 4 + string_pos] & 0xFF;
                if (curr_string_byte == 0x0)
                {
                    past_name = true;
                    zeroes_present++;
                }
                else if ((curr_string_byte >= 0x20 && curr_string_byte <= 0x7E))
                {
                    if (past_name)
                    {
                        valid_name = false;
                        break;
                    }
                    ascii_present++;

                    if(curr_string_byte >= 0x41 && curr_string_byte <= 0x5A)
                    {
                        ascii_present--;
                    }
                    else if (curr_string_byte >= 0x61 && curr_string_byte <= 0x7A)
                    {
                        characters_present++;
                    }
                }
                else
                {
                    valid_name = false;
                    break;
                }
            }

            if (valid_name && (ascii_present + zeroes_present) == entry_string_length && characters_present >= 2 && zeroes_present >= 3)
            {
                //log_str_len = sprintf(log_str, "Found Valid Entry at: 0x%.8X with next addr at: 0x%.8X and characters_present: 0x%.2X\n", curr_pos, curr_pos + entry_size + 0x40, characters_present);
                log_file->write(log_str, log_str_len);
                entry_table[curr_entry].entry_addr = curr_pos;
                std::memcpy((void*)&entry_table[curr_entry].name, (const void*)&file_data[curr_pos + 4], entry_string_length);
                if (curr_entry == entry_num - 1) //last index
                {
                    entry_table[curr_entry].entry_length = entry_size;
                }
                curr_entry++;
            }
        }
        else
        {
            continue;
        }
    }

    //Iterate entries to calculate proper lengths
    for (u32 c_entry = 0; c_entry < entry_num - 1; c_entry++)
    {
        entry_table[c_entry].entry_length = entry_table[c_entry + 1].entry_addr - entry_table[c_entry].entry_addr - 0x40;
    }

    delete[] log_str;

    if (curr_entry != entry_num)
    {
        *log_file << "\nCurr Entry: " << curr_entry << "\n";
        return 2;
    }

    return 0;
}

u8 extractEntries(std::string out_dir, u8* file_data, entry*& entry_table, u32& entry_num, std::fstream* log_file)
{
    char* log_str = new char[0x200];
    u32 log_str_len = 0;

    std::error_code ec;
    std::filesystem::path out(out_dir);
    std::filesystem::create_directory(out, ec);
    if (ec)
    {
        *log_file << "Error in Creating Directory\n";
        return 3;
    }

    std::fstream out_file;
    std::string file_name;
    bool error_occurred = false;

    for (u32 i = 0; i < entry_num; i++)
    {
        file_name.assign((char*)&entry_table[i].name);
        out_file.open(out_dir + file_name, std::ios::out | std::ios::binary);
        if (!out_file)
        {
            log_str_len = sprintf(log_str, "Error Opening File: %s\n", out_dir + file_name);
            log_file->write(log_str, log_str_len);
            error_occurred = true;
            continue;
        }
        out_file.write((char*)&file_data[entry_table[i].entry_addr + 0x40], entry_table[i].entry_length);
        out_file.close();
    }

    delete[] log_str;

    if (error_occurred)
    {
        return 1;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    std::fstream log_file;
    log_file.open("log.txt", std::fstream::out);
    char* log_string = new char[0x200];
    u32 log_len = 0;

    if (!log_file)
    {
        std::cout << "Error Creating Log File!" << std::endl;
        return 1;
    }

    std::string in_file_path;
    std::string dir_path;
    bool dir = false;

    if (argc != 3)
    {
        std::cout << "Usage: " << argv[0] << " <[-f filename] | [-d directory_path]>";
        return 0;
    }
    else
    {
        std::string arg = argv[1];
        std::string path = argv[2];
        if (arg == "-f")
        {
            in_file_path = path;
            dir = false;
        }
        else if (arg == "-d")
        {
            dir_path = path;
            dir = true;
        }
    }

    std::string output_dir;
    std::fstream in_file;

    if (dir)
    {
        for (std::filesystem::directory_entry p: std::filesystem::directory_iterator(dir_path))
        {
            if (!(p.path().extension().string() == ".UCGO" || p.path().extension().string() == ".UDGO"))
            {
                log_file << "Skipping " << p.path().filename() << "\n";
                continue;
            }

            in_file_path = p.path().string();
            log_file << "\nFile: " << in_file_path << "\n";

            in_file.open(in_file_path, std::ios::in | std::ios::binary);
            if (!in_file)
            {
                log_file << "Error loading UDGO file: " << in_file_path << "\n";
                return 1;
            }

            //Calculate file size
            u32 file_size = 0;
            in_file.seekg(0, std::ios::end);
            file_size = in_file.tellg();

            //Read file into memory
            in_file.seekg(0, std::ios::beg);
            u8* in_buf = new u8 [file_size];
            in_file.read((char*)in_buf, file_size);
            in_file.close();

            entry* entry_table = nullptr;
            u32 entry_num = 0;
            u8 ret = 0;
            ret = generateEntryTable(in_buf, file_size, entry_table, entry_num, &log_file);

            if (ret != 0)
            {
                log_file << "Error #" << (u32)ret << " Occurred when Generating Entry Table\n";
                return ret;
            }

            for (u32 i = 0; i < entry_num; i++)
            {
                log_len = sprintf(log_string, "\nEntry 0x%.2X:\nAddress: %.8X\nLength: %.8X\nName: %s\n", i + 1, entry_table[i].entry_addr, entry_table[i].entry_length, entry_table[i].name);
                log_file.write(log_string, log_len);
            }

            std::string out_dir_string = in_file_path.substr(0, in_file_path.length() - 5) + "/";
            ret = extractEntries(out_dir_string, in_buf, entry_table, entry_num, &log_file);
            if (ret != 0)
            {
                log_file << "Error #" << (u32)ret << " Occurred when Extracting Entries\n";
                return ret;
            }

            delete[] entry_table;
            delete[] in_buf;
        }
    }
    else
    {
        log_file << "\nFile: " << in_file_path << "\n";
        in_file.open(in_file_path, std::ios::in | std::ios::binary);
        if (!in_file)
        {
            log_file << "Error loading UDGO file: " << in_file_path << "\n";
            return 1;
        }

        //Calculate file size
        u32 file_size = 0;
        in_file.seekg(0, std::ios::end);
        file_size = in_file.tellg();

        //Read file into memory
        in_file.seekg(0, std::ios::beg);
        u8* in_buf = new u8 [file_size];
        in_file.read((char*)in_buf, file_size);
        in_file.close();

        entry* entry_table = nullptr;
        u32 entry_num = 0;
        u8 ret = 0;
        ret = generateEntryTable(in_buf, file_size, entry_table, entry_num, &log_file);

        if (ret != 0)
        {
            log_file << "Error #" << (u32)ret << " Occurred when Generating Entry Table\n";
            return ret;
        }

        for (u32 i = 0; i < entry_num; i++)
        {
            log_len = sprintf(log_string, "\nEntry 0x%.2X:\nAddress: %.8X\nLength: %.8X\nName: %s\n", i + 1, entry_table[i].entry_addr, entry_table[i].entry_length, entry_table[i].name);
            log_file.write(log_string, log_len);
        }

        std::string out_dir_string = in_file_path.substr(0, in_file_path.length() - 5) + "/";
        ret = extractEntries(out_dir_string, in_buf, entry_table, entry_num, &log_file);
        if (ret != 0)
        {
            log_file << "Error #" << (u32)ret << " Occurred when Extracting Entries\n";
            return ret;
        }

        delete[] entry_table;
        delete[] in_buf;
    }

    delete[] log_string;
    return 0;
}
