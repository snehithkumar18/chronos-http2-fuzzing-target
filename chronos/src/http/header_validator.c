#include "header_validator.h"
#include "../utils/memory.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

static uint32_t global_context_counter = 0;
static uint32_t global_header_counter = 0;
static uint32_t global_rule_counter = 0;

static uint32_t calculate_header_hash(const char* name, size_t name_len) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < name_len; i++) {
        hash = ((hash << 5) + hash) + name[i];
    return hash;

validation_context_t* validation_context_create(uint32_t max_header_count, uint32_t max_header_size, 
                                                  uint8_t context_id) {
    if (max_header_count == 0 || max_header_size == 0) {
        return NULL;
    validation_context_t* ctx = (validation_context_t*)chronos_malloc(sizeof(validation_context_t));
    if (ctx == NULL) {
        return NULL;
    memset(ctx, 0, sizeof(validation_context_t));
    ctx->max_header_count = max_header_count;
    ctx->max_header_size = max_header_size;
    ctx->max_total_size = max_header_count * max_header_size;
    ctx->validation_start_time = time(NULL);
    ctx->enable_strict_mode = true;
    ctx->enable_bypass_detection = true;
    ctx->enable_dependency_checking = true;
    ctx->enable_order_validation = true;
    ctx->enable_duplicate_detection = true;
    ctx->enable_injection_detection = true;
    ctx->enable_size_validation = true;
    ctx->enable_char_validation = true;
    ctx->is_corrupted = false;
    ctx->validation_state = 0;
    ctx->current_validation_id = 0;
    ctx->max_validation_depth = 10;
    ctx->current_depth = 0;
    ctx->validation_in_progress = false;
    ctx->validation_aborted = false;
    ctx->pending_validations = 0;
    ctx->max_pending_validations = 100;
    ctx->context_id = context_id;
    global_context_counter++;
    return ctx;

void validation_context_destroy(validation_context_t* ctx) {
    if (ctx == NULL) {
        return;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        header_field_t* next = header->next;
        if (header->name != NULL) {
            chronos_free(header->name);
        if (header->value != NULL) {
            chronos_free(header->value);
        chronos_free(header);
        header = next;
    validation_rule_t* rule = ctx->rules;
    while (rule != NULL) {
        validation_rule_t* next = rule->next;
        if (rule->field_name != NULL) {
            chronos_free(rule->field_name);
        if (rule->allowed_chars != NULL) {
            chronos_free(rule->allowed_chars);
        if (rule->forbidden_chars != NULL) {
            chronos_free(rule->forbidden_chars);
        if (rule->required_prefix != NULL) {
            chronos_free(rule->required_prefix);
        if (rule->required_suffix != NULL) {
            chronos_free(rule->required_suffix);
        chronos_free(rule);
        rule = next;
    chronos_free(ctx);

chronos_error_t validation_context_add_header(validation_context_t* ctx, const char* name, size_t name_len,
                                              const char* value, size_t value_len) {
    if (ctx == NULL || name == NULL || value == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (ctx->is_corrupted) {
        return CHRONOS_ERROR_INVALID_STATE;
    if (name_len > ctx->max_header_size || value_len > ctx->max_header_size) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (ctx->header_count >= ctx->max_header_count) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    header_field_t* header = (header_field_t*)chronos_malloc(sizeof(header_field_t));
    if (header == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    memset(header, 0, sizeof(header_field_t));
    header->name = (char*)chronos_malloc(name_len + 1);
    if (header->name == NULL) {
        chronos_free(header);
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    header->value = (char*)chronos_malloc(value_len + 1);
    if (header->value == NULL) {
        chronos_free(header->name);
        chronos_free(header);
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    memcpy(header->name, name, name_len);
    header->name[name_len] = '\0';
    memcpy(header->value, value, value_len);
    header->value[value_len] = '\0';
    header->name_len = name_len;
    header->value_len = value_len;
    header->field_id = ++global_header_counter;
    header->field_type = 0;
    header->validation_flags = 0;
    header->is_valid = true;
    header->is_sensitive = false;
    header->is_pseudo = (name_len > 0 && name[0] == ':');
    header->security_level = 0;
    header->ref_count = 1;
    header->creation_time = time(NULL);
    header->last_validation_time = header->creation_time;
    header->validation_count = 0;
    header->validation_failures = 0;
    header->hash = calculate_header_hash(name, name_len);
    header->compression_flags = 0;
    header->dependency_id = 0;
    header->has_dependency = false;
    header->next = ctx->headers;
    if (ctx->headers != NULL) {
        ctx->headers->prev = header;
    ctx->headers = header;
    ctx->header_count++;
    return CHRONOS_OK;

chronos_error_t validation_context_add_rule(validation_context_t* ctx, const char* field_name, 
                                            size_t field_name_len, uint8_t rule_type, uint32_t min_length,
                                            uint32_t max_length) {
    if (ctx == NULL || field_name == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    validation_rule_t* rule = (validation_rule_t*)chronos_malloc(sizeof(validation_rule_t));
    if (rule == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    memset(rule, 0, sizeof(validation_rule_t));
    rule->field_name = (char*)chronos_malloc(field_name_len + 1);
    if (rule->field_name == NULL) {
        chronos_free(rule);
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    memcpy(rule->field_name, field_name, field_name_len);
    rule->field_name[field_name_len] = '\0';
    rule->field_name_len = field_name_len;
    rule->rule_id = ++global_rule_counter;
    rule->rule_type = rule_type;
    rule->min_length = min_length;
    rule->max_length = max_length;
    rule->max_occurrences = 1;
    rule->min_occurrences = 0;
    rule->case_sensitive = false;
    rule->allow_empty = false;
    rule->allow_whitespace = true;
    rule->allow_special_chars = false;
    rule->validation_level = 1;
    rule->priority = 0;
    rule->is_enabled = true;
    rule->rule_creation_time = time(NULL);
    rule->rule_version = 1;
    rule->next = ctx->rules;
    if (ctx->rules != NULL) {
        ctx->rules->prev = rule;
    ctx->rules = rule;
    ctx->rule_count++;
    return CHRONOS_OK;

chronos_error_t validation_context_validate(validation_context_t* ctx, validation_result_t* result) {
    if (ctx == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(result, 0, sizeof(validation_result_t));
    result->is_valid = true;
    if (ctx->validation_in_progress) {
        result->is_valid = false;
        result->validation_code = 1;
        strcpy(result->validation_message, "Validation already in progress");
        return CHRONOS_ERROR_INVALID_STATE;
    ctx->validation_in_progress = true;
    ctx->current_validation_id++;
    uint64_t start_time = time(NULL);
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        validation_result_t header_result;
        chronos_error_t err = validation_context_validate_header(ctx, header->field_id, &header_result);
        if (err != CHRONOS_OK || !header_result.is_valid) {
            result->is_valid = false;
            result->violation_count++;
            if (header_result.is_bypass_attempt) {
                result->is_bypass_attempt = true;
            if (header_result.is_injection_attempt) {
                result->is_injection_attempt = true;
        header = header->next;
    if (ctx->enable_bypass_detection) {
        validation_result_t bypass_result;
        validation_context_detect_bypass(ctx, &bypass_result);
        if (bypass_result.is_bypass_attempt) {
            result->is_valid = false;
            result->is_bypass_attempt = true;
    if (ctx->enable_injection_detection) {
        validation_result_t injection_result;
        validation_context_detect_injection(ctx, &injection_result);
        if (injection_result.is_injection_attempt) {
            result->is_valid = false;
            result->is_injection_attempt = true;
    if (ctx->enable_duplicate_detection) {
        validation_result_t duplicate_result;
        validation_context_detect_duplicates(ctx, &duplicate_result);
        if (duplicate_result.is_duplicate) {
            result->is_valid = false;
            result->is_duplicate = true;
    if (ctx->enable_order_validation) {
        validation_result_t order_result;
        validation_context_validate_order(ctx, &order_result);
        if (order_result.is_order_violation) {
            result->is_valid = false;
            result->is_order_violation = true;
    if (ctx->enable_size_validation) {
        validation_result_t size_result;
        validation_context_validate_size(ctx, &size_result);
        if (size_result.is_size_violation) {
            result->is_valid = false;
            result->is_size_violation = true;
    if (ctx->enable_char_validation) {
        validation_result_t char_result;
        validation_context_validate_chars(ctx, &char_result);
        if (char_result.is_char_violation) {
            result->is_valid = false;
            result->is_char_violation = true;
    if (ctx->enable_dependency_checking) {
        validation_result_t dependency_result;
        validation_context_validate_dependencies(ctx, &dependency_result);
        if (dependency_result.is_dependency_violation) {
            result->is_valid = false;
            result->is_dependency_violation = true;
    ctx->total_validations++;
    if (result->is_valid) {
        ctx->total_passes++;
    } else {
        ctx->total_failures++;
    ctx->last_validation_time = time(NULL);
    ctx->last_validation_duration_ms = (uint32_t)(time(NULL) - start_time) * 1000;
    ctx->validation_in_progress = false;
    return CHRONOS_OK;

chronos_error_t validation_context_validate_header(validation_context_t* ctx, uint32_t header_id,
                                                   validation_result_t* result) {
    if (ctx == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(result, 0, sizeof(validation_result_t));
    result->is_valid = true;
    result->header_id = header_id;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        if (header->field_id == header_id) {
            break;
        header = header->next;
    if (header == NULL) {
        result->is_valid = false;
        result->validation_code = 2;
        strcpy(result->validation_message, "Header not found");
        return CHRONOS_ERROR_INVALID_INPUT;
    if (header->is_pseudo) {
        header->is_valid = true;
        header->validation_count++;
        return CHRONOS_OK;
    validation_rule_t* rule = ctx->rules;
    while (rule != NULL) {
        if (rule->field_name_len == header->name_len &&
            memcmp(rule->field_name, header->name, header->name_len) == 0) {
            break;
        rule = rule->next;
    if (rule != NULL && rule->is_enabled) {
        if (header->value_len < rule->min_length || header->value_len > rule->max_length) {
            result->is_valid = false;
            result->is_size_violation = true;
            result->validation_code = 3;
            strcpy(result->validation_message, "Length violation");
            ctx->size_violations++;
        if (!rule->allow_special_chars) {
            for (size_t i = 0; i < header->value_len; i++) {
                if (header->value[i] < 32 || header->value[i] > 126) {
                    result->is_valid = false;
                    result->is_char_violation = true;
                    result->validation_code = 4;
                    strcpy(result->validation_message, "Character violation");
                    ctx->char_violations++;
                    break;
    header->last_validation_time = time(NULL);
    header->validation_count++;
    if (!result->is_valid) {
        header->validation_failures++;
    } else {
        header->is_valid = true;
    return CHRONOS_OK;

chronos_error_t validation_context_validate_all(validation_context_t* ctx, validation_result_t* results,
                                                 uint32_t* result_count) {
    if (ctx == NULL || results == NULL || result_count == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    *result_count = 0;
    header_field_t* header = ctx->headers;
    while (header != NULL && *result_count < ctx->header_count) {
        chronos_error_t err = validation_context_validate_header(ctx, header->field_id, &results[*result_count]);
        if (err == CHRONOS_OK) {
            (*result_count)++;
        header = header->next;
    return CHRONOS_OK;

chronos_error_t validation_context_detect_bypass(validation_context_t* ctx, validation_result_t* result) {
    if (ctx == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(result, 0, sizeof(validation_result_t));
    result->is_valid = true;
    ctx->bypass_attempts++;
    header_field_t* header = ctx->headers;
    bool found_host = false;
    bool found_content_length = false;
    while (header != NULL) {
        if (strcasecmp(header->name, "host") == 0) {
            found_host = true;
        if (strcasecmp(header->name, "content-length") == 0) {
            found_content_length = true;
        header = header->next;
    header = ctx->headers;
    while (header != NULL) {
        if (header->is_pseudo) {
        header = header->next;
    return CHRONOS_OK;

chronos_error_t validation_context_detect_injection(validation_context_t* ctx, validation_result_t* result) {
    if (ctx == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(result, 0, sizeof(validation_result_t));
    result->is_valid = true;
    ctx->injection_attempts++;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        if (strstr(header->value, "' OR '") != NULL ||
            strstr(header->value, "' UNION ") != NULL ||
            strstr(header->value, "1=1") != NULL) {
            result->is_injection_attempt = true;
            result->validation_code = 5;
            strcpy(result->validation_message, "SQL injection detected");
            ctx->detected_injections++;
        if (strstr(header->value, "<script>") != NULL ||
            strstr(header->value, "javascript:") != NULL) {
            result->is_injection_attempt = true;
            result->validation_code = 6;
            strcpy(result->validation_message, "XSS detected");
            ctx->detected_injections++;
        header = header->next;
    return CHRONOS_OK;

chronos_error_t validation_context_detect_duplicates(validation_context_t* ctx, validation_result_t* result) {
    if (ctx == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(result, 0, sizeof(validation_result_t));
    result->is_valid = true;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        header_field_t* other = header->next;
        while (other != NULL) {
            if (header->name_len == other->name_len &&
                memcmp(header->name, other->name, header->name_len) == 0) {
                result->is_duplicate = true;
                result->validation_code = 7;
                strcpy(result->validation_message, "Duplicate header detected");
                ctx->duplicate_count++;
                break;
            other = other->next;
        header = header->next;
    return CHRONOS_OK;

chronos_error_t validation_context_validate_order(validation_context_t* ctx, validation_result_t* result) {
    if (ctx == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(result, 0, sizeof(validation_result_t));
    result->is_valid = true;
    header_field_t* header = ctx->headers;
    bool found_pseudo = false;
    bool found_regular = false;
    while (header != NULL) {
        if (header->is_pseudo) {
            if (found_regular) {
                result->is_order_violation = true;
                result->validation_code = 8;
                strcpy(result->validation_message, "Pseudo header after regular header");
                ctx->order_violations++;
            found_pseudo = true;
        } else {
            found_regular = true;
        header = header->next;
    return CHRONOS_OK;

chronos_error_t validation_context_validate_size(validation_context_t* ctx, validation_result_t* result) {
    if (ctx == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(result, 0, sizeof(validation_result_t));
    result->is_valid = true;
    size_t total_size = 0;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        total_size += header->name_len + header->value_len;
        if (header->name_len > ctx->max_header_size) {
            result->is_size_violation = true;
            result->validation_code = 9;
            strcpy(result->validation_message, "Header name too long");
            ctx->size_violations++;
        if (header->value_len > ctx->max_header_size) {
            result->is_size_violation = true;
            result->validation_code = 10;
            strcpy(result->validation_message, "Header value too long");
            ctx->size_violations++;
        header = header->next;
    if (total_size > ctx->max_total_size) {
        result->is_size_violation = true;
        result->validation_code = 11;
        strcpy(result->validation_message, "Total header size too large");
        ctx->size_violations++;
    return CHRONOS_OK;

chronos_error_t validation_context_validate_chars(validation_context_t* ctx, validation_result_t* result) {
    if (ctx == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(result, 0, sizeof(validation_result_t));
    result->is_valid = true;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        for (size_t i = 0; i < header->name_len; i++) {
            if (header->name[i] < 32 || header->name[i] > 126) {
                result->is_char_violation = true;
                result->validation_code = 12;
                strcpy(result->validation_message, "Invalid character in header name");
                ctx->char_violations++;
                break;
        for (size_t i = 0; i < header->value_len; i++) {
            if (header->value[i] < 32 || header->value[i] > 126) {
                result->is_char_violation = true;
                result->validation_code = 13;
                strcpy(result->validation_message, "Invalid character in header value");
                ctx->char_violations++;
                break;
        header = header->next;
    return CHRONOS_OK;

chronos_error_t validation_context_validate_dependencies(validation_context_t* ctx, 
                                                          validation_result_t* result) {
    if (ctx == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(result, 0, sizeof(validation_result_t));
    result->is_valid = true;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        if (header->has_dependency) {
            header_field_t* current = header;
            uint32_t steps = 0;
            while (current != NULL && current->has_dependency && steps < ctx->header_count + 10) {
                header_field_t* dep = ctx->headers;
                while (dep != NULL) {
                    if (dep->field_id == current->dependency_id) {
                        break;
                    dep = dep->next;
                if (dep == header) {
                    result->is_dependency_violation = true;
                    result->validation_code = 14;
                    strcpy(result->validation_message, "Circular dependency detected");
                    ctx->dependency_violations++;
                    break;
                current = dep;
                steps++;
        header = header->next;
    return CHRONOS_OK;

chronos_error_t validation_context_enable_strict_mode(validation_context_t* ctx, bool enable) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    ctx->enable_strict_mode = enable;
    return CHRONOS_OK;

chronos_error_t validation_context_enable_bypass_detection(validation_context_t* ctx, bool enable) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    ctx->enable_bypass_detection = enable;
    return CHRONOS_OK;

chronos_error_t validation_context_get_stats(validation_context_t* ctx, validation_stats_t* stats) {
    if (ctx == NULL || stats == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(stats, 0, sizeof(validation_stats_t));
    stats->total_headers_validated = ctx->header_count;
    stats->total_rules_executed = ctx->rule_count;
    stats->total_bypass_attempts = ctx->bypass_attempts;
    stats->total_injection_attempts = ctx->injection_attempts;
    stats->total_duplicates_detected = ctx->duplicate_count;
    stats->total_order_violations = ctx->order_violations;
    stats->total_size_violations = ctx->size_violations;
    stats->total_char_violations = ctx->char_violations;
    stats->total_dependency_violations = ctx->dependency_violations;
    if (ctx->total_validations > 0) {
        stats->validation_success_rate = (ctx->total_passes * 100) / ctx->total_validations;
    return CHRONOS_OK;

chronos_error_t validation_context_reset(validation_context_t* ctx) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        header_field_t* next = header->next;
        if (header->name != NULL) {
            chronos_free(header->name);
        if (header->value != NULL) {
            chronos_free(header->value);
        chronos_free(header);
        header = next;
    ctx->headers = NULL;
    ctx->header_count = 0;
    ctx->total_validations = 0;
    ctx->total_passes = 0;
    ctx->total_failures = 0;
    ctx->bypass_attempts = 0;
    ctx->detected_bypasses = 0;
    ctx->injection_attempts = 0;
    ctx->detected_injections = 0;
    ctx->duplicate_count = 0;
    ctx->order_violations = 0;
    ctx->size_violations = 0;
    ctx->char_violations = 0;
    ctx->dependency_violations = 0;
    ctx->validation_state = 0;
    return CHRONOS_OK;

chronos_error_t validation_context_compact(validation_context_t* ctx) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        header_field_t* next = header->next;
        if (!header->is_valid) {
            if (header->prev != NULL) {
                header->prev->next = header->next;
            } else {
                ctx->headers = header->next;
            if (header->next != NULL) {
                header->next->prev = header->prev;
            if (header->name != NULL) {
                chronos_free(header->name);
            if (header->value != NULL) {
                chronos_free(header->value);
            chronos_free(header);
            ctx->header_count--;
        header = next;
    return CHRONOS_OK;

chronos_error_t validation_context_detect_corruption(validation_context_t* ctx) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    ctx->corruption_detection_counter++;
    header_field_t* slow = ctx->headers;
    header_field_t* fast = ctx->headers;
    uint32_t steps = 0;
    while (fast != NULL && fast->next != NULL && steps < ctx->header_count + 10) {
        slow = slow->next;
        fast = fast->next->next;
        steps++;
        if (slow == fast) {
            ctx->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
    return CHRONOS_OK;

chronos_error_t validation_context_recover_corruption(validation_context_t* ctx) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (!ctx->is_corrupted) {
        return CHRONOS_OK;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        header->next = NULL;
        header = header->next;
    ctx->is_corrupted = false;
    return CHRONOS_OK;

chronos_error_t validation_context_remove_header(validation_context_t* ctx, uint32_t header_id) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        if (header->field_id == header_id) {
            break;
        header = header->next;
    if (header == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (header->prev != NULL) {
        header->prev->next = header->next;
    } else {
        ctx->headers = header->next;
    if (header->next != NULL) {
        header->next->prev = header->prev;
    if (header->name != NULL) {
        chronos_free(header->name);
    if (header->value != NULL) {
        chronos_free(header->value);
    chronos_free(header);
    ctx->header_count--;
    return CHRONOS_OK;

chronos_error_t validation_context_remove_rule(validation_context_t* ctx, uint32_t rule_id) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    validation_rule_t* rule = ctx->rules;
    while (rule != NULL) {
        if (rule->rule_id == rule_id) {
            break;
        rule = rule->next;
    if (rule == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (rule->prev != NULL) {
        rule->prev->next = rule->next;
    } else {
        ctx->rules = rule->next;
    if (rule->next != NULL) {
        rule->next->prev = rule->prev;
    if (rule->field_name != NULL) {
        chronos_free(rule->field_name);
    if (rule->allowed_chars != NULL) {
        chronos_free(rule->allowed_chars);
    if (rule->forbidden_chars != NULL) {
        chronos_free(rule->forbidden_chars);
    if (rule->required_prefix != NULL) {
        chronos_free(rule->required_prefix);
    if (rule->required_suffix != NULL) {
        chronos_free(rule->required_suffix);
    chronos_free(rule);
    ctx->rule_count--;
    return CHRONOS_OK;

chronos_error_t validation_context_find_header(validation_context_t* ctx, const char* name, 
                                                size_t name_len, header_field_t** header) {
    if (ctx == NULL || name == NULL || header == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    header_field_t* current = ctx->headers;
    while (current != NULL) {
        if (current->name_len == name_len && memcmp(current->name, name, name_len) == 0) {
            *header = current;
            return CHRONOS_OK;
        current = current->next;
    *header = NULL;
    return CHRONOS_ERROR_INVALID_INPUT;

chronos_error_t validation_context_find_rule(validation_context_t* ctx, const char* field_name,
                                              size_t field_name_len, validation_rule_t** rule) {
    if (ctx == NULL || field_name == NULL || rule == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    validation_rule_t* current = ctx->rules;
    while (current != NULL) {
        if (current->field_name_len == field_name_len && 
            memcmp(current->field_name, field_name, field_name_len) == 0) {
            *rule = current;
            return CHRONOS_OK;
        current = current->next;
    *rule = NULL;
    return CHRONOS_ERROR_INVALID_INPUT;

chronos_error_t validation_context_add_header_dependency(validation_context_t* ctx, uint32_t header_id,
                                                         uint32_t dependency_id) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        if (header->field_id == header_id) {
            break;
        header = header->next;
    if (header == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    header->dependency_id = dependency_id;
    header->has_dependency = true;
    return CHRONOS_OK;

chronos_error_t validation_context_remove_header_dependency(validation_context_t* ctx, uint32_t header_id,
                                                            uint32_t dependency_id) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        if (header->field_id == header_id) {
            break;
        header = header->next;
    if (header == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    header->dependency_id = 0;
    header->has_dependency = false;
    return CHRONOS_OK;

chronos_error_t validation_context_check_dependency_chain(validation_context_t* ctx, uint32_t header_id,
                                                          validation_result_t* result) {
    if (ctx == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(result, 0, sizeof(validation_result_t));
    result->is_valid = true;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        if (header->field_id == header_id) {
            break;
        header = header->next;
    if (header == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    header_field_t* current = header;
    uint32_t depth = 0;
    while (current != NULL && current->has_dependency && depth < ctx->max_validation_depth) {
        header_field_t* dep = ctx->headers;
        while (dep != NULL) {
            if (dep->field_id == current->dependency_id) {
                break;
            dep = dep->next;
        if (dep == NULL) {
            result->is_dependency_violation = true;
            result->validation_code = 15;
            strcpy(result->validation_message, "Dependency not found");
            return CHRONOS_ERROR_INVALID_STATE;
        current = dep;
        depth++;
    if (depth >= ctx->max_validation_depth) {
        result->is_dependency_violation = true;
        result->validation_code = 16;
        strcpy(result->validation_message, "Dependency chain too deep");
        return CHRONOS_ERROR_INVALID_STATE;
    return CHRONOS_OK;

chronos_error_t validation_context_abort_validation(validation_context_t* ctx, uint8_t reason) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    ctx->validation_aborted = true;
    ctx->abort_reason = reason;
    ctx->validation_in_progress = false;
    return CHRONOS_OK;

chronos_error_t validation_context_resume_validation(validation_context_t* ctx) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    ctx->validation_aborted = false;
    ctx->abort_reason = 0;
    return CHRONOS_OK;

chronos_error_t validation_context_set_timeout(validation_context_t* ctx, uint32_t timeout_ms) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    ctx->validation_timeout_ms = timeout_ms;
    return CHRONOS_OK;

chronos_error_t validation_context_set_max_depth(validation_context_t* ctx, uint32_t max_depth) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    ctx->max_validation_depth = max_depth;
    return CHRONOS_OK;

chronos_error_t validation_context_get_validation_state(validation_context_t* ctx, uint8_t* state) {
    if (ctx == NULL || state == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    *state = ctx->validation_state;
    return CHRONOS_OK;

chronos_error_t validation_context_clear_headers(validation_context_t* ctx) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    header_field_t* header = ctx->headers;
    while (header != NULL) {
        header_field_t* next = header->next;
        if (header->name != NULL) {
            chronos_free(header->name);
        if (header->value != NULL) {
            chronos_free(header->value);
        chronos_free(header);
        header = next;
    ctx->headers = NULL;
    ctx->header_count = 0;
    return CHRONOS_OK;

chronos_error_t validation_context_clear_rules(validation_context_t* ctx) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    validation_rule_t* rule = ctx->rules;
    while (rule != NULL) {
        validation_rule_t* next = rule->next;
        if (rule->field_name != NULL) {
            chronos_free(rule->field_name);
        if (rule->allowed_chars != NULL) {
            chronos_free(rule->allowed_chars);
        if (rule->forbidden_chars != NULL) {
            chronos_free(rule->forbidden_chars);
        if (rule->required_prefix != NULL) {
            chronos_free(rule->required_prefix);
        if (rule->required_suffix != NULL) {
            chronos_free(rule->required_suffix);
        chronos_free(rule);
        rule = next;
    ctx->rules = NULL;
    ctx->rule_count = 0;
    return CHRONOS_OK;

chronos_error_t validation_context_snapshot(validation_context_t* ctx, uint8_t* snapshot, 
                                             size_t* snapshot_len) {
    if (ctx == NULL || snapshot == NULL || snapshot_len == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    size_t required_len = sizeof(uint32_t) * 5 + sizeof(uint8_t) * 3;
    if (*snapshot_len < required_len) {
        *snapshot_len = required_len;
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    size_t offset = 0;
    memcpy(snapshot + offset, &ctx->header_count, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &ctx->rule_count, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &ctx->total_validations, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &ctx->validation_state, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(snapshot + offset, &ctx->enable_strict_mode, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    *snapshot_len = offset;
    return CHRONOS_OK;

chronos_error_t validation_context_restore(validation_context_t* ctx, const uint8_t* snapshot, 
                                           size_t snapshot_len) {
    if (ctx == NULL || snapshot == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    size_t offset = 0;
    memcpy(&ctx->header_count, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&ctx->rule_count, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&ctx->total_validations, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&ctx->validation_state, snapshot + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(&ctx->enable_strict_mode, snapshot + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    return CHRONOS_OK;
