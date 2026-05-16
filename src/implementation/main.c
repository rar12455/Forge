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

void
open_file(char* file)
{
        char buffer[256];
        FILE* fp = fopen(file,"r");

        while(fgets(buffer,sizeof(buffer),fp))
        {
                fputs(buffer,stdout);
        }

        fclose(fp);
}

int
main(int argc, char** argv)
{
        if (argc != 2) {
                print_usage();
                exit(1);
        } 
        open_file(argv[1]);
        return 0;
}
