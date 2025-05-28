; Function Signature (Windows x64):
; masked_greenletter_compare(correct_ptr: RCX_arg, word_ptr: RDX_arg)
; Returns: RAX = 1 if match, 0 if no pass

section .text
global masked_greenletter_compare ; name

masked_greenletter_compare:
    ;
    ; stack needed?
    ; prob not, simple function
    ;

    ; need to check the word in RCX for whitespace ' '
    ; make a mask out of that
    ;
    ; do some tests first
    ; see what the mask does in pcmpistrm
    ;
    ; xmm1: correct word
    ; xmm2: whitespace detect
    ; xmm0: mask
    ; xmm3: all ones for mask invert
    ; xmm4: word
    ;

    pxor xmm1, xmm1
    movd xmm1, [rcx]
    pinsrb xmm1, [rcx+4], 4 

    pxor xmm4, xmm4
    movd xmm4, [rdx]
    pinsrb xmm4, [rdx+4], 4

    ; pxor xmm2, xmm2
    ; pxor xmm3, xmm3
    ; mov eax, 0xFFFFFFFF
    ; movd xmm3, eax
    ; pinsrb xmm3, al, 4  ; ones for first 5 bytes
    ; mov al, 0x20
    ; pinsrb xmm2, al, 0
    ; pcmpistrm xmm2, xmm1, 0x40
    ; pxor xmm0, xmm3 ; corrected mask in xmm0
    
    pxor xmm3, xmm3
    pcmpeqb xmm3, xmm3 ; set all 1
    
    mov rax, 0x2020202020202020
    movq xmm0, rax
    ; movlhps xmm0, xmm0
    pcmpeqb xmm0, xmm1
    
    pxor xmm1, xmm4 ; xmm1 has comparison
    pxor xmm0, xmm3 ; invert mask

    ptest xmm1, xmm0 ; AND word with mask, if result all zero then places match
    
    ; check zero flag after ptest
    setz al
    movzx rax, al

    ret