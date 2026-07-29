#ifndef CHRONOS_FRAME_REASSEMBLY_H
#define CHRONOS_FRAME_REASSEMBLY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common.h"

typedef struct frame_fragment {
    uint8_t* data;
    size_t size;
    size_t offset;
    uint32_t stream_id;
    uint8_t frame_type;
    uint8_t flags;
    uint32_t fragment_id;
    bool is_last;
    uint64_t arrival_time;
    uint32_t sequence_number;
    struct frame_fragment* next;
    struct frame_fragment* prev;
} frame_fragment_t;

typedef struct reassembly_context {
    frame_fragment_t* fragments[CHRONOS_MAX_FRAGMENTS];
    uint32_t fragment_count;
    uint32_t total_fragments_received;
    uint32_t total_fragments_assembled;
    uint32_t next_sequence_number;
    uint32_t expected_fragments;
    uint64_t reassembly_start_time;
    uint64_t last_fragment_time;
    bool is_complete;
    bool has_timeout;
    uint32_t timeout_ms;
    uint8_t reassembly_state;
    uint32_t missing_fragments;
    uint32_t duplicate_fragments;
    uint32_t out_of_order_fragments;
    uint32_t corrupted_fragments;
    bool enable_deduplication;
    bool enable_reordering;
    bool enable_integrity_check;
    uint32_t integrity_check_counter;
    uint32_t reassembly_attempts;
    uint32_t max_reassembly_attempts;
    bool is_corrupted;
} reassembly_context_t;

typedef struct reassembly_stats {
    uint32_t total_frames_received;
    uint32_t total_frames_assembled;
    uint32_t total_frames_failed;
    uint32_t total_fragments_dropped;
    uint32_t total_reassembly_time_ms;
    uint32_t average_reassembly_time_ms;
    uint32_t peak_fragment_count;
    uint32_t timeout_count;
    uint32_t integrity_failure_count;
    uint32_t duplicate_count;
    uint32_t out_of_order_count;
} reassembly_stats_t;

reassembly_context_t* reassembly_context_create(uint32_t timeout_ms);
void reassembly_context_destroy(reassembly_context_t* ctx);
chronos_error_t reassembly_add_fragment(reassembly_context_t* ctx, const uint8_t* data, size_t size, 
                                        uint32_t stream_id, uint8_t frame_type, uint8_t flags, 
                                        uint32_t offset, bool is_last);
chronos_error_t reassembly_assemble_frame(reassembly_context_t* ctx, uint8_t* output, size_t output_len, 
                                          size_t* bytes_assembled);
chronos_error_t reassembly_check_complete(reassembly_context_t* ctx);
chronos_error_t reassembly_check_timeout(reassembly_context_t* ctx);
chronos_error_t reassembly_reset(reassembly_context_t* ctx);
chronos_error_t reassembly_get_stats(reassembly_context_t* ctx, reassembly_stats_t* stats);
chronos_error_t reassembly_validate_integrity(reassembly_context_t* ctx);
chronos_error_t reassembly_handle_duplicate(reassembly_context_t* ctx, frame_fragment_t* fragment);
chronos_error_t reassembly_handle_out_of_order(reassembly_context_t* ctx, frame_fragment_t* fragment);
chronos_error_t reassembly_compact_fragments(reassembly_context_t* ctx);
chronos_error_t reassembly_detect_corruption(reassembly_context_t* ctx);
chronos_error_t reassembly_recover_corruption(reassembly_context_t* ctx);
bool reassembly_is_fragment_valid(reassembly_context_t* ctx, frame_fragment_t* fragment);
chronos_error_t reassembly_update_sequence(reassembly_context_t* ctx, uint32_t sequence_number);
chronos_error_t reassembly_mark_fragment_received(reassembly_context_t* ctx, uint32_t fragment_id);
chronos_error_t reassembly_find_missing_fragments(reassembly_context_t* ctx, uint32_t* missing, size_t* count);

