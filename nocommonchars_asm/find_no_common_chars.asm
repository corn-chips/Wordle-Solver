; Function Signature (Windows x64):
; find_no_common_chars(haystack_ptr: RCX_arg, haystack_len: RDX_arg,
;                      needles_ptr: R8_arg,  needles_len: R9_arg)
; Returns: RAX = 1 if no common, 0 if common

section .text
global find_no_common_chars_sse42 ; Keep C++ export name

find_no_common_chars_sse42:
    push rbp
    mov rbp, rsp
    sub rsp, 32        ; temp_needles_buffer at [rbp-16]

    push rbx
    push rsi
    push rdi
    push r12           ; current_needles_ptr
    push r13           ; remaining_needles_len
    push r14           ; initial_haystack_ptr
    push r15           ; initial_haystack_len

    cld                ; Direction flag forward

    mov r14, rcx       ; r14 = initial_haystack_ptr
    mov r15, rdx       ; r15 = initial_haystack_len
    mov r12, r8        ; r12 = initial_needles_ptr
    mov r13, r9        ; r13 = initial_needles_len

    test r13, r13      ; needles_len == 0?
    jz .return_true
    test r15, r15      ; haystack_len == 0?
    jz .return_true

    lea r11, [rbp - 16] ; R11 = temp_needles_buffer address

.outer_loop_needles:
    ; EAX will hold needles_chunk_len for PCMPESTRI (<=16)
    mov rax, r13       ; remaining_needles_len
    cmp rax, 16
    jbe .set_needles_len1
    mov rax, 16
.set_needles_len1:
    ; EAX now holds length for XMM1 for PCMPESTRI

    ; --- Prepare XMM1 (needles_chunk) ---
    push rdi            ; Save RDI, RSI, RCX as they are used temporarily
    push rsi
    push rcx

    mov rdi, r11        ; Dest: temp_buffer (from R11)
    mov rsi, r12        ; Src: current_needles_ptr (from R12)
    mov rcx, rax        ; Count for rep movsb: needles_chunk_len (from RAX)

    pxor xmm7, xmm7     ; Zero xmm7
    movups [rdi], xmm7  ; Zero temp_buffer (use MOVUPS for safety, RDI might not be 16B aligned)
                        ; Although R11 from [rbp-16] *could* be aligned with careful stack setup,
                        ; movups is safer if not perfectly sure.
    rep movsb           ; Copy needles_chunk to temp_buffer
    movups xmm1, [r11]  ; Load XMM1 from start of temp_buffer (R11)

    pop rcx
    pop rsi
    pop rdi
    ; XMM1 ready, EAX has its length for PCMPESTRI

    ; --- Haystack Iteration ---
    mov rbx, r14            ; RBX = current_haystack_ptr, reset to initial_haystack_ptr
    mov r10, r15            ; R10 = remaining_haystack_len_for_this_scan, reset to initial_haystack_len

.inner_loop_haystack:
    test r10, r10           ; Test remaining_haystack_len_for_this_scan
    jz .advance_to_next_needles_chunk ; Haystack exhausted for this needles_chunk

    ; EDX will hold haystack_chunk_len for PCMPESTRI (<=16)
    mov rdx, r10       ; current remaining_haystack_len
    cmp rdx, 16
    jbe .set_haystack_len1
    mov rdx, 16
.set_haystack_len1:
    ; EDX now holds length for [RBX] for PCMPESTRI

    ; PCMPESTRI xmm1, xmm2/m128, imm8
    ; Length of xmm1 is in EAX. Length of xmm2/m128 is in EDX.
    ; imm8 = 0x02 (unsigned BYTES, equal any, positive polarity, output index & CFlag)
    pcmpestri xmm1, [rbx], 0x02 ; <<< CRITICAL FIX: IMM8 IS NOW 0x02

    jc .match_found             ; If CF=1, a common byte was found

    ; No match in this chunk pair:
    add rbx, rdx            ; Advance current_haystack_ptr
    sub r10, rdx            ; Decrement remaining_haystack_len_for_this_scan
    jmp .inner_loop_haystack

.advance_to_next_needles_chunk:
    ; RAX still holds needles_chunk_len from this outer iteration
    add r12, rax            ; Advance current_needles_ptr
    sub r13, rax            ; Decrement overall_remaining_needles_len
    jnz .outer_loop_needles ; If more needles, continue

    ; All needles processed against all haystack, no match found overall
.return_true:
    mov rax, 1              ; Return 1 (true: no common characters)
    jmp .done

.match_found:
    mov rax, 0              ; Return 0 (false: common character was found)
    ; jmp .done ; Fallthrough is fine

.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    add rsp, 32             ; Deallocate temp buffer space
    pop rbp
    ret