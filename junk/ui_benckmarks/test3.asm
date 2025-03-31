	.text
	.file	"test3.cpp"
	.globl	_Z4recttt4rgba                  # -- Begin function _Z4recttt4rgba
	.p2align	4, 0x90
	.type	_Z4recttt4rgba,@function
_Z4recttt4rgba:                         # @_Z4recttt4rgba
	.cfi_startproc
# %bb.0:
                                        # kill: def $edx killed $edx def $rdx
	shlq	$32, %rdx
	movl	%esi, %ecx
	shlq	$16, %rcx
	orq	%rdx, %rcx
	movl	%edi, %eax
	orq	%rcx, %rax
	retq
.Lfunc_end0:
	.size	_Z4recttt4rgba, .Lfunc_end0-_Z4recttt4rgba
	.cfi_endproc
                                        # -- End function
	.globl	_Z5test76bufferjb               # -- Begin function _Z5test76bufferjb
	.p2align	4, 0x90
	.type	_Z5test76bufferjb,@function
_Z5test76bufferjb:                      # @_Z5test76bufferjb
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
	movl	%edx, 20(%rsp)                  # 4-byte Spill
	movl	%esi, 4(%rsp)                   # 4-byte Spill
	movq	%rdi, %rbx
	movl	96(%rsp), %r15d
	movl	100(%rsp), %ecx
	movq	104(%rsp), %rax
	movq	112(%rsp), %rdx
	movq	%rdx, 32(%rsp)                  # 8-byte Spill
	movq	120(%rsp), %r14
	movq	128(%rsp), %rdx
	movq	%rdx, 24(%rsp)                  # 8-byte Spill
	leal	4(%r15), %r12d
	cmpl	%ecx, %r12d
	jbe	.LBB1_1
	.p2align	4, 0x90
.LBB1_2:                                # =>This Inner Loop Header: Depth=1
	movl	%ecx, %ebp
	leal	(%rbp,%rbp), %ecx
	cmpl	%r12d, %ebp
	jb	.LBB1_2
# %bb.3:
	movl	%ebp, %esi
	movq	%rax, %rdi
	callq	*%r14
	jmp	.LBB1_4
.LBB1_1:
	movl	%ecx, %ebp
.LBB1_4:
	movq	%r14, 8(%rsp)                   # 8-byte Spill
	movl	$4, (%rax,%r15)
	leal	16(%r15), %r13d
	cmpl	%ebp, %r13d
	jbe	.LBB1_5
	.p2align	4, 0x90
.LBB1_6:                                # =>This Inner Loop Header: Depth=1
	movl	%ebp, %r14d
	leal	(%r14,%r14), %ebp
	cmpl	%r13d, %r14d
	jb	.LBB1_6
# %bb.7:
	movl	%r14d, %esi
	movq	%rax, %rdi
	callq	*8(%rsp)                        # 8-byte Folded Reload
	jmp	.LBB1_8
.LBB1_5:
	movl	%ebp, %r14d
.LBB1_8:
	movl	%r12d, %ecx
	movb	$0, (%rax,%rcx)
	movabsq	$280375465082880, %rdx          # imm = 0xFF0000000000
	movq	%rdx, 4(%rax,%rcx)
	cmpb	$0, 20(%rsp)                    # 1-byte Folded Reload
	je	.LBB1_13
# %bb.9:
	addl	$28, %r15d
	cmpl	%r14d, %r15d
	jbe	.LBB1_10
	.p2align	4, 0x90
.LBB1_11:                               # =>This Inner Loop Header: Depth=1
	movl	%r14d, %r12d
	leal	(%r12,%r12), %r14d
	cmpl	%r15d, %r12d
	jb	.LBB1_11
# %bb.12:
	movl	$0, 4(%rsp)                     # 4-byte Folded Spill
	jmp	.LBB1_15
.LBB1_13:
	addl	$28, %r15d
	cmpl	%r14d, %r15d
	jbe	.LBB1_16
	.p2align	4, 0x90
.LBB1_14:                               # =>This Inner Loop Header: Depth=1
	movl	%r14d, %r12d
	leal	(%r12,%r12), %r14d
	cmpl	%r15d, %r12d
	jb	.LBB1_14
.LBB1_15:
	movl	%r12d, %esi
	movq	%rax, %rdi
	callq	*8(%rsp)                        # 8-byte Folded Reload
	movl	%r12d, %r14d
.LBB1_16:
	movl	4(%rsp), %edx                   # 4-byte Reload
	jmp	.LBB1_17
.LBB1_10:
	xorl	%edx, %edx
.LBB1_17:
	movl	%r13d, %ecx
	movb	$0, (%rax,%rcx)
	movw	$0, 4(%rax,%rcx)
	movw	%dx, 6(%rax,%rcx)
	movl	$65280, 8(%rax,%rcx)            # imm = 0xFF00
	leal	4(%r15), %ebp
	cmpl	%r14d, %ebp
	jbe	.LBB1_18
	.p2align	4, 0x90
.LBB1_19:                               # =>This Inner Loop Header: Depth=1
	movl	%r14d, %r12d
	leal	(%r12,%r12), %r14d
	cmpl	%ebp, %r12d
	jb	.LBB1_19
# %bb.20:
	movl	%r12d, %esi
	movq	%rax, %rdi
	movq	8(%rsp), %r14                   # 8-byte Reload
	callq	*%r14
	jmp	.LBB1_21
.LBB1_18:
	movl	%r14d, %r12d
	movq	8(%rsp), %r14                   # 8-byte Reload
