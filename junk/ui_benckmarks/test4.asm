	.text
	.file	"test4.cpp"
	.globl	_Z8ui_test710ui_contextjb       # -- Begin function _Z8ui_test710ui_contextjb
	.p2align	4, 0x90
	.type	_Z8ui_test710ui_contextjb,@function
_Z8ui_test710ui_contextjb:              # @_Z8ui_test710ui_contextjb
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	pushq	%r15
	.cfi_def_cfa_offset 24
	pushq	%r14
	.cfi_def_cfa_offset 32
	pushq	%r13
	.cfi_def_cfa_offset 40
	pushq	%r12
	.cfi_def_cfa_offset 48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	subq	$104, %rsp
	.cfi_def_cfa_offset 160
	.cfi_offset %rbx, -56
	.cfi_offset %r12, -48
	.cfi_offset %r13, -40
	.cfi_offset %r14, -32
	.cfi_offset %r15, -24
	.cfi_offset %rbp, -16
	movl	%edx, 12(%rsp)                  # 4-byte Spill
                                        # kill: def $esi killed $esi def $rsi
	movq	%rsi, 16(%rsp)                  # 8-byte Spill
	movq	%rdi, %rbx
	movq	%fs:40, %rax
	movq	%rax, 96(%rsp)
	movl	160(%rsp), %r13d
	movl	164(%rsp), %ecx
	movq	168(%rsp), %rax
	movq	176(%rsp), %rdx
	movq	%rdx, 24(%rsp)                  # 8-byte Spill
	movq	184(%rsp), %r12
	leaq	192(%rsp), %r15
	movaps	192(%rsp), %xmm0
	movaps	%xmm0, 32(%rsp)
	xorps	%xmm0, %xmm0
	movups	%xmm0, 55(%rsp)
	leal	24(%r13), %r14d
	cmpl	%ecx, %r14d
	jbe	.LBB0_1
	.p2align	4, 0x90
.LBB0_2:                                # =>This Inner Loop Header: Depth=1
	movl	%ecx, %ebp
	leal	(%rbp,%rbp), %ecx
	cmpl	%r14d, %ebp
	jb	.LBB0_2
# %bb.3:
	movl	%ebp, %esi
	movq	%rax, %rdi
	callq	*%r12
	jmp	.LBB0_4
.LBB0_1:
	movl	%ecx, %ebp
.LBB0_4:
	movq	%r12, (%rsp)                    # 8-byte Spill
	movb	$1, (%rax,%r13)
	movups	48(%rsp), %xmm0
	movups	%xmm0, 1(%rax,%r13)
	movq	63(%rsp), %rcx
	movq	%rcx, 16(%rax,%r13)
	movups	(%r15), %xmm0
	movaps	%xmm0, 80(%rsp)
	leal	40(%r13), %r15d
	cmpl	%ebp, %r15d
	jbe	.LBB0_5
	.p2align	4, 0x90
.LBB0_6:                                # =>This Inner Loop Header: Depth=1
	movl	%ebp, %r12d
	leal	(%r12,%r12), %ebp
	cmpl	%r15d, %r12d
	jb	.LBB0_6
# %bb.7:
	movl	%r12d, %esi
	movq	%rax, %rdi
	callq	*(%rsp)                         # 8-byte Folded Reload
	jmp	.LBB0_8
.LBB0_5:
	movl	%ebp, %r12d
.LBB0_8:
	movabsq	$280375465082880, %rdx          # imm = 0xFF0000000000
	movl	%r14d, %ecx
	movb	$4, (%rax,%rcx)
	movq	%rdx, 8(%rax,%rcx)
	cmpb	$0, 12(%rsp)                    # 1-byte Folded Reload
	je	.LBB0_15
# %bb.9:
	movaps	80(%rsp), %xmm0
	movaps	%xmm0, 48(%rsp)
	addl	$56, %r13d
	cmpl	%r12d, %r13d
	jbe	.LBB0_10
# %bb.11:
	movq	%rdx, %r14
	.p2align	4, 0x90
.LBB0_12:                               # =>This Inner Loop Header: Depth=1
	movl	%r12d, %ebp
	leal	(%rbp,%rbp), %r12d
	cmpl	%r13d, %ebp
	jb	.LBB0_12
# %bb.13:
	movl	%ebp, %esi
	movq	%rax, %rdi
	movq	(%rsp), %r12                    # 8-byte Reload
	callq	*%r12
	movq	%r14, %rdx
	jmp	.LBB0_14
.LBB0_15:
	movq	16(%rsp), %r14                  # 8-byte Reload
	shll	$16, %r14d
	orq	%rdx, %r14
	movaps	80(%rsp), %xmm0
	movaps	%xmm0, 48(%rsp)
	addl	$56, %r13d
	cmpl	%r12d, %r13d
	jbe	.LBB0_16
	.p2align	4, 0x90
.LBB0_17:                               # =>This Inner Loop Header: Depth=1
	movl	%r12d, %ebp
	leal	(%rbp,%rbp), %r12d
	cmpl	%r13d, %ebp
	jb	.LBB0_17
# %bb.18:
	movl	%ebp, %esi
	movq	%rax, %rdi
	movq	(%rsp), %r12                    # 8-byte Reload
	callq	*%r12
	jmp	.LBB0_19
.LBB0_10:
	movl	%r12d, %ebp
	movq	(%rsp), %r12                    # 8-byte Reload
.LBB0_14:
	movl	%r15d, %ecx
	movb	$4, (%rax,%rcx)
	movq	%rdx, 8(%rax,%rcx)
	jmp	.LBB0_20
.LBB0_16:
	movl	%r12d, %ebp
	movq	(%rsp), %r12                    # 8-byte Reload
.LBB0_19:
	movl	%r15d, %ecx
	movb	$4, (%rax,%rcx)
	movq	%r14, 8(%rax,%rcx)
.LBB0_20:
	movaps	48(%rsp), %xmm0
	movaps	%xmm0, 80(%rsp)
	movaps	80(%rsp), %xmm0
	movaps	%xmm0, 32(%rsp)
	movaps	%xmm0, 48(%rsp)
	leal	8(%r13), %r14d
	cmpl	%ebp, %r14d
	jbe	.LBB0_21
	.p2align	4, 0x90
.LBB0_22:                               # =>This Inner Loop Header: Depth=1
	movl	%ebp, %r15d
	leal	(%r15,%r15), %ebp
	cmpl	%r14d, %r15d
	jb	.LBB0_22
# %bb.23:
	movl	%r15d, %esi
	movq	%rax, %rdi
	callq	*%r12
	jmp	.LBB0_24
.LBB0_21:
	movl	%ebp, %r15d
.LBB0_24:
	movl	%r13d, %ecx
	movq	$2, (%rax,%rcx)
	movaps	48(%rsp), %xmm0
	movaps	%xmm0, 32(%rsp)
	movl	%r14d, (%rbx)
	movl	%r15d, 4(%rbx)
	movq	%rax, 8(%rbx)
	movq	24(%rsp), %rax                  # 8-byte Reload
	movq	%rax, 16(%rbx)
	movq	%r12, 24(%rbx)
	movaps	32(%rsp), %xmm0
	movups	%xmm0, 32(%rbx)
	movq	%fs:40, %rax
	cmpq	96(%rsp), %rax
	jne	.LBB0_26
# %bb.25:
	movq	%rbx, %rax
	addq	$104, %rsp
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%r12
	.cfi_def_cfa_offset 40
	popq	%r13
	.cfi_def_cfa_offset 32
	popq	%r14
	.cfi_def_cfa_offset 24
	popq	%r15
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
.LBB0_26:
	.cfi_def_cfa_offset 160
	callq	__stack_chk_fail@PLT
.Lfunc_end0:
	.size	_Z8ui_test710ui_contextjb, .Lfunc_end0-_Z8ui_test710ui_contextjb
	.cfi_endproc
                                        # -- End function
	.section	.rodata.cst16,"aM",@progbits,16
	.p2align	4, 0x0                          # -- Begin function main
.LCPI1_0:
	.zero	16
	.text
	.globl	main
	.p2align	4, 0x90
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	pushq	%r15
	.cfi_def_cfa_offset 24
	pushq	%r14
	.cfi_def_cfa_offset 32
	pushq	%r13
	.cfi_def_cfa_offset 40
	pushq	%r12
	.cfi_def_cfa_offset 48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	subq	$40, %rsp
	.cfi_def_cfa_offset 96
	.cfi_offset %rbx, -56
	.cfi_offset %r12, -48
	.cfi_offset %r13, -40
	.cfi_offset %r14, -32
	.cfi_offset %r15, -24
	.cfi_offset %rbp, -16
	movq	%fs:40, %rax
	movq	%rax, 32(%rsp)
	movabsq	$280375465082880, %r15          # imm = 0xFF0000000000
	movl	$256, %edi                      # imm = 0x100
	callq	malloc@PLT
	movq	%rax, %rbx
	movq	%rax, %r12
	incq	%r12
	movl	$200000000, %ebp                # imm = 0xBEBC200
	leaq	16(%rsp), %r13
	.p2align	4, 0x90
.LBB1_1:                                # =>This Inner Loop Header: Depth=1
	callq	rand@PLT
	movl	%eax, %r14d
	callq	rand@PLT
	xorps	%xmm0, %xmm0
	movups	%xmm0, (%r13)
	movb	$1, (%rbx)
	movq	24(%rsp), %rax
	movq	%rax, 15(%r12)
	movups	9(%rsp), %xmm0
	movups	%xmm0, (%r12)
	movb	$4, 24(%rbx)
	movq	%r15, 32(%rbx)
	shll	$16, %r14d
	orq	%r15, %r14
	movb	$4, 40(%rbx)
	movq	%r14, 48(%rbx)
	movq	$2, 56(%rbx)
	decl	%ebp
	jne	.LBB1_1
# %bb.2:
	callq	rand@PLT
	movl	%eax, %ecx
	imulq	$68174085, %rcx, %rcx           # imm = 0x4104105
	shrq	$32, %rcx
	movl	%eax, %edx
	subl	%ecx, %edx
	shrl	%edx
	addl	%ecx, %edx
	shrl	$5, %edx
	movl	%edx, %ecx
	shll	$6, %ecx
	subl	%ecx, %edx
	addl	%eax, %edx
	movzbl	(%rbx,%rdx), %edx
	movq	%fs:40, %rax
	cmpq	32(%rsp), %rax
	jne	.LBB1_4
# %bb.3:
	leaq	.L.str(%rip), %rdi
	movl	$64, %esi
	xorl	%eax, %eax
	callq	printf@PLT
	xorl	%eax, %eax
	addq	$40, %rsp
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%r12
	.cfi_def_cfa_offset 40
	popq	%r13
	.cfi_def_cfa_offset 32
	popq	%r14
	.cfi_def_cfa_offset 24
	popq	%r15
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
.LBB1_4:
	.cfi_def_cfa_offset 96
	callq	__stack_chk_fail@PLT
.Lfunc_end1:
	.size	main, .Lfunc_end1-main
	.cfi_endproc
                                        # -- End function
	.globl	_Z3fasz                         # -- Begin function _Z3fasz
	.p2align	4, 0x90
	.type	_Z3fasz,@function
_Z3fasz:                                # @_Z3fasz
	.cfi_startproc
# %bb.0:
	retq
.Lfunc_end2:
	.size	_Z3fasz, .Lfunc_end2-_Z3fasz
	.cfi_endproc
                                        # -- End function
	.type	.L.str,@object                  # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"size = %u asfasf %u\n"
	.size	.L.str, 21

	.section	".linker-options","e",@llvm_linker_options
	.ident	"clang version 16.0.6"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym __stack_chk_fail
