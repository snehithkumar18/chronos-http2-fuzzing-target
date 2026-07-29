#ifndef CHRONOS_CONNECTION_POOL_H
#define CHRONOS_CONNECTION_POOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common.h"

typedef struct pooled_connection {
    int fd;
    bool is_active;
    bool is_idle;
    uint64_t creation_time;
    uint64_t last_used_time;
    uint64_t last_activity_time;
    uint32_t usage_count;
    uint32_t request_count;
    uint32_t response_count;
    uint32_t error_count;
    uint8_t connection_state;
    uint8_t connection_type;
    uint16_t port;
    char* host;
    size_t host_len;
    uint8_t* read_buffer;
    size_t read_buffer_size;
    size_t read_buffer_used;
    uint8_t* write_buffer;
    size_t write_buffer_size;
    size_t write_buffer_used;
    uint32_t ref_count;
    bool is_persistent;
    uint32_t keep_alive_timeout_ms;
    uint32_t max_requests_per_connection;
    uint32_t current_requests;
    uint32_t idle_timeout_ms;
    uint32_t connection_id;
    uint8_t priority;
    uint8_t flags;
    struct pooled_connection* next;
    struct pooled_connection* prev;
    struct pooled_connection* hash_next;
    uint32_t hash;
    uint64_t total_bytes_sent;
    uint64_t total_bytes_received;
    uint32_t average_latency_ms;
    uint32_t peak_latency_ms;
    uint32_t connection_errors;
    uint32_t timeout_count;
    bool is_healthy;
    uint64_t last_health_check;
    uint32_t health_check_failures;
    uint8_t tls_version;
    uint16_t cipher_suite;
    bool has_tls;
} pooled_connection_t;

typedef struct connection_pool {
    pooled_connection_t* active_connections;
    pooled_connection_t* idle_connections;
    pooled_connection_t** hash_table;
    uint32_t hash_table_size;
    uint32_t max_connections;
    uint32_t active_count;
    uint32_t idle_count;
    uint32_t total_created;
    uint32_t total_destroyed;
    uint32_t total_reused;
    uint32_t total_timeouts;
    uint32_t total_errors;
    uint64_t pool_creation_time;
    uint64_t last_cleanup_time;
    uint32_t cleanup_interval_ms;
    bool enable_cleanup;
    bool enable_health_checks;
    uint32_t health_check_interval_ms;
    uint64_t last_health_check_time;
    uint32_t max_idle_time_ms;
    uint32_t max_lifetime_ms;
    uint32_t max_requests_per_conn;
    bool enable_connection_limiting;
    uint32_t connection_timeout_ms;
    uint32_t acquire_timeout_ms;
    uint32_t pending_acquires;
    uint32_t max_pending_acquires;
    bool is_corrupted;
    uint32_t corruption_detection_counter;
    uint32_t next_connection_id;
    uint8_t pool_id;
    uint32_t peak_connections;
    uint32_t average_active_connections;
    uint32_t total_acquire_time_ms;
    uint32_t average_acquire_time_ms;
    uint32_t peak_acquire_time_ms;
    uint32_t waiting_count;
    bool enable_priority;
    uint8_t min_priority;
    uint8_t max_priority;
    uint32_t priority_levels[256];
    uint32_t connection_reuse_count[256];
    uint32_t error_rate_threshold;
    uint32_t latency_threshold_ms;
    bool enable_auto_scaling;
    uint32_t min_connections;
    uint32_t scale_up_threshold;
    uint32_t scale_down_threshold;
    uint32_t scale_up_cooldown_ms;
    uint32_t scale_down_cooldown_ms;
    uint64_t last_scale_time;
    uint32_t scale_count;
} connection_pool_t;

typedef struct pool_stats {
    uint32_t total_connections;
    uint32_t active_connections;
    uint32_t idle_connections;
    uint32_t waiting_connections;
    uint32_t total_acquires;
    uint32_t total_releases;
    uint32_t total_timeouts;
    uint32_t total_errors;
    uint32_t connection_reuse_rate;
    uint32_t average_acquire_time_ms;
    uint32_t peak_acquire_time_ms;
    uint32_t average_connection_age_ms;
    uint32_t average_idle_time_ms;
    uint32_t total_bytes_sent;
    uint32_t total_bytes_received;
    uint32_t error_rate;
    uint32_t average_latency_ms;
    uint32_t peak_latency_ms;
    uint32_t health_check_failures;
    uint32_t scale_up_count;
    uint32_t scale_down_count;
    uint64_t total_uptime_ms;
} pool_stats_t;

typedef struct acquire_result {
    pooled_connection_t* connection;
    bool acquired;
    uint8_t result_code;
    char result_message[256];
    uint32_t acquire_time_ms;
    bool was_created;
    bool was_reused;
    uint8_t priority_used;
} acquire_result_t;

