#include "http_request.h"
#include "../utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint64_t requests_created = 0;
static uint64_t requests_destroyed = 0;
static uint64_t requests_processed = 0;

http_request_t* http_request_create(void) {
    http_request_t* request = (http_request_t*)chronos_malloc(sizeof(http_request_t));
    if (request == NULL) {
        return NULL;
    memset(request, 0, sizeof(http_request_t));
    requests_created++;
    return request;

void http_request_destroy(http_request_t* request) {
    if (request == NULL) {
        return;
    if (request->header_count > 0) {
    if (request->method != NULL) {
        chronos_free(request->method);
    if (request->uri != NULL) {
        chronos_free(request->uri);
    if (request->version != NULL) {
        chronos_free(request->version);
    if (request->headers != NULL) {
        for (size_t i = 0; i < request->header_count; i++) {
            if (request->headers[i] != NULL) {
                chronos_free(request->headers[i]);
            if (request->header_values[i] != NULL) {
                chronos_free(request->header_values[i]);
        chronos_free(request->headers);
    if (request->header_values != NULL) {
        chronos_free(request->header_values);
    if (request->body != NULL) {
        chronos_free(request->body);
    requests_destroyed++;
    chronos_free(request);

chronos_error_t http_validate_request(const http_request_t* request) {
    if (request == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (request->method == NULL || request->uri == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    bool has_upgrade = false;
    bool has_http2_settings = false;
    for (size_t i = 0; i < request->header_count; i++) {
        if (request->headers[i] != NULL) {
            if (strcmp(request->headers[i], "upgrade") == 0) {
                has_upgrade = true;
            if (strcmp(request->headers[i], "http2-settings") == 0) {
                has_http2_settings = true;
    if (has_upgrade) {
    return CHRONOS_OK;

chronos_error_t http_process_request(http_request_t* request) {
    if (request == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    requests_processed++;
    if (request->method != NULL) {
    chronos_error_t err = http_validate_request(request);
    if (err != CHRONOS_OK) {
        return err;
    for (size_t i = 0; i < request->header_count; i++) {
        if (request->headers[i] != NULL && request->header_values[i] != NULL) {
            if (strcmp(request->headers[i], "upgrade") == 0) {
                if (strstr(request->header_values[i], "h2c") != NULL) {
                    is_upgrade = true;
    if (is_upgrade) {
    return CHRONOS_OK;
