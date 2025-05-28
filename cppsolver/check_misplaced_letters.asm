; Function Signature (Windows x64):
; check_misplaced_letters(
;   word_ptr: RCX_arg,
;   misplacedList_ptr: RDX_arg,
;   misplacedListSizes_ptr: R8_arg
; )
; Returns: RAX = 1 if pass, 0 if no pass

section .text
global check_misplaced_letters ; name

check_misplaced_letters:
    ;
    ; stack probably needed, algorithm more complex
    ; it would be better if not though
    ; 
    ; misplace list representation:
    ; go through the sizes list, use as misplace list offset
    ; increment a counter
    ; 
    ; code? vvv
    ;
    ; could this be unrolled lol
    ;
    ; int offset = 0;
    ; for(int i = 0; i < 5; i++) {
    ;     for(int j = 0; j < misplacedListSizes_ptr[i]; j++) {
    ;         char ltr = misplacedList_ptr[offset];
    ;          offset++;
    ;        
    ;         if(ltr == word_ptr[i]) return 0; //not a valid word if same slot
    ;         //now check rest of the word if contains
    ;         
    ;         //broadcast ltr to entire xmm
    ;         //pcmpeqb to word
    ;         //if all 0, does not contain and its not a possible word
    ;         //otherwise, its a valid word and can continue checking the rest
    ;
    ;
    ;         
    ;     }
    ; }
    ;
    ; register allocation:
    ; r9: offset
    ; r10: i
    ; r11: j
    ; 
    ; char ltr: keep in mem, need for broadcast anyways
    ; 
    ; xmm0: all ltr
    ; xmm1: zero
    ; xmm2: word
    ; xmm3: ones
    ;
    ; use rax for any temporary stuff, intermed calculations
    
    pxor xmm1, xmm1 ; zero xmm1
    pxor xmm3, xmm3
    pcmpeqb xmm3, xmm3 ; ones xmm3

    ; set xmm2 to word
    pxor xmm2, xmm2 ; zero
    movd xmm2, [rcx]
    pinsrb xmm2, [rcx+4], 4

    xor r9, r9 ; offset = 0
    
    xor r10, r10 ; i = 0

    xor r11, r11 ; j = 0    

    xor rax, rax
    
    i_letterloop:
    cmp r10, 0x05
    je i_letterloop_end
    
    xor r11, r11 ; int j = 0
    j_misplacedlettersloop1:
    ; j loop logic
    cmp r11b, byte [r8 + r10] ; end loop if j == misplacedListSizes_ptr[i]
    je j_misplacedlettersloop1_end ; end loop

    ; if ltr == word_ptr[i]
    mov al, [rdx+r9]
    inc r9 ; offset++ after doing compare

    cmp al, byte [rcx + r10]
    jne j_misplacedlettersloop1_passedSameSlot
    
    ; same letter, filter fails and is not a possible word    
    xor rax, rax ; return 0
    ret 

    j_misplacedlettersloop1_passedSameSlot:

    ; check rest of the word
    ; ltr is in al
    ; movzx eax, al ;  redundant instruction, already zeroed and only using first byte
    movd xmm0, eax 
    pshufb xmm0, xmm1 ; broadcasting al (ltr) to all xmm0 bytes
    
    ; compare letters to word (xmm2)
    pcmpeqb xmm0, xmm2

    ; if all zero, no letters match, fail
    ptest xmm0, xmm3 ; will set zf to 1 if all zero
    jnz j_misplacedlettersloop1_passedLetterCheck
    ; fail    
    xor rax, rax ; return 0
    ret 
    j_misplacedlettersloop1_passedLetterCheck:
    
    inc r11 ; j++
    jmp j_misplacedlettersloop1
    j_misplacedlettersloop1_end:

    inc r10 ; i++
    jmp i_letterloop

    i_letterloop_end:
    
    mov rax, 1 ; return 1
    ret