//
// Created by Luis Yanes (EI) on 23/11/2017.
//

#ifndef BSG_FILESYSTEM_H
#define BSG_FILESYSTEM_H

#include <string>
#include <iostream>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

namespace sglib {
    bool check_file(std::string &filepath);
    bool check_or_create_directory(std::string &output_prefix);
    void remove_directory(std::string path);
    std::string create_temp_directory(std::string tmpBase);
    bool copy_file(const std::string& from_p, const std::string& to_p, bool fail_if_exists);
}
#endif //BSG_FILESYSTEM_H