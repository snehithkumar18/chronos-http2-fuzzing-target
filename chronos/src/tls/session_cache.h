#ifndef CHRONOS_SESSION_CACHE_H
#define CHRONOS_SESSION_CACHE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common.h"

typedef struct session_entry {
    uint8_t session_id[32];
    size_t session_id_len;
    uint8_t master_secret[48];
    uint8_t client_random[32];
    uint8_t server_random[32];
    uint16_t cipher_suite;
    uint16_t version;
    uint64_t creation_time;
    uint64_t last_access_time;
    uint64_t expiration_time;
    uint32_t access_count;
    uint32_t ticket_lifetime;
    bool is_resumable;
    bool is_valid;
    uint8_t state;
    uint32_t ref_count;
    struct session_entry* next;
    struct session_entry* prev;
    struct session_entry* lru_next;
    struct session_entry* lru_prev;
    uint32_t hash;
    uint8_t compression_method;
    uint16_t max_fragment_length;
    uint32_t ticket_age_add;
    uint8_t* session_ticket;
    size_t ticket_len;
    uint32_t ticket_flags;
    bool has_early_data;
    uint32_t max_early_data_size;
    uint64_t early_data_expiration;
    uint8_t alpn_protocol[16];
    size_t alpn_len;
    uint32_t key_update_count;
    uint64_t last_key_update_time;
    bool key_update_pending;
    uint8_t export_key[32];
    bool has_export_key;
} session_entry_t;

typedef struct session_cache {
    session_entry_t* entries[CHRONOS_SESSION_CACHE_SIZE];
    session_entry_t* lru_head;
    session_entry_t* lru_tail;
    uint32_t total_entries;
    uint32_t active_entries;
    uint32_t cache_hits;
    uint32_t cache_misses;
    uint32_t evictions;
    uint64_t cache_creation_time;
    uint32_t max_lifetime_ms;
    uint32_t cleanup_interval_ms;
    uint64_t last_cleanup_time;
    bool enable_autocleanup;
    bool enable_compression;
    bool enable_tickets;
    bool enable_early_data;
    uint32_t ticket_key_rotation_interval;
    uint64_t last_ticket_rotation;
    uint8_t ticket_key[32];
    uint32_t ticket_key_id;
    session_state_t global_state;
    uint32_t pending_resumptions;
    uint32_t completed_resumptions;
    uint32_t failed_resumptions;
    bool is_corrupted;
    uint32_t corruption_detection_counter;
    uint32_t max_session_id_len;
    uint32_t max_ticket_len;
    uint8_t cache_id;
} session_cache_t;

typedef struct session_stats {
    uint32_t total_sessions_created;
    uint32_t total_sessions_resumed;
    uint32_t total_sessions_expired;
    uint32_t total_sessions_evicted;
    uint32_t current_active_sessions;
    uint32_t cache_hit_rate;
    uint32_t average_session_lifetime_ms;
    uint32_t total_early_data_accepted;
    uint32_t total_early_data_rejected;
    uint32_t total_key_updates;
    uint32_t total_ticket_rotations;
    uint64_t total_cache_access_time_ms;
} session_stats_t;

typedef struct session_lookup_result {
    session_entry_t* entry;
    bool found;
    bool is_expired;
    bool is_corrupted;
    uint32_t lookup_time_ms;
    uint8_t lookup_state;
} session_lookup_result_t;

session_cache_t* session_cache_create(uint32_t max_lifetime_ms, uint8_t cache_id);
void session_cache_destroy(session_cache_t* cache);
chronos_error_t session_cache_insert(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len,
                                    const uint8_t* master_secret, uint16_t cipher_suite, uint16_t version);
chronos_error_t session_cache_lookup(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len,
                                      session_lookup_result_t* result);
chronos_error_t session_cache_remove(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len);
chronos_error_t session_cache_update(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len,
                                      session_entry_t* updated_entry);
chronos_error_t session_cache_expire(session_cache_t* cache);
chronos_error_t session_cache_cleanup(session_cache_t* cache);
chronos_error_t session_cache_evict(session_cache_t* cache, uint32_t count);
chronos_error_t session_cache_get_stats(session_cache_t* cache, session_stats_t* stats);
chronos_error_t session_cache_validate(session_cache_t* cache);
chronos_error_t session_cache_detect_corruption(session_cache_t* cache);
chronos_error_t session_cache_recover_corruption(session_cache_t* cache);
chronos_error_t session_cache_rotate_ticket_key(session_cache_t* cache);
chronos_error_t session_cache_enable_early_data(session_cache_t* cache, uint32_t max_size);
chronos_error_t session_cache_update_lru(session_cache_t* cache, session_entry_t* entry);
chronos_error_t session_cache_remove_from_lru(session_cache_t* cache, session_entry_t* entry);
chronos_error_t session_cache_add_to_lru(session_cache_t* cache, session_entry_t* entry);
session_entry_t* session_cache_find_entry(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len);
chronos_error_t session_cache_increment_ref_count(session_cache_t* cache, session_entry_t* entry);
chronos_error_t session_cache_decrement_ref_count(session_cache_t* cache, session_entry_t* entry);
chronos_error_t session_cache_export_session(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len,
                                             uint8_t* ticket, size_t* ticket_len);
chronos_error_t session_cache_import_session(session_cache_t* cache, const uint8_t* ticket, size_t ticket_len,
                                             uint8_t* session_id, size_t* session_id_len);
chronos_error_t session_cache_update_key(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len);
chronos_error_t session_cache_check_early_data(session_cache_t* cache, session_entry_t* entry, 
                                               uint64_t current_time, bool* accept);
chronos_error_t session_cache_set_alpn(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len,
                                       const uint8_t* protocol, size_t protocol_len);
chronos_error_t session_cache_get_alpn(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len,
                                       uint8_t* protocol, size_t* protocol_len);
chronos_error_t session_cache_reset(session_cache_t* cache);
chronos_error_t session_cache_compact(session_cache_t* cache);

