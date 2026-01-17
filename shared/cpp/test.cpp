//
// Created by ghima on 16-01-2026.
//
#include <iostream>

__declspec(dllexport) void print_simple_message(const char *test) {
    std::cout << "This is a hello world message " << test << std::endl;
}