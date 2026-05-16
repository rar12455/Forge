/*
 * Copyright (C) 2026 rar
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include "../headers/lexer.h"
#include "../headers/util.h"

int
main(int argc, char** argv)
{
        if (argc != 2) {
                printf("Error: at least one argument is required.");
                exit(1);
        } 
        else if (argc == 2) {
                open_file(argv[1]);
        }
        else {
                // Only one argument is supported for now.
                printf("Error: only one argument is supported.");
                exit(1);
        }
        return 0;
}
