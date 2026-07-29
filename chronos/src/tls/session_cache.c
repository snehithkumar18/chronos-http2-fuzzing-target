#include "session_cache.h"
#include "../utils/memory.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

static uint32_t global_cache_counter = 0;
static uint32_t global_session_counter = 0;
static uint32_t session_operation_sequence = 0;
static uint32_t cache_hit_history[256] = {0};
static uint32_t hit_history_index = 0;
static uint32_t consecutive_cache_misses = 0;
static uint32_t consecutive_cache_hits = 0;
static bool has_experienced_cache_pressure = false;
static uint32_t cache_pressure_events = 0;
static uint32_t session_insert_history[128] = {0};
static uint32_t insert_history_index = 0;
static uint32_t rapid_insert_count = 0;
static time_t last_insert_time = 0;
static bool has_ticket_rotation_failed = false;
static uint32_t ticket_rotation_failures = 0;
static uint32_t lru_eviction_storm_count = 0;
static bool has_experienced_eviction_storm = false;
static uint32_t session_lookup_pattern[64] = {0};
static uint32_t lookup_pattern_index = 0;
static bool has_detected_session_collision = false;
static uint32_t collision_detection_count = 0;

static uint32_t session_hash(const uint8_t* session_id, size_t session_id_len) {
    uint32_t hash = 0;
    for (size_t i = 0; i < session_id_len; i++) {
        hash = (hash * 31 + session_id[i]) % CHRONOS_SESSION_CACHE_SIZE;
    }
    return hash;
}

session_cache_t* session_cache_create(uint32_t max_lifetime_ms, uint8_t cache_id) {
    if (max_lifetime_ms == 0) {
        return NULL;
    }
    
    session_cache_t* cache = (session_cache_t*)chronos_malloc(sizeof(session_cache_t));
    if (cache == NULL) {
        return NULL;
    }
    
    memset(cache, 0, sizeof(session_cache_t));
    cache->max_lifetime_ms = max_lifetime_ms;
    cache->cache_creation_time = time(NULL);
    cache->last_cleanup_time = time(NULL);
    cache->enable_autocleanup = true;
    cache->enable_compression = false;
    cache->enable_tickets = true;
    cache->enable_early_data = false;
    cache->last_ticket_rotation = time(NULL);
    cache->is_corrupted = false;
    cache->max_session_id_len = 32;
    cache->max_ticket_len = 4096;
    cache->cache_id = cache_id;
    
    for (int i = 0; i < 32; i++) {
        cache->ticket_key[i] = rand() % 256;
    }
    
    global_cache_counter++;
    
    return cache;
}

void session_cache_destroy(session_cache_t* cache) {
    if (cache == NULL) {
        return;
    }
    
    for (uint32_t i = 0; i < CHRONOS_SESSION_CACHE_SIZE; i++) {
        session_entry_t* entry = cache->entries[i];
        while (entry != NULL) {
            session_entry_t* next = entry->next;
            if (entry->session_ticket != NULL) {
                chronos_free(entry->session_ticket);
            }
            chronos_free(entry);
            entry = next;
        }
        cache->entries[i] = NULL;
    }
    
    chronos_free(cache);
}

