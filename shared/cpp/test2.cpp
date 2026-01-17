//
// Created by ghima on 17-01-2026.
//
#include <iostream>

__declspec(dllexport) void print_simple_message_two(const char *test) {
    std::cout << "This is a hello world message from common 2 " << test << std::endl;
}