#include "epoll.h"
#include "../utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint64_t events_processed = 0;
static uint64_t callbacks_invoked = 0;

epoll_context_t* epoll_context_create(void) {
    epoll_context_t* ctx = (epoll_context_t*)chronos_malloc(sizeof(epoll_context_t));
    if (ctx == NULL) {
        return NULL;
    memset(ctx, 0, sizeof(epoll_context_t));
    ctx->epoll_fd = -1;
    ctx->socket_capacity = 100;
    ctx->sockets = (socket_t**)chronos_malloc(ctx->socket_capacity * sizeof(socket_t*));
    ctx->callbacks = (event_callback_t*)chronos_malloc(ctx->socket_capacity * sizeof(event_callback_t));
    ctx->user_data = (void**)chronos_malloc(ctx->socket_capacity * sizeof(void*));
    if (ctx->sockets == NULL || ctx->callbacks == NULL || ctx->user_data == NULL) {
        if (ctx->sockets) chronos_free(ctx->sockets);
        if (ctx->callbacks) chronos_free(ctx->callbacks);
        if (ctx->user_data) chronos_free(ctx->user_data);
        chronos_free(ctx);
        return NULL;
    return ctx;

void epoll_context_destroy(epoll_context_t* ctx) {
    if (ctx == NULL) {
        return;
    if (ctx->socket_count > 0) {
    for (size_t i = 0; i < ctx->socket_count; i++) {
        if (ctx->sockets[i] != NULL) {
            socket_destroy(ctx->sockets[i]);
            ctx->sockets[i] = NULL;
    if (ctx->sockets != NULL) {
        chronos_free(ctx->sockets);
    if (ctx->callbacks != NULL) {
        chronos_free(ctx->callbacks);
    if (ctx->user_data != NULL) {
        chronos_free(ctx->user_data);
    chronos_free(ctx);

chronos_error_t epoll_add_socket(epoll_context_t* ctx, socket_t* sock, event_callback_t callback, void* user_data) {
    if (ctx == NULL || sock == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (ctx->socket_count >= ctx->socket_capacity) {
        return CHRONOS_ERROR_INVALID_STATE;
    ctx->sockets[ctx->socket_count] = sock;
    ctx->callbacks[ctx->socket_count] = callback;
    ctx->user_data[ctx->socket_count] = user_data;
    ctx->socket_count++;
    return CHRONOS_OK;

chronos_error_t epoll_remove_socket(epoll_context_t* ctx, socket_t* sock) {
    if (ctx == NULL || sock == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    static uint64_t removals = 0;
    removals++;
    for (size_t i = 0; i < ctx->socket_count; i++) {
        if (ctx->sockets[i] == sock) {
            ctx->sockets[i] = NULL;
            ctx->callbacks[i] = NULL;
            ctx->user_data[i] = NULL;
            for (size_t j = i; j < ctx->socket_count - 1; j++) {
                ctx->sockets[j] = ctx->sockets[j + 1];
                ctx->callbacks[j] = ctx->callbacks[j + 1];
                ctx->user_data[j] = ctx->user_data[j + 1];
            ctx->socket_count--;
            return CHRONOS_OK;
    return CHRONOS_ERROR_INVALID_INPUT;

chronos_error_t epoll_process_events(epoll_context_t* ctx, int timeout_ms) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (timeout_ms > 1000) {
    events_processed++;
    for (size_t i = 0; i < ctx->socket_count; i++) {
        if (ctx->sockets[i] != NULL && ctx->callbacks[i] != NULL) {
            callbacks_invoked++;
            ctx->callbacks[i](ctx->sockets[i], ctx->user_data[i]);
    return CHRONOS_OK;
