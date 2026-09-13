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
	.ascii "big"
	.byte 0
/* end data */

.text
.balign 16
.globl vayu_main
vayu_main:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	pushq %rsi
	pushq %rdi
	movl $0, %esi
Lbb2:
	cmpq $5, %rsi
	setl %al
	movzbl %al, %eax
	movslq %eax, %rax
	cmpq $0, %rax
	jz Lbb4
	subq $32, %rsp
	movq %rsi, %rcx
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	addq $1, %rsi
	jmp Lbb2
Lbb4:
	subq $32, %rsp
	leaq vayu_strlit_0(%rip), %rcx
	callq vayu_print_str_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	cmpq $3, %rsi
	setg %al
	movzbl %al, %eax
	movslq %eax, %rax
	cmpq $0, %rax
	jz Lbb6
	subq $32, %rsp
	leaq vayu_strlit_1(%rip), %rcx
	callq vayu_print_str_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
Lbb6:
	subq $32, %rsp
	leaq vayu_strlit_0(%rip), %rcx
	callq vayu_print_str_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	movl $0, %eax
	movl $0, %ecx
Lbb8:
	cmpq $10, %rcx
	jge Lbb10
	addq %rcx, %rax
	addq $1, %rcx
	jmp Lbb8
Lbb10:
	movq %rax, %rcx
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
	movl $4, %edx
	movl $3, %ecx
	callq vayu_fn_add
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	movl $20, %edx
	movl $10, %ecx
	callq vayu_fn_add
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	subq $32, %rsp
	movl $5, %ecx
	callq vayu_fn_fact
	movq %rax, %rcx
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	movl $0, %edi
	movl $0, %esi
Lbb13:
	cmpq $100, %rdi
	jge Lbb20
	cmpq $42, %rdi
	setz %al
	movzbl %al, %eax
	movslq %eax, %rax
	cmpq $0, %rax
	jnz Lbb19
	subq $32, %rsp
	movl $2, %edx
	movq %rdi, %rcx
	callq vayu_mod
	subq $-32, %rsp
	cmpq $0, %rax
	setz %al
	movzbl %al, %eax
	movslq %eax, %rax
	cmpq $0, %rax
	jnz Lbb18
	subq $32, %rsp
	movl $2, %edx
	movq %rdi, %rcx
	callq vayu_mod
	subq $-32, %rsp
	cmpq $0, %rax
	setz %al
	movzbl %al, %eax
	movslq %eax, %rax
	cmpq $0, %rax
	jnz Lbb18
	addq $1, %rsi
Lbb18:
	addq $1, %rdi
	jmp Lbb13
Lbb19:
	movq %rsi, %rcx
	jmp Lbb21
Lbb20:
	movq %rsi, %rcx
Lbb21:
	subq $32, %rsp
	callq vayu_print_int_noln
	subq $-32, %rsp
	subq $32, %rsp
	callq vayu_print_ln
	subq $-32, %rsp
	popq %rdi
	popq %rsi
	leave
	ret
/* end function vayu_main */

.text
.balign 16
vayu_fn_add:
	endbr64
	movq %rcx, %rax
	addq %rdx, %rax
	ret
/* end function vayu_fn_add */

.text
.balign 16
vayu_fn_fact:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	subq $8, %rsp
	pushq %rsi
	cmpq $1, %rcx
	setle %al
	movzbl %al, %eax
	movslq %eax, %rax
	cmpq $0, %rax
	jnz Lbb27
	movq %rcx, %rsi
	subq $1, %rcx
	subq $32, %rsp
	callq vayu_fn_fact
	movq %rsi, %rcx
	subq $-32, %rsp
	imulq %rcx, %rax
	jmp Lbb28
Lbb27:
	movl $1, %eax
Lbb28:
	popq %rsi
	leave
	ret
/* end function vayu_fn_fact */

