; /** defines bool y puntero **/
%define NULL 0
%define TRUE 1
%define FALSE 0

section .data
    empty_str: db 0

section .text

global string_proc_list_create_asm
global string_proc_node_create_asm
global string_proc_list_add_node_asm
global string_proc_list_concat_asm

; FUNCIONES auxiliares que pueden llegar a necesitar:
extern malloc
extern free
extern str_concat


; string_proc_list_create_asm:
; Reserva 16 bytes y pone a 0 los campos first y last
global string_proc_list_create_asm
extern malloc

section .text
string_proc_list_create_asm:
    mov rdi, 16       ; Tamaño de la estructura (2 punteros de 8 bytes cada uno)
    call malloc       ; Llama a malloc; el puntero se devuelve en RAX
    test rax, rax     ; Verifica que malloc no haya fallado
    jz .return_null   ; Si malloc falla, retorna NULL (0)

    mov qword [rax], 0      ; Inicializar list->first = NULL
    mov qword [rax+8], 0    ; Inicializar list->last = NULL
    ret

.return_null:
    xor rax, rax
    ret

; -------------------------------------------------------------------
; string_proc_node_create_asm:
;   Crea un nuevo nodo de la lista (32 bytes) e inicializa:
;     - node->next      = NULL (offset 0)
;     - node->previous  = NULL (offset 8)
;     - node->type      = valor recibido (offset 16)
;     - node->hash      = puntero recibido (offset 24)
;
; Convención System V (x86_64 Linux):
;   Entrada:
;      RDI = type (uint8_t)  ; usaremos su byte inferior (r10b)
;      RSI = pointer a hash (char*)
;   Salida:
;      RAX = pointer al nuevo nodo (string_proc_node*)
;
; No se copia la cadena, se asigna el puntero directamente.
; -------------------------------------------------------------------


global string_proc_node_create_asm
extern malloc

section .text
string_proc_node_create_asm:
    push rbx                  ; Preservar rbx
    mov rdi, 32               ; Tamaño del nodo (32 bytes)
    call malloc               ; Reserva memoria; puntero devuelto en RAX
    test rax, rax
    jz .return_null
    mov rbx, rax              ; Guardar puntero en rbx
    mov qword [rbx], 0        ; Inicializa node->next = NULL (offset 0)
    mov qword [rbx+8], 0      ; Inicializa node->previous = NULL (offset 8)
    mov byte [rbx+16], dil    ; node->type = valor de DIL (parte baja de RDI)
    mov qword [rbx+24], rsi   ; node->hash = puntero recibido en RSI
    mov rax, rbx              ; Prepara valor de retorno
    pop rbx                   ; Restaurar rbx
    ret

.return_null:
    xor rax, rax
    pop rbx
    ret

; -------------------------------------------------------------------
; void string_proc_list_add_node_asm(string_proc_list* list, uint8_t type, char* hash);
;
; Parámetros:
;   RDI = pointer a list (string_proc_list*)
;   RSI = type (uint8_t)
;   RDX = pointer a hash (char*)
;
; Se usará la función externa:
;   string_proc_node_create_asm:
;       - Entrada: RDI = type, RSI = hash
;       - Salida: RAX = pointer al nodo creado
;
; La estructura string_proc_list se organiza así:
;   offset 0: pointer first (8 bytes)
;   offset 8: pointer last (8 bytes)
;
; La estructura de string_proc_node se asume de tamaño 32 bytes:
;   offset 0: pointer next (8 bytes)
;   offset 8: pointer previous (8 bytes)
;   offset 16: uint8_t type (1 byte) + padding
;   offset 24: pointer hash (8 bytes)
; -------------------------------------------------------------------
global string_proc_list_add_node_asm
extern string_proc_node_create_asm

section .text

