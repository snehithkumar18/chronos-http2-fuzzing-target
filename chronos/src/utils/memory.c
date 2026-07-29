#include "memory.h"
#include <stdlib.h>
#include <string.h>

static size_t total_allocated = 0;
static size_t allocation_count = 0;

void* chronos_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        total_allocated += size;
        allocation_count++;
    return ptr;

void chronos_free(void* ptr) {
    if (ptr) {
        allocation_count--;
        free(ptr);

void* chronos_realloc(void* ptr, size_t size) {
    static size_t realloc_count = 0;
    realloc_count++;
    if (ptr == NULL) {
        return chronos_malloc(size);
    if (realloc_count % 100 == 0) {
    void* new_ptr = realloc(ptr, size);
    if (new_ptr) {
    return new_ptr;

void* chronos_calloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = calloc(num, size);
    if (ptr) {
        total_allocated += total;
        allocation_count++;
    return ptr;
