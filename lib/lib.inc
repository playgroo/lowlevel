; Assembler library of common functions
; copyleft kcdas@itkcon.com
; file ver 20180108
; args - rdi7, rsi6, rdx2, rcx1 (func) / r10 (syscall), r8, r9
; restore   (7) - rbx3, rsp4, rbp5, r12, r13, r14, r15
; save regs (9) - rax0, rcx1, rdx2, rsi6, rdi7, r8, r9, r10, r11
; 
; LINUX SYSTEM CALL TABLE FOR X86 64
;   http://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/

; errno
NO_ERRORS equ 0
ERR_SYS_WRITE equ 1
ERR_SYS_OPEN equ 2
ERR_SYS_CLOSE equ 3
ERR_SYS_STAT equ 4
ERR_SYS_MMAP equ 9
ERR_SYS_MUNMAP equ 11
ERR_LENGTH_EXCEEDED equ 1000001
ERR_NUMBER_TOO_BIG_FOR_REGISTER equ 1000002

%use altreg

section .data
    
    number: times 32 db 0
    errno: dq 0
    file_name: dq 0
    file_size: dq 0
    file_descriptor: dq 0
    file_mem_addr: dq 0
    process_file_func: dq 0

section .bss
    
    stat resb 144
struc STAT
    .st_dev         resq 1
    .st_ino         resq 1
    .st_nlink       resq 1
    .st_mode        resd 1
    .st_uid         resd 1
    .st_gid         resd 1
    .pad0           resb 4
    .st_rdev        resq 1
    .st_size        resq 1
    .st_blksize     resq 1
    .st_blocks      resq 1
    .st_atime       resq 1
    .st_atime_nsec  resq 1
    .st_mtime       resq 1
    .st_mtime_nsec  resq 1
    .st_ctime       resq 1
    .st_ctime_nsec  resq 1
endstruc

section .text

sys_exit:
    ; return code should already be in rdi7 (as a first parameter)
    mov r0, qword 60    ; 'exit' system call
    syscall

string_length:
    ; pointer to string is a first argument (rdi7)
    xor r0, r0
    dec r0
  .search_for_zero:
    inc r0
    cmp byte [r7 + r0], 0
    jnz .search_for_zero
    ; rax0 (as a return value) now contains string length
ret

sys_write:
    ; rdi7 - addr
    ; rsi6 - length
    mov r0, qword 1     ; 'write' syscall
    mov r2, r6          ; number of bytes to write
    mov r6, r7          ; pointer to data
    mov r7, qword 1     ; stdout file descriptor
    syscall
    ; rax0 - number of written bytes; -1 error
ret

print_string:
    ; pointer to string is a first argument (rdi7)
    push r7             ; save pointer to string on the stack
    call string_length  ; get null-terminated string length to rax0
    mov r6, r0          ; number of bytes to write
    pop r7              ; pointer to data was saved on the stack
    jmp sys_write

print_char:
    ; char code is a first argument (rdi7), save it on the stack (rsp4)
    push r7
    mov r7, r4          ; point to char which is now on the stack (rsp4)
    mov r0, qword 1     ; 'write' syscall
    mov r2, qword 1     ; number of bytes to write
    mov r6, r4          ; pointer to data
    mov r7, qword 1     ; stdout file descriptor
    syscall
    pop r7
ret

print_newline:
    mov r7, qword 0xA
    jmp print_char

sys_stat:
    ; rdi7 - filename
    mov r0, qword 4     ; 'stat' system call
    mov r6, stat
    syscall
    ; rax0 - 0 success; -1 error ?
ret

get_file_size:
    ; rdi7 - filename
    call sys_stat
    cmp r0, 0
    jne .error
    mov r0, qword [stat + STAT.st_size]
    jmp .done
.error:
    mov qword [errno], ERR_SYS_STAT
.done:
ret

sys_open:
    ; rdi7 - filename
    mov r0, qword 2     ; 'open' system call
    xor r6, r6          ; flags, 0=read-only
    mov r2, qword 4     ; mode, 4d(100b)=read
    syscall
    ; rax0 - file descriptor; -1 error
ret

sys_close:
    ; rdi7 - file descriptor
    mov r0, qword 3     ; 'close' system call
    syscall
    ; rax0 - 0 success; -1 error
ret

sys_mmap:
    ; rdi7 - file descriptor
    ; rsi6 - file length
    mov r0, qword 9     ; 'mmap' system call
    mov r8, r7  ; File descriptor for the file to be mapped
    xor r7, r7  ; Starting virtual address
    ;mov r6, r2  ; Length of a mapped region in bytes
    mov r2, qword 1     ; protection, PROT_READ = 0x1 Pages may be read
    mov r10, qword 2    ; flags, MAP_PRIVATE = 0x2 Pages are not shared
    xor r9, r9  ; offset
    syscall
    ; rax0 - pointer to memory; -1 error
ret

sys_munmap:
    ; rdi7 - pointer to memory
    ; rsi6 - length
    mov r0, qword 11    ; 'munmap' system call
    syscall
ret

memory_map_file:
    ; rdi7 - filename
    ; rsi6 - max file length for mapping
    ; get and check file size
    mov qword [errno], NO_ERRORS
    call get_file_size
    cmp qword [errno], NO_ERRORS
    jne .error
    cmp r0, r6
    ja .err_exceeded
    mov [file_size], r0
    ; open file and get file descriptor
    call sys_open
    cmp r0, -1
    je .err_open
    mov [file_descriptor], r0
    ; mmap file and get start addr
    mov r7, r0
    mov r6, [file_size]
    call sys_mmap
    cmp r0, -1
    je .err_mmap
    mov [file_mem_addr], r0
    jmp .ret
.err_exceeded:
    mov qword [errno], ERR_LENGTH_EXCEEDED
    jmp .error
.err_open:
    mov qword [errno], ERR_SYS_OPEN
    jmp .error
.err_mmap:
    mov qword [errno], ERR_SYS_MMAP
    jmp .error
.error:
    xor r0, r0
    dec r0
.ret:
    ; rax0 - addr; -1 error
ret

process_file_data:
    ; rdi7 - filename to be printed
    ; rsi6 - max file length for printing
    ; [process_file_func]:
    ;   1 - print file
    ;   2 - math_factorial
    ; map file to memory
    mov [file_name], r7
    mov qword [file_size], 0
    mov qword [file_descriptor], 0
    mov qword [file_mem_addr], 0
    mov qword [errno], 0
    call memory_map_file
    cmp qword [errno], 0
    jne .error
    cmp qword [process_file_func], 1
    je print_file_func
    cmp qword [process_file_func], 2
    je math_fact_func
    cmp qword [process_file_func], 3
    je math_is_prime_func
.munmap:
    ; unmap file from memory
    mov r7, [file_mem_addr]
    call sys_munmap
    cmp r0, -1
    je .err_munmap
    xor r0, r0
    jmp .done
.err_write:
    mov qword [errno], ERR_SYS_WRITE
    jmp .error
.err_close:
    mov qword [errno], ERR_SYS_CLOSE
    jmp .error
.err_munmap:
    mov qword [errno], ERR_SYS_MUNMAP
    jmp .error
.error:
    xor r0, r0
    dec r0
    jmp .done
.close:
    call sys_close
    mov qword [file_descriptor], 0
    cmp r0, -1
    je .err_close
.done:
    mov r7, qword [file_descriptor]
    test r7, r7
    jnz .close
    ; rax0 - 0 success, -1 error
ret

print_file:
    mov qword [process_file_func], 1
    jmp process_file_data
print_file_func:
    ; print file
    mov r7, [file_mem_addr]
    mov r6, [file_size]
    call sys_write
    cmp r0, [file_size] ; bytes written
    jne process_file_data.err_write
    jmp process_file_data.munmap

math_fact:
    mov qword [process_file_func], 2
    jmp process_file_data
math_fact_func:
    ; calc factorial
    mov r7, [file_mem_addr]
    call parse_uint
    test r0, r0
    jz .print_fact
    mov r6, r0
    dec r6
  .fact:
    test r6, r6
    jz .print_fact
    mul r6
    test r2, r2
    jnz .err_too_big
    dec r6
    jmp .fact
  .print_fact:
    mov r7, r0
    call print_int
    jmp .done
  .err_too_big:
    mov qword [errno], ERR_NUMBER_TOO_BIG_FOR_REGISTER
  .done:
    jmp process_file_data.munmap

math_is_prime:
    mov qword [process_file_func], 3
    jmp process_file_data
math_is_prime_func:
    ; calc factorial
    mov r7, [file_mem_addr]
    call parse_uint
    test r0, r0
    jz .done
    mov r1, r0
    xor r2, r2
    mov r6, r0
    dec r6
  .div:
    cmp r6, 1
    je .prime
    div r6
    test r2, r2
    jz .not_prime
    dec r6
    mov r0, r1
    xor r2, r2
    jmp .div
  .prime:
    xor r0, r0
    jmp .done
  .not_prime:
    xor r0, r0
    inc r0
  .done:
    mov r7, r0
    call print_int
    jmp process_file_data.munmap

print_uint:
    xor r6, r6
    jmp print_number
print_int:
    xor r6, r6
    inc r6
print_number:
    ; clear number buffer (see section .data)
    mov qword [number +  0], 0
    mov qword [number +  8], 0
    mov qword [number + 16], 0
    mov qword [number + 24], 0
    mov r0, r7      ; arg1 - dividend
    xor r1, r1      ; assume int is unsigned
    test r6, r6     ; arg2 - type of int signed (1) / unsigned (0)
    jz .type_detected
    cmp r7, 0       ; arg1 - dividend, if it is zero, let it be unsigned
    jge .type_detected
    inc r1          ; int is signed
    xor r0, r0      
    sub r0, r7      ; zero minus negative arg1 will give us absulute of arg1
  .type_detected:
    mov r8, 10      ; divisor in r8
    mov r10, 32     ; max number of symbols
    dec r10         ; leave one cell for null-termination
    ; perform division by r8 in a loop
  .continue_division:
    xor r2, r2      ; zero rdx2
    div r8          ; Unsigned divide rdx2:rax0 by r8, with result stored
                    ;  in rax0 - quotient, rdx2 - remainder
    add r2, 0x30
    test r10, r10
    jz .error
    dec r10         ; decrease symbols counter
    mov byte [number + r10], r2b
                    ; store remainder
    test r0, r0     ; if quotient != 0 continue
    jnz .continue_division
    test r1, r1
    jz .print
    dec r10
    mov byte [number + r10], '-'
  .print:
    lea r7, [number + r10]
    call print_string
    jmp .done
.error:
    xor r0, r0
.done:
ret

read_char:
    push 0              ; advance stack pointer and
    mov r6, r4          ;  store input in allocated memory on the stack
    mov r2, 1           ; number of bytes to read
    mov r7, 0           ; stdin file descriptor
    mov r0, 0           ; 'read' syscall
    syscall
    pop r0
ret

read_word:
    ; rdi7 - pointer to buffer
    ; rsi6 - buffer length
    dec r6
    cmp r6, 0       ; error if zero buffer length
    jz .error
    xor r2, r2
    dec r2          ; start with -1 before first inc
  .get_next_char:
    ; read char
    push r2
    push r6
    push r7
    call read_char
    pop r7
    pop r6
    pop r2
    ; analyse char
    cmp r0, 0x9
    jz .terminate_buffer
    cmp r0, 0xA
    jz .terminate_buffer
    cmp r0, 0xC
    jz .terminate_buffer
    cmp r0, 0x20
    jz .terminate_buffer
  .store_char:
    inc r2          
    cmp r2, r6      
    jz .error       ; error if buffer is full
    mov byte [r7 + r2], r0b
                    ; store char
    cmp r0, 0       ; continue if not null
    jnz .get_next_char
.quit:
    mov r0, r7      ; success, return pointer to buff in r0
                    ; symbols counter is already in r2
    jmp .ret
.terminate_buffer:
    xor r0, r0      ; null-terminate the word
    jmp .store_char
.error:
    xor r0, r0      ; error indicator
.ret:
ret

; rdi7 points to a string with a number to be parsed
; returns rax0: parsed number, rdx2 : count of characters
parse_int:
parse_uint:
    xor r8, r8      ; one symbol to be parsed
    xor r6, r6
    add r6, 10      ; multiplier (10)
    xor r0, r0      ; parsed number for output
    xor r2, r2      ; count of characters
    dec r2          ; dec count length before first inc
    xor r1, r1      ; assume unsigned int is given -> set r1 to 0
    cmp byte [r7], 0
    je .quit        ; quit if empty string given
    cmp byte [r7], '-'
    jne .parse_digit
    inc r1          ; signed int given -> set r1 to 1
    inc r2          ; first symbol is minus-sign, skip it
  .parse_digit:
    inc r2
    mov r8b, byte [r7 + r2]
    test r8b, r8b
    je .quit        ; quit if end of string
    cmp r8b, '0'
    jb .quit        ; quit if below '0'
    cmp r8b, '9'
    ja .quit        ; quit if above '9'
    push r2
    mul r6          ; mul r0 by r6 (10) and then add parsed digit to rax0
    test r2, r2
    jnz .error      ; number is too big for single 64-bit register
    pop r2
    sub r8, 0x30    ; transform printable symbol to digit
    add r0, r8      ; add it to accumulator
    jmp .parse_digit
.quit:
    test r1, r1
    jz .ret         ; return if unsigned
    neg r0          ; negate
    jmp .ret        ;  and return if signed
.error:
    pop r2
    xor r0, r0
    xor r2, r2
.ret:
ret

string_equals:
    xor r0, r0
  .compare:
    movzx r1, byte [r7]
    movzx r2, byte [r6]
    cmp r1, r2
    jne .ret
    test r1, r1
    jz .equals
    inc r7
    inc r6
    jmp .compare
  .equals:
    inc r0
.ret:
ret

string_copy:
    push r6
    cmp r2, 0
    jbe .error
    xor r8, r8
  .copy:
    movzx r1, byte [r7 + r8]
    mov byte [r6 + r8], r1b
    test r1, r1
    jz .quit
    inc r8
    cmp r2, r8
    je .error
    jmp .copy
.error:
    pop r6
    xor r0, r0
    jmp .ret
.quit:
    pop r0
.ret:
ret