string_proc_list_add_node_asm:
    push rbp
    mov rbp, rsp
    push rbx                ; preserva rbx (callee-saved)
    push r12                ; preserva r12
    push r13                ; preserva r13

    ; Guardar la dirección de la lista en r12
    mov r12, rdi           ; r12 = list

    ; Preparar los parámetros para llamar a string_proc_node_create_asm:
    ; Tenemos: type en RSI y hash en RDX de nuestra función.
    ; Nuestro llamado a string_proc_node_create_asm debe recibir en:
    ;   RDI = type (uint8_t)
    ;   RSI = hash (char*)
    mov rdi, rsi           ; mueve el parámetro type a RDI
    mov rsi, rdx           ; mueve el parámetro hash a RSI
    call string_proc_node_create_asm
    ; El nuevo nodo se obtiene en RAX; lo guardamos en r13.
    mov r13, rax

    ; Insertar el nodo en la lista apuntada por r12.
    ; Revisar si la lista está vacía (list->first == 0)
    mov rbx, [r12]         ; rbx = list->first
    cmp rbx, 0
    jne .non_empty
    ; Lista vacía: asignar el nuevo nodo como primer y último.
    mov qword [r12], r13      ; list->first = new node
    mov qword [r12+8], r13    ; list->last  = new node
    jmp .finish

.non_empty:
    ; Lista no vacía: obtener el actual último nodo: list->last
    mov rbx, [r12+8]       ; rbx = list->last
    ; Enlaza el nuevo nodo:
    ;   - Asigna en el campo next del último nodo el nuevo nodo.
    mov qword [rbx], r13   ; last->next = new node
    ;   - Asigna en el campo previous del nuevo nodo el antiguo último.
    mov qword [r13+8], rbx ; new node->previous = old last
    ; Actualiza list->last a new node.
    mov qword [r12+8], r13

.finish:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; -------------------------------------------------------------------
; string_proc_list_concat_asm:
;
; Parámetros de entrada (System V x86_64):
;   RDI = pointer a list (string_proc_list*)
;   RSI = mode (uint8_t) 
;   RDX = pointer a prefix (const char*)
;
; Comportamiento:
;   1. Inicializa "result" haciendo:
;          result = strdup(prefix)
;
;   2. Recorre la lista (desde list->first hasta NULL). Para cada nodo:
;          si (node->type == mode)
;              new_result = str_concat(result, node->hash)
;              free(result)
;              result = new_result
;
;   3. Retorna "result". El caller debe liberarla.
;
; Se usan las funciones externas:
;   strdup, str_concat, free.
; -------------------------------------------------------------------

global string_proc_list_concat_asm
extern strdup      ; duplica la cadena
extern str_concat  ; concatena dos cadenas (no libera sus argumentos)
extern free        ; libera memoria

string_proc_list_concat_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14

    ; Guardar el parámetro list en r12.
    mov r12, rdi             ; r12 = list (string_proc_list*)
    ; Cargar el parámetro mode (uint8_t, en RSI) en r14.
    movzx r14, sil           ; r14 = mode (0..255)

    ; Inicializar result: duplicar el prefijo.
    mov rdi, rdx             ; rdi = prefix
    call strdup              ; result = strdup(prefix)
    mov rbx, rax             ; rbx guarda result

    ; Obtener el primer nodo: list->first (offset 0).
    mov rdx, qword [r12]     ; rdx = current node pointer

.loop:
    cmp rdx, 0
    je .done_iteration       ; si no hay nodo, salimos del bucle

    ; Comparar node->type (offset 16) con el modo (en r14b).
    mov al, byte [rdx+16]    ; cargar el tipo de nodo
    cmp al, r14b
    jne .skip_node          ; si no coincide, saltamos

    ; Si coincide, concatenar:
    ; Llama a str_concat(result, node->hash)
    mov rdi, rbx             ; primer parámetro: current result
    mov rsi, qword [rdx+24]   ; segundo parámetro: node->hash
    call str_concat         ; new_result = str_concat(result, node->hash)
    ; Liberar el string viejo.
    mov rdi, rbx
    call free
    ; Actualizar result.
    mov rbx, rax

.skip_node:
    ; Avanzar al siguiente nodo (node->next en offset 0).
    mov rdx, qword [rdx]
    jmp .loop

.done_iteration:
    mov rax, rbx           ; preparar el resultado en RAX

    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

