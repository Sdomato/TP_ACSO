; tamaño de puntero (64 bits) = 8

; ------ offsets de struct lista ------
; L_HEAD = 0
; L_TAIL = 8
; L_SZ   = 16

; ------ offsets de struct nodo  ------
; N_NEXT = 0
; N_PREV = 8
; N_TYPE = 16
; N_HASH = 24
; N_SZ   = 32

section .data
    empty0: db 0

section .text
    global string_proc_list_create_asm
    global string_proc_node_create_asm
    global string_proc_list_add_node_asm
    global string_proc_list_concat_asm
    extern malloc
    extern free
    extern str_concat

; --------------------------------------------------
; string_proc_list_create_asm: crea lista vacía
;   RAX ← ptr lista | 0 si malloc falla
; --------------------------------------------------
string_proc_list_create_asm:
    mov     rdi, 16              ; L_SZ
    call    malloc
    test    rax, rax
    jz      .LIST_FAIL
    mov     qword [rax+0], 0     ; L_HEAD
    mov     qword [rax+8], 0     ; L_TAIL
    ret
.LIST_FAIL:
    xor     rax, rax
    ret

; --------------------------------------------------
; string_proc_node_create_asm: crea nodo (DIL = tipo, RSI = hash)
;   RAX ← ptr nodo | 0 si malloc falla
; --------------------------------------------------
string_proc_node_create_asm:
    push    r12                  ; preserva hash
    push    rbx                  ; preserva tipo / rbx original

    mov     r12, rsi             ; r12 = hash (64‑bit)
    mov     bl,  dil             ; bl = tipo (byte)

    mov     rdi, 32              ; N_SZ
    call    malloc
    test    rax, rax
    jz      .NODE_FAIL

    mov     qword [rax+0], 0     ; N_NEXT
    mov     qword [rax+8], 0     ; N_PREV
    mov     qword [rax+24], r12  ; N_HASH
    mov     byte  [rax+16], bl   ; N_TYPE

    pop     rbx
    pop     r12
    ret
.NODE_FAIL:
    xor     rax, rax
    pop     rbx
    pop     r12
    ret

; --------------------------------------------------
; string_proc_list_add_node_asm: agrega al final
;   RDI = lista, RSI = tipo, RDX = hash
; --------------------------------------------------
string_proc_list_add_node_asm:
    push    rbx
    mov     rbx, rdi            ; rbx = lista
    mov     rdi, rsi            ; tipo → DIL
    mov     rsi, rdx            ; hash → RSI
    call    string_proc_node_create_asm
    test    rax, rax
    je      .ADD_END

    mov     rcx, [rbx+8]        ; L_TAIL
    test    rcx, rcx
    jnz     .ADD_NOT_EMPTY

    ; lista vacía
    mov     [rbx+0], rax        ; L_HEAD
    mov     [rbx+8], rax        ; L_TAIL
    jmp     .ADD_END

.ADD_NOT_EMPTY:
    mov     [rcx+0], rax        ; N_NEXT
    mov     [rax+8], rcx        ; N_PREV
    mov     [rbx+8], rax        ; L_TAIL

.ADD_END:
    pop     rbx
    ret

; --------------------------------------------------
; string_proc_list_concat_asm: concatena hashes del tipo indicado
;   RDI = lista, SIL = tipo filtro, RDX = base
;   RAX ← cadena resultante (heap)
; --------------------------------------------------
string_proc_list_concat_asm:
    push    r12    ; acumulador
    push    r13    ; filtro
    push    r14    ; lista (fijo)
    push    r15    ; cursor

    mov     r14, rdi
    movzx   r13d, sil

    ; acumulador = "" + base
    lea     rdi, [rel empty0]
    mov     rsi, rdx
    call    str_concat
    mov     r12, rax

    mov     r15, [r14+0]        ; L_HEAD
.loop:
    test    r15, r15
    je      .done
    cmp     byte [r15+16], r13b ; N_TYPE
    jne     .skip

    mov     rdi, r12            ; acumulador
    mov     rsi, [r15+24]       ; N_HASH
    call    str_concat          ; RAX = new
    xchg    r12, rax            ; r12=new, rax=old (to free)
    mov     rdi, rax
    call    free

.skip:
    mov     r15, [r15+0]        ; N_NEXT
    jmp     .loop

.done:
    mov     rax, r12

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    ret
