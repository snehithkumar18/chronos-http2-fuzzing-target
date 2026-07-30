#include "connection_state.h"
#include "../utils/memory.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

connection_state_machine_t* connection_state_machine_create(uint32_t connection_id) {
    connection_state_machine_t* sm = (connection_state_machine_t*)chronos_malloc(sizeof(connection_state_machine_t));
    if (sm == NULL) {
        return NULL;
    }
    
    memset(sm, 0, sizeof(connection_state_machine_t));
    sm->current_state = CONN_STATE_IDLE;
    sm->previous_state = CONN_STATE_IDLE;
    sm->target_state = CONN_STATE_IDLE;
    sm->state_entry_time = time(NULL);
    sm->state_change_count = 0;
    sm->total_state_changes = 0;
    sm->is_transitioning = false;
    sm->is_blocked = false;
    sm->block_count = 0;
    sm->min_state_duration_ms = 0;
    sm->enable_timeout_check = true;
    sm->enable_state_validation = true;
    sm->validation_counter = 0;
    sm->invalid_state_count = 0;
    sm->is_corrupted = false;
    sm->corruption_detection_counter = 0;
    sm->transition_history_size = 100;
    sm->event_queue_size = 1000;
    sm->pending_events = 0;
    sm->processed_events = 0;
    sm->dropped_events = 0;
    sm->max_queue_depth = 0;
    sm->current_queue_depth = 0;
    sm->connection_id = connection_id;
    sm->connection_creation_time = time(NULL);
    sm->state_machine_version = 1;
    sm->last_transition_id = 0;
    sm->last_event_id = 0;
    sm->state_flags = 0;
    sm->concurrent_transitions = 0;
    sm->max_concurrent_transitions = 5;
    sm->enable_concurrent_transitions = false;
    sm->rollback_count = 0;
    sm->commit_count = 0;
    sm->abort_count = 0;
    
    return sm;
}

void connection_state_machine_destroy(connection_state_machine_t* sm) {
    if (sm == NULL) {
        return;
    }
    
    state_transition_t* transition = sm->transition_history;
    while (transition != NULL) {
        state_transition_t* next = transition->next;
        chronos_free(transition);
        transition = next;
    }
    
    connection_event_t* event = sm->event_queue_head;
    while (event != NULL) {
        connection_event_t* next = event->next;
        chronos_free(event);
        event = next;
    }
    
    chronos_free(sm);
}

chronos_error_t connection_state_machine_transition(connection_state_machine_t* sm, connection_state_t new_state) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (sm->is_corrupted) {
        return CHRONOS_ERROR_INVALID_STATE;
    }
    
    if (sm->is_blocked) {
        sm->block_count++;
        return CHRONOS_ERROR_INVALID_STATE;
    }
    
    if (sm->is_transitioning && !sm->enable_concurrent_transitions) {
        return CHRONOS_ERROR_INVALID_STATE;
    }
    
    chronos_error_t err = connection_state_validate_transition(sm, sm->current_state, new_state);
    if (err != CHRONOS_OK) {
        sm->invalid_state_count++;
        return err;
    }
    
    sm->previous_state = sm->current_state;
    sm->target_state = new_state;
    sm->is_transitioning = true;
    
    state_transition_t* transition = (state_transition_t*)chronos_malloc(sizeof(state_transition_t));
    if (transition == NULL) {
        sm->is_transitioning = false;
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    }
    
    memset(transition, 0, sizeof(state_transition_t));
    transition->from_state = sm->current_state;
    transition->to_state = new_state;
    transition->timestamp = time(NULL);
    transition->is_valid = true;
    transition->transition_flags = 0;
    transition->event_count = sm->pending_events;
    transition->transition_id = ++sm->last_transition_id;
    transition->parent_transition_id = 0;
    
    connection_state_add_transition_to_history(sm, transition);
    
    uint64_t exit_time = time(NULL);
    sm->state_exit_time = exit_time;
    sm->state_duration_ms = (uint32_t)(exit_time - sm->state_entry_time) * 1000;
    sm->current_state = new_state;
    sm->state_entry_time = time(NULL);
    sm->state_change_count++;
    sm->total_state_changes++;
    sm->concurrent_transitions++;
    sm->is_transitioning = false;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_add_event(connection_state_machine_t* sm, connection_event_t* event) {
    if (sm == NULL || event == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (sm->is_corrupted) {
        return CHRONOS_ERROR_INVALID_STATE;
    }
    
    event->event_id = ++sm->last_event_id;
    event->timestamp = time(NULL);
    event->is_processed = false;
    event->next = NULL;
    event->prev = NULL;
    
    if (sm->pending_events >= sm->event_queue_size) {
        if (sm->event_queue_head != NULL) {
            connection_event_t* old = sm->event_queue_head;
            sm->event_queue_head = old->next;
            if (sm->event_queue_head != NULL) {
                sm->event_queue_head->prev = NULL;
            } else {
                sm->event_queue_tail = NULL;
            }
            chronos_free(old);
            sm->dropped_events++;
            sm->pending_events--;
        }
    }
    
    event->prev = sm->event_queue_tail;
    if (sm->event_queue_tail != NULL) {
        sm->event_queue_tail->next = event;
    }
    sm->event_queue_tail = event;
    
    if (sm->event_queue_head == NULL) {
        sm->event_queue_head = event;
    }
    
    sm->pending_events++;
    sm->current_queue_depth = sm->pending_events;
    if (sm->current_queue_depth > sm->max_queue_depth) {
        sm->max_queue_depth = sm->current_queue_depth;
    }
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_process_events(connection_state_machine_t* sm) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (sm->is_corrupted) {
        return CHRONOS_ERROR_INVALID_STATE;
    }
    
    connection_event_t* event = sm->event_queue_head;
    uint32_t processed = 0;
    
    while (event != NULL && processed < 100) {
        connection_event_t* next = event->next;
        
        event->is_processed = true;
        event->processing_time = time(NULL);
        
        switch (sm->current_state) {
            case CONN_STATE_IDLE:
                break;
            case CONN_STATE_CONNECTING:
                break;
            case CONN_STATE_HANDSHAKE:
                break;
            case CONN_STATE_PREFACE:
                break;
            case CONN_STATE_SETTINGS:
                break;
            case CONN_STATE_OPEN:
                break;
            case CONN_STATE_CLOSING:
                break;
            case CONN_STATE_CLOSED:
                break;
            case CONN_STATE_DRAINING:
                break;
            case CONN_STATE_ERROR:
                break;
            default:
                sm->invalid_state_count++;
                break;
        }
        
        if (event->prev != NULL) {
            event->prev->next = event->next;
        } else {
            sm->event_queue_head = event->next;
        }
        
        if (event->next != NULL) {
            event->next->prev = event->prev;
        } else {
            sm->event_queue_tail = event->prev;
        }
        
        chronos_free(event);
        sm->pending_events--;
        sm->processed_events++;
        processed++;
        
        event = next;
    }
    
    sm->current_queue_depth = sm->pending_events;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_validate(connection_state_machine_t* sm, state_validation_result_t* result) {
    if (sm == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    memset(result, 0, sizeof(state_validation_result_t));
    result->is_valid = true;
    result->actual_state = sm->current_state;
    result->expected_state = sm->current_state;
    sm->validation_counter++;
    
    chronos_error_t err = connection_state_check_state_invariant(sm);
    if (err != CHRONOS_OK) {
        result->is_valid = false;
        result->validation_code = 1;
        strcpy(result->validation_message, "State invariant violation");
        result->error_count++;
    }
    
    if (sm->is_blocked) {
        uint64_t block_duration = time(NULL) - sm->block_start_time;
        if (block_duration > 30000) {
            result->is_valid = false;
            result->validation_code = 2;
            strcpy(result->validation_message, "State blocked for too long");
            result->error_count++;
        }
    }
    
    if (sm->is_transitioning) {
        uint64_t transition_duration = time(NULL) - sm->state_entry_time;
        if (transition_duration > 30000) {
            result->is_valid = false;
            result->validation_code = 3;
            strcpy(result->validation_message, "Transition stuck");
            result->error_count++;
        }
    }
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_check_timeout(connection_state_machine_t* sm) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (!sm->enable_timeout_check) {
        return CHRONOS_OK;
    }
    
    uint64_t current_time = time(NULL);
    uint64_t state_duration = current_time - sm->state_entry_time;
    
    if (state_duration > (sm->state_timeout_ms / 1000)) {
        sm->is_blocked = true;
        sm->block_start_time = current_time;
        sm->block_count++;
        return CHRONOS_ERROR_TIMEOUT;
    }
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_block(connection_state_machine_t* sm, uint8_t block_reason) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    sm->is_blocked = true;
    sm->block_reason = block_reason;
    sm->block_start_time = time(NULL);
    sm->block_count++;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_unblock(connection_state_machine_t* sm) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    sm->is_blocked = false;
    sm->block_reason = 0;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_rollback(connection_state_machine_t* sm, uint32_t transition_id) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    state_transition_t* transition = NULL;
    connection_state_find_transition(sm, transition_id, &transition);
    if (transition == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    sm->current_state = transition->from_state;
    sm->state_entry_time = time(NULL);
    sm->rollback_count++;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_commit(connection_state_machine_t* sm) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    sm->commit_count++;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_abort(connection_state_machine_t* sm) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    sm->abort_count++;
    sm->current_state = CONN_STATE_ERROR;
    sm->state_entry_time = time(NULL);
    return CHRONOS_OK;
}

chronos_error_t connection_state_get_stats(connection_state_machine_t* sm, state_machine_stats_t* stats) {
    if (sm == NULL || stats == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    memset(stats, 0, sizeof(state_machine_stats_t));
    stats->total_transitions = sm->total_state_changes;
    stats->total_events = sm->processed_events;
    stats->total_blocks = sm->block_count;
    stats->total_validations = sm->validation_counter;
    stats->peak_queue_depth = sm->max_queue_depth;
    stats->current_state_duration_ms = (uint32_t)(time(NULL) - sm->state_entry_time) * 1000;
    stats->rollback_count = sm->rollback_count;
    stats->commit_count = sm->commit_count;
    stats->abort_count = sm->abort_count;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_detect_corruption(connection_state_machine_t* sm) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    sm->corruption_detection_counter++;
    
    state_transition_t* slow = sm->transition_history;
    state_transition_t* fast = sm->transition_history;
    
    uint32_t steps = 0;
    while (fast != NULL && fast->next != NULL && steps < sm->transition_history_size + 10) {
        slow = slow->next;
        fast = fast->next->next;
        steps++;
        
        if (slow == fast) {
            sm->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        }
    }
    
    connection_event_t* event_slow = sm->event_queue_head;
    connection_event_t* event_fast = sm->event_queue_head;
    steps = 0;
    
    while (event_fast != NULL && event_fast->next != NULL && steps < sm->event_queue_size + 10) {
        event_slow = event_slow->next;
        event_fast = event_fast->next->next;
        steps++;
        
        if (event_slow == event_fast) {
            sm->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        }
    }
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_recover_corruption(connection_state_machine_t* sm) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (!sm->is_corrupted) {
        return CHRONOS_OK;
    }
    
    state_transition_t* transition = sm->transition_history;
    while (transition != NULL) {
        transition->next = NULL;
        transition = transition->next;
    }
    
    connection_event_t* event = sm->event_queue_head;
    while (event != NULL) {
        event->next = NULL;
        event = event->next;
    }
    
    sm->is_corrupted = false;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_compact_history(connection_state_machine_t* sm) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    uint32_t keep_count = sm->transition_history_size / 2;
    uint32_t count = 0;
    
    state_transition_t* transition = sm->transition_history;
    while (transition != NULL && count < keep_count) {
        count++;
        transition = transition->next;
    }
    
    if (transition != NULL) {
        state_transition_t* to_free = transition->next;
        transition->next = NULL;
        sm->transition_history_tail = transition;
        
        while (to_free != NULL) {
            state_transition_t* next = to_free->next;
            chronos_free(to_free);
            to_free = next;
        }
    }
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_clear_events(connection_state_machine_t* sm) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    connection_event_t* event = sm->event_queue_head;
    while (event != NULL) {
        connection_event_t* next = event->next;
        chronos_free(event);
        event = next;
    }
    
    sm->event_queue_head = NULL;
    sm->event_queue_tail = NULL;
    sm->pending_events = 0;
    sm->current_queue_depth = 0;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_set_timeout(connection_state_machine_t* sm, uint32_t timeout_ms) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    sm->state_timeout_ms = timeout_ms;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_enable_validation(connection_state_machine_t* sm, bool enable) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    sm->enable_state_validation = enable;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_enable_concurrent(connection_state_machine_t* sm, bool enable) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    sm->enable_concurrent_transitions = enable;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_get_current_state(connection_state_machine_t* sm, connection_state_t* state) {
    if (sm == NULL || state == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    *state = sm->current_state;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_get_previous_state(connection_state_machine_t* sm, connection_state_t* state) {
    if (sm == NULL || state == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    *state = sm->previous_state;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_get_state_duration(connection_state_machine_t* sm, uint32_t* duration_ms) {
    if (sm == NULL || duration_ms == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    *duration_ms = (uint32_t)(time(NULL) - sm->state_entry_time) * 1000;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_add_transition_to_history(connection_state_machine_t* sm, 
                                                           state_transition_t* transition) {
    if (sm == NULL || transition == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    transition->next = sm->transition_history;
    if (sm->transition_history != NULL) {
        sm->transition_history->prev = transition;
    }
    sm->transition_history = transition;
    
    if (sm->transition_history_tail == NULL) {
        sm->transition_history_tail = transition;
    }
    
    uint32_t count = 0;
    state_transition_t* current = sm->transition_history;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    
    if (count > sm->transition_history_size) {
        connection_state_compact_history(sm);
    }
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_find_transition(connection_state_machine_t* sm, uint32_t transition_id, 
                                                  state_transition_t** transition) {
    if (sm == NULL || transition == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    state_transition_t* current = sm->transition_history;
    while (current != NULL) {
        if (current->transition_id == transition_id) {
            *transition = current;
            return CHRONOS_OK;
        }
        current = current->next;
    }
    
    *transition = NULL;
    
    return CHRONOS_ERROR_INVALID_INPUT;
}

chronos_error_t connection_state_validate_transition(connection_state_machine_t* sm, 
                                                     connection_state_t from_state, connection_state_t to_state) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    switch (from_state) {
        case CONN_STATE_IDLE:
            if (to_state != CONN_STATE_CONNECTING && to_state != CONN_STATE_CLOSED) {
                return CHRONOS_ERROR_INVALID_STATE;
            }
            break;
        case CONN_STATE_CONNECTING:
            if (to_state != CONN_STATE_HANDSHAKE && to_state != CONN_STATE_CLOSED && to_state != CONN_STATE_ERROR) {
                return CHRONOS_ERROR_INVALID_STATE;
            }
            break;
        case CONN_STATE_HANDSHAKE:
            if (to_state != CONN_STATE_PREFACE && to_state != CONN_STATE_CLOSED && to_state != CONN_STATE_ERROR) {
                return CHRONOS_ERROR_INVALID_STATE;
            }
            break;
        case CONN_STATE_PREFACE:
            if (to_state != CONN_STATE_SETTINGS && to_state != CONN_STATE_CLOSED && to_state != CONN_STATE_ERROR) {
                return CHRONOS_ERROR_INVALID_STATE;
            }
            break;
        case CONN_STATE_SETTINGS:
            if (to_state != CONN_STATE_OPEN && to_state != CONN_STATE_CLOSED && to_state != CONN_STATE_ERROR) {
                return CHRONOS_ERROR_INVALID_STATE;
            }
            break;
        case CONN_STATE_OPEN:
            if (to_state != CONN_STATE_CLOSING && to_state != CONN_STATE_DRAINING && to_state != CONN_STATE_CLOSED && to_state != CONN_STATE_ERROR) {
                return CHRONOS_ERROR_INVALID_STATE;
            }
            break;
        case CONN_STATE_CLOSING:
            if (to_state != CONN_STATE_CLOSED && to_state != CONN_STATE_ERROR) {
                return CHRONOS_ERROR_INVALID_STATE;
            }
            break;
        case CONN_STATE_DRAINING:
            if (to_state != CONN_STATE_CLOSED && to_state != CONN_STATE_ERROR) {
                return CHRONOS_ERROR_INVALID_STATE;
            }
            break;
        case CONN_STATE_CLOSED:
            return CHRONOS_ERROR_INVALID_STATE;
        case CONN_STATE_ERROR:
            return CHRONOS_ERROR_INVALID_STATE;
        default:
            return CHRONOS_ERROR_INVALID_STATE;
    }
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_check_state_invariant(connection_state_machine_t* sm) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (sm->is_blocked && sm->current_state == CONN_STATE_OPEN) {
        return CHRONOS_ERROR_INVALID_STATE;
    }
    
    if (sm->is_transitioning && sm->current_state == CONN_STATE_IDLE) {
        return CHRONOS_ERROR_INVALID_STATE;
    }
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_reset(connection_state_machine_t* sm) {
    if (sm == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    sm->current_state = CONN_STATE_IDLE;
    sm->previous_state = CONN_STATE_IDLE;
    sm->target_state = CONN_STATE_IDLE;
    sm->state_entry_time = time(NULL);
    sm->state_change_count = 0;
    sm->is_transitioning = false;
    sm->is_blocked = false;
    sm->block_count = 0;
    sm->invalid_state_count = 0;
    sm->is_corrupted = false;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_snapshot(connection_state_machine_t* sm, uint8_t* snapshot, size_t* snapshot_len) {
    if (sm == NULL || snapshot == NULL || snapshot_len == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    size_t required_len = sizeof(connection_state_t) * 3 + sizeof(uint64_t) * 2 + sizeof(uint32_t) * 10;
    if (*snapshot_len < required_len) {
        *snapshot_len = required_len;
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    }
    
    size_t offset = 0;
    memcpy(snapshot + offset, &sm->current_state, sizeof(connection_state_t));
    offset += sizeof(connection_state_t);
    memcpy(snapshot + offset, &sm->previous_state, sizeof(connection_state_t));
    offset += sizeof(connection_state_t);
    memcpy(snapshot + offset, &sm->target_state, sizeof(connection_state_t));
    offset += sizeof(connection_state_t);
    memcpy(snapshot + offset, &sm->state_entry_time, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(snapshot + offset, &sm->state_exit_time, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(snapshot + offset, &sm->state_change_count, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &sm->total_state_changes, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &sm->is_transitioning, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &sm->is_blocked, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &sm->block_reason, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &sm->block_count, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    *snapshot_len = offset;
    
    return CHRONOS_OK;
}

chronos_error_t connection_state_restore(connection_state_machine_t* sm, const uint8_t* snapshot, size_t snapshot_len) {
    if (sm == NULL || snapshot == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    size_t offset = 0;
    memcpy(&sm->current_state, snapshot + offset, sizeof(connection_state_t));
    offset += sizeof(connection_state_t);
    memcpy(&sm->previous_state, snapshot + offset, sizeof(connection_state_t));
    offset += sizeof(connection_state_t);
    memcpy(&sm->target_state, snapshot + offset, sizeof(connection_state_t));
    offset += sizeof(connection_state_t);
    memcpy(&sm->state_entry_time, snapshot + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(&sm->state_exit_time, snapshot + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(&sm->state_change_count, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&sm->total_state_changes, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&sm->is_transitioning, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&sm->is_blocked, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&sm->block_reason, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&sm->block_count, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    return CHRONOS_OK;
}
