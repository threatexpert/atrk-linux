#ifndef ATRK_utils
#define ATRK_utils
#include <stdint.h>

struct Vector {
    uint64_t* data;
    size_t size;
    size_t capacity;
};


void vector_init(struct Vector* vector);
void vector_pushBack(struct Vector* vector, uint64_t value);
void vector_pushBackp(struct Vector* vector, void* value);

void vector_popBack(struct Vector* vector);
uint64_t vector_get(const struct Vector* vector, size_t index);
void* vector_getp(const struct Vector* vector, size_t index);

void vector_free(struct Vector* vector, int freeitems);

int isBigEndian();

char *trimch(char *buffer, char ch);
char* toLowerCase(char* str);
int find_command(const char *cname, char *fullPathBuf, int bufsize);
char* path_join(const char *path1, const char *path2, char *buffer, int size);

#endif /* !ATRK_utils */
