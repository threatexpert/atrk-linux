#include "pred.h"
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <ctype.h>
#include "utils.h"

#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"

void vector_init(struct Vector* vector) {
    vector->data = NULL;
    vector->size = 0;
    vector->capacity = 0;
}

void vector_pushBack(struct Vector* vector, uint64_t value) {
    if (vector->size == vector->capacity) {
        size_t newCapacity = (vector->capacity == 0) ? 1 : vector->capacity * 2;
        uint64_t* newData = (uint64_t*)realloc(vector->data, newCapacity * sizeof(uint64_t));
        if (newData == NULL) {
            fprintf(stderr, "Memory allocation failed.\n");
            exit(1);
            return;
        }
        vector->data = newData;
        vector->capacity = newCapacity;
    }
    vector->data[vector->size++] = value;
}

void vector_pushBackp(struct Vector* vector, void* value) {
    vector_pushBack(vector, (uint64_t)value);
}

void vector_popBack(struct Vector* vector) {
    if (vector->size > 0) {
        vector->size--;
        if (vector->size <= vector->capacity / 4) {
            size_t newCapacity = vector->capacity / 2;
            uint64_t* newData = (uint64_t*)realloc(vector->data, newCapacity * sizeof(uint64_t));
            if (newData != NULL || newCapacity == 0) {
                vector->data = newData;
                vector->capacity = newCapacity;
            }
        }
    }
}

uint64_t vector_get(const struct Vector* vector, size_t index) {
    if (index < vector->size) {
        return vector->data[index];
    }
    fprintf(stderr, "Index out of range.\n");
    exit(1);
    return 0;
}

void* vector_getp(const struct Vector* vector, size_t index) {
    return (void*)vector_get(vector, index);
}

void vector_free(struct Vector* vector, int freeitems) {
    size_t i;
    if (freeitems && vector->data) {
        for (i=0; i<vector->size; i++) {
            free((void*)vector->data[i]);
        }
    }
    free(vector->data);
    vector->data = NULL;
    vector->size = 0;
    vector->capacity = 0;
}

int isBigEndian() {
    int num = 1;
    unsigned char* ptr = (unsigned char*)&num;
    return (int)(!*ptr);
}


char *trimch(char *buffer, char ch)
{
    int nLen = (int)strlen(buffer);
    char *pos = buffer + nLen - 1;
    while (pos > buffer)
    {
        if (*pos == ch)
        {
            *pos = '\0';
            --pos;
        }
        else
        {
            break;
        }
    }
    pos = buffer;
    while (pos < buffer + nLen)
    {
        if (*pos == ch)
        {
            pos++;
        }
        else
        {
            break;
        }
    }
    return pos;
}

char* toLowerCase(char* str) {
    char *p = str;
    while (*p) {
        *p = tolower((unsigned char)*p);
        p++;
    }
    return str;
}

int find_command(const char *cname, char *fullPathBuf, int bufsize) {
    char *path = getenv("PATH");
    if (path == NULL) {
        return 0;
    }

    const char *add = ":/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin";

    char *pathCopy = (char*)malloc(strlen(path) + strlen(add) + 1);
    if (pathCopy == NULL) {
        return 0;
    }
    strcpy(pathCopy, path);
    strcat(pathCopy, add);
    memset(fullPathBuf, 0, bufsize);

    char *token = strtok(pathCopy, ":");

    while (token != NULL) {
        snprintf(fullPathBuf, bufsize-1, "%s/%s", token, cname);
        if (access(fullPathBuf, F_OK | X_OK) == 0) {
            free(pathCopy);
            return 1;
        }
        token = strtok(NULL, ":");
    }
    memset(fullPathBuf, 0, bufsize);
    free(pathCopy);
    return 0;
}

char* path_join(const char *path1, const char *path2, char *buffer, int size) {
    int n1 = (int)strlen(path1);
    int n2 = (int)strlen(path2);

    if (n1 == 0 || path2[0] == '/') {
        snprintf(buffer, size-1, "%s", path2);
        return buffer;
    } else {
        if (path1[n1-1] == '/') {
            snprintf(buffer, size-1, "%s%s", path1, path2);
            return buffer;
        }else{
            snprintf(buffer, size-1, "%s/%s", path1, path2);
            return buffer;
        }
    }
}
