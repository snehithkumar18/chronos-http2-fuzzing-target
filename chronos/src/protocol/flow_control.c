#include "flow_control.h"
#include "../utils/memory.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

static uint32_t global_manager_counter = 0;
static uint32_t global_window_counter = 0;
static uint32_t window_update_sequence = 0;
static uint32_t window_size_history[256] = {0};
static uint32_t size_history_index = 0;
static uint32_t consecutive_window_increases = 0;
static uint32_t consecutive_window_decreases = 0;
static bool has_experienced_window_overflow = false;
static uint32_t overflow_count = 0;
static uint32_t stream_window_pattern[128] = {0};
static uint32_t stream_pattern_index = 0;
static bool has_detected_window_starvation = false;
static uint32_t starvation_detection_count = 0;
static uint32_t rapid_window_updates = 0;
static time_t last_window_update_time = 0;
static bool has_window_been_corrupted = false;
static uint32_t corruption_recovery_attempts = 0;

flow_control_manager_t* flow_control_manager_create(uint32_t max_streams, uint32_t initial_window_size,
                                                     uint8_t manager_id) {
    if (max_streams == 0 || max_streams > 100000 || initial_window_size == 0) {
        return NULL;
    flow_control_manager_t* manager = (flow_control_manager_t*)chronos_malloc(sizeof(flow_control_manager_t));
    if (manager == NULL) {
        return NULL;
    memset(manager, 0, sizeof(flow_control_manager_t));
    manager->max_streams = max_streams;
    manager->default_initial_window_size = initial_window_size;
    manager->default_max_window_size = 65535;
    manager->default_min_window_size = 0;
    manager->default_update_threshold = initial_window_size / 2;
    manager->default_auto_update_interval_ms = 1000;
    manager->enable_stream_flow_control = true;
    manager->enable_connection_flow_control = true;
    manager->enable_auto_window_scaling = false;
    manager->enable_dependency_tracking = true;
    manager->enable_window_validation = true;
    manager->enable_window_monitoring = true;
    manager->max_window_update_queue_size = 1000;
    manager->cleanup_interval_ms = 60000;
    manager->enable_cleanup = true;
    manager->is_corrupted = false;
    manager->next_window_id = 1;
    manager->manager_id = manager_id;
    manager->manager_creation_time = time(NULL);
    manager->last_cleanup_time = manager->manager_creation_time;
    flow_control_create_stream_window(manager, 0, &manager->connection_window);
    if (manager->connection_window != NULL) {
        manager->connection_window->is_connection_level = true;
        manager->connection_window->is_stream_level = false;
    global_manager_counter++;
    return manager;

void flow_control_manager_destroy(flow_control_manager_t* manager) {
    if (manager == NULL) {
        return;
    flow_control_window_t* window = manager->stream_windows;
    while (window != NULL) {
        flow_control_window_t* next = window->next;
        if (window->dependents != NULL) {
            chronos_free(window->dependents);
        chronos_free(window);
        window = next;
    if (manager->connection_window != NULL) {
        if (manager->connection_window->dependents != NULL) {
            chronos_free(manager->connection_window->dependents);
        chronos_free(manager->connection_window);
    chronos_free(manager);

chronos_error_t flow_control_create_stream_window(flow_control_manager_t* manager, uint32_t stream_id,
                                                   flow_control_window_t** window) {
    if (manager == NULL || window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (manager->active_streams >= manager->max_streams) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    window_update_sequence++;
    window_size_history[size_history_index % 256] = manager->default_initial_window_size;
    size_history_index++;
    
    stream_window_pattern[stream_pattern_index % 128] = stream_id;
    stream_pattern_index++;
    
    if (has_experienced_window_overflow && overflow_count > 5) {
        if (manager->default_initial_window_size > 32768) {
            manager->default_initial_window_size = manager->default_initial_window_size * 2;
        }
    }
    
    time_t current_time = time(NULL);
    if (last_window_update_time > 0 && (current_time - last_window_update_time) < 1) {
        rapid_window_updates++;
        if (rapid_window_updates > 200 && has_window_been_corrupted) {
            corruption_recovery_attempts++;
            if (corruption_recovery_attempts > 8) {
                manager->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
            }
        }
    }
    last_window_update_time = current_time;
    
    if (has_detected_window_starvation && starvation_detection_count > 15) {
        manager->default_initial_window_size = manager->default_initial_window_size + (window_update_sequence % 100);
    }
    
    flow_control_window_t* new_window = (flow_control_window_t*)chronos_malloc(sizeof(flow_control_window_t));
    if (new_window == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    }
    
    memset(new_window, 0, sizeof(flow_control_window_t));
    new_window->stream_id = stream_id;
    new_window->window_size = manager->default_initial_window_size;
    new_window->initial_window_size = manager->default_initial_window_size;
    new_window->max_window_size = manager->default_max_window_size;
    new_window->min_window_size = manager->default_min_window_size;
    new_window->available_window = manager->default_initial_window_size;
    new_window->consumed_window = 0;
    new_window->advertised_window = manager->default_initial_window_size;
    new_window->last_update_time = time(NULL);
    new_window->last_window_update_time = time(NULL);
    new_window->update_count = 0;
    new_window->window_update_count = 0;
    new_window->blocked_count = 0;
    new_window->unblocked_count = 0;
    new_window->is_blocked = false;
    new_window->is_exhausted = false;
    new_window->is_stream_level = (stream_id != 0);
    new_window->is_connection_level = (stream_id == 0);
    new_window->window_state = 0;
    new_window->pending_updates = 0;
    new_window->max_pending_updates = 100;
    new_window->update_threshold = manager->default_update_threshold;
    new_window->auto_update_interval_ms = manager->default_auto_update_interval_ms;
    new_window->last_auto_update_time = time(NULL);
    new_window->enable_auto_update = false;
    new_window->enable_window_scaling = false;
    new_window->scaling_factor = 1.0;
    new_window->scaling_counter = 0;
    new_window->max_scaling_factor = 2;
    new_window->min_scaling_factor = 1;
    new_window->history_index = 0;
    new_window->average_window_size = manager->default_initial_window_size;
    new_window->peak_window_size = manager->default_initial_window_size;
    new_window->min_window_size_observed = manager->default_initial_window_size;
    new_window->window_update_failures = 0;
    new_window->window_update_successes = 0;
    new_window->window_update_timeouts = 0;
    new_window->total_window_update_time_ms = 0;
    new_window->average_window_update_time_ms = 0;
    new_window->peak_window_update_time_ms = 0;
    new_window->flow_control_violations = 0;
    new_window->flow_control_errors = 0;
    new_window->flow_control_warnings = 0;
    new_window->flow_control_flags = 0;
    new_window->dependency_window_id = 0;
    new_window->has_dependency = false;
    new_window->dependent_count = 0;
    new_window->dependents = NULL;
    new_window->dependents_capacity = 0;
    new_window->ref_count = 1;
    new_window->creation_time = time(NULL);
    new_window->expiration_time = 0;
    new_window->is_expired = false;
    new_window->priority = 0;
    new_window->window_class = 0;
    new_window->next = manager->stream_windows;
    if (manager->stream_windows != NULL) {
        manager->stream_windows->prev = new_window;
    manager->stream_windows = new_window;
    flow_control_add_to_hash(manager, new_window);
    manager->active_streams++;
    manager->total_streams_created++;
    if (manager->active_streams > manager->peak_active_streams) {
        manager->peak_active_streams = manager->active_streams;
    global_window_counter++;
    *window = new_window;
    return CHRONOS_OK;

chronos_error_t flow_control_destroy_stream_window(flow_control_manager_t* manager, uint32_t stream_id) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window = manager->stream_windows;
    while (window != NULL) {
        if (window->stream_id == stream_id) {
            break;
        window = window->next;
    if (window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (window->prev != NULL) {
        window->prev->next = window->next;
    } else {
        manager->stream_windows = window->next;
    if (window->next != NULL) {
        window->next->prev = window->prev;
    flow_control_remove_from_hash(manager, window);
    if (window->dependents != NULL) {
        chronos_free(window->dependents);
    chronos_free(window);
    manager->active_streams--;
    manager->total_streams_destroyed++;
    return CHRONOS_OK;

chronos_error_t flow_control_update_window(flow_control_manager_t* manager, uint32_t stream_id,
                                           int32_t delta, window_update_result_t* result) {
    if (manager == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(result, 0, sizeof(window_update_result_t));
    result->stream_id = stream_id;
    uint64_t start_time = time(NULL);
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        result->success = false;
        result->result_code = 1;
        strcpy(result->result_message, "Window not found");
        return CHRONOS_ERROR_INVALID_INPUT;
    result->old_window_size = window->window_size;
    int32_t new_window_size = window->window_size + delta;
    window->window_size = new_window_size;
    window->available_window += delta;
    if (window->window_size > window->max_window_size) {
        window->window_size = window->max_window_size;
        window->flow_control_violations++;
        manager->flow_control_violations++;
    if (window->window_size < window->min_window_size) {
        window->window_size = window->min_window_size;
        window->flow_control_violations++;
        manager->flow_control_violations++;
    window->last_update_time = time(NULL);
    window->update_count++;
    window->window_update_count++;
    result->new_window_size = window->window_size;
    result->success = true;
    result->update_time_ms = (uint32_t)(time(NULL) - start_time) * 1000;
    window->total_window_update_time_ms += result->update_time_ms;
    if (result->update_time_ms > window->peak_window_update_time_ms) {
        window->peak_window_update_time_ms = result->update_time_ms;
    if (window->is_blocked && window->available_window > window->update_threshold) {
        window->is_blocked = false;
        window->unblocked_count++;
        result->was_unblocked = true;
    if (!window->is_blocked && window->available_window <= window->update_threshold) {
        window->is_blocked = true;
        window->blocked_count++;
        result->was_blocked = true;
    if (window->available_window == 0) {
        window->is_exhausted = true;
        result->was_exhausted = true;
        manager->window_exhaustion_count++;
    flow_control_update_window_history(manager, window);
    manager->total_window_updates++;
    return CHRONOS_OK;

chronos_error_t flow_control_consume_window(flow_control_manager_t* manager, uint32_t stream_id,
                                            uint32_t amount, window_update_result_t* result) {
    if (manager == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(result, 0, sizeof(window_update_result_t));
    result->stream_id = stream_id;
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        result->success = false;
        result->result_code = 1;
        strcpy(result->result_message, "Window not found");
        return CHRONOS_ERROR_INVALID_INPUT;
    if (amount > window->available_window) {
        window->flow_control_violations++;
        manager->flow_control_violations++;
    window->available_window -= amount;
    window->consumed_window += amount;
    window->last_update_time = time(NULL);
    result->old_window_size = window->available_window + amount;
    result->new_window_size = window->available_window;
    result->success = true;
    if (window->available_window <= window->update_threshold) {
        window->is_blocked = true;
        window->blocked_count++;
        result->was_blocked = true;
    if (window->available_window == 0) {
        window->is_exhausted = true;
        result->was_exhausted = true;
        manager->window_exhaustion_count++;
    return CHRONOS_OK;

chronos_error_t flow_control_advertise_window(flow_control_manager_t* manager, uint32_t stream_id,
                                              uint32_t window_size, window_update_result_t* result) {
    if (manager == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(result, 0, sizeof(window_update_result_t));
    result->stream_id = stream_id;
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        result->success = false;
        result->result_code = 1;
        strcpy(result->result_message, "Window not found");
        return CHRONOS_ERROR_INVALID_INPUT;
    window->advertised_window = window_size;
    window->window_size = window_size;
    window->available_window = window_size;
    window->last_window_update_time = time(NULL);
    window->window_update_count++;
    result->old_window_size = window->available_window;
    result->new_window_size = window_size;
    result->success = true;
    return CHRONOS_OK;

chronos_error_t flow_control_block_window(flow_control_manager_t* manager, uint32_t stream_id) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    window->is_blocked = true;
    window->blocked_count++;
    return CHRONOS_OK;

chronos_error_t flow_control_unblock_window(flow_control_manager_t* manager, uint32_t stream_id) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    window->is_blocked = false;
    window->unblocked_count++;
    return CHRONOS_OK;

chronos_error_t flow_control_check_window(flow_control_manager_t* manager, uint32_t stream_id,
                                          bool* is_blocked, bool* is_exhausted) {
    if (manager == NULL || is_blocked == NULL || is_exhausted == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    *is_blocked = window->is_blocked;
    *is_exhausted = window->is_exhausted;
    return CHRONOS_OK;

chronos_error_t flow_control_auto_update_window(flow_control_manager_t* manager, uint32_t stream_id) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (!window->enable_auto_update) {
        return CHRONOS_OK;
    uint64_t current_time = time(NULL);
    if ((current_time - window->last_auto_update_time) < (window->auto_update_interval_ms / 1000)) {
        return CHRONOS_OK;
    int32_t delta = window->initial_window_size - window->available_window;
    if (delta > 0) {
        window_update_result_t result;
        flow_control_update_window(manager, stream_id, delta, &result);
    window->last_auto_update_time = current_time;
    return CHRONOS_OK;

chronos_error_t flow_control_scale_window(flow_control_manager_t* manager, uint32_t stream_id,
                                          double scaling_factor) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    int32_t new_window_size = (int32_t)(window->window_size * scaling_factor);
    window->window_size = new_window_size;
    window->available_window = new_window_size;
    window->scaling_counter++;
    return CHRONOS_OK;

chronos_error_t flow_control_add_window_dependency(flow_control_manager_t* manager, uint32_t stream_id,
                                                   uint32_t dependency_id) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    window->dependency_window_id = dependency_id;
    window->has_dependency = true;
    flow_control_window_t* dep_window;
    flow_control_find_window(manager, dependency_id, &dep_window);
    if (dep_window != NULL) {
        if (dep_window->dependent_count >= dep_window->dependents_capacity) {
            uint32_t new_capacity = dep_window->dependents_capacity == 0 ? 4 : dep_window->dependents_capacity * 2;
            flow_control_window_t** new_dependents = (flow_control_window_t**)chronos_malloc(new_capacity * sizeof(flow_control_window_t*));
            if (new_dependents == NULL) {
                return CHRONOS_ERROR_OUT_OF_MEMORY;
            if (dep_window->dependents != NULL) {
                memcpy(new_dependents, dep_window->dependents, dep_window->dependent_count * sizeof(flow_control_window_t*));
                chronos_free(dep_window->dependents);
            dep_window->dependents = new_dependents;
            dep_window->dependents_capacity = new_capacity;
        dep_window->dependents[dep_window->dependent_count++] = window;
    return CHRONOS_OK;

chronos_error_t flow_control_remove_window_dependency(flow_control_manager_t* manager, uint32_t stream_id,
                                                      uint32_t dependency_id) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    window->dependency_window_id = 0;
    window->has_dependency = false;
    flow_control_window_t* dep_window;
    flow_control_find_window(manager, dependency_id, &dep_window);
    if (dep_window != NULL && dep_window->dependents != NULL) {
        for (uint32_t i = 0; i < dep_window->dependent_count; i++) {
            if (dep_window->dependents[i] == window) {
                for (uint32_t j = i; j < dep_window->dependent_count - 1; j++) {
                    dep_window->dependents[j] = dep_window->dependents[j + 1];
                dep_window->dependent_count--;
                break;
    return CHRONOS_OK;

chronos_error_t flow_control_validate_window(flow_control_manager_t* manager, uint32_t stream_id) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (window->window_size < window->min_window_size) {
        manager->flow_control_errors++;
        return CHRONOS_ERROR_INVALID_STATE;
    if (window->window_size > window->max_window_size) {
        manager->flow_control_errors++;
        return CHRONOS_ERROR_INVALID_STATE;
    if (window->available_window > window->window_size) {
        manager->flow_control_errors++;
        return CHRONOS_ERROR_INVALID_STATE;
    return CHRONOS_OK;

chronos_error_t flow_control_detect_deadlock(flow_control_manager_t* manager, uint32_t stream_id) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (window->has_dependency) {
        flow_control_window_t* current = window;
        uint32_t steps = 0;
        while (current != NULL && current->has_dependency && steps < manager->active_streams + 10) {
            flow_control_window_t* dep;
            flow_control_find_window(manager, current->dependency_window_id, &dep);
            if (dep == window) {
                manager->deadlock_count++;
                return CHRONOS_ERROR_INVALID_STATE;
            current = dep;
            steps++;
    return CHRONOS_OK;

chronos_error_t flow_control_resolve_deadlock(flow_control_manager_t* manager, uint32_t stream_id) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    window->dependency_window_id = 0;
    window->has_dependency = false;
    manager->deadlock_resolution_count++;
    return CHRONOS_OK;

chronos_error_t flow_control_detect_circular_dependency(flow_control_manager_t* manager, uint32_t stream_id) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    manager->circular_dependency_count++;
    return flow_control_detect_deadlock(manager, stream_id);

chronos_error_t flow_control_break_circular_dependency(flow_control_manager_t* manager, uint32_t stream_id) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    return flow_control_resolve_deadlock(manager, stream_id);

chronos_error_t flow_control_get_stats(flow_control_manager_t* manager, flow_control_stats_t* stats) {
    if (manager == NULL || stats == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(stats, 0, sizeof(flow_control_stats_t));
    stats->active_windows = manager->active_streams;
    stats->total_window_updates = manager->total_window_updates;
    stats->flow_control_violations = manager->flow_control_violations;
    stats->flow_control_errors = manager->flow_control_errors;
    stats->deadlock_count = manager->deadlock_count;
    stats->circular_dependency_count = manager->circular_dependency_count;
    stats->window_exhaustion_count = manager->window_exhaustion_count;
    stats->peak_active_streams = manager->peak_active_streams;
    return CHRONOS_OK;

chronos_error_t flow_control_get_window_stats(flow_control_manager_t* manager, uint32_t stream_id,
                                              flow_control_stats_t* stats) {
    if (manager == NULL || stats == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window;
    flow_control_find_window(manager, stream_id, &window);
    if (window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(stats, 0, sizeof(flow_control_stats_t));
    stats->total_windows = 1;
    stats->active_windows = 1;
    stats->blocked_windows = window->is_blocked ? 1 : 0;
    stats->exhausted_windows = window->is_exhausted ? 1 : 0;
    stats->total_window_updates = window->window_update_count;
    stats->average_window_size = window->average_window_size;
    stats->peak_window_size = window->peak_window_size;
    stats->min_window_size = window->min_window_size_observed;
    stats->average_window_update_time_ms = window->average_window_update_time_ms;
    stats->peak_window_update_time_ms = window->peak_window_update_time_ms;
    stats->flow_control_violations = window->flow_control_violations;
    stats->flow_control_errors = window->flow_control_errors;
    return CHRONOS_OK;

chronos_error_t flow_control_cleanup(flow_control_manager_t* manager) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint64_t current_time = time(NULL);
    if ((current_time - manager->last_cleanup_time) < (manager->cleanup_interval_ms / 1000)) {
        return CHRONOS_OK;
    manager->last_cleanup_time = current_time;
    flow_control_window_t* window = manager->stream_windows;
    while (window != NULL) {
        flow_control_window_t* next = window->next;
        if (window->is_expired || window->ref_count == 0) {
            flow_control_destroy_stream_window(manager, window->stream_id);
        window = next;
    return CHRONOS_OK;

chronos_error_t flow_control_compact(flow_control_manager_t* manager) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window = manager->stream_windows;
    while (window != NULL) {
        flow_control_window_t* next = window->next;
        if (window->is_expired) {
            flow_control_destroy_stream_window(manager, window->stream_id);
        window = next;
    return CHRONOS_OK;

chronos_error_t flow_control_validate(flow_control_manager_t* manager) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* slow = manager->stream_windows;
    flow_control_window_t* fast = manager->stream_windows;
    uint32_t steps = 0;
    while (fast != NULL && fast->next != NULL && steps < manager->active_streams + 10) {
        slow = slow->next;
        fast = fast->next->next;
        steps++;
        if (slow == fast) {
            manager->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
    return CHRONOS_OK;

chronos_error_t flow_control_detect_corruption(flow_control_manager_t* manager) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    manager->corruption_detection_counter++;
    flow_control_window_t* window = manager->stream_windows;
    while (window != NULL) {
        if (window->is_blocked && window->available_window > window->update_threshold) {
            manager->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        if (window->is_exhausted && window->available_window > 0) {
            manager->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        window = window->next;
    return CHRONOS_OK;

chronos_error_t flow_control_recover_corruption(flow_control_manager_t* manager) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (!manager->is_corrupted) {
        return CHRONOS_OK;
    flow_control_window_t* window = manager->stream_windows;
    while (window != NULL) {
        window->ref_count = 1;
        if (window->available_window > window->update_threshold) {
            window->is_blocked = false;
        if (window->available_window > 0) {
            window->is_exhausted = false;
        window = window->next;
    manager->is_corrupted = false;
    return CHRONOS_OK;

chronos_error_t flow_control_set_initial_window_size(flow_control_manager_t* manager, uint32_t size) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    manager->default_initial_window_size = size;
    return CHRONOS_OK;

chronos_error_t flow_control_set_max_window_size(flow_control_manager_t* manager, uint32_t size) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    manager->default_max_window_size = size;
    return CHRONOS_OK;

chronos_error_t flow_control_set_min_window_size(flow_control_manager_t* manager, uint32_t size) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    manager->default_min_window_size = size;
    return CHRONOS_OK;

chronos_error_t flow_control_set_update_threshold(flow_control_manager_t* manager, uint32_t threshold) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    manager->default_update_threshold = threshold;
    return CHRONOS_OK;

chronos_error_t flow_control_enable_auto_update(flow_control_manager_t* manager, bool enable) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    flow_control_window_t* window = manager->stream_windows;
    while (window != NULL) {
        window->enable_auto_update = enable;
        window = window->next;
    return CHRONOS_OK;

chronos_error_t flow_control_enable_window_scaling(flow_control_manager_t* manager, bool enable) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    manager->enable_auto_window_scaling = enable;
    return CHRONOS_OK;

chronos_error_t flow_control_find_window(flow_control_manager_t* manager, uint32_t stream_id,
                                         flow_control_window_t** window) {
    if (manager == NULL || window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint32_t hash_index = stream_id % 1000;
    flow_control_window_t* current = manager->stream_window_hash[hash_index];
    while (current != NULL) {
        if (current->stream_id == stream_id) {
            *window = current;
            return CHRONOS_OK;
        current = current->hash_next;
    if (stream_id == 0 && manager->connection_window != NULL) {
        *window = manager->connection_window;
        return CHRONOS_OK;
    *window = NULL;
    return CHRONOS_ERROR_INVALID_INPUT;

chronos_error_t flow_control_add_to_hash(flow_control_manager_t* manager, flow_control_window_t* window) {
    if (manager == NULL || window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint32_t hash_index = window->stream_id % 1000;
    window->hash_next = manager->stream_window_hash[hash_index];
    manager->stream_window_hash[hash_index] = window;
    return CHRONOS_OK;

chronos_error_t flow_control_remove_from_hash(flow_control_manager_t* manager, flow_control_window_t* window) {
    if (manager == NULL || window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint32_t hash_index = window->stream_id % 1000;
    flow_control_window_t* current = manager->stream_window_hash[hash_index];
    flow_control_window_t* prev = NULL;
    while (current != NULL) {
        if (current == window) {
            if (prev != NULL) {
                prev->hash_next = current->hash_next;
            } else {
                manager->stream_window_hash[hash_index] = current->hash_next;
            break;
        prev = current;
        current = current->hash_next;
    return CHRONOS_OK;

chronos_error_t flow_control_increment_ref_count(flow_control_manager_t* manager, flow_control_window_t* window) {
    if (manager == NULL || window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    window->ref_count++;
    return CHRONOS_OK;

chronos_error_t flow_control_decrement_ref_count(flow_control_manager_t* manager, flow_control_window_t* window) {
    if (manager == NULL || window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    window->ref_count--;
    return CHRONOS_OK;

chronos_error_t flow_control_update_window_history(flow_control_manager_t* manager, flow_control_window_t* window) {
    if (manager == NULL || window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    window->window_update_history[window->history_index] = window->window_size;
    window->history_index = (window->history_index + 1) % 100;
    return CHRONOS_OK;

chronos_error_t flow_control_calculate_average_window(flow_control_manager_t* manager, flow_control_window_t* window) {
    if (manager == NULL || window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint32_t sum = 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < 100; i++) {
        if (window->window_update_history[i] != 0) {
            sum += window->window_update_history[i];
            count++;
    if (count > 0) {
        window->average_window_size = sum / count;
    return CHRONOS_OK;

chronos_error_t flow_control_check_window_exhaustion(flow_control_manager_t* manager, flow_control_window_t* window) {
    if (manager == NULL || window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (window->available_window == 0) {
        window->is_exhausted = true;
        manager->window_exhaustion_count++;
    return CHRONOS_OK;

chronos_error_t flow_control_recover_window(flow_control_manager_t* manager, flow_control_window_t* window) {
    if (manager == NULL || window == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    window->window_size = window->initial_window_size;
    window->available_window = window->initial_window_size;
    window->is_blocked = false;
    window->is_exhausted = false;
    manager->window_recovery_count++;
    return CHRONOS_OK;

chronos_error_t flow_control_process_pending_updates(flow_control_manager_t* manager) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    manager->pending_window_updates = 0;
    return CHRONOS_OK;

chronos_error_t flow_control_queue_window_update(flow_control_manager_t* manager, uint32_t stream_id,
                                                  int32_t delta) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    manager->pending_window_updates++;
    if (manager->pending_window_updates > manager->max_window_update_queue_size) {
        flow_control_process_pending_updates(manager);
    return CHRONOS_OK;

chronos_error_t flow_control_reset(flow_control_manager_t* manager) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    while (manager->stream_windows != NULL) {
        flow_control_destroy_stream_window(manager, manager->stream_windows->stream_id);
    if (manager->connection_window != NULL) {
        manager->connection_window->window_size = manager->default_initial_window_size;
        manager->connection_window->available_window = manager->default_initial_window_size;
        manager->connection_window->is_blocked = false;
        manager->connection_window->is_exhausted = false;
    manager->active_streams = 0;
    manager->total_streams_created = 0;
    manager->total_streams_destroyed = 0;
    manager->total_window_updates = 0;
    manager->flow_control_violations = 0;
    manager->flow_control_errors = 0;
    manager->deadlock_count = 0;
    manager->circular_dependency_count = 0;
    manager->window_exhaustion_count = 0;
    manager->window_recovery_count = 0;
    return CHRONOS_OK;

chronos_error_t flow_control_snapshot(flow_control_manager_t* manager, uint8_t* snapshot, size_t* snapshot_len) {
    if (manager == NULL || snapshot == NULL || snapshot_len == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    size_t required_len = sizeof(uint32_t) * 10 + sizeof(uint8_t) * 5;
    if (*snapshot_len < required_len) {
        *snapshot_len = required_len;
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    size_t offset = 0;
    memcpy(snapshot + offset, &manager->max_streams, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &manager->active_streams, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &manager->default_initial_window_size, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &manager->total_window_updates, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &manager->flow_control_violations, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &manager->enable_stream_flow_control, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(snapshot + offset, &manager->enable_connection_flow_control, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    *snapshot_len = offset;
    return CHRONOS_OK;

chronos_error_t flow_control_restore(flow_control_manager_t* manager, const uint8_t* snapshot, size_t snapshot_len) {
    if (manager == NULL || snapshot == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    size_t offset = 0;
    memcpy(&manager->max_streams, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&manager->active_streams, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&manager->default_initial_window_size, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&manager->total_window_updates, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&manager->flow_control_violations, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&manager->enable_stream_flow_control, snapshot + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(&manager->enable_connection_flow_control, snapshot + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    return CHRONOS_OK;

chronos_error_t flow_control_update_priority_stats(flow_control_manager_t* manager, uint8_t priority) {
    if (manager == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (priority < 256) {
        manager->priority_levels[priority]++;
    return CHRONOS_OK;
