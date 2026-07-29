#ifndef CHRONOS_CONNECTION_STATE_H
#define CHRONOS_CONNECTION_STATE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common.h"

typedef struct connection_event {
    uint32_t event_id;
    uint64_t timestamp;
    uint8_t event_type;
    uint32_t stream_id;
    uint32_t related_stream_id;
    uint8_t frame_type;
    uint32_t frame_length;
    uint8_t event_flags;
    int32_t window_delta;
    uint32_t settings_id;
    uint32_t settings_value;
    uint8_t error_code;
    char error_message[128];
    struct connection_event* next;
    struct connection_event* prev;
    uint32_t sequence_number;
    bool is_processed;
    uint64_t processing_time;
} connection_event_t;

typedef struct state_transition {
    connection_state_t from_state;
    connection_state_t to_state;
    uint8_t trigger_type;
    uint32_t trigger_value;
    uint64_t timestamp;
    bool is_valid;
    uint8_t transition_flags;
    uint32_t event_count;
    connection_event_t* trigger_event;
    struct state_transition* next;
    struct state_transition* prev;
    uint32_t transition_id;
    uint32_t parent_transition_id;
} state_transition_t;

typedef struct connection_state_machine {
    connection_state_t current_state;
    connection_state_t previous_state;
    connection_state_t target_state;
    uint64_t state_entry_time;
    uint64_t state_exit_time;
    uint32_t state_duration_ms;
    uint32_t state_change_count;
    uint32_t total_state_changes;
    bool is_transitioning;
    bool is_blocked;
    uint8_t block_reason;
    uint32_t block_count;
    uint64_t block_start_time;
    uint32_t max_state_duration_ms;
    uint32_t min_state_duration_ms;
    uint32_t state_timeout_ms;
    bool enable_timeout_check;
    bool enable_state_validation;
    uint32_t validation_counter;
    uint32_t invalid_state_count;
    bool is_corrupted;
    uint32_t corruption_detection_counter;
    uint32_t transition_history_size;
    state_transition_t* transition_history;
    state_transition_t* transition_history_tail;
    uint32_t event_queue_size;
    connection_event_t* event_queue_head;
    connection_event_t* event_queue_tail;
    uint32_t pending_events;
    uint32_t processed_events;
    uint32_t dropped_events;
    uint32_t max_queue_depth;
    uint32_t current_queue_depth;
    uint32_t connection_id;
    uint64_t connection_creation_time;
    uint32_t state_machine_version;
    uint32_t last_transition_id;
    uint32_t last_event_id;
    uint8_t state_flags;
    uint32_t concurrent_transitions;
    uint32_t max_concurrent_transitions;
    bool enable_concurrent_transitions;
    uint32_t rollback_count;
    uint32_t commit_count;
    uint32_t abort_count;
} connection_state_machine_t;

typedef struct state_machine_stats {
    uint32_t total_transitions;
    uint32_t total_events;
    uint32_t total_timeouts;
    uint32_t total_blocks;
    uint32_t total_validations;
    uint32_t total_corruptions;
    uint32_t average_state_duration_ms;
    uint32_t peak_queue_depth;
    uint32_t current_state_duration_ms;
    uint32_t rollback_count;
    uint32_t commit_count;
    uint32_t abort_count;
    uint64_t total_processing_time_ms;
    uint32_t state_change_frequency;
} state_machine_stats_t;

typedef struct state_validation_result {
    bool is_valid;
    uint8_t validation_code;
    char validation_message[256];
    uint32_t validation_time_ms;
    connection_state_t expected_state;
    connection_state_t actual_state;
    uint32_t mismatch_count;
    uint32_t warning_count;
    uint32_t error_count;
} state_validation_result_t;

connection_state_machine_t* connection_state_machine_create(uint32_t connection_id);
void connection_state_machine_destroy(connection_state_machine_t* sm);
chronos_error_t connection_state_transition(connection_state_machine_t* sm, connection_state_t new_state, 
                                           uint8_t trigger_type, uint32_t trigger_value);
chronos_error_t connection_state_add_event(connection_state_machine_t* sm, connection_event_t* event);
chronos_error_t connection_state_process_events(connection_state_machine_t* sm);
chronos_error_t connection_state_validate(connection_state_machine_t* sm, state_validation_result_t* result);
chronos_error_t connection_state_check_timeout(connection_state_machine_t* sm);
chronos_error_t connection_state_block(connection_state_machine_t* sm, uint8_t block_reason);
chronos_error_t connection_state_unblock(connection_state_machine_t* sm);
chronos_error_t connection_state_rollback(connection_state_machine_t* sm, uint32_t transition_id);
chronos_error_t connection_state_commit(connection_state_machine_t* sm);
chronos_error_t connection_state_abort(connection_state_machine_t* sm);
chronos_error_t connection_state_get_stats(connection_state_machine_t* sm, state_machine_stats_t* stats);
chronos_error_t connection_state_detect_corruption(connection_state_machine_t* sm);
chronos_error_t connection_state_recover_corruption(connection_state_machine_t* sm);
chronos_error_t connection_state_compact_history(connection_state_machine_t* sm);
chronos_error_t connection_state_clear_events(connection_state_machine_t* sm);
chronos_error_t connection_state_set_timeout(connection_state_machine_t* sm, uint32_t timeout_ms);
chronos_error_t connection_state_enable_validation(connection_state_machine_t* sm, bool enable);
chronos_error_t connection_state_enable_concurrent(connection_state_machine_t* sm, bool enable);
chronos_error_t connection_state_get_current_state(connection_state_machine_t* sm, connection_state_t* state);
chronos_error_t connection_state_get_previous_state(connection_state_machine_t* sm, connection_state_t* state);
chronos_error_t connection_state_get_state_duration(connection_state_machine_t* sm, uint32_t* duration_ms);
chronos_error_t connection_state_add_transition_to_history(connection_state_machine_t* sm, 
                                                           state_transition_t* transition);
chronos_error_t connection_state_find_transition(connection_state_machine_t* sm, uint32_t transition_id, 
                                                  state_transition_t** transition);
chronos_error_t connection_state_validate_transition(connection_state_machine_t* sm, 
                                                     connection_state_t from_state, connection_state_t to_state);
chronos_error_t connection_state_check_state_invariant(connection_state_machine_t* sm);
chronos_error_t connection_state_reset(connection_state_machine_t* sm);
chronos_error_t connection_state_snapshot(connection_state_machine_t* sm, uint8_t* snapshot, size_t* snapshot_len);
chronos_error_t connection_state_restore(connection_state_machine_t* sm, const uint8_t* snapshot, size_t snapshot_len);