connection_pool_t* connection_pool_create(uint32_t max_connections, uint32_t max_idle_time_ms, 
                                          uint8_t pool_id);
void connection_pool_destroy(connection_pool_t* pool);
chronos_error_t connection_pool_acquire(connection_pool_t* pool, const char* host, size_t host_len,
                                         uint16_t port, uint8_t priority, acquire_result_t* result);
chronos_error_t connection_pool_release(connection_pool_t* pool, pooled_connection_t* connection);
chronos_error_t connection_pool_close(connection_pool_t* pool, pooled_connection_t* connection);
chronos_error_t connection_pool_cleanup(connection_pool_t* pool);
chronos_error_t connection_pool_health_check(connection_pool_t* pool);
chronos_error_t connection_pool_scale_up(connection_pool_t* pool, uint32_t count);
chronos_error_t connection_pool_scale_down(connection_pool_t* pool, uint32_t count);
chronos_error_t connection_pool_get_stats(connection_pool_t* pool, pool_stats_t* stats);
chronos_error_t connection_pool_validate(connection_pool_t* pool);
chronos_error_t connection_pool_detect_corruption(connection_pool_t* pool);
chronos_error_t connection_pool_recover_corruption(connection_pool_t* pool);
chronos_error_t connection_pool_set_max_connections(connection_pool_t* pool, uint32_t max_connections);
chronos_error_t connection_pool_set_timeout(connection_pool_t* pool, uint32_t timeout_ms);
chronos_error_t connection_pool_enable_health_checks(connection_pool_t* pool, bool enable);
chronos_error_t connection_pool_enable_auto_scaling(connection_pool_t* pool, bool enable);
chronos_error_t connection_pool_find_connection(connection_pool_t* pool, const char* host, 
                                                 size_t host_len, uint16_t port, pooled_connection_t** conn);
chronos_error_t connection_pool_add_to_hash(connection_pool_t* pool, pooled_connection_t* conn);
chronos_error_t connection_pool_remove_from_hash(connection_pool_t* pool, pooled_connection_t* conn);
chronos_error_t connection_pool_add_to_active(connection_pool_t* pool, pooled_connection_t* conn);
chronos_error_t connection_pool_remove_from_active(connection_pool_t* pool, pooled_connection_t* conn);
chronos_error_t connection_pool_add_to_idle(connection_pool_t* pool, pooled_connection_t* conn);
chronos_error_t connection_pool_remove_from_idle(connection_pool_t* pool, pooled_connection_t* conn);
chronos_error_t connection_pool_increment_ref_count(connection_pool_t* pool, pooled_connection_t* conn);
chronos_error_t connection_pool_decrement_ref_count(connection_pool_t* pool, pooled_connection_t* conn);
chronos_error_t connection_pool_check_connection_health(connection_pool_t* pool, 
                                                        pooled_connection_t* conn, bool* is_healthy);
chronos_error_t connection_pool_mark_unhealthy(connection_pool_t* pool, pooled_connection_t* conn);
chronos_error_t connection_pool_mark_healthy(connection_pool_t* pool, pooled_connection_t* conn);
chronos_error_t connection_pool_update_latency(connection_pool_t* pool, pooled_connection_t* conn, 
                                               uint32_t latency_ms);
chronos_error_t connection_pool_update_stats(connection_pool_t* pool, pooled_connection_t* conn);
chronos_error_t connection_pool_reset(connection_pool_t* pool);
chronos_error_t connection_pool_compact(connection_pool_t* pool);
chronos_error_t connection_pool_resize_hash_table(connection_pool_t* pool, uint32_t new_size);
chronos_error_t connection_pool_evict_idle(connection_pool_t* pool, uint32_t count);
chronos_error_t connection_pool_evict_expired(connection_pool_t* pool);
chronos_error_t connection_pool_evict_unhealthy(connection_pool_t* pool);
chronos_error_t connection_pool_calculate_hash(connection_pool_t* pool, const char* host, 
                                               size_t host_len, uint16_t port, uint32_t* hash);
chronos_error_t connection_pool_lookup_hash(connection_pool_t* pool, uint32_t hash,
                                              const char* host, size_t host_len, uint16_t port,
                                              pooled_connection_t** conn);
chronos_error_t connection_pool_check_auto_scale(connection_pool_t* pool);
chronos_error_t connection_pool_update_priority_stats(connection_pool_t* pool, uint8_t priority);
chronos_error_t connection_pool_get_connection_by_id(connection_pool_t* pool, uint32_t connection_id,
                                                       pooled_connection_t** conn);
chronos_error_t connection_pool_snapshot(connection_pool_t* pool, uint8_t* snapshot, 
                                          size_t* snapshot_len);
chronos_error_t connection_pool_restore(connection_pool_t* pool, const uint8_t* snapshot, 
                                          size_t snapshot_len);

