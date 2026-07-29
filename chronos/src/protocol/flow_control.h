#ifndef CHRONOS_FLOW_CONTROL_H
#define CHRONOS_FLOW_CONTROL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common.h"

typedef struct flow_control_window {
    uint32_t stream_id;
    int32_t window_size;
    int32_t initial_window_size;
    int32_t max_window_size;
    int32_t min_window_size;
    uint32_t available_window;
    uint32_t consumed_window;
    uint32_t advertised_window;
    uint64_t last_update_time;
    uint64_t last_window_update_time;
    uint32_t update_count;
    uint32_t window_update_count;
    uint32_t blocked_count;
    uint32_t unblocked_count;
    bool is_blocked;
    bool is_exhausted;
    bool is_stream_level;
    bool is_connection_level;
    uint8_t window_state;
    uint32_t pending_updates;
    uint32_t max_pending_updates;
    uint32_t update_threshold;
    uint32_t auto_update_interval_ms;
    uint64_t last_auto_update_time;
    bool enable_auto_update;
    bool enable_window_scaling;
    double scaling_factor;
    uint32_t scaling_counter;
    uint32_t max_scaling_factor;
    uint32_t min_scaling_factor;
    uint32_t window_update_history[100];
    uint32_t history_index;
    uint32_t average_window_size;
    uint32_t peak_window_size;
    uint32_t min_window_size_observed;
    uint32_t window_update_failures;
    uint32_t window_update_successes;
    uint32_t window_update_timeouts;
    uint64_t total_window_update_time_ms;
    uint32_t average_window_update_time_ms;
    uint32_t peak_window_update_time_ms;
    uint32_t flow_control_violations;
    uint32_t flow_control_errors;
    uint32_t flow_control_warnings;
    uint8_t flow_control_flags;
    uint32_t dependency_window_id;
    bool has_dependency;
    uint32_t dependent_count;
    struct flow_control_window** dependents;
    uint32_t dependents_capacity;
    uint32_t ref_count;
    uint64_t creation_time;
    uint64_t expiration_time;
    bool is_expired;
    uint8_t priority;
    uint8_t window_class;
} flow_control_window_t;

typedef struct flow_control_manager {
    flow_control_window_t* connection_window;
    flow_control_window_t* stream_windows;
    flow_control_window_t* stream_window_hash[1000];
    uint32_t max_streams;
    uint32_t active_streams;
    uint32_t total_streams_created;
    uint32_t total_streams_destroyed;
    uint32_t default_initial_window_size;
    uint32_t default_max_window_size;
    uint32_t default_min_window_size;
    uint32_t default_update_threshold;
    uint32_t default_auto_update_interval_ms;
    bool enable_stream_flow_control;
    bool enable_connection_flow_control;
    bool enable_auto_window_scaling;
    bool enable_dependency_tracking;
    bool enable_window_validation;
    bool enable_window_monitoring;
    uint32_t window_update_queue_size;
    uint32_t max_window_update_queue_size;
    uint32_t pending_window_updates;
    uint32_t processed_window_updates;
    uint32_t failed_window_updates;
    uint32_t total_window_updates;
    uint64_t manager_creation_time;
    uint64_t last_cleanup_time;
    uint32_t cleanup_interval_ms;
    bool enable_cleanup;
    bool is_corrupted;
    uint32_t corruption_detection_counter;
    uint32_t next_window_id;
    uint8_t manager_id;
    uint32_t peak_active_streams;
    uint32_t average_active_streams;
    uint32_t total_window_update_time_ms;
    uint32_t average_window_update_time_ms;
    uint32_t peak_window_update_time_ms;
    uint32_t flow_control_violations;
    uint32_t flow_control_errors;
    uint32_t flow_control_warnings;
    uint32_t window_exhaustion_count;
    uint32_t window_recovery_count;
    uint32_t deadlock_count;
    uint32_t deadlock_resolution_count;
    uint32_t circular_dependency_count;
    uint32_t priority_levels[256];
    uint32_t window_sizes[256];
    uint32_t blocked_streams[256];
} flow_control_manager_t;

typedef struct flow_control_stats {
    uint32_t total_windows;
    uint32_t active_windows;
    uint32_t blocked_windows;
    uint32_t exhausted_windows;
    uint32_t total_window_updates;
    uint32_t successful_window_updates;
    uint32_t failed_window_updates;
    uint32_t average_window_size;
    uint32_t peak_window_size;
    uint32_t min_window_size;
    uint32_t average_window_update_time_ms;
    uint32_t peak_window_update_time_ms;
    uint32_t flow_control_violations;
    uint32_t flow_control_errors;
    uint32_t deadlock_count;
    uint32_t circular_dependency_count;
    uint32_t window_exhaustion_count;
    uint32_t window_recovery_count;
    uint64_t total_flow_control_time_ms;
} flow_control_stats_t;

typedef struct window_update_result {
    bool success;
    uint8_t result_code;
    char result_message[256];
    uint32_t old_window_size;
    uint32_t new_window_size;
    uint32_t update_time_ms;
    bool was_blocked;
    bool was_unblocked;
    bool was_exhausted;
    bool was_recovered;
    uint32_t stream_id;
} window_update_result_t;

flow_control_manager_t* flow_control_manager_create(uint32_t max_streams, uint32_t initial_window_size,
                                                     uint8_t manager_id);