.LBB1_21:
	movl	%r15d, %ecx
	movl	$5, (%rax,%rcx)
	movl	%ebp, (%rbx)
	movl	%r12d, 4(%rbx)
	movq	%rax, 8(%rbx)
	movq	32(%rsp), %rax                  # 8-byte Reload
	movq	%rax, 16(%rbx)
	movq	%r14, 24(%rbx)
	movq	24(%rsp), %rax                  # 8-byte Reload
	movq	%rax, 32(%rbx)
	movq	%rbx, %rax
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
.Lfunc_end1:
	.size	_Z5test76bufferjb, .Lfunc_end1-_Z5test76bufferjb
	.cfi_endproc
                                        # -- End function
	.globl	main                            # -- Begin function main
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
	pushq	%r12
	.cfi_def_cfa_offset 40
	pushq	%rbx
	.cfi_def_cfa_offset 48
	subq	$48, %rsp
	.cfi_def_cfa_offset 96
	.cfi_offset %rbx, -48
	.cfi_offset %r12, -40
	.cfi_offset %r14, -32
	.cfi_offset %r15, -24
	.cfi_offset %rbp, -16
	movq	%fs:40, %rax
	movq	%rax, 40(%rsp)
	movl	$256, %edi                      # imm = 0x100
	callq	malloc@PLT
	movq	%rax, %rbx
	movl	$200000000, %r14d               # imm = 0xBEBC200
	movabsq	$280375465082880, %r15          # imm = 0xFF0000000000
	movabsq	$21474901760, %r12              # imm = 0x50000FF00
	.p2align	4, 0x90
.LBB2_1:                                # =>This Inner Loop Header: Depth=1
	callq	rand@PLT
	movl	%eax, %ebp
	callq	rand@PLT
	movl	$4, (%rbx)
	movb	$0, 4(%rbx)
	movq	%r15, 8(%rbx)
	movb	$0, 16(%rbx)
	movw	$0, 20(%rbx)
	movw	%bp, 22(%rbx)
	movq	%r12, 24(%rbx)
	decl	%r14d
	jne	.LBB2_1
# %bb.2:
	movq	%rbx, 8(%rsp)
	leaq	_ZN3$_48__invokeEm(%rip), %rax
	movq	%rax, 16(%rsp)
	leaq	_ZN3$_58__invokeEPvm(%rip), %rax
	movq	%rax, 24(%rsp)
	leaq	_ZN3$_68__invokeEPv(%rip), %rax
	movq	%rax, 32(%rsp)
	callq	rand@PLT
	movl	%eax, %ecx
	imulq	$138547333, %rcx, %rcx          # imm = 0x8421085
	shrq	$32, %rcx
	movl	%eax, %edx
	subl	%ecx, %edx
	shrl	%edx
	addl	%ecx, %edx
	shrl	$4, %edx
	movl	%edx, %ecx
	shll	$5, %ecx
	subl	%ecx, %edx
	addl	%eax, %edx
	movl	8(%rsp,%rdx,4), %edx
	movq	%fs:40, %rax
	cmpq	40(%rsp), %rax
	jne	.LBB2_4
# %bb.3:
	leaq	.L.str(%rip), %rdi
	movl	$32, %esi
	xorl	%eax, %eax
	callq	printf@PLT
	xorl	%eax, %eax
	addq	$48, %rsp
	.cfi_def_cfa_offset 48
	popq	%rbx
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r14
	.cfi_def_cfa_offset 24
	popq	%r15
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
.LBB2_4:
	.cfi_def_cfa_offset 96
	callq	__stack_chk_fail@PLT
.Lfunc_end2:
	.size	main, .Lfunc_end2-main
	.cfi_endproc
                                        # -- End function
	.globl	_Z3fasz                         # -- Begin function _Z3fasz
	.p2align	4, 0x90
	.type	_Z3fasz,@function
_Z3fasz:                                # @_Z3fasz
	.cfi_startproc
# %bb.0:
	retq
.Lfunc_end3:
	.size	_Z3fasz, .Lfunc_end3-_Z3fasz
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function _ZN3$_48__invokeEm
	.type	_ZN3$_48__invokeEm,@function
_ZN3$_48__invokeEm:                     # @"_ZN3$_48__invokeEm"
	.cfi_startproc
# %bb.0:
	jmp	malloc@PLT                      # TAILCALL
.Lfunc_end4:
	.size	_ZN3$_48__invokeEm, .Lfunc_end4-_ZN3$_48__invokeEm
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function _ZN3$_58__invokeEPvm
	.type	_ZN3$_58__invokeEPvm,@function
_ZN3$_58__invokeEPvm:                   # @"_ZN3$_58__invokeEPvm"
	.cfi_startproc
# %bb.0:
	jmp	realloc@PLT                     # TAILCALL
.Lfunc_end5:
	.size	_ZN3$_58__invokeEPvm, .Lfunc_end5-_ZN3$_58__invokeEPvm
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function _ZN3$_68__invokeEPv
	.type	_ZN3$_68__invokeEPv,@function
_ZN3$_68__invokeEPv:                    # @"_ZN3$_68__invokeEPv"
	.cfi_startproc
# %bb.0:
	jmp	free@PLT                        # TAILCALL
.Lfunc_end6:
	.size	_ZN3$_68__invokeEPv, .Lfunc_end6-_ZN3$_68__invokeEPv
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
	.addrsig_sym _ZN3$_48__invokeEm
	.addrsig_sym _ZN3$_58__invokeEPvm
	.addrsig_sym _ZN3$_68__invokeEPv
	.addrsig_sym __stack_chk_fail
