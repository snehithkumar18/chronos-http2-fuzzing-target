#ifndef CHRONOS_HEADER_VALIDATOR_H
#define CHRONOS_HEADER_VALIDATOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common.h"

typedef struct header_field {
    char* name;
    char* value;
    size_t name_len;
    size_t value_len;
    uint32_t field_id;
    uint8_t field_type;
    uint8_t validation_flags;
    bool is_valid;
    bool is_sensitive;
    bool is_pseudo;
    uint8_t security_level;
    uint32_t ref_count;
    uint64_t creation_time;
    uint64_t last_validation_time;
    uint32_t validation_count;
    uint32_t validation_failures;
    struct header_field* next;
    struct header_field* prev;
    uint32_t hash;
    uint8_t compression_flags;
    uint32_t dependency_id;
    bool has_dependency;
} header_field_t;

typedef struct validation_rule {
    uint32_t rule_id;
    char* field_name;
    size_t field_name_len;
    uint8_t rule_type;
    uint32_t min_length;
    uint32_t max_length;
    char* allowed_chars;
    char* forbidden_chars;
    char* required_prefix;
    char* required_suffix;
    uint32_t max_occurrences;
    uint32_t min_occurrences;
    bool case_sensitive;
    bool allow_empty;
    bool allow_whitespace;
    bool allow_special_chars;
    uint8_t validation_level;
    uint32_t priority;
    bool is_enabled;
    uint64_t rule_creation_time;
    uint32_t rule_version;
    struct validation_rule* next;
    struct validation_rule* prev;
} validation_rule_t;

typedef struct validation_context {
    header_field_t* headers;
    validation_rule_t* rules;
    uint32_t header_count;
    uint32_t rule_count;
    uint32_t total_validations;
    uint32_t total_passes;
    uint32_t total_failures;
    uint32_t total_skips;
    uint64_t validation_start_time;
    uint64_t last_validation_time;
    bool enable_strict_mode;
    bool enable_bypass_detection;
    bool enable_dependency_checking;
    bool enable_order_validation;
    bool enable_duplicate_detection;
    bool enable_injection_detection;
    bool enable_size_validation;
    bool enable_char_validation;
    uint32_t max_header_count;
    uint32_t max_header_size;
    uint32_t max_total_size;
    uint32_t validation_timeout_ms;
    uint64_t last_validation_duration_ms;
    bool is_corrupted;
    uint32_t corruption_detection_counter;
    uint32_t bypass_attempts;
    uint32_t detected_bypasses;
    uint32_t injection_attempts;
    uint32_t detected_injections;
    uint32_t duplicate_count;
    uint32_t order_violations;
    uint32_t size_violations;
    uint32_t char_violations;
    uint32_t dependency_violations;
    uint8_t validation_state;
    uint32_t current_validation_id;
    uint32_t max_validation_depth;
    uint32_t current_depth;
    bool validation_in_progress;
    bool validation_aborted;
    uint8_t abort_reason;
    uint32_t pending_validations;
    uint32_t max_pending_validations;
    uint8_t context_id;
} validation_context_t;

typedef struct validation_result {
    bool is_valid;
    uint8_t validation_code;
    char validation_message[256];
    uint32_t header_id;
    uint32_t rule_id;
    uint64_t validation_time;
    uint32_t validation_duration_ms;
    uint8_t severity;
    bool is_bypass_attempt;
    bool is_injection_attempt;
    bool is_duplicate;
    bool is_order_violation;
    bool is_size_violation;
    bool is_char_violation;
    bool is_dependency_violation;
    uint32_t violation_count;
    uint32_t warning_count;
    char detailed_message[512];
} validation_result_t;

typedef struct validation_stats {
    uint32_t total_headers_validated;
    uint32_t total_rules_executed;
    uint32_t total_bypass_attempts;
    uint32_t total_injection_attempts;
    uint32_t total_duplicates_detected;
    uint32_t total_order_violations;
    uint32_t total_size_violations;
    uint32_t total_char_violations;
    uint32_t total_dependency_violations;
    uint32_t average_validation_time_ms;
    uint32_t peak_validation_time_ms;
    uint32_t validation_success_rate;
    uint32_t strict_mode_violations;
    uint32_t bypass_detection_rate;
    uint32_t injection_detection_rate;
    uint64_t total_validation_time_ms;
} validation_stats_t;

validation_context_t* validation_context_create(uint32_t max_header_count, uint32_t max_header_size, 
                                                  uint8_t context_id);
void validation_context_destroy(validation_context_t* ctx);
chronos_error_t validation_context_add_header(validation_context_t* ctx, const char* name, size_t name_len,
                                              const char* value, size_t value_len);
chronos_error_t validation_context_add_rule(validation_context_t* ctx, const char* field_name, 
                                            size_t field_name_len, uint8_t rule_type, uint32_t min_length,
                                            uint32_t max_length);
chronos_error_t validation_context_validate(validation_context_t* ctx, validation_result_t* result);
chronos_error_t validation_context_validate_header(validation_context_t* ctx, uint32_t header_id,
                                                   validation_result_t* result);
chronos_error_t validation_context_validate_all(validation_context_t* ctx, validation_result_t* results,
                                                 uint32_t* result_count);
chronos_error_t validation_context_detect_bypass(validation_context_t* ctx, validation_result_t* result);
chronos_error_t validation_context_detect_injection(validation_context_t* ctx, validation_result_t* result);
chronos_error_t validation_context_detect_duplicates(validation_context_t* ctx, validation_result_t* result);
chronos_error_t validation_context_validate_order(validation_context_t* ctx, validation_result_t* result);
chronos_error_t validation_context_validate_size(validation_context_t* ctx, validation_result_t* result);
chronos_error_t validation_context_validate_chars(validation_context_t* ctx, validation_result_t* result);
chronos_error_t validation_context_validate_dependencies(validation_context_t* ctx, 
                                                          validation_result_t* result);
chronos_error_t validation_context_enable_strict_mode(validation_context_t* ctx, bool enable);
chronos_error_t validation_context_enable_bypass_detection(validation_context_t* ctx, bool enable);
chronos_error_t validation_context_get_stats(validation_context_t* ctx, validation_stats_t* stats);
chronos_error_t validation_context_reset(validation_context_t* ctx);
chronos_error_t validation_context_compact(validation_context_t* ctx);
chronos_error_t validation_context_detect_corruption(validation_context_t* ctx);
chronos_error_t validation_context_recover_corruption(validation_context_t* ctx);
chronos_error_t validation_context_remove_header(validation_context_t* ctx, uint32_t header_id);
chronos_error_t validation_context_remove_rule(validation_context_t* ctx, uint32_t rule_id);
chronos_error_t validation_context_find_header(validation_context_t* ctx, const char* name, 
                                                size_t name_len, header_field_t** header);
chronos_error_t validation_context_find_rule(validation_context_t* ctx, const char* field_name,
                                              size_t field_name_len, validation_rule_t** rule);
chronos_error_t validation_context_add_header_dependency(validation_context_t* ctx, uint32_t header_id,
                                                         uint32_t dependency_id);
chronos_error_t validation_context_remove_header_dependency(validation_context_t* ctx, uint32_t header_id,
                                                            uint32_t dependency_id);
chronos_error_t validation_context_check_dependency_chain(validation_context_t* ctx, uint32_t header_id,
                                                          validation_result_t* result);
chronos_error_t validation_context_abort_validation(validation_context_t* ctx, uint8_t reason);
chronos_error_t validation_context_resume_validation(validation_context_t* ctx);
chronos_error_t validation_context_set_timeout(validation_context_t* ctx, uint32_t timeout_ms);
chronos_error_t validation_context_set_max_depth(validation_context_t* ctx, uint32_t max_depth);
chronos_error_t validation_context_get_validation_state(validation_context_t* ctx, uint8_t* state);
chronos_error_t validation_context_clear_headers(validation_context_t* ctx);
chronos_error_t validation_context_clear_rules(validation_context_t* ctx);
chronos_error_t validation_context_snapshot(validation_context_t* ctx, uint8_t* snapshot, 
                                             size_t* snapshot_len);
chronos_error_t validation_context_restore(validation_context_t* ctx, const uint8_t* snapshot, 
                                           size_t snapshot_len);