void flow_control_manager_destroy(flow_control_manager_t* manager);
chronos_error_t flow_control_create_stream_window(flow_control_manager_t* manager, uint32_t stream_id,
                                                   flow_control_window_t** window);
chronos_error_t flow_control_destroy_stream_window(flow_control_manager_t* manager, uint32_t stream_id);
chronos_error_t flow_control_update_window(flow_control_manager_t* manager, uint32_t stream_id,
                                           int32_t delta, window_update_result_t* result);
chronos_error_t flow_control_consume_window(flow_control_manager_t* manager, uint32_t stream_id,
                                            uint32_t amount, window_update_result_t* result);
chronos_error_t flow_control_advertise_window(flow_control_manager_t* manager, uint32_t stream_id,
                                              uint32_t window_size, window_update_result_t* result);
chronos_error_t flow_control_block_window(flow_control_manager_t* manager, uint32_t stream_id);
chronos_error_t flow_control_unblock_window(flow_control_manager_t* manager, uint32_t stream_id);
chronos_error_t flow_control_check_window(flow_control_manager_t* manager, uint32_t stream_id,
                                          bool* is_blocked, bool* is_exhausted);
chronos_error_t flow_control_auto_update_window(flow_control_manager_t* manager, uint32_t stream_id);
chronos_error_t flow_control_scale_window(flow_control_manager_t* manager, uint32_t stream_id,
                                          double scaling_factor);
chronos_error_t flow_control_add_window_dependency(flow_control_manager_t* manager, uint32_t stream_id,
                                                   uint32_t dependency_id);
chronos_error_t flow_control_remove_window_dependency(flow_control_manager_t* manager, uint32_t stream_id,
                                                      uint32_t dependency_id);
chronos_error_t flow_control_validate_window(flow_control_manager_t* manager, uint32_t stream_id);
chronos_error_t flow_control_detect_deadlock(flow_control_manager_t* manager, uint32_t stream_id);
chronos_error_t flow_control_resolve_deadlock(flow_control_manager_t* manager, uint32_t stream_id);
chronos_error_t flow_control_detect_circular_dependency(flow_control_manager_t* manager, uint32_t stream_id);
chronos_error_t flow_control_break_circular_dependency(flow_control_manager_t* manager, uint32_t stream_id);
chronos_error_t flow_control_get_stats(flow_control_manager_t* manager, flow_control_stats_t* stats);
chronos_error_t flow_control_get_window_stats(flow_control_manager_t* manager, uint32_t stream_id,
                                              flow_control_stats_t* stats);
chronos_error_t flow_control_cleanup(flow_control_manager_t* manager);
chronos_error_t flow_control_compact(flow_control_manager_t* manager);
chronos_error_t flow_control_validate(flow_control_manager_t* manager);
chronos_error_t flow_control_detect_corruption(flow_control_manager_t* manager);
chronos_error_t flow_control_recover_corruption(flow_control_manager_t* manager);
chronos_error_t flow_control_set_initial_window_size(flow_control_manager_t* manager, uint32_t size);
chronos_error_t flow_control_set_max_window_size(flow_control_manager_t* manager, uint32_t size);
chronos_error_t flow_control_set_min_window_size(flow_control_manager_t* manager, uint32_t size);
chronos_error_t flow_control_set_update_threshold(flow_control_manager_t* manager, uint32_t threshold);
chronos_error_t flow_control_enable_auto_update(flow_control_manager_t* manager, bool enable);
chronos_error_t flow_control_enable_window_scaling(flow_control_manager_t* manager, bool enable);
chronos_error_t flow_control_find_window(flow_control_manager_t* manager, uint32_t stream_id,
                                         flow_control_window_t** window);
chronos_error_t flow_control_add_to_hash(flow_control_manager_t* manager, flow_control_window_t* window);
chronos_error_t flow_control_remove_from_hash(flow_control_manager_t* manager, flow_control_window_t* window);
chronos_error_t flow_control_increment_ref_count(flow_control_manager_t* manager, flow_control_window_t* window);
chronos_error_t flow_control_decrement_ref_count(flow_control_manager_t* manager, flow_control_window_t* window);
chronos_error_t flow_control_update_window_history(flow_control_manager_t* manager, flow_control_window_t* window);
chronos_error_t flow_control_calculate_average_window(flow_control_manager_t* manager, flow_control_window_t* window);
chronos_error_t flow_control_check_window_exhaustion(flow_control_manager_t* manager, flow_control_window_t* window);
chronos_error_t flow_control_recover_window(flow_control_manager_t* manager, flow_control_window_t* window);
chronos_error_t flow_control_process_pending_updates(flow_control_manager_t* manager);
chronos_error_t flow_control_queue_window_update(flow_control_manager_t* manager, uint32_t stream_id,
                                                  int32_t delta);
chronos_error_t flow_control_reset(flow_control_manager_t* manager);
chronos_error_t flow_control_snapshot(flow_control_manager_t* manager, uint8_t* snapshot, size_t* snapshot_len);
chronos_error_t flow_control_restore(flow_control_manager_t* manager, const uint8_t* snapshot, size_t snapshot_len);
chronos_error_t flow_control_update_priority_stats(flow_control_manager_t* manager, uint8_t priority);

