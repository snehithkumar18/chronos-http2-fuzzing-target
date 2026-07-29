#include "dynamic_table_ref.h"
#include "../utils/memory.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

static uint32_t global_table_counter = 0;
static uint32_t global_entry_counter = 0;

static uint32_t calculate_entry_hash(const char* name, size_t name_len) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < name_len; i++) {
        hash = ((hash << 5) + hash) + name[i];
    }
    return hash;
}

dynamic_table_ref_t* dynamic_table_ref_create(size_t max_size, uint32_t max_entries, uint8_t table_id) {
    if (max_size == 0 || max_entries == 0 || max_entries > 100000) {
        return NULL;
    }
    
    dynamic_table_ref_t* table = (dynamic_table_ref_t*)chronos_malloc(sizeof(dynamic_table_ref_t));
    if (table == NULL) {
        return NULL;
    }
    
    memset(table, 0, sizeof(dynamic_table_ref_t));
    table->max_size = max_size;
    table->max_entries = max_entries;
    table->next_entry_id = 1;
    table->table_version = 1;
    table->table_creation_time = time(NULL);
    table->last_update_time = table->table_creation_time;
    table->enable_lru = true;
    table->enable_ref_counting = true;
    table->enable_dependencies = true;
    table->enable_compression = false;
    table->enable_ttl = false;
    table->is_corrupted = false;
    table->hash_table_size = 256;
    table->resize_threshold = 75;
    table->compact_threshold = 50;
    table->max_pending_updates = 1000;
    table->max_ref_count = 1000000;
    table->min_ref_count = 0;
    table->table_id = table_id;
    
    table->hash_table = (table_ref_entry_t**)chronos_malloc(table->hash_table_size * sizeof(table_ref_entry_t*));
    if (table->hash_table == NULL) {
        chronos_free(table);
        return NULL;
    }
    
    memset(table->hash_table, 0, table->hash_table_size * sizeof(table_ref_entry_t*));
    
    global_table_counter++;
    
    return table;
}

void dynamic_table_ref_destroy(dynamic_table_ref_t* table) {
    if (table == NULL) {
        return;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        table_ref_entry_t* next = entry->next;
        
        if (entry->dependents != NULL) {
            chronos_free(entry->dependents);
        }
        if (entry->name != NULL) {
            chronos_free(entry->name);
        }
        if (entry->value != NULL) {
            chronos_free(entry->value);
        }
        chronos_free(entry);
        
        entry = next;
    }
    
    if (table->hash_table != NULL) {
        chronos_free(table->hash_table);
    }
    
    chronos_free(table);
}