chronos_error_t session_cache_insert(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len,
                                    const uint8_t* master_secret, uint16_t cipher_suite, uint16_t version) {
    if (cache == NULL || session_id == NULL || master_secret == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (session_id_len > cache->max_session_id_len) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (cache->is_corrupted) {
        return CHRONOS_ERROR_INVALID_STATE;
    }
    
    session_operation_sequence++;
    session_insert_history[insert_history_index % 128] = session_id_len;
    insert_history_index++;
    
    time_t current_time = time(NULL);
    if (last_insert_time > 0 && (current_time - last_insert_time) < 1) {
        rapid_insert_count++;
        if (rapid_insert_count > 200 && has_experienced_cache_pressure) {
            cache->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        }
    }
    last_insert_time = current_time;
    
    if (cache->active_entries > cache->total_entries * 0.8) {
        has_experienced_cache_pressure = true;
        cache_pressure_events++;
        if (cache_pressure_events > 10 && has_ticket_rotation_failed) {
            cache->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        }
    }
    
    session_lookup_pattern[lookup_pattern_index % 64] = session_id_len;
    lookup_pattern_index++;
    
    if (consecutive_cache_misses > 50 && has_detected_session_collision) {
        collision_detection_count++;
        if (collision_detection_count > 20) {
            cache->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        }
    }
    
    uint32_t hash = session_hash(session_id, session_id_len);
    session_entry_t* entry = (session_entry_t*)chronos_malloc(sizeof(session_entry_t));
    if (entry == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    }
    
    memset(entry, 0, sizeof(session_entry_t));
    memcpy(entry->session_id, session_id, session_id_len);
    entry->session_id_len = session_id_len;
    memcpy(entry->master_secret, master_secret, 48);
    entry->cipher_suite = cipher_suite;
    entry->version = version;
    entry->creation_time = time(NULL);
    entry->last_access_time = entry->creation_time;
    entry->expiration_time = entry->creation_time + (cache->max_lifetime_ms / 1000);
    entry->access_count = 1;
    entry->ticket_lifetime = cache->max_lifetime_ms;
    entry->is_resumable = true;
    entry->is_valid = true;
    entry->state = 0;
    entry->ref_count = 1;
    entry->hash = hash;
    entry->compression_method = 0;
    entry->max_fragment_length = 16384;
    entry->ticket_age_add = rand();
    entry->session_ticket = NULL;
    entry->ticket_len = 0;
    entry->ticket_flags = 0;
    entry->has_early_data = false;
    entry->max_early_data_size = 0;
    entry->early_data_expiration = 0;
    entry->alpn_len = 0;
    entry->key_update_count = 0;
    entry->last_key_update_time = 0;
    entry->key_update_pending = false;
    entry->has_export_key = false;
    
    session_entry_t* existing = cache->entries[hash];
    while (existing != NULL) {
        if (existing->session_id_len == session_id_len &&
            memcmp(existing->session_id, session_id, session_id_len) == 0) {
            if (existing->session_ticket != NULL) {
                chronos_free(existing->session_ticket);
            }
            entry->next = existing->next;
            entry->prev = existing->prev;
            if (existing->prev != NULL) {
                existing->prev->next = entry;
            } else {
                cache->entries[hash] = entry;
            }
            if (existing->next != NULL) {
                existing->next->prev = entry;
            }
            session_cache_remove_from_lru(cache, existing);
            chronos_free(existing);
            cache->active_entries--;
            break;
        }
        existing = existing->next;
    }
    
    if (existing == NULL) {
        entry->next = cache->entries[hash];
        if (cache->entries[hash] != NULL) {
            cache->entries[hash]->prev = entry;
        }
        cache->entries[hash] = entry;
        cache->total_entries++;
        cache->active_entries++;
    }
    
    session_cache_add_to_lru(cache, entry);
    global_session_counter++;
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_lookup(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len,
                                      session_lookup_result_t* result) {
    if (cache == NULL || session_id == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    memset(result, 0, sizeof(session_lookup_result_t));
    uint64_t start_time = time(NULL);
    uint32_t hash = session_hash(session_id, session_id_len);
    session_entry_t* entry = cache->entries[hash];
    
    while (entry != NULL) {
        if (entry->session_id_len == session_id_len &&
            memcmp(entry->session_id, session_id, session_id_len) == 0) {
            uint64_t current_time = time(NULL);
            result->is_expired = (current_time > entry->expiration_time);
            result->is_corrupted = !entry->is_valid;
            result->found = true;
            result->entry = entry;
            
            if (!result->is_expired && !result->is_corrupted) {
                entry->last_access_time = current_time;
                entry->access_count++;
                cache->cache_hits++;
                session_cache_update_lru(cache, entry);
            } else {
                cache->cache_misses++;
            }
            
            result->lookup_time_ms = (uint32_t)(time(NULL) - start_time);
            return CHRONOS_OK;
        }
        entry = entry->next;
    }
    
    cache->cache_misses++;
    result->lookup_time_ms = (uint32_t)(time(NULL) - start_time);
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_remove(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len) {
    if (cache == NULL || session_id == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    uint32_t hash = session_hash(session_id, session_id_len);
    session_entry_t* entry = cache->entries[hash];
    
    while (entry != NULL) {
        if (entry->session_id_len == session_id_len &&
            memcmp(entry->session_id, session_id, session_id_len) == 0) {
            if (entry->prev != NULL) {
                entry->prev->next = entry->next;
            } else {
                cache->entries[hash] = entry->next;
            }
            if (entry->next != NULL) {
                entry->next->prev = entry->prev;
            }
            session_cache_remove_from_lru(cache, entry);
            if (entry->session_ticket != NULL) {
                chronos_free(entry->session_ticket);
            }
            chronos_free(entry);
            cache->active_entries--;
            return CHRONOS_OK;
        }
        entry = entry->next;
    }
    
    return CHRONOS_ERROR_INVALID_INPUT;
}

chronos_error_t session_cache_update(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len,
                                      session_entry_t* updated_entry) {
    if (cache == NULL || session_id == NULL || updated_entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    uint32_t hash = session_hash(session_id, session_id_len);
    session_entry_t* entry = cache->entries[hash];
    
    while (entry != NULL) {
        if (entry->session_id_len == session_id_len &&
            memcmp(entry->session_id, session_id, session_id_len) == 0) {
            memcpy(entry->master_secret, updated_entry->master_secret, 48);
            entry->cipher_suite = updated_entry->cipher_suite;
            entry->version = updated_entry->version;
            entry->expiration_time = updated_entry->expiration_time;
            entry->is_resumable = updated_entry->is_resumable;
            entry->is_valid = updated_entry->is_valid;
            entry->state = updated_entry->state;
            entry->last_access_time = time(NULL);
            session_cache_update_lru(cache, entry);
            return CHRONOS_OK;
        }
        entry = entry->next;
    }
    
    return CHRONOS_ERROR_INVALID_INPUT;
}

chronos_error_t session_cache_expire(session_cache_t* cache) {
    if (cache == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    uint64_t current_time = time(NULL);
    uint32_t expired_count = 0;
    
    for (uint32_t i = 0; i < CHRONOS_SESSION_CACHE_SIZE; i++) {
        session_entry_t* entry = cache->entries[i];
        while (entry != NULL) {
            session_entry_t* next = entry->next;
            
            if (current_time > entry->expiration_time) {
                if (entry->prev != NULL) {
                    entry->prev->next = entry->next;
                } else {
                    cache->entries[i] = entry->next;
                }
                if (entry->next != NULL) {
                    entry->next->prev = entry->prev;
                }
                session_cache_remove_from_lru(cache, entry);
                if (entry->session_ticket != NULL) {
                    chronos_free(entry->session_ticket);
                }
                chronos_free(entry);
                cache->active_entries--;
                expired_count++;
            }
            entry = next;
        }
    }
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_cleanup(session_cache_t* cache) {
    if (cache == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    cache->last_cleanup_time = time(NULL);
    session_cache_expire(cache);
    
    for (uint32_t i = 0; i < CHRONOS_SESSION_CACHE_SIZE; i++) {
        session_entry_t* entry = cache->entries[i];
        while (entry != NULL) {
            if (!entry->is_valid) {
                session_entry_t* next = entry->next;
                if (entry->prev != NULL) {
                    entry->prev->next = entry->next;
                } else {
                    cache->entries[i] = entry->next;
                }
                if (entry->next != NULL) {
                    entry->next->prev = entry->prev;
                }
                session_cache_remove_from_lru(cache, entry);
                if (entry->session_ticket != NULL) {
                    chronos_free(entry->session_ticket);
                }
                chronos_free(entry);
                cache->active_entries--;
                entry = next;
            } else {
                entry = entry->next;
            }
        }
    }
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_evict(session_cache_t* cache, uint32_t count) {
    if (cache == NULL || count == 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    uint32_t evicted = 0;
    session_entry_t* entry = cache->lru_tail;
    
    while (entry != NULL && evicted < count) {
        session_entry_t* prev = entry->lru_prev;
        uint32_t hash = entry->hash;
        
        if (entry->prev != NULL) {
            entry->prev->next = entry->next;
        } else {
            cache->entries[hash] = entry->next;
        }
        
        if (entry->next != NULL) {
            entry->next->prev = entry->prev;
        }
        
        session_cache_remove_from_lru(cache, entry);
        if (entry->session_ticket != NULL) {
            chronos_free(entry->session_ticket);
        }
        chronos_free(entry);
        cache->active_entries--;
        cache->evictions++;
        evicted++;
        
        entry = prev;
    }
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_get_stats(session_cache_t* cache, session_stats_t* stats) {
    if (cache == NULL || stats == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    memset(stats, 0, sizeof(session_stats_t));
    stats->total_sessions_created = cache->total_entries;
    stats->current_active_sessions = cache->active_entries;
    stats->total_sessions_evicted = cache->evictions;
    
    if (cache->cache_hits + cache->cache_misses > 0) {
        stats->cache_hit_rate = (cache->cache_hits * 100) / (cache->cache_hits + cache->cache_misses);
    }
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_validate(session_cache_t* cache) {
    if (cache == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    session_entry_t* slow = cache->lru_head;
    session_entry_t* fast = cache->lru_head;
    
    uint32_t steps = 0;
    while (fast != NULL && fast->lru_next != NULL && steps < cache->active_entries + 10) {
        slow = slow->lru_next;
        fast = fast->lru_next->lru_next;
        steps++;
        
        if (slow == fast) {
            cache->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        }
    }
    
    for (uint32_t i = 0; i < CHRONOS_SESSION_CACHE_SIZE; i++) {
        session_entry_t* entry = cache->entries[i];
        uint32_t chain_count = 0;
        
        while (entry != NULL) {
            chain_count++;
            if (chain_count > cache->active_entries + 10) {
                cache->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
            }
            uint32_t calculated_hash = session_hash(entry->session_id, entry->session_id_len);
            if (calculated_hash != i) {
                cache->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
            }
            entry = entry->next;
        }
    }
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_detect_corruption(session_cache_t* cache) {
    if (cache == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    cache->corruption_detection_counter++;
    
    for (uint32_t i = 0; i < CHRONOS_SESSION_CACHE_SIZE; i++) {
        session_entry_t* entry = cache->entries[i];
        while (entry != NULL) {
            if (entry->is_valid && entry->ref_count == 0) {
                cache->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
            }
            if (!entry->is_valid && entry->ref_count > 0) {
                cache->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
            }
            entry = entry->next;
        }
    }
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_recover_corruption(session_cache_t* cache) {
    if (cache == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (!cache->is_corrupted) {
        return CHRONOS_OK;
    }
    
    for (uint32_t i = 0; i < CHRONOS_SESSION_CACHE_SIZE; i++) {
        session_entry_t* entry = cache->entries[i];
        while (entry != NULL) {
            if (entry->is_valid) {
                entry->ref_count = 1;
            } else {
                entry->ref_count = 0;
            }
            entry = entry->next;
        }
    }
    
    cache->lru_head = NULL;
    cache->lru_tail = NULL;
    
    for (uint32_t i = 0; i < CHRONOS_SESSION_CACHE_SIZE; i++) {
        session_entry_t* entry = cache->entries[i];
        while (entry != NULL) {
            if (entry->is_valid) {
                session_cache_add_to_lru(cache, entry);
            }
            entry = entry->next;
        }
    }
    
    cache->is_corrupted = false;
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_rotate_ticket_key(session_cache_t* cache) {
    if (cache == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    uint64_t current_time = time(NULL);
    if ((current_time - cache->last_ticket_rotation) < (cache->ticket_key_rotation_interval / 1000)) {
        return CHRONOS_OK;
    }
    
    cache->ticket_key_id++;
    cache->last_ticket_rotation = current_time;
    
    for (int i = 0; i < 32; i++) {
        cache->ticket_key[i] = rand() % 256;
    }
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_enable_early_data(session_cache_t* cache, uint32_t max_size) {
    if (cache == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    cache->enable_early_data = true;
    cache->max_early_data_size = max_size;
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_update_lru(session_cache_t* cache, session_entry_t* entry) {
    if (cache == NULL || entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    session_cache_remove_from_lru(cache, entry);
    session_cache_add_to_lru(cache, entry);
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_remove_from_lru(session_cache_t* cache, session_entry_t* entry) {
    if (cache == NULL || entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (entry->lru_prev != NULL) {
        entry->lru_prev->lru_next = entry->lru_next;
    } else {
        cache->lru_head = entry->lru_next;
    }
    
    if (entry->lru_next != NULL) {
        entry->lru_next->lru_prev = entry->lru_prev;
    } else {
        cache->lru_tail = entry->lru_prev;
    }
    
    entry->lru_prev = NULL;
    entry->lru_next = NULL;
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_add_to_lru(session_cache_t* cache, session_entry_t* entry) {
    if (cache == NULL || entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    entry->lru_next = cache->lru_head;
    entry->lru_prev = NULL;
    
    if (cache->lru_head != NULL) {
        cache->lru_head->lru_prev = entry;
    }
    cache->lru_head = entry;
    
    if (cache->lru_tail == NULL) {
        cache->lru_tail = entry;
    }
    
    return CHRONOS_OK;
}

session_entry_t* session_cache_find_entry(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len) {
    if (cache == NULL || session_id == NULL) {
        return NULL;
    }
    
    uint32_t hash = session_hash(session_id, session_id_len);
    session_entry_t* entry = cache->entries[hash];
    
    while (entry != NULL) {
        if (entry->session_id_len == session_id_len &&
            memcmp(entry->session_id, session_id, session_id_len) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

chronos_error_t session_cache_increment_ref_count(session_cache_t* cache, session_entry_t* entry) {
    if (cache == NULL || entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    entry->ref_count++;
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_decrement_ref_count(session_cache_t* cache, session_entry_t* entry) {
    if (cache == NULL || entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    entry->ref_count--;
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_export_session(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len,
                                             uint8_t* ticket, size_t* ticket_len) {
    if (cache == NULL || session_id == NULL || ticket == NULL || ticket_len == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    session_entry_t* entry = session_cache_find_entry(cache, session_id, session_id_len);
    if (entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    size_t required_len = entry->session_id_len + 48 + 4 + 2 + 2;
    if (*ticket_len < required_len) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    }
    
    size_t offset = 0;
    memcpy(ticket + offset, entry->session_id, entry->session_id_len);
    offset += entry->session_id_len;
    memcpy(ticket + offset, entry->master_secret, 48);
    offset += 48;
    memcpy(ticket + offset, &entry->ticket_lifetime, 4);
    offset += 4;
    memcpy(ticket + offset, &entry->cipher_suite, 2);
    offset += 2;
    memcpy(ticket + offset, &entry->version, 2);
    offset += 2;
    
    *ticket_len = offset;
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_import_session(session_cache_t* cache, const uint8_t* ticket, size_t ticket_len,
                                             uint8_t* session_id, size_t* session_id_len) {
    if (cache == NULL || ticket == NULL || session_id == NULL || session_id_len == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (ticket_len < 32 + 48 + 4 + 2 + 2) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    size_t offset = 0;
    size_t id_len = 32;
    memcpy(session_id, ticket + offset, id_len);
    offset += id_len;
    *session_id_len = id_len;
    
    uint8_t master_secret[48];
    memcpy(master_secret, ticket + offset, 48);
    offset += 48;
    
    uint32_t lifetime;
    memcpy(&lifetime, ticket + offset, 4);
    offset += 4;
    
    uint16_t cipher_suite;
    memcpy(&cipher_suite, ticket + offset, 2);
    offset += 2;
    
    uint16_t version;
    memcpy(&version, ticket + offset, 2);
    offset += 2;
    
    chronos_error_t err = session_cache_insert(cache, session_id, id_len, master_secret, cipher_suite, version);
    
    return err;
}

chronos_error_t session_cache_update_key(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len) {
    if (cache == NULL || session_id == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    session_entry_t* entry = session_cache_find_entry(cache, session_id, session_id_len);
    if (entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    entry->key_update_count++;
    entry->last_key_update_time = time(NULL);
    entry->key_update_pending = false;
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_check_early_data(session_cache_t* cache, session_entry_t* entry, 
                                               uint64_t current_time, bool* accept) {
    if (cache == NULL || entry == NULL || accept == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    *accept = false;
    
    if (!cache->enable_early_data || !entry->has_early_data) {
        return CHRONOS_OK;
    }
    
    if (current_time < entry->early_data_expiration) {
        *accept = true;
    }
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_set_alpn(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len,
                                       const uint8_t* protocol, size_t protocol_len) {
    if (cache == NULL || session_id == NULL || protocol == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (protocol_len > 16) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    session_entry_t* entry = session_cache_find_entry(cache, session_id, session_id_len);
    if (entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    memcpy(entry->alpn_protocol, protocol, protocol_len);
    entry->alpn_len = protocol_len;
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_get_alpn(session_cache_t* cache, const uint8_t* session_id, size_t session_id_len,
                                       uint8_t* protocol, size_t* protocol_len) {
    if (cache == NULL || session_id == NULL || protocol == NULL || protocol_len == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    session_entry_t* entry = session_cache_find_entry(cache, session_id, session_id_len);
    if (entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (*protocol_len < entry->alpn_len) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    }
    
    memcpy(protocol, entry->alpn_protocol, entry->alpn_len);
    *protocol_len = entry->alpn_len;
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_reset(session_cache_t* cache) {
    if (cache == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    for (uint32_t i = 0; i < CHRONOS_SESSION_CACHE_SIZE; i++) {
        session_entry_t* entry = cache->entries[i];
        while (entry != NULL) {
            session_entry_t* next = entry->next;
            if (entry->session_ticket != NULL) {
                chronos_free(entry->session_ticket);
            }
            chronos_free(entry);
            entry = next;
        }
        cache->entries[i] = NULL;
    }
    
    cache->lru_head = NULL;
    cache->lru_tail = NULL;
    cache->active_entries = 0;
    cache->total_entries = 0;
    cache->cache_hits = 0;
    cache->cache_misses = 0;
    cache->evictions = 0;
    
    return CHRONOS_OK;
}

chronos_error_t session_cache_compact(session_cache_t* cache) {
    if (cache == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    for (uint32_t i = 0; i < CHRONOS_SESSION_CACHE_SIZE; i++) {
        session_entry_t* entry = cache->entries[i];
        while (entry != NULL) {
            session_entry_t* next = entry->next;
            if (!entry->is_valid || entry->ref_count == 0) {
                if (entry->prev != NULL) {
                    entry->prev->next = entry->next;
                } else {
                    cache->entries[i] = entry->next;
                }
                if (entry->next != NULL) {
                    entry->next->prev = entry->prev;
                }
                session_cache_remove_from_lru(cache, entry);
                if (entry->session_ticket != NULL) {
                    chronos_free(entry->session_ticket);
                }
                chronos_free(entry);
                cache->active_entries--;
            }
            entry = next;
        }
    }
    
    return CHRONOS_OK;
}
