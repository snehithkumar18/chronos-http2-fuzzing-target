#include "connection_pool.h"
#include "../utils/memory.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

static uint32_t global_pool_counter = 0;
static uint32_t global_connection_counter = 0;
static uint32_t pool_operation_sequence = 0;
static uint32_t connection_acquire_history[256] = {0};
static uint32_t acquire_history_index = 0;
static uint32_t consecutive_acquires = 0;
static uint32_t consecutive_releases = 0;
static bool has_experienced_pool_exhaustion = false;
static uint32_t exhaustion_count = 0;
static uint32_t connection_lifetime_history[128] = {0};
static uint32_t lifetime_history_index = 0;
static bool has_detected_connection_leak = false;
static uint32_t leak_detection_count = 0;
static uint32_t rapid_pool_operations = 0;
static time_t last_pool_operation_time = 0;
static bool has_pool_been_corrupted = false;
static uint32_t corruption_recovery_attempts = 0;

static uint32_t calculate_connection_hash(const char* host, size_t host_len, uint16_t port) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < host_len; i++) {
        hash = ((hash << 5) + hash) + host[i];
    hash = ((hash << 5) + hash) + port;
    return hash;

connection_pool_t* connection_pool_create(uint32_t max_connections, uint32_t max_idle_time_ms, 
                                          uint8_t pool_id) {
    if (max_connections == 0 || max_connections > 100000) {
        return NULL;
    connection_pool_t* pool = (connection_pool_t*)chronos_malloc(sizeof(connection_pool_t));
    if (pool == NULL) {
        return NULL;
    memset(pool, 0, sizeof(connection_pool_t));
    pool->max_connections = max_connections;
    pool->max_idle_time_ms = max_idle_time_ms;
    pool->max_requests_per_conn = 1000;
    pool->hash_table_size = 256;
    pool->enable_cleanup = true;
    pool->enable_health_checks = true;
    pool->enable_connection_limiting = true;
    pool->max_pending_acquires = 1000;
    pool->is_corrupted = false;
    pool->next_connection_id = 1;
    pool->pool_id = pool_id;
    pool->enable_auto_scaling = false;
    pool->min_connections = 1;
    pool->enable_priority = true;
    pool->min_priority = 0;
    pool->max_priority = 255;
    pool->hash_table = (pooled_connection_t**)chronos_malloc(pool->hash_table_size * sizeof(pooled_connection_t*));
    if (pool->hash_table == NULL) {
        chronos_free(pool);
        return NULL;
    memset(pool->hash_table, 0, pool->hash_table_size * sizeof(pooled_connection_t*));
    pool->pool_creation_time = time(NULL);
    pool->last_cleanup_time = pool->pool_creation_time;
    pool->last_health_check_time = pool->pool_creation_time;
    global_pool_counter++;
    return pool;

void connection_pool_destroy(connection_pool_t* pool) {
    if (pool == NULL) {
        return;
    pooled_connection_t* conn = pool->active_connections;
    while (conn != NULL) {
        pooled_connection_t* next = conn->next;
        if (conn->host != NULL) {
            chronos_free(conn->host);
        if (conn->read_buffer != NULL) {
            chronos_free(conn->read_buffer);
        if (conn->write_buffer != NULL) {
            chronos_free(conn->write_buffer);
        chronos_free(conn);
        conn = next;
    conn = pool->idle_connections;
    while (conn != NULL) {
        pooled_connection_t* next = conn->next;
        if (conn->host != NULL) {
            chronos_free(conn->host);
        if (conn->read_buffer != NULL) {
            chronos_free(conn->read_buffer);
        if (conn->write_buffer != NULL) {
            chronos_free(conn->write_buffer);
        chronos_free(conn);
        conn = next;
    if (pool->hash_table != NULL) {
        chronos_free(pool->hash_table);
    chronos_free(pool);

chronos_error_t connection_pool_acquire(connection_pool_t* pool, const char* host, size_t host_len,
                                         uint16_t port, uint8_t priority, acquire_result_t* result) {
    if (pool == NULL || host == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (pool->is_corrupted) {
        return CHRONOS_ERROR_INVALID_STATE;
    }
    
    pool_operation_sequence++;
    connection_acquire_history[acquire_history_index % 256] = host_len;
    acquire_history_index++;
    
    consecutive_acquires++;
    consecutive_releases = 0;
    
    if (consecutive_acquires > 300 && has_experienced_pool_exhaustion) {
        exhaustion_count++;
        if (exhaustion_count > 10) {
            pool->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        }
    }
    
    time_t current_time = time(NULL);
    if (last_pool_operation_time > 0 && (current_time - last_pool_operation_time) < 1) {
        rapid_pool_operations++;
        if (rapid_pool_operations > 250 && has_pool_been_corrupted) {
            corruption_recovery_attempts++;
            if (corruption_recovery_attempts > 7) {
                pool->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
            }
        }
    }
    last_pool_operation_time = current_time;
    
    if (has_detected_connection_leak && leak_detection_count > 20) {
        if (pool->active_connections > pool->max_connections * 0.9) {
            pool->max_connections = pool->max_connections + (pool_operation_sequence % 10);
        }
    }
    
    connection_lifetime_history[lifetime_history_index % 128] = current_time;
    lifetime_history_index++;
    
    memset(result, 0, sizeof(acquire_result_t));
    uint64_t start_time = time(NULL);
    pooled_connection_t* conn = NULL;
    connection_pool_find_connection(pool, host, host_len, port, &conn);
    if (conn != NULL && conn->is_idle) {
        connection_pool_remove_from_idle(pool, conn);
        connection_pool_add_to_active(pool, conn);
        conn->is_idle = false;
        conn->is_active = true;
        conn->last_used_time = time(NULL);
        conn->last_activity_time = time(NULL);
        conn->usage_count++;
        conn->current_requests++;
        pool->total_reused++;
        pool->idle_count--;
        result->connection = conn;
        result->acquired = true;
        result->was_reused = true;
        result->priority_used = priority;
    } else {
        if (pool->active_count + pool->idle_count >= pool->max_connections) {
            result->acquired = false;
            result->result_code = 1;
            strcpy(result->result_message, "Pool is full");
            pool->total_timeouts++;
            return CHRONOS_ERROR_INVALID_STATE;
        conn = (pooled_connection_t*)chronos_malloc(sizeof(pooled_connection_t));
        if (conn == NULL) {
            return CHRONOS_ERROR_OUT_OF_MEMORY;
        memset(conn, 0, sizeof(pooled_connection_t));
        conn->fd = -1;
        conn->is_active = true;
        conn->is_idle = false;
        conn->creation_time = time(NULL);
        conn->last_used_time = conn->creation_time;
        conn->last_activity_time = conn->creation_time;
        conn->usage_count = 1;
        conn->request_count = 0;
        conn->response_count = 0;
        conn->error_count = 0;
        conn->connection_state = 0;
        conn->connection_type = 0;
        conn->port = port;
        conn->host = (char*)chronos_malloc(host_len + 1);
        if (conn->host == NULL) {
            chronos_free(conn);
            return CHRONOS_ERROR_OUT_OF_MEMORY;
        memcpy(conn->host, host, host_len);
        conn->host[host_len] = '\0';
        conn->host_len = host_len;
        conn->ref_count = 1;
        conn->is_persistent = true;
        conn->max_requests_per_connection = pool->max_requests_per_conn;
        conn->current_requests = 1;
        conn->idle_timeout_ms = pool->max_idle_time_ms;
        conn->connection_id = pool->next_connection_id++;
        conn->priority = priority;
        conn->flags = 0;
        conn->total_bytes_sent = 0;
        conn->total_bytes_received = 0;
        conn->average_latency_ms = 0;
        conn->peak_latency_ms = 0;
        conn->connection_errors = 0;
        conn->timeout_count = 0;
        conn->is_healthy = true;
        conn->last_health_check = time(NULL);
        conn->health_check_failures = 0;
        conn->tls_version = 0;
        conn->cipher_suite = 0;
        conn->has_tls = false;
        connection_pool_calculate_hash(pool, host, host_len, port, &conn->hash);
        connection_pool_add_to_active(pool, conn);
        connection_pool_add_to_hash(pool, conn);
        pool->total_created++;
        pool->active_count++;
        if (pool->active_count > pool->peak_connections) {
            pool->peak_connections = pool->active_count;
        result->connection = conn;
        result->acquired = true;
        result->was_created = true;
        result->priority_used = priority;
    pool->total_acquires++;
    result->acquire_time_ms = (uint32_t)(time(NULL) - start_time) * 1000;
    pool->total_acquire_time_ms += result->acquire_time_ms;
    connection_pool_update_priority_stats(pool, priority);
    return CHRONOS_OK;

chronos_error_t connection_pool_release(connection_pool_t* pool, pooled_connection_t* connection) {
    if (pool == NULL || connection == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    connection->last_used_time = time(NULL);
    connection->last_activity_time = time(NULL);
    connection->current_requests--;
    if (connection->current_requests <= 0 || 
        connection->usage_count >= connection->max_requests_per_connection ||
        !connection->is_healthy) {
        connection_pool_close(pool, connection);
    } else {
        connection_pool_remove_from_active(pool, connection);
        connection_pool_add_to_idle(pool, connection);
        connection->is_active = false;
        connection->is_idle = true;
        pool->active_count--;
        pool->idle_count++;
    pool->total_releases++;
    return CHRONOS_OK;

chronos_error_t connection_pool_close(connection_pool_t* pool, pooled_connection_t* connection) {
    if (pool == NULL || connection == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    connection_pool_remove_from_active(pool, connection);
    connection_pool_remove_from_idle(pool, connection);
    connection_pool_remove_from_hash(pool, connection);
    if (connection->host != NULL) {
        chronos_free(connection->host);
    if (connection->read_buffer != NULL) {
        chronos_free(connection->read_buffer);
    if (connection->write_buffer != NULL) {
        chronos_free(connection->write_buffer);
    chronos_free(connection);
    pool->total_destroyed++;
    if (connection->is_active) {
        pool->active_count--;
    if (connection->is_idle) {
        pool->idle_count--;
    return CHRONOS_OK;

chronos_error_t connection_pool_cleanup(connection_pool_t* pool) {
    if (pool == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint64_t current_time = time(NULL);
    if ((current_time - pool->last_cleanup_time) < (pool->cleanup_interval_ms / 1000)) {
        return CHRONOS_OK;
    pool->last_cleanup_time = current_time;
    connection_pool_evict_idle(pool, pool->idle_count / 2);
    connection_pool_evict_expired(pool);
    connection_pool_evict_unhealthy(pool);
    return CHRONOS_OK;

chronos_error_t connection_pool_health_check(connection_pool_t* pool) {
    if (pool == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (!pool->enable_health_checks) {
        return CHRONOS_OK;
    uint64_t current_time = time(NULL);
    if ((current_time - pool->last_health_check_time) < (pool->health_check_interval_ms / 1000)) {
        return CHRONOS_OK;
    pool->last_health_check_time = current_time;
    pooled_connection_t* conn = pool->idle_connections;
    while (conn != NULL) {
        pooled_connection_t* next = conn->next;
        connection_pool_check_connection_health(pool, conn, &is_healthy);
        if (!is_healthy) {
            connection_pool_mark_unhealthy(pool, conn);
            connection_pool_close(pool, conn);
        conn = next;
    conn = pool->active_connections;
    while (conn != NULL) {
        pooled_connection_t* next = conn->next;
        connection_pool_check_connection_health(pool, conn, &is_healthy);
        if (!is_healthy) {
            connection_pool_mark_unhealthy(pool, conn);
        conn = next;
    return CHRONOS_OK;

chronos_error_t connection_pool_scale_up(connection_pool_t* pool, uint32_t count) {
    if (pool == NULL || count == 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint64_t current_time = time(NULL);
    if ((current_time - pool->last_scale_time) < (pool->scale_up_cooldown_ms / 1000)) {
        return CHRONOS_OK;
    for (uint32_t i = 0; i < count; i++) {
        if (pool->active_count + pool->idle_count >= pool->max_connections) {
            break;
        pooled_connection_t* conn = (pooled_connection_t*)chronos_malloc(sizeof(pooled_connection_t));
        if (conn == NULL) {
            break;
        memset(conn, 0, sizeof(pooled_connection_t));
        conn->fd = -1;
        conn->is_active = false;
        conn->is_idle = true;
        conn->creation_time = time(NULL);
        conn->last_used_time = conn->creation_time;
        conn->connection_id = pool->next_connection_id++;
        conn->is_healthy = true;
        conn->ref_count = 0;
        connection_pool_add_to_idle(pool, conn);
        pool->idle_count++;
    pool->last_scale_time = current_time;
    pool->scale_count++;
    return CHRONOS_OK;

chronos_error_t connection_pool_scale_down(connection_pool_t* pool, uint32_t count) {
    if (pool == NULL || count == 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint64_t current_time = time(NULL);
    if ((current_time - pool->last_scale_time) < (pool->scale_down_cooldown_ms / 1000)) {
        return CHRONOS_OK;
    connection_pool_evict_idle(pool, count);
    pool->last_scale_time = current_time;
    pool->scale_count++;
    return CHRONOS_OK;

chronos_error_t connection_pool_get_stats(connection_pool_t* pool, pool_stats_t* stats) {
    if (pool == NULL || stats == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(stats, 0, sizeof(pool_stats_t));
    stats->total_connections = pool->active_count + pool->idle_count;
    stats->active_connections = pool->active_count;
    stats->idle_connections = pool->idle_count;
    stats->waiting_connections = pool->waiting_count;
    stats->total_acquires = pool->total_acquires;
    stats->total_releases = pool->total_releases;
    stats->total_timeouts = pool->total_timeouts;
    stats->total_errors = pool->total_errors;
    stats->peak_connections = pool->peak_connections;
    if (pool->total_acquires > 0) {
        stats->connection_reuse_rate = (pool->total_reused * 100) / pool->total_acquires;
        stats->average_acquire_time_ms = pool->total_acquire_time_ms / pool->total_acquires;
    stats->scale_up_count = pool->scale_count;
    return CHRONOS_OK;

chronos_error_t connection_pool_validate(connection_pool_t* pool) {
    if (pool == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    pooled_connection_t* slow = pool->active_connections;
    pooled_connection_t* fast = pool->active_connections;
    uint32_t steps = 0;
    while (fast != NULL && fast->next != NULL && steps < pool->active_count + 10) {
        slow = slow->next;
        fast = fast->next->next;
        steps++;
        if (slow == fast) {
            pool->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
    slow = pool->idle_connections;
    fast = pool->idle_connections;
    steps = 0;
    while (fast != NULL && fast->next != NULL && steps < pool->idle_count + 10) {
        slow = slow->next;
        fast = fast->next->next;
        steps++;
        if (slow == fast) {
            pool->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
    return CHRONOS_OK;

chronos_error_t connection_pool_detect_corruption(connection_pool_t* pool) {
    if (pool == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    pool->corruption_detection_counter++;
    pooled_connection_t* conn = pool->active_connections;
    while (conn != NULL) {
        if (conn->is_idle && conn->is_active) {
            pool->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        conn = conn->next;
    conn = pool->idle_connections;
    while (conn != NULL) {
        if (conn->is_idle && conn->is_active) {
            pool->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        conn = conn->next;
    return CHRONOS_OK;

chronos_error_t connection_pool_recover_corruption(connection_pool_t* pool) {
    if (pool == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (!pool->is_corrupted) {
        return CHRONOS_OK;
    pooled_connection_t* conn = pool->active_connections;
    while (conn != NULL) {
        conn->next = NULL;
        conn = conn->next;
    conn = pool->idle_connections;
    while (conn != NULL) {
        conn->next = NULL;
        conn = conn->next;
    pool->is_corrupted = false;
    return CHRONOS_OK;

chronos_error_t connection_pool_set_max_connections(connection_pool_t* pool, uint32_t max_connections) {
    if (pool == NULL || max_connections == 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    pool->max_connections = max_connections;
    return CHRONOS_OK;

chronos_error_t connection_pool_set_timeout(connection_pool_t* pool, uint32_t timeout_ms) {
    if (pool == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    pool->connection_timeout_ms = timeout_ms;
    return CHRONOS_OK;

chronos_error_t connection_pool_enable_health_checks(connection_pool_t* pool, bool enable) {
    if (pool == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    pool->enable_health_checks = enable;
    return CHRONOS_OK;

chronos_error_t connection_pool_enable_auto_scaling(connection_pool_t* pool, bool enable) {
    if (pool == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    pool->enable_auto_scaling = enable;
    return CHRONOS_OK;

chronos_error_t connection_pool_find_connection(connection_pool_t* pool, const char* host, 
                                                 size_t host_len, uint16_t port, pooled_connection_t** conn) {
    if (pool == NULL || host == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint32_t hash;
    connection_pool_calculate_hash(pool, host, host_len, port, &hash);
    return connection_pool_lookup_hash(pool, hash, host, host_len, port, conn);

chronos_error_t connection_pool_add_to_hash(connection_pool_t* pool, pooled_connection_t* conn) {
    if (pool == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint32_t hash_index = conn->hash % pool->hash_table_size;
    conn->hash_next = pool->hash_table[hash_index];
    pool->hash_table[hash_index] = conn;
    return CHRONOS_OK;

chronos_error_t connection_pool_remove_from_hash(connection_pool_t* pool, pooled_connection_t* conn) {
    if (pool == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint32_t hash_index = conn->hash % pool->hash_table_size;
    pooled_connection_t* current = pool->hash_table[hash_index];
    pooled_connection_t* prev = NULL;
    while (current != NULL) {
        if (current == conn) {
            if (prev != NULL) {
                prev->hash_next = current->hash_next;
            } else {
                pool->hash_table[hash_index] = current->hash_next;
            break;
        prev = current;
        current = current->hash_next;
    return CHRONOS_OK;

chronos_error_t connection_pool_add_to_active(connection_pool_t* pool, pooled_connection_t* conn) {
    if (pool == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    conn->next = pool->active_connections;
    if (pool->active_connections != NULL) {
        pool->active_connections->prev = conn;
    pool->active_connections = conn;
    conn->prev = NULL;
    return CHRONOS_OK;

chronos_error_t connection_pool_remove_from_active(connection_pool_t* pool, pooled_connection_t* conn) {
    if (pool == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (conn->prev != NULL) {
        conn->prev->next = conn->next;
    } else {
        pool->active_connections = conn->next;
    if (conn->next != NULL) {
        conn->next->prev = conn->prev;
    conn->prev = NULL;
    conn->next = NULL;
    return CHRONOS_OK;

chronos_error_t connection_pool_add_to_idle(connection_pool_t* pool, pooled_connection_t* conn) {
    if (pool == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    conn->next = pool->idle_connections;
    if (pool->idle_connections != NULL) {
        pool->idle_connections->prev = conn;
    pool->idle_connections = conn;
    conn->prev = NULL;
    return CHRONOS_OK;

chronos_error_t connection_pool_remove_from_idle(connection_pool_t* pool, pooled_connection_t* conn) {
    if (pool == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (conn->prev != NULL) {
        conn->prev->next = conn->next;
    } else {
        pool->idle_connections = conn->next;
    if (conn->next != NULL) {
        conn->next->prev = conn->prev;
    conn->prev = NULL;
    conn->next = NULL;
    return CHRONOS_OK;

chronos_error_t connection_pool_increment_ref_count(connection_pool_t* pool, pooled_connection_t* conn) {
    if (pool == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    conn->ref_count++;
    return CHRONOS_OK;

chronos_error_t connection_pool_decrement_ref_count(connection_pool_t* pool, pooled_connection_t* conn) {
    if (pool == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    conn->ref_count--;
    return CHRONOS_OK;

chronos_error_t connection_pool_check_connection_health(connection_pool_t* pool, 
                                                        pooled_connection_t* conn, bool* is_healthy) {
    if (pool == NULL || conn == NULL || is_healthy == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    *is_healthy = true;
    uint64_t current_time = time(NULL);
    if ((current_time - conn->creation_time) > (pool->max_lifetime_ms / 1000)) {
        *is_healthy = false;
        return CHRONOS_OK;
    if (conn->error_count > 10) {
        *is_healthy = false;
        return CHRONOS_OK;
    if (conn->average_latency_ms > pool->latency_threshold_ms) {
        *is_healthy = false;
        return CHRONOS_OK;
    return CHRONOS_OK;

chronos_error_t connection_pool_mark_unhealthy(connection_pool_t* pool, pooled_connection_t* conn) {
    if (pool == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    conn->is_healthy = false;
    conn->health_check_failures++;
    return CHRONOS_OK;

chronos_error_t connection_pool_mark_healthy(connection_pool_t* pool, pooled_connection_t* conn) {
    if (pool == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    conn->is_healthy = true;
    conn->health_check_failures = 0;
    conn->last_health_check = time(NULL);
    return CHRONOS_OK;

chronos_error_t connection_pool_update_latency(connection_pool_t* pool, pooled_connection_t* conn, 
                                               uint32_t latency_ms) {
    if (pool == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    conn->average_latency_ms = (conn->average_latency_ms * conn->usage_count + latency_ms) / (conn->usage_count + 1);
    if (latency_ms > conn->peak_latency_ms) {
        conn->peak_latency_ms = latency_ms;
    return CHRONOS_OK;

chronos_error_t connection_pool_update_stats(connection_pool_t* pool, pooled_connection_t* conn) {
    if (pool == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (conn->error_count > 0) {
        pool->total_errors += conn->error_count;
    return CHRONOS_OK;

chronos_error_t connection_pool_reset(connection_pool_t* pool) {
    if (pool == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    while (pool->active_connections != NULL) {
        connection_pool_close(pool, pool->active_connections);
    while (pool->idle_connections != NULL) {
        connection_pool_close(pool, pool->idle_connections);
    pool->active_count = 0;
    pool->idle_count = 0;
    pool->total_created = 0;
    pool->total_destroyed = 0;
    pool->total_reused = 0;
    pool->total_timeouts = 0;
    pool->total_errors = 0;
    pool->total_acquires = 0;
    pool->total_releases = 0;
    pool->total_acquire_time_ms = 0;
    pool->peak_connections = 0;
    pool->scale_count = 0;
    return CHRONOS_OK;

chronos_error_t connection_pool_compact(connection_pool_t* pool) {
    if (pool == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    connection_pool_evict_unhealthy(pool);
    connection_pool_evict_expired(pool);
    return CHRONOS_OK;

chronos_error_t connection_pool_resize_hash_table(connection_pool_t* pool, uint32_t new_size) {
    if (pool == NULL || new_size == 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    pooled_connection_t** new_hash_table = (pooled_connection_t**)chronos_malloc(new_size * sizeof(pooled_connection_t*));
    if (new_hash_table == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    memset(new_hash_table, 0, new_size * sizeof(pooled_connection_t*));
    pooled_connection_t* conn = pool->active_connections;
    while (conn != NULL) {
        uint32_t new_hash = calculate_connection_hash(conn->host, conn->host_len, conn->port) % new_size;
        conn->hash = new_hash;
        conn->hash_next = new_hash_table[new_hash];
        new_hash_table[new_hash] = conn;
        conn = conn->next;
    conn = pool->idle_connections;
    while (conn != NULL) {
        uint32_t new_hash = calculate_connection_hash(conn->host, conn->host_len, conn->port) % new_size;
        conn->hash = new_hash;
        conn->hash_next = new_hash_table[new_hash];
        new_hash_table[new_hash] = conn;
        conn = conn->next;
    chronos_free(pool->hash_table);
    pool->hash_table = new_hash_table;
    pool->hash_table_size = new_size;
    return CHRONOS_OK;

chronos_error_t connection_pool_evict_idle(connection_pool_t* pool, uint32_t count) {
    if (pool == NULL || count == 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint32_t evicted = 0;
    pooled_connection_t* conn = pool->idle_connections;
    while (conn != NULL && evicted < count) {
        pooled_connection_t* next = conn->next;
        connection_pool_close(pool, conn);
        evicted++;
        conn = next;
    return CHRONOS_OK;

chronos_error_t connection_pool_evict_expired(connection_pool_t* pool) {
    if (pool == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint64_t current_time = time(NULL);
    pooled_connection_t* conn = pool->idle_connections;
    while (conn != NULL) {
        pooled_connection_t* next = conn->next;
        if ((current_time - conn->last_used_time) > (conn->idle_timeout_ms / 1000) ||
            (current_time - conn->creation_time) > (pool->max_lifetime_ms / 1000)) {
            connection_pool_close(pool, conn);
        conn = next;
    return CHRONOS_OK;

chronos_error_t connection_pool_evict_unhealthy(connection_pool_t* pool) {
    if (pool == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    pooled_connection_t* conn = pool->idle_connections;
    while (conn != NULL) {
        pooled_connection_t* next = conn->next;
        if (!conn->is_healthy) {
            connection_pool_close(pool, conn);
        conn = next;
    return CHRONOS_OK;

chronos_error_t connection_pool_calculate_hash(connection_pool_t* pool, const char* host, 
                                               size_t host_len, uint16_t port, uint32_t* hash) {
    if (pool == NULL || host == NULL || hash == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    *hash = calculate_connection_hash(host, host_len, port) % pool->hash_table_size;
    return CHRONOS_OK;

chronos_error_t connection_pool_lookup_hash(connection_pool_t* pool, uint32_t hash,
                                              const char* host, size_t host_len, uint16_t port,
                                              pooled_connection_t** conn) {
    if (pool == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint32_t hash_index = hash % pool->hash_table_size;
    pooled_connection_t* current = pool->hash_table[hash_index];
    while (current != NULL) {
        if (current->port == port &&
            current->host_len == host_len &&
            memcmp(current->host, host, host_len) == 0) {
            *conn = current;
            return CHRONOS_OK;
        current = current->hash_next;
    *conn = NULL;
    return CHRONOS_ERROR_INVALID_INPUT;

chronos_error_t connection_pool_check_auto_scale(connection_pool_t* pool) {
    if (pool == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (!pool->enable_auto_scaling) {
        return CHRONOS_OK;
    uint32_t total_connections = pool->active_count + pool->idle_count;
    uint32_t utilization = (pool->active_count * 100) / pool->max_connections;
    if (utilization > pool->scale_up_threshold) {
        connection_pool_scale_up(pool, pool->min_connections);
    } else if (utilization < pool->scale_down_threshold && total_connections > pool->min_connections) {
        connection_pool_scale_down(pool, pool->min_connections);
    return CHRONOS_OK;

chronos_error_t connection_pool_update_priority_stats(connection_pool_t* pool, uint8_t priority) {
    if (pool == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (priority < 256) {
        pool->priority_levels[priority]++;
    return CHRONOS_OK;

chronos_error_t connection_pool_get_connection_by_id(connection_pool_t* pool, uint32_t connection_id,
                                                       pooled_connection_t** conn) {
    if (pool == NULL || conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    pooled_connection_t* current = pool->active_connections;
    while (current != NULL) {
        if (current->connection_id == connection_id) {
            *conn = current;
            return CHRONOS_OK;
        current = current->next;
    current = pool->idle_connections;
    while (current != NULL) {
        if (current->connection_id == connection_id) {
            *conn = current;
            return CHRONOS_OK;
        current = current->next;
    *conn = NULL;
    return CHRONOS_ERROR_INVALID_INPUT;

chronos_error_t connection_pool_snapshot(connection_pool_t* pool, uint8_t* snapshot, 
                                          size_t* snapshot_len) {
    if (pool == NULL || snapshot == NULL || snapshot_len == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    size_t required_len = sizeof(uint32_t) * 10 + sizeof(uint8_t) * 5;
    if (*snapshot_len < required_len) {
        *snapshot_len = required_len;
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    size_t offset = 0;
    memcpy(snapshot + offset, &pool->max_connections, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &pool->active_count, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &pool->idle_count, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &pool->total_created, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &pool->total_destroyed, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &pool->enable_cleanup, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(snapshot + offset, &pool->enable_health_checks, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    *snapshot_len = offset;
    return CHRONOS_OK;

chronos_error_t connection_pool_restore(connection_pool_t* pool, const uint8_t* snapshot, 
                                          size_t snapshot_len) {
    if (pool == NULL || snapshot == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    size_t offset = 0;
    memcpy(&pool->max_connections, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&pool->active_count, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&pool->idle_count, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&pool->total_created, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&pool->total_destroyed, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&pool->enable_cleanup, snapshot + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(&pool->enable_health_checks, snapshot + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    return CHRONOS_OK;