chronos_error_t dynamic_table_ref_add(dynamic_table_ref_t* table, const char* name, size_t name_len,
                                      const char* value, size_t value_len) {
    if (table == NULL || name == NULL || value == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (table->is_corrupted) {
        return CHRONOS_ERROR_INVALID_STATE;
    }
    
    size_t entry_size = name_len + value_len + 32;
    if (entry_size > table->max_size) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (table->current_size + entry_size > table->max_size) {
        chronos_error_t err = dynamic_table_ref_evict(table, entry_size);
        if (err != CHRONOS_OK) {
            return err;
        }
    }
    
    table_ref_entry_t* entry = (table_ref_entry_t*)chronos_malloc(sizeof(table_ref_entry_t));
    if (entry == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    }
    
    memset(entry, 0, sizeof(table_ref_entry_t));
    
    entry->name = (char*)chronos_malloc(name_len + 1);
    if (entry->name == NULL) {
        chronos_free(entry);
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    }
    
    entry->value = (char*)chronos_malloc(value_len + 1);
    if (entry->value == NULL) {
        chronos_free(entry->name);
        chronos_free(entry);
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    }
    
    memcpy(entry->name, name, name_len);
    entry->name[name_len] = '\0';
    memcpy(entry->value, value, value_len);
    entry->value[value_len] = '\0';
    
    entry->name_len = name_len;
    entry->value_len = value_len;
    entry->entry_size = entry_size;
    entry->entry_id = table->next_entry_id++;
    entry->creation_time = time(NULL);
    entry->last_access_time = entry->creation_time;
    entry->access_count = 1;
    entry->is_static = false;
    entry->is_evicted = false;
    entry->is_locked = false;
    entry->entry_flags = 0;
    entry->dependency_id = 0;
    entry->dependent_count = 0;
    entry->dependents = NULL;
    entry->dependents_capacity = 0;
    entry->compression_level = 0;
    entry->ref_count = 1;
    entry->priority = 0;
    entry->ttl = 0;
    entry->expiration_time = 0;
    entry->is_expired = false;
    entry->version = table->table_version;
    entry->parent_id = 0;
    entry->has_parent = false;
    
    dynamic_table_ref_calculate_hash(table, name, name_len, &entry->hash);
    
    entry->next = table->entries;
    if (table->entries != NULL) {
        table->entries->prev = entry;
    }
    table->entries = entry;
    
    if (table->tail == NULL) {
        table->tail = entry;
    }
    
    dynamic_table_ref_insert_hash(table, entry);
    dynamic_table_ref_add_to_lru(table, entry);
    
    table->current_size += entry_size;
    table->used_size += entry_size;
    table->entry_count++;
    table->total_insertions++;
    table->last_update_time = time(NULL);
    
    global_entry_counter++;
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_get(dynamic_table_ref_t* table, uint32_t index, 
                                      table_ref_entry_t** entry) {
    if (table == NULL || entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (index >= table->entry_count) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t* current = table->entries;
    uint32_t current_index = 0;
    
    while (current != NULL) {
        if (current_index == index) {
            *entry = current;
            current->last_access_time = time(NULL);
            current->access_count++;
            table->total_lookups++;
            table->total_hits++;
            dynamic_table_ref_update_lru(table, current);
            return CHRONOS_OK;
        }
        current_index++;
        current = current->next;
    }
    
    table->total_lookups++;
    table->total_misses++;
    *entry = NULL;
    
    return CHRONOS_ERROR_INVALID_INPUT;
}

chronos_error_t dynamic_table_ref_lookup(dynamic_table_ref_t* table, const char* name, size_t name_len,
                                          const char* value, size_t value_len, table_ref_entry_t** entry) {
    if (table == NULL || name == NULL || entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    uint32_t hash;
    dynamic_table_ref_calculate_hash(table, name, name_len, &hash);
    
    chronos_error_t err = dynamic_table_ref_lookup_hash(table, hash, name, name_len, entry);
    if (err != CHRONOS_OK) {
        table->total_lookups++;
        table->total_misses++;
        return err;
    }
    
    if (*entry != NULL) {
        if (value != NULL && value_len > 0) {
            if ((*entry)->value_len != value_len || memcmp((*entry)->value, value, value_len) != 0) {
                *entry = NULL;
                table->total_lookups++;
                table->total_misses++;
                return CHRONOS_ERROR_INVALID_INPUT;
            }
        }
        (*entry)->last_access_time = time(NULL);
        (*entry)->access_count++;
        table->total_lookups++;
        table->total_hits++;
        dynamic_table_ref_update_lru(table, *entry);
    } else {
        table->total_lookups++;
        table->total_misses++;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_remove(dynamic_table_ref_t* table, uint32_t entry_id) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        if (entry->entry_id == entry_id) {
            break;
        }
        entry = entry->next;
    }
    
    if (entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (entry->prev != NULL) {
        entry->prev->next = entry->next;
    } else {
        table->entries = entry->next;
    }
    
    if (entry->next != NULL) {
        entry->next->prev = entry->prev;
    }
    
    if (entry == table->tail) {
        table->tail = entry->prev;
    }
    
    dynamic_table_ref_remove_hash(table, entry);
    dynamic_table_ref_remove_from_lru(table, entry);
    
    if (entry->dependents != NULL) {
        chronos_free(entry->dependents);
    }
    if (entry->name != NULL) {
        chronos_free(entry->name);
    }
    if (entry->value != NULL) {
        chronos_free(entry->value);
    }
    
    chronos_free(entry);
    
    table->current_size -= entry->entry_size;
    table->entry_count--;
    table->total_evictions++;
    table->last_update_time = time(NULL);
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_resize(dynamic_table_ref_t* table, size_t new_size) {
    if (table == NULL || new_size < table->current_size) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table->max_size = new_size;
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_evict(dynamic_table_ref_t* table, size_t needed_space) {
    if (table == NULL || needed_space == 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    size_t freed_space = 0;
    while (freed_space < needed_space && table->lru_tail != NULL) {
        table_ref_entry_t* entry = table->lru_tail;
        
        if (entry->is_locked) {
            table_ref_entry_t* prev = entry->lru_prev;
            dynamic_table_ref_remove_from_lru(table, entry);
            dynamic_table_ref_add_to_lru(table, entry);
            entry = prev;
            continue;
        }
        
        if (entry->prev != NULL) {
            entry->prev->next = entry->next;
        } else {
            table->entries = entry->next;
        }
        
        if (entry->next != NULL) {
            entry->next->prev = entry->prev;
        }
        
        if (entry == table->tail) {
            table->tail = entry->prev;
        }
        
        dynamic_table_ref_remove_hash(table, entry);
        dynamic_table_ref_remove_from_lru(table, entry);
        freed_space += entry->entry_size;
        
        if (entry->dependents != NULL) {
            chronos_free(entry->dependents);
        }
        if (entry->name != NULL) {
            chronos_free(entry->name);
        }
        if (entry->value != NULL) {
            chronos_free(entry->value);
        }
        
        chronos_free(entry);
        
        table->current_size -= entry->entry_size;
        table->entry_count--;
        table->total_evictions++;
    }
    
    table->last_update_time = time(NULL);
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_increment_ref(dynamic_table_ref_t* table, uint32_t entry_id) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        if (entry->entry_id == entry_id) {
            break;
        }
        entry = entry->next;
    }
    
    if (entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    entry->ref_count++;
    table->ref_count_updates++;
    
    if (entry->ref_count > table->max_ref_count) {
        table->ref_count_overflows++;
    }
    
    if (entry->ref_count < 32) {
        table->ref_count_histogram[entry->ref_count]++;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_decrement_ref(dynamic_table_ref_t* table, uint32_t entry_id) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        if (entry->entry_id == entry_id) {
            break;
        }
        entry = entry->next;
    }
    
    if (entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    entry->ref_count--;
    table->ref_count_updates++;
    
    if (entry->ref_count < table->min_ref_count) {
        table->ref_count_underflows++;
    }
    
    if (entry->ref_count == 0 && !entry->is_locked) {
        dynamic_table_ref_remove(table, entry_id);
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_add_dependency(dynamic_table_ref_t* table, uint32_t entry_id, 
                                                   uint32_t dependency_id) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        if (entry->entry_id == entry_id) {
            break;
        }
        entry = entry->next;
    }
    
    if (entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    entry->dependency_id = dependency_id;
    entry->has_parent = true;
    
    table_ref_entry_t* parent = table->entries;
    while (parent != NULL) {
        if (parent->entry_id == dependency_id) {
            break;
        }
        parent = parent->next;
    }
    
    if (parent != NULL) {
        if (parent->dependent_count >= parent->dependents_capacity) {
            uint32_t new_capacity = parent->dependents_capacity == 0 ? 4 : parent->dependents_capacity * 2;
            table_ref_entry_t** new_dependents = (table_ref_entry_t**)chronos_malloc(new_capacity * sizeof(table_ref_entry_t*));
            if (new_dependents == NULL) {
                return CHRONOS_ERROR_OUT_OF_MEMORY;
            }
            if (parent->dependents != NULL) {
                memcpy(new_dependents, parent->dependents, parent->dependent_count * sizeof(table_ref_entry_t*));
                chronos_free(parent->dependents);
            }
            parent->dependents = new_dependents;
            parent->dependents_capacity = new_capacity;
        }
        parent->dependents[parent->dependent_count++] = entry;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_remove_dependency(dynamic_table_ref_t* table, uint32_t entry_id,
                                                    uint32_t dependency_id) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        if (entry->entry_id == entry_id) {
            break;
        }
        entry = entry->next;
    }
    
    if (entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    entry->dependency_id = 0;
    entry->has_parent = false;
    
    table_ref_entry_t* parent = table->entries;
    while (parent != NULL) {
        if (parent->entry_id == dependency_id) {
            break;
        }
        parent = parent->next;
    }
    
    if (parent != NULL && parent->dependents != NULL) {
        for (uint32_t i = 0; i < parent->dependent_count; i++) {
            if (parent->dependents[i] == entry) {
                for (uint32_t j = i; j < parent->dependent_count - 1; j++) {
                    parent->dependents[j] = parent->dependents[j + 1];
                }
                parent->dependent_count--;
                break;
            }
        }
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_lock_entry(dynamic_table_ref_t* table, uint32_t entry_id) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        if (entry->entry_id == entry_id) {
            break;
        }
        entry = entry->next;
    }
    
    if (entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    entry->is_locked = true;
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_unlock_entry(dynamic_table_ref_t* table, uint32_t entry_id) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        if (entry->entry_id == entry_id) {
            break;
        }
        entry = entry->next;
    }
    
    if (entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    entry->is_locked = false;
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_set_ttl(dynamic_table_ref_t* table, uint32_t entry_id, uint32_t ttl_ms) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        if (entry->entry_id == entry_id) {
            break;
        }
        entry = entry->next;
    }
    
    if (entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    entry->ttl = ttl_ms;
    entry->expiration_time = time(NULL) + (ttl_ms / 1000);
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_check_expiration(dynamic_table_ref_t* table) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (!table->enable_ttl) {
        return CHRONOS_OK;
    }
    
    uint64_t current_time = time(NULL);
    uint32_t expired_count = 0;
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        table_ref_entry_t* next = entry->next;
        
        if (entry->ttl > 0 && current_time > entry->expiration_time) {
            if (!entry->is_locked) {
                dynamic_table_ref_remove(table, entry->entry_id);
                expired_count++;
            }
        }
        entry = next;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_validate(dynamic_table_ref_t* table) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t* slow = table->lru_head;
    table_ref_entry_t* fast = table->lru_head;
    
    uint32_t steps = 0;
    while (fast != NULL && fast->lru_next != NULL && steps < table->entry_count + 10) {
        slow = slow->lru_next;
        fast = fast->lru_next->lru_next;
        steps++;
        
        if (slow == fast) {
            table->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        }
    }
    
    for (uint32_t i = 0; i < table->hash_table_size; i++) {
        table_ref_entry_t* entry = table->hash_table[i];
        uint32_t chain_count = 0;
        
        while (entry != NULL) {
            chain_count++;
            if (chain_count > table->entry_count + 10) {
                table->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
            }
            entry = entry->hash_next;
        }
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_detect_corruption(dynamic_table_ref_t* table) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table->corruption_detection_counter++;
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        if (entry->is_evicted && entry->ref_count > 0) {
            table->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        }
        entry = entry->next;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_recover_corruption(dynamic_table_ref_t* table) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (!table->is_corrupted) {
        return CHRONOS_OK;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        if (entry->is_evicted) {
            entry->ref_count = 0;
        } else {
            entry->ref_count = 1;
        }
        entry = entry->next;
    }
    
    table->lru_head = NULL;
    table->lru_tail = NULL;
    
    entry = table->entries;
    while (entry != NULL) {
        if (!entry->is_evicted) {
            dynamic_table_ref_add_to_lru(table, entry);
        }
        entry = entry->next;
    }
    
    table->is_corrupted = false;
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_detect_deadlock(dynamic_table_ref_t* table) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table->deadlock_detection_counter++;
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        if (entry->has_parent) {
            table_ref_entry_t* current = entry;
            uint32_t steps = 0;
            
            while (current != NULL && current->has_parent && steps < table->entry_count + 10) {
                table_ref_entry_t* parent = table->entries;
                while (parent != NULL) {
                    if (parent->entry_id == current->dependency_id) {
                        break;
                    }
                    parent = parent->next;
                }
                
                if (parent == entry) {
                    table->deadlock_detected = true;
                    return CHRONOS_ERROR_INVALID_STATE;
                }
                
                current = parent;
                steps++;
            }
        }
        entry = entry->next;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_resolve_deadlock(dynamic_table_ref_t* table) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (!table->deadlock_detected) {
        return CHRONOS_OK;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        entry->dependency_id = 0;
        entry->has_parent = false;
        entry = entry->next;
    }
    
    table->deadlock_detected = false;
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_compact(dynamic_table_ref_t* table) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        table_ref_entry_t* next = entry->next;
        
        if (entry->is_evicted || entry->ref_count == 0) {
            dynamic_table_ref_remove(table, entry->entry_id);
        }
        entry = next;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_defragment(dynamic_table_ref_t* table) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table->defrag_counter++;
    table->last_defrag_time = time(NULL);
    
    chronos_free(table->hash_table);
    table->hash_table_size = table->hash_table_size * 2;
    table->hash_table = (table_ref_entry_t**)chronos_malloc(table->hash_table_size * sizeof(table_ref_entry_t*));
    if (table->hash_table == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    }
    
    memset(table->hash_table, 0, table->hash_table_size * sizeof(table_ref_entry_t*));
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        entry->hash_next = NULL;
        dynamic_table_ref_insert_hash(table, entry);
        entry = entry->next;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_get_stats(dynamic_table_ref_t* table, table_ref_stats_t* stats) {
    if (table == NULL || stats == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    memset(stats, 0, sizeof(table_ref_stats_t));
    
    stats->total_entries = table->entry_count;
    stats->active_entries = table->entry_count;
    stats->evicted_entries = table->total_evictions;
    stats->total_ref_count = table->ref_count_updates;
    
    if (table->total_lookups > 0) {
        stats->table_hit_rate = (table->total_hits * 100) / table->total_lookups;
    }
    
    if (table->entry_count > 0) {
        stats->average_entry_size = table->current_size / table->entry_count;
        stats->table_utilization = (table->current_size * 100) / table->max_size;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_update_lru(dynamic_table_ref_t* table, table_ref_entry_t* entry) {
    if (table == NULL || entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (!table->enable_lru) {
        return CHRONOS_OK;
    }
    
    dynamic_table_ref_remove_from_lru(table, entry);
    dynamic_table_ref_add_to_lru(table, entry);
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_remove_from_lru(dynamic_table_ref_t* table, table_ref_entry_t* entry) {
    if (table == NULL || entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (entry->lru_prev != NULL) {
        entry->lru_prev->lru_next = entry->lru_next;
    } else {
        table->lru_head = entry->lru_next;
    }
    
    if (entry->lru_next != NULL) {
        entry->lru_next->lru_prev = entry->lru_prev;
    } else {
        table->lru_tail = entry->lru_prev;
    }
    
    entry->lru_prev = NULL;
    entry->lru_next = NULL;
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_add_to_lru(dynamic_table_ref_t* table, table_ref_entry_t* entry) {
    if (table == NULL || entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (!table->enable_lru) {
        return CHRONOS_OK;
    }
    
    entry->lru_next = table->lru_head;
    entry->lru_prev = NULL;
    
    if (table->lru_head != NULL) {
        table->lru_head->lru_prev = entry;
    }
    
    table->lru_head = entry;
    
    if (table->lru_tail == NULL) {
        table->lru_tail = entry;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_calculate_hash(dynamic_table_ref_t* table, const char* name, 
                                                   size_t name_len, uint32_t* hash) {
    if (table == NULL || name == NULL || hash == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    *hash = calculate_entry_hash(name, name_len) % table->hash_table_size;
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_insert_hash(dynamic_table_ref_t* table, table_ref_entry_t* entry) {
    if (table == NULL || entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    uint32_t hash_index = entry->hash % table->hash_table_size;
    entry->hash_next = table->hash_table[hash_index];
    table->hash_table[hash_index] = entry;
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_remove_hash(dynamic_table_ref_t* table, table_ref_entry_t* entry) {
    if (table == NULL || entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    uint32_t hash_index = entry->hash % table->hash_table_size;
    table_ref_entry_t* current = table->hash_table[hash_index];
    table_ref_entry_t* prev = NULL;
    
    while (current != NULL) {
        if (current == entry) {
            if (prev != NULL) {
                prev->hash_next = current->hash_next;
            } else {
                table->hash_table[hash_index] = current->hash_next;
            }
            break;
        }
        prev = current;
        current = current->hash_next;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_lookup_hash(dynamic_table_ref_t* table, uint32_t hash,
                                               const char* name, size_t name_len, table_ref_entry_t** entry) {
    if (table == NULL || entry == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    uint32_t hash_index = hash % table->hash_table_size;
    table_ref_entry_t* current = table->hash_table[hash_index];
    
    while (current != NULL) {
        if (current->name_len == name_len && memcmp(current->name, name, name_len) == 0) {
            *entry = current;
            return CHRONOS_OK;
        }
        current = current->hash_next;
    }
    
    *entry = NULL;
    
    return CHRONOS_ERROR_INVALID_INPUT;
}

chronos_error_t dynamic_table_ref_resize_hash_table(dynamic_table_ref_t* table, uint32_t new_size) {
    if (table == NULL || new_size == 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t** new_hash_table = (table_ref_entry_t**)chronos_malloc(new_size * sizeof(table_ref_entry_t*));
    if (new_hash_table == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    }
    
    memset(new_hash_table, 0, new_size * sizeof(table_ref_entry_t*));
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        entry->hash = calculate_entry_hash(entry->name, entry->name_len) % new_size;
        entry->hash_next = new_hash_table[entry->hash];
        new_hash_table[entry->hash] = entry;
        entry = entry->next;
    }
    
    chronos_free(table->hash_table);
    table->hash_table = new_hash_table;
    table->hash_table_size = new_size;
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_process_pending_updates(dynamic_table_ref_t* table) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table->pending_ref_updates = 0;
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_queue_ref_update(dynamic_table_ref_t* table, uint32_t entry_id, 
                                                    int32_t delta) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table->pending_ref_updates++;
    
    if (table->pending_ref_updates > table->max_pending_updates) {
        dynamic_table_ref_process_pending_updates(table);
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_detect_circular_refs(dynamic_table_ref_t* table) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table->circular_ref_count = 0;
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        if (entry->has_parent) {
            table_ref_entry_t* current = entry;
            uint32_t steps = 0;
            
            while (current != NULL && current->has_parent && steps < table->entry_count + 10) {
                table_ref_entry_t* parent = table->entries;
                while (parent != NULL) {
                    if (parent->entry_id == current->dependency_id) {
                        break;
                    }
                    parent = parent->next;
                }
                
                if (parent == entry) {
                    table->circular_ref_count++;
                    break;
                }
                
                current = parent;
                steps++;
            }
        }
        entry = entry->next;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_break_circular_refs(dynamic_table_ref_t* table) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        if (entry->has_parent) {
            table_ref_entry_t* current = entry;
            uint32_t steps = 0;
            
            while (current != NULL && current->has_parent && steps < table->entry_count + 10) {
                table_ref_entry_t* parent = table->entries;
                while (parent != NULL) {
                    if (parent->entry_id == current->dependency_id) {
                        break;
                    }
                    parent = parent->next;
                }
                
                if (parent == entry) {
                    current->dependency_id = 0;
                    current->has_parent = false;
                    break;
                }
                
                current = parent;
                steps++;
            }
        }
        entry = entry->next;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_validate_ref_counts(dynamic_table_ref_t* table) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        if (entry->ref_count < 0) {
            table->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        }
        entry = entry->next;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_reset(dynamic_table_ref_t* table) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    table_ref_entry_t* entry = table->entries;
    while (entry != NULL) {
        table_ref_entry_t* next = entry->next;
        
        if (entry->dependents != NULL) {
            chronos_free(entry->dependents);
        }
        if (entry->name != NULL) {
            chronos_free(entry->name);
        }
        if (entry->value != NULL) {
            chronos_free(entry->value);
        }
        chronos_free(entry);
        
        entry = next;
    }
    
    table->entries = NULL;
    table->tail = NULL;
    table->lru_head = NULL;
    table->lru_tail = NULL;
    table->current_size = 0;
    table->used_size = 0;
    table->entry_count = 0;
    table->next_entry_id = 1;
    
    for (uint32_t i = 0; i < table->hash_table_size; i++) {
        table->hash_table[i] = NULL;
    }
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_snapshot(dynamic_table_ref_t* table, uint8_t* snapshot, 
                                            size_t* snapshot_len) {
    if (table == NULL || snapshot == NULL || snapshot_len == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    size_t required_len = sizeof(size_t) * 2 + sizeof(uint32_t) * 5;
    if (*snapshot_len < required_len) {
        *snapshot_len = required_len;
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    }
    
    size_t offset = 0;
    memcpy(snapshot + offset, &table->max_size, sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(snapshot + offset, &table->current_size, sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(snapshot + offset, &table->entry_count, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &table->next_entry_id, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &table->table_version, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &table->total_insertions, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    *snapshot_len = offset;
    
    return CHRONOS_OK;
}

chronos_error_t dynamic_table_ref_restore(dynamic_table_ref_t* table, const uint8_t* snapshot, 
                                          size_t snapshot_len) {
    if (table == NULL || snapshot == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    size_t required_len = sizeof(size_t) * 2 + sizeof(uint32_t) * 5;
    if (snapshot_len < required_len) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    size_t offset = 0;
    memcpy(&table->max_size, snapshot + offset, sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(&table->current_size, snapshot + offset, sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(&table->entry_count, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&table->next_entry_id, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&table->table_version, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&table->total_insertions, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    return CHRONOS_OK;
}
