; Function Signature (Windows x64):
; find_no_common_chars(haystack_ptr: RCX_arg, haystack_len: RDX_arg,
;                      needles_ptr: R8_arg,  needles_len: R9_arg)
; Returns: RAX = 1 if no common characters, 0 if common characters are found.

section .text
global find_no_common_chars_sse42 ; Exported name for C++

find_no_common_chars_sse42:
    push rbp
    mov rbp, rsp
    ; Allocate space for two 16-byte temp buffers + ensure stack alignment
    ; temp_needles_buffer at [rbp-16]
    ; temp_haystack_buffer at [rbp-32]
    sub rsp, 48        ; 16 (needles) + 16 (haystack) + 16 (padding/other use)
                       ; (Original_RSP - 8 for push rbp) - 48 = Original_RSP - 56.
                       ; If Original_RSP was xxxx0, RSP is now xxxx8.
                       ; [rbp-16] = (Orig_RSP-8) - 16 = Orig_RSP - 24 (xxxx8) -> not 16B aligned
                       ; [rbp-32] = (Orig_RSP-8) - 32 = Orig_RSP - 40 (xxxx0) -> IS 16B aligned
                       ; Let's use [rbp-32] for needles, [rbp-48] for haystack for alignment with MOVAPS if desired
                       ; Or stick to MOVUPS and simpler [rbp-16], [rbp-32] as planned.
                       ; For this version, we'll use MOVUPS, so precise buffer alignment is less critical
                       ; than overall stack pointer alignment for calls (which Windows ABI ensures for us on entry).

    ; Save callee-saved registers
    push rbx
    push rsi
    push rdi
    push r10           ; Will use r10 for temp_needles_buffer_ptr
    push r11           ; Will use r11 for temp_haystack_buffer_ptr
    push r12           ; current_needles_ptr
    push r13           ; remaining_needles_len
    push r14           ; initial_haystack_ptr
    push r15           ; initial_haystack_len

    cld                ; Ensure string operations increment pointers

    ; Copy arguments to preserved registers
    mov r14, rcx       ; r14 = initial_haystack_ptr
    mov r15, rdx       ; r15 = initial_haystack_len
    mov r12, r8        ; r12 = initial_needles_ptr
    mov r13, r9        ; r13 = initial_needles_len

    ; Edge case: If needles_len is 0, no characters to find.
    test r13, r13
    jz .return_true
    ; Edge case: If haystack_len is 0 (and needles_len > 0).
    test r15, r15
    jz .return_true

    ; Setup pointers to temp buffers on stack
    lea r10, [rbp - 16] ; R10 = temp_needles_buffer_ptr
    lea r11, [rbp - 32] ; R11 = temp_haystack_buffer_ptr

    pxor xmm7, xmm7    ; Zero out XMM7 for clearing buffers

.outer_loop_needles:
    ; --- Prepare XMM1 with needles_chunk ---
    ; RAX = length of current needles_chunk to copy (max 16)
    mov rax, r13       ; remaining_needles_len
    cmp rax, 16
    jbe .set_needles_chunk_len
    mov rax, 16
.set_needles_chunk_len:

    ; Temporarily save registers that rep movsb will clobber (RDI, RSI, RCX)
    push rdi
    push rsi
    push rcx

    mov rdi, r10        ; Dest: temp_needles_buffer (from R10)
    mov rsi, r12        ; Src: current_needles_ptr (from R12)
    mov rcx, rax        ; Count: needles_chunk_len (from RAX)

    movups [rdi], xmm7  ; Zero temp_needles_buffer (16 bytes) using XMM7
    rep movsb           ; Copy needles_chunk into buffer. RDI, RSI, RCX are changed.
    movups xmm1, [r10]  ; Load XMM1 from the start of temp_needles_buffer (R10)

    pop rcx             ; Restore RCX, RSI, RDI
    pop rsi
    pop rdi
    ; XMM1 is now loaded with the current needles_chunk, effectively null-terminated within 16 bytes.
    ; RAX still holds the length of the needles_chunk that was copied.

    ; --- Haystack Iteration: Compare XMM1 against chunks of haystack ---
    mov rbx, r14            ; RBX = current_haystack_ptr, reset to initial_haystack_ptr
    mov r9, r15             ; R9 = remaining_haystack_len for this scan, reset

.inner_loop_haystack:
    test r9, r9             ; Is remaining_haystack_len (R9) zero?
    jz .advance_to_next_needles_chunk ; If so, haystack exhausted for this needles_chunk

    ; --- Prepare XMM2 with haystack_chunk ---
    ; R8_chunk_len will store the length of the current haystack_chunk to copy (max 16)
    mov r8, r9         ; Copy remaining_haystack_len to R8
    cmp r8, 16
    jbe .set_haystack_chunk_len
    mov r8, 16
.set_haystack_chunk_len:
    ; R8 now holds the actual length of the haystack_chunk for this inner iteration.

    push rdi            ; Save RDI, RSI, RCX
    push rsi
    push rcx

    mov rdi, r11        ; Dest: temp_haystack_buffer (from R11)
    mov rsi, rbx        ; Src: current_haystack_ptr (from RBX)
    mov rcx, r8         ; Count: haystack_chunk_len (from R8)

    movups [rdi], xmm7  ; Zero temp_haystack_buffer (16 bytes) using XMM7 (still zero)
    rep movsb           ; Copy haystack_chunk into buffer.
    movups xmm2, [r11]  ; Load XMM2 from the start of temp_haystack_buffer (R11)

    pop rcx
    pop rsi
    pop rdi
    ; XMM2 is now loaded with the current haystack_chunk, effectively null-terminated.
    ; R8 holds the number of bytes that were actually part of this haystack_chunk.

    ; For PCMPISTRI, EAX and EDX specify max scan length for nulls in XMM1/XMM2.
    ; The nulls in the data determine the effective string lengths for comparison.
    mov eax, 16         ; Tell PCMPISTRI to scan up to 16 bytes in XMM1 for its null.
    mov edx, 16         ; Tell PCMPISTRI to scan up to 16 bytes in XMM2 for its null.

    ; PCMPISTRI xmm1, xmm2, imm8
    ; imm8 = 0x02 (unsigned BYTES, equal any, positive polarity, output index & CFlag)
    pcmpistri xmm1, xmm2, 0x02

    jc .match_found     ; If CF=1, a common byte was found, jump to set RAX=0 and finish.

    ; No match in this chunk pair. Advance haystack pointer and decrement remaining length.
    add rbx, r8             ; Advance current_haystack_ptr by R8 (actual_chunk_len)
    sub r9, r8              ; Decrement remaining_haystack_len (R9) by R8
    jmp .inner_loop_haystack

.advance_to_next_needles_chunk:
    ; Current needles_chunk (in XMM1, original length in RAX) has been compared against entire haystack.
    add r12, rax            ; Advance current_needles_ptr by length of needles_chunk processed
    sub r13, rax            ; Decrement remaining_needles_len
    jnz .outer_loop_needles ; If more needles, continue outer loop

    ; All needles processed, and no common character found throughout.
.return_true:
    mov rax, 1              ; Set result to 1 (true: no common characters)
    jmp .done

.match_found:
    mov rax, 0              ; Set result to 0 (false: common character found)
    ; Fallthrough to .done

.done:
    ; Restore callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop rdi
    pop rsi
    pop rbx

    add rsp, 48             ; Deallocate stack space for temp buffers
    pop rbp
    ret