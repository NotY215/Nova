.data
.balign 8
vayu_strlit_0:
	.quad 3
	.ascii "---"
	.byte 0
/* end data */

.data
.balign 8
vayu_strlit_1:
	.quad 3
	.ascii "Rex"
	.byte 0
/* end data */

.data
.balign 8
vayu_strlit_2:
	.quad 11
	.ascii "hello world"
	.byte 0
/* end data */

.data
.balign 8
vayu_strlit_3:
	.quad 5
	.ascii "world"
	.byte 0
/* end data */

.data
.balign 8
vayu_strlit_4:
	.quad 5
	.ascii "hello"
	.byte 0
/* end data */

.data
.balign 8
vayu_strlit_5:
	.quad 1
	.ascii "d"
	.byte 0
/* end data */

.data
.balign 8
vayu_strlit_6:
	.quad 3
	.ascii "123"
	.byte 0
/* end data */

.data
.balign 8
vayu_strlit_7:
	.quad 3
	.ascii "abc"
	.byte 0
/* end data */

.data
.balign 8
vayu_strlit_8:
	.quad 3
	.ascii "def"
	.byte 0
/* end data */

.data
.balign 8
vayu_strlit_9:
	.quad 1
	.ascii "a"
	.byte 0
/* end data */

.data
.balign 8
vayu_strlit_10:
	.quad 14
	.ascii " makes a sound"
	.byte 0
/* end data */

.data
.balign 8
vayu_strlit_11:
	.quad 10
	.ascii " says woof"
	.byte 0
/* end data */

.text
.balign 16
.globl vayu_main
vayu_main:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	subq $8, %rsp
	pushq %rbx
	pushq %rsi
	pushq %rdi
	subq $32, %rsp
	movl $4, %edx
	movl $3, %ecx
	callq vayu_ctor_Point
	movq %rax, %rcx
	subq $-32, %rsp
	movq %rcx, %rsi
	movq (%rcx), %rcx
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	movq %rsi, %rcx
	subq $-32, %rsp
	movq %rcx, %rsi
	movq 8(%rcx), %rcx
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	movq %rsi, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_mth_Point_mag_sq
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	leaq vayu_strlit_0(%rip), %rcx
	callq vayu_print_str_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	leaq vayu_strlit_1(%rip), %rcx
	callq vayu_ctor_Dog
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_mth_Dog_speak
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	leaq vayu_strlit_0(%rip), %rcx
	callq vayu_print_str_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_list_new
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	movl $10, %edx
	movq %rcx, %rsi
	callq vayu_list_push
	movq %rsi, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	movl $20, %edx
	movq %rcx, %rsi
	callq vayu_list_push
	movq %rsi, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	movl $30, %edx
	movq %rcx, %rsi
	callq vayu_list_push
	movq %rsi, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	movl $1, %edx
	movq %rcx, %rsi
	callq vayu_len
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	movq %rsi, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	movl $0, %edx
	movq %rcx, %rsi
	callq vayu_list_get
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	movq %rsi, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	movl $2, %edx
	movq %rcx, %rsi
	callq vayu_list_get
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	movq %rsi, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	movl $99, %r8d
	movl $1, %edx
	movq %rcx, %rsi
	callq vayu_list_set
	movq %rsi, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	movl $1, %edx
	movq %rcx, %rsi
	callq vayu_list_get
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	movq %rsi, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	movl $40, %edx
	movq %rcx, %rsi
	callq vayu_list_push
	movq %rsi, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	movl $1, %edx
	callq vayu_len
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	leaq vayu_strlit_0(%rip), %rcx
	callq vayu_print_str_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_list_new
	movq %rax, %rsi
	subq $-32, %rsp
	subq $32, %rsp
	movl $1, %edx
	movq %rsi, %rcx
	callq vayu_list_push
	subq $-32, %rsp
	subq $32, %rsp
	movl $2, %edx
	movq %rsi, %rcx
	callq vayu_list_push
	subq $-32, %rsp
	subq $32, %rsp
	movl $3, %edx
	movq %rsi, %rcx
	callq vayu_list_push
	subq $-32, %rsp
	subq $32, %rsp
	movl $4, %edx
	movq %rsi, %rcx
	callq vayu_list_push
	subq $-32, %rsp
	subq $32, %rsp
	movl $5, %edx
	movq %rsi, %rcx
	callq vayu_list_push
	subq $-32, %rsp
	movl $0, %edi
	movl $0, %ebx
Lbb2:
	subq $32, %rsp
	movl $1, %edx
	movq %rsi, %rcx
	callq vayu_len
	subq $-32, %rsp
	cmpq %rax, %rbx
	jge Lbb4
	subq $32, %rsp
	movq %rbx, %rdx
	movq %rsi, %rcx
	callq vayu_list_get
	subq $-32, %rsp
	addq %rax, %rdi
	addq $1, %rbx
	jmp Lbb2
Lbb4:
	movq %rdi, %rcx
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	leaq vayu_strlit_0(%rip), %rcx
	callq vayu_print_str_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	leaq vayu_strlit_2(%rip), %rcx
	callq vayu_str_upper
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_str_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	leaq vayu_strlit_3(%rip), %rdx
	leaq vayu_strlit_2(%rip), %rcx
	callq vayu_str_contains
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_bool_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	leaq vayu_strlit_3(%rip), %rdx
	leaq vayu_strlit_2(%rip), %rcx
	callq vayu_str_find
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	leaq vayu_strlit_4(%rip), %rdx
	leaq vayu_strlit_2(%rip), %rcx
	callq vayu_str_starts_with
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_bool_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	leaq vayu_strlit_5(%rip), %rdx
	leaq vayu_strlit_2(%rip), %rcx
	callq vayu_str_ends_with
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_bool_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	leaq vayu_strlit_0(%rip), %rcx
	callq vayu_print_str_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	movl $0, %edx
	movl $42, %ecx
	callq vayu_int_to_str
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_str_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	leaq vayu_strlit_6(%rip), %rcx
	callq vayu_str_to_int
	subq $-32, %rsp
	movq %rax, %rcx
	addq $1, %rcx
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	leaq vayu_strlit_8(%rip), %rdx
	leaq vayu_strlit_7(%rip), %rcx
	callq vayu_str_concat
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_str_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	leaq vayu_strlit_9(%rip), %rdx
	leaq vayu_strlit_9(%rip), %rcx
	callq vayu_str_eq
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_bool_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	popq %rdi
	popq %rsi
	popq %rbx
	leave
	ret
/* end function vayu_main */

.text
.balign 16
vayu_mth_Point_mag_sq:
	endbr64
	movq (%rcx), %rax
	imulq %rax, %rax
	movq 8(%rcx), %rcx
	imulq %rcx, %rcx
	addq %rcx, %rax
	ret
/* end function vayu_mth_Point_mag_sq */

.text
.balign 16
vayu_mth_Point___init__:
	endbr64
	movq %rdx, (%rcx)
	movq %r8, 8(%rcx)
	movl $0, %eax
	ret
/* end function vayu_mth_Point___init__ */

.text
.balign 16
vayu_ctor_Point:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	pushq %rsi
	pushq %rdi
	movq %rdx, %rdi
	movq %rcx, %rsi
	subq $32, %rsp
	movl $16, %ecx
	callq vayu_alloc
	movq %rdi, %r8
	movq %rsi, %rdx
	movq %rax, %rsi
	subq $-32, %rsp
	subq $32, %rsp
	movq %rsi, %rcx
	callq vayu_mth_Point___init__
	movq %rsi, %rax
	subq $-32, %rsp
	popq %rdi
	popq %rsi
	leave
	ret
/* end function vayu_ctor_Point */

.text
.balign 16
vayu_mth_Animal_speak:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	movq 16(%rcx), %rcx
	subq $32, %rsp
	leaq vayu_strlit_10(%rip), %rdx
	callq vayu_str_concat
	subq $-32, %rsp
	leave
	ret
/* end function vayu_mth_Animal_speak */

.text
.balign 16
vayu_mth_Animal___init__:
	endbr64
	movq %rdx, 16(%rcx)
	movl $0, %eax
	ret
/* end function vayu_mth_Animal___init__ */

.text
.balign 16
vayu_ctor_Animal:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	subq $8, %rsp
	pushq %rsi
	movq %rcx, %rsi
	subq $32, %rsp
	movl $24, %ecx
	callq vayu_alloc
	movq %rsi, %rdx
	movq %rax, %rsi
	subq $-32, %rsp
	subq $32, %rsp
	movq %rsi, %rcx
	callq vayu_mth_Animal___init__
	movq %rsi, %rax
	subq $-32, %rsp
	popq %rsi
	leave
	ret
/* end function vayu_ctor_Animal */

.text
.balign 16
vayu_mth_Dog_speak:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	movq 16(%rcx), %rcx
	subq $32, %rsp
	leaq vayu_strlit_11(%rip), %rdx
	callq vayu_str_concat
	subq $-32, %rsp
	leave
	ret
/* end function vayu_mth_Dog_speak */

.text
.balign 16
vayu_mth_Dog___init__:
	endbr64
	movq %rdx, 16(%rcx)
	movl $0, %eax
	ret
/* end function vayu_mth_Dog___init__ */

.text
.balign 16
vayu_ctor_Dog:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	subq $8, %rsp
	pushq %rsi
	movq %rcx, %rsi
	subq $32, %rsp
	movl $24, %ecx
	callq vayu_alloc
	movq %rsi, %rdx
	movq %rax, %rsi
	subq $-32, %rsp
	subq $32, %rsp
	movq %rsi, %rcx
	callq vayu_mth_Dog___init__
	movq %rsi, %rax
	subq $-32, %rsp
	popq %rsi
	leave
	ret
/* end function vayu_ctor_Dog */

