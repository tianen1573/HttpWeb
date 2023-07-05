#pragma once

#include <iostream>
#include <string>
#include <ctime>
#include <unistd.h>

enum
{
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};
const char *const levalStr[] = {
    "DEBUG",
    "INFO",
    "WARING",
    "ERROR",
    "FATAL"};


// #define LOG(leval, message) Log(levalStr[leval], message, __FILE__, __LINE__)
#define LOG(leval, message) Log(#leval, message, __FILE__, __LINE__)

void Log(std::string leval, std::string message, std::string file_name, int line)
{
    std::cerr << "[" << leval << "]"
              << "[" << time(nullptr) << "]"
              << "[" << message << "]"
              << "[" << file_name << "___" << line << "]" << std::endl;
}