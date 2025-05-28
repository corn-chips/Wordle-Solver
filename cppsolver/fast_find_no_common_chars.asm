; Function Signature (Windows x64):
; fast_find_no_common_chars_sse42(haystack_ptr: RCX_arg, needles_ptr: RDX_arg)
; Returns: RAX = 1 if common, 0 if no common

section .text
global fast_find_no_common_chars_sse42 ; export function name

fast_find_no_common_chars_sse42:
    ; register allocation???
    ; use RAX for return
    ; RCX = haystack
    ; RDX = needles, assume null terminated to use implicit string instructions
    ;
    ; R8-9: ok for use
    ; R10-15: push to stack before use
    ; RDI, RSI, RBX: push to stack before use
    ;
    ; XMM, YMM 0-5: ok for use
    ; XMM, YMM 6-15: push to stack before use
    ; 
    ; pseudocode:
    ; load 5 byte wordle word into xmm register
    ; assume needles null terminated
    ; pcmpistri xmmreg, needles, 0x00
    ; return carry flag
    ;
    ; xmm0: word
    ; rcx: wordptr
    ; rdx: needlesptr
    ;

    ; we dont need the stack here
    ; push rbp
    ; mov rbp, rsp

    pxor xmm0, xmm0 ; zero
    movd xmm0, [rcx] ; load 4 bytes
    pinsrb xmm0, [rcx+4], 4 ; put that 5th byte into the xmm reg

    pcmpistri xmm0, [rdx], 0x00 ; look for characters in xmm0 that are in [rdx]

    ; set return
    setc al ; if char exists, cf = 1
    movzx rax, al

    ; no stack
    ; pop rbp
    ret