/*
 * kernel_syscalls.s: hand-written assembly, retail (US v1.01) 0x001f59c0-0x001f6240.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x00: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f59c0 */
	.set	noreorder
	.text
	.globl	RFU000_FullReset
	.ent	RFU000_FullReset
RFU000_FullReset:
	li	$3,0				# $3 = syscall number 0x00
	syscall
	jr	$31
	nop
	.end	RFU000_FullReset

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x01 (ResetEE): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f59d0 */
	.set	noreorder
	.text
	.globl	ResetEE
	.ent	ResetEE
ResetEE:
	li	$3,1				# $3 = syscall number 0x01
	syscall
	jr	$31
	nop
	.end	ResetEE

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x02 (SetGsCrt): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f59e0 */
	.set	noreorder
	.text
	.globl	SetGsCrt
	.ent	SetGsCrt
SetGsCrt:
	li	$3,2				# $3 = syscall number 0x02
	syscall
	jr	$31
	nop
	.end	SetGsCrt

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x03: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f59f0 */
	.set	noreorder
	.text
	.globl	RFU003
	.ent	RFU003
RFU003:
	li	$3,3				# $3 = syscall number 0x03
	syscall
	jr	$31
	nop
	.end	RFU003

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x04 (_Exit): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5a00 */
	.set	noreorder
	.text
	.globl	_Exit
	.ent	_Exit
_Exit:
	li	$3,4				# $3 = syscall number 0x04
	syscall
	jr	$31
	nop
	.end	_Exit

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x05: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5a10 */
	.set	noreorder
	.text
	.globl	RFU005_ResumeIntrDispatch
	.ent	RFU005_ResumeIntrDispatch
RFU005_ResumeIntrDispatch:
	li	$3,5				# $3 = syscall number 0x05
	syscall
	jr	$31
	nop
	.end	RFU005_ResumeIntrDispatch

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x06 (LoadExecPS2): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5a20 */
	.set	noreorder
	.text
	.globl	LoadExecPS2
	.ent	LoadExecPS2
LoadExecPS2:
	li	$3,6				# $3 = syscall number 0x06
	syscall
	jr	$31
	nop
	.end	LoadExecPS2

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x07 (ExecPS2): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5a30 */
	.set	noreorder
	.text
	.globl	ExecPS2
	.ent	ExecPS2
ExecPS2:
	li	$3,7				# $3 = syscall number 0x07
	syscall
	jr	$31
	nop
	.end	ExecPS2

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x08: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5a40 */
	.set	noreorder
	.text
	.globl	RFU008_ResumeT3IntrDispatch
	.ent	RFU008_ResumeT3IntrDispatch
RFU008_ResumeT3IntrDispatch:
	li	$3,8				# $3 = syscall number 0x08
	syscall
	jr	$31
	nop
	.end	RFU008_ResumeT3IntrDispatch

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x09: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5a50 */
	.set	noreorder
	.text
	.globl	RFU009
	.ent	RFU009
RFU009:
	li	$3,9				# $3 = syscall number 0x09
	syscall
	jr	$31
	nop
	.end	RFU009

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x0a (AddSbusIntcHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5a60 */
	.set	noreorder
	.text
	.globl	AddSbusIntcHandler
	.ent	AddSbusIntcHandler
AddSbusIntcHandler:
	li	$3,10				# $3 = syscall number 0x0a
	syscall
	jr	$31
	nop
	.end	AddSbusIntcHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x0b (RemoveSbusIntcHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5a70 */
	.set	noreorder
	.text
	.globl	RemoveSbusIntcHandler
	.ent	RemoveSbusIntcHandler
RemoveSbusIntcHandler:
	li	$3,11				# $3 = syscall number 0x0b
	syscall
	jr	$31
	nop
	.end	RemoveSbusIntcHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x0c (Interrupt2Iop): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5a80 */
	.set	noreorder
	.text
	.globl	Interrupt2Iop
	.ent	Interrupt2Iop
Interrupt2Iop:
	li	$3,12				# $3 = syscall number 0x0c
	syscall
	jr	$31
	nop
	.end	Interrupt2Iop

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x0d (SetVTLBRefillHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5a90 */
	.set	noreorder
	.text
	.globl	SetVTLBRefillHandler
	.ent	SetVTLBRefillHandler
SetVTLBRefillHandler:
	li	$3,13				# $3 = syscall number 0x0d
	syscall
	jr	$31
	nop
	.end	SetVTLBRefillHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x0e (SetVCommonHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5aa0 */
	.set	noreorder
	.text
	.globl	SetVCommonHandler
	.ent	SetVCommonHandler
SetVCommonHandler:
	li	$3,14				# $3 = syscall number 0x0e
	syscall
	jr	$31
	nop
	.end	SetVCommonHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x0f (SetVInterruptHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5ab0 */
	.set	noreorder
	.text
	.globl	SetVInterruptHandler
	.ent	SetVInterruptHandler
SetVInterruptHandler:
	li	$3,15				# $3 = syscall number 0x0f
	syscall
	jr	$31
	nop
	.end	SetVInterruptHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x10 (AddIntcHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5ac0 */
	.set	noreorder
	.text
	.globl	AddIntcHandler
	.ent	AddIntcHandler
AddIntcHandler:
	li	$3,16				# $3 = syscall number 0x10
	syscall
	jr	$31
	nop
	.end	AddIntcHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x10 (AddIntcHandler2): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5ad0 */
	.set	noreorder
	.text
	.globl	AddIntcHandler2
	.ent	AddIntcHandler2
AddIntcHandler2:
	li	$3,16				# $3 = syscall number 0x10
	syscall
	jr	$31
	nop
	.end	AddIntcHandler2

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x11 (RemoveIntcHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5ae0 */
	.set	noreorder
	.text
	.globl	RemoveIntcHandler
	.ent	RemoveIntcHandler
RemoveIntcHandler:
	li	$3,17				# $3 = syscall number 0x11
	syscall
	jr	$31
	nop
	.end	RemoveIntcHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x12 (AddDmacHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5af0 */
	.set	noreorder
	.text
	.globl	AddDmacHandler
	.ent	AddDmacHandler
AddDmacHandler:
	li	$3,18				# $3 = syscall number 0x12
	syscall
	jr	$31
	nop
	.end	AddDmacHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x12 (AddDmacHandler2): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5b00 */
	.set	noreorder
	.text
	.globl	AddDmacHandler2
	.ent	AddDmacHandler2
AddDmacHandler2:
	li	$3,18				# $3 = syscall number 0x12
	syscall
	jr	$31
	nop
	.end	AddDmacHandler2

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x13 (RemoveDmacHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5b10 */
	.set	noreorder
	.text
	.globl	RemoveDmacHandler
	.ent	RemoveDmacHandler
RemoveDmacHandler:
	li	$3,19				# $3 = syscall number 0x13
	syscall
	jr	$31
	nop
	.end	RemoveDmacHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x14 (_EnableIntc): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5b20 */
	.set	noreorder
	.text
	.globl	_EnableIntc
	.ent	_EnableIntc
_EnableIntc:
	li	$3,20				# $3 = syscall number 0x14
	syscall
	jr	$31
	nop
	.end	_EnableIntc

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x15 (_DisableIntc): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5b30 */
	.set	noreorder
	.text
	.globl	_DisableIntc
	.ent	_DisableIntc
_DisableIntc:
	li	$3,21				# $3 = syscall number 0x15
	syscall
	jr	$31
	nop
	.end	_DisableIntc

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x16 (_EnableDmac): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5b40 */
	.set	noreorder
	.text
	.globl	_EnableDmac
	.ent	_EnableDmac
_EnableDmac:
	li	$3,22				# $3 = syscall number 0x16
	syscall
	jr	$31
	nop
	.end	_EnableDmac

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x17 (_DisableDmac): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5b50 */
	.set	noreorder
	.text
	.globl	_DisableDmac
	.ent	_DisableDmac
_DisableDmac:
	li	$3,23				# $3 = syscall number 0x17
	syscall
	jr	$31
	nop
	.end	_DisableDmac

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0xfc (SetAlarm): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5b60 */
	.set	noreorder
	.text
	.globl	SetAlarm
	.ent	SetAlarm
SetAlarm:
	li	$3,252				# $3 = syscall number 0xfc
	syscall
	jr	$31
	nop
	.end	SetAlarm

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0xfd (ReleaseAlarm): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5b70 */
	.set	noreorder
	.text
	.globl	ReleaseAlarm
	.ent	ReleaseAlarm
ReleaseAlarm:
	li	$3,253				# $3 = syscall number 0xfd
	syscall
	jr	$31
	nop
	.end	ReleaseAlarm

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x1a (_iEnableIntc): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5b80 */
	.set	noreorder
	.text
	.globl	_iEnableIntc
	.ent	_iEnableIntc
_iEnableIntc:
	li	$3,-26				# $3 = syscall number -0x1a
	syscall
	jr	$31
	nop
	.end	_iEnableIntc

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x1b (_iDisableIntc): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5b90 */
	.set	noreorder
	.text
	.globl	_iDisableIntc
	.ent	_iDisableIntc
_iDisableIntc:
	li	$3,-27				# $3 = syscall number -0x1b
	syscall
	jr	$31
	nop
	.end	_iDisableIntc

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x1c (_iEnableDmac): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5ba0 */
	.set	noreorder
	.text
	.globl	_iEnableDmac
	.ent	_iEnableDmac
_iEnableDmac:
	li	$3,-28				# $3 = syscall number -0x1c
	syscall
	jr	$31
	nop
	.end	_iEnableDmac

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x1d (_iDisableDmac): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5bb0 */
	.set	noreorder
	.text
	.globl	_iDisableDmac
	.ent	_iDisableDmac
_iDisableDmac:
	li	$3,-29				# $3 = syscall number -0x1d
	syscall
	jr	$31
	nop
	.end	_iDisableDmac

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0xfe (iSetAlarm): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5bc0 */
	.set	noreorder
	.text
	.globl	iSetAlarm
	.ent	iSetAlarm
iSetAlarm:
	li	$3,-254				# $3 = syscall number -0xfe
	syscall
	jr	$31
	nop
	.end	iSetAlarm

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0xff (iReleaseAlarm): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5bd0 */
	.set	noreorder
	.text
	.globl	iReleaseAlarm
	.ent	iReleaseAlarm
iReleaseAlarm:
	li	$3,-255				# $3 = syscall number -0xff
	syscall
	jr	$31
	nop
	.end	iReleaseAlarm

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x20 (CreateThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5be0 */
	.set	noreorder
	.text
	.globl	CreateThread
	.ent	CreateThread
CreateThread:
	li	$3,32				# $3 = syscall number 0x20
	syscall
	jr	$31
	nop
	.end	CreateThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x21 (DeleteThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5bf0 */
	.set	noreorder
	.text
	.globl	DeleteThread
	.ent	DeleteThread
DeleteThread:
	li	$3,33				# $3 = syscall number 0x21
	syscall
	jr	$31
	nop
	.end	DeleteThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x22 (_StartThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5c00 */
	.set	noreorder
	.text
	.globl	_StartThread
	.ent	_StartThread
_StartThread:
	li	$3,34				# $3 = syscall number 0x22
	syscall
	jr	$31
	nop
	.end	_StartThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x23 (ExitThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5c10 */
	.set	noreorder
	.text
	.globl	ExitThread
	.ent	ExitThread
ExitThread:
	li	$3,35				# $3 = syscall number 0x23
	syscall
	jr	$31
	nop
	.end	ExitThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x24 (ExitDeleteThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5c20 */
	.set	noreorder
	.text
	.globl	ExitDeleteThread
	.ent	ExitDeleteThread
ExitDeleteThread:
	li	$3,36				# $3 = syscall number 0x24
	syscall
	jr	$31
	nop
	.end	ExitDeleteThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x25 (TerminateThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5c30 */
	.set	noreorder
	.text
	.globl	TerminateThread
	.ent	TerminateThread
TerminateThread:
	li	$3,37				# $3 = syscall number 0x25
	syscall
	jr	$31
	nop
	.end	TerminateThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x26 (iTerminateThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5c40 */
	.set	noreorder
	.text
	.globl	iTerminateThread
	.ent	iTerminateThread
iTerminateThread:
	li	$3,-38				# $3 = syscall number -0x26
	syscall
	jr	$31
	nop
	.end	iTerminateThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x27 (DisableDispatchThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5c50 */
	.set	noreorder
	.text
	.globl	DisableDispatchThread
	.ent	DisableDispatchThread
DisableDispatchThread:
	li	$3,39				# $3 = syscall number 0x27
	syscall
	jr	$31
	nop
	.end	DisableDispatchThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x28 (EnableDispatchThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5c60 */
	.set	noreorder
	.text
	.globl	EnableDispatchThread
	.ent	EnableDispatchThread
EnableDispatchThread:
	li	$3,40				# $3 = syscall number 0x28
	syscall
	jr	$31
	nop
	.end	EnableDispatchThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x29 (ChangeThreadPriority): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5c70 */
	.set	noreorder
	.text
	.globl	ChangeThreadPriority
	.ent	ChangeThreadPriority
ChangeThreadPriority:
	li	$3,41				# $3 = syscall number 0x29
	syscall
	jr	$31
	nop
	.end	ChangeThreadPriority

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x2a (iChangeThreadPriority): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5c80 */
	.set	noreorder
	.text
	.globl	iChangeThreadPriority
	.ent	iChangeThreadPriority
iChangeThreadPriority:
	li	$3,-42				# $3 = syscall number -0x2a
	syscall
	jr	$31
	nop
	.end	iChangeThreadPriority

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x2b (RotateThreadReadyQueue): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5c90 */
	.set	noreorder
	.text
	.globl	RotateThreadReadyQueue
	.ent	RotateThreadReadyQueue
RotateThreadReadyQueue:
	li	$3,43				# $3 = syscall number 0x2b
	syscall
	jr	$31
	nop
	.end	RotateThreadReadyQueue

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x2c (iRotateThreadReadyQueue): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5ca0 */
	.set	noreorder
	.text
	.globl	iRotateThreadReadyQueue
	.ent	iRotateThreadReadyQueue
iRotateThreadReadyQueue:
	li	$3,-44				# $3 = syscall number -0x2c
	syscall
	jr	$31
	nop
	.end	iRotateThreadReadyQueue

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x2d (ReleaseWaitThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5cb0 */
	.set	noreorder
	.text
	.globl	ReleaseWaitThread
	.ent	ReleaseWaitThread
ReleaseWaitThread:
	li	$3,45				# $3 = syscall number 0x2d
	syscall
	jr	$31
	nop
	.end	ReleaseWaitThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x2e (iReleaseWaitThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5cc0 */
	.set	noreorder
	.text
	.globl	iReleaseWaitThread
	.ent	iReleaseWaitThread
iReleaseWaitThread:
	li	$3,-46				# $3 = syscall number -0x2e
	syscall
	jr	$31
	nop
	.end	iReleaseWaitThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x2f (GetThreadId): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5cd0 */
	.set	noreorder
	.text
	.globl	GetThreadId
	.ent	GetThreadId
GetThreadId:
	li	$3,47				# $3 = syscall number 0x2f
	syscall
	jr	$31
	nop
	.end	GetThreadId

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x30 (ReferThreadStatus): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5ce0 */
	.set	noreorder
	.text
	.globl	ReferThreadStatus
	.ent	ReferThreadStatus
ReferThreadStatus:
	li	$3,48				# $3 = syscall number 0x30
	syscall
	jr	$31
	nop
	.end	ReferThreadStatus

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x31 (iReferThreadStatus): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5cf0 */
	.set	noreorder
	.text
	.globl	iReferThreadStatus
	.ent	iReferThreadStatus
iReferThreadStatus:
	li	$3,-49				# $3 = syscall number -0x31
	syscall
	jr	$31
	nop
	.end	iReferThreadStatus

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x32 (SleepThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5d00 */
	.set	noreorder
	.text
	.globl	SleepThread
	.ent	SleepThread
SleepThread:
	li	$3,50				# $3 = syscall number 0x32
	syscall
	jr	$31
	nop
	.end	SleepThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x33 (WakeupThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5d10 */
	.set	noreorder
	.text
	.globl	WakeupThread
	.ent	WakeupThread
WakeupThread:
	li	$3,51				# $3 = syscall number 0x33
	syscall
	jr	$31
	nop
	.end	WakeupThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x34 (_iWakeupThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5d20 */
	.set	noreorder
	.text
	.globl	_iWakeupThread
	.ent	_iWakeupThread
_iWakeupThread:
	li	$3,-52				# $3 = syscall number -0x34
	syscall
	jr	$31
	nop
	.end	_iWakeupThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x35 (CancelWakeupThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5d30 */
	.set	noreorder
	.text
	.globl	CancelWakeupThread
	.ent	CancelWakeupThread
CancelWakeupThread:
	li	$3,53				# $3 = syscall number 0x35
	syscall
	jr	$31
	nop
	.end	CancelWakeupThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x36 (iCancelWakeupThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5d40 */
	.set	noreorder
	.text
	.globl	iCancelWakeupThread
	.ent	iCancelWakeupThread
iCancelWakeupThread:
	li	$3,-54				# $3 = syscall number -0x36
	syscall
	jr	$31
	nop
	.end	iCancelWakeupThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x37 (SuspendThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5d50 */
	.set	noreorder
	.text
	.globl	SuspendThread
	.ent	SuspendThread
SuspendThread:
	li	$3,55				# $3 = syscall number 0x37
	syscall
	jr	$31
	nop
	.end	SuspendThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x38 (_iSuspendThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5d60 */
	.set	noreorder
	.text
	.globl	_iSuspendThread
	.ent	_iSuspendThread
_iSuspendThread:
	li	$3,-56				# $3 = syscall number -0x38
	syscall
	jr	$31
	nop
	.end	_iSuspendThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x39 (ResumeThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5d70 */
	.set	noreorder
	.text
	.globl	ResumeThread
	.ent	ResumeThread
ResumeThread:
	li	$3,57				# $3 = syscall number 0x39
	syscall
	jr	$31
	nop
	.end	ResumeThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x3a (iResumeThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5d80 */
	.set	noreorder
	.text
	.globl	iResumeThread
	.ent	iResumeThread
iResumeThread:
	li	$3,-58				# $3 = syscall number -0x3a
	syscall
	jr	$31
	nop
	.end	iResumeThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x3b (JoinThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5d90 */
	.set	noreorder
	.text
	.globl	JoinThread
	.ent	JoinThread
JoinThread:
	li	$3,59				# $3 = syscall number 0x3b
	syscall
	jr	$31
	nop
	.end	JoinThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x3c (SetupThread): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5da0 */
	.set	noreorder
	.text
	.globl	SetupThread
	.ent	SetupThread
SetupThread:
	li	$3,60				# $3 = syscall number 0x3c
	syscall
	jr	$31
	nop
	.end	SetupThread

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x3d (SetupHeap): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5db0 */
	.set	noreorder
	.text
	.globl	SetupHeap
	.ent	SetupHeap
SetupHeap:
	li	$3,61				# $3 = syscall number 0x3d
	syscall
	jr	$31
	nop
	.end	SetupHeap

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x3e (EndOfHeap): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5dc0 */
	.set	noreorder
	.text
	.globl	EndOfHeap
	.ent	EndOfHeap
EndOfHeap:
	li	$3,62				# $3 = syscall number 0x3e
	syscall
	jr	$31
	nop
	.end	EndOfHeap

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x3f: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5dd0 */
	.set	noreorder
	.text
	.globl	RFU063
	.ent	RFU063
RFU063:
	li	$3,63				# $3 = syscall number 0x3f
	syscall
	jr	$31
	nop
	.end	RFU063

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x40 (CreateSema): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5de0 */
	.set	noreorder
	.text
	.globl	CreateSema
	.ent	CreateSema
CreateSema:
	li	$3,64				# $3 = syscall number 0x40
	syscall
	jr	$31
	nop
	.end	CreateSema

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x41 (DeleteSema): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5df0 */
	.set	noreorder
	.text
	.globl	DeleteSema
	.ent	DeleteSema
DeleteSema:
	li	$3,65				# $3 = syscall number 0x41
	syscall
	jr	$31
	nop
	.end	DeleteSema

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x42 (SignalSema): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5e00 */
	.set	noreorder
	.text
	.globl	SignalSema
	.ent	SignalSema
SignalSema:
	li	$3,66				# $3 = syscall number 0x42
	syscall
	jr	$31
	nop
	.end	SignalSema

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x43 (iSignalSema): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5e10 */
	.set	noreorder
	.text
	.globl	iSignalSema
	.ent	iSignalSema
iSignalSema:
	li	$3,-67				# $3 = syscall number -0x43
	syscall
	jr	$31
	nop
	.end	iSignalSema

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x44 (WaitSema): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5e20 */
	.set	noreorder
	.text
	.globl	WaitSema
	.ent	WaitSema
WaitSema:
	li	$3,68				# $3 = syscall number 0x44
	syscall
	jr	$31
	nop
	.end	WaitSema

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x45 (PollSema): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5e30 */
	.set	noreorder
	.text
	.globl	PollSema
	.ent	PollSema
PollSema:
	li	$3,69				# $3 = syscall number 0x45
	syscall
	jr	$31
	nop
	.end	PollSema

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x46 (iPollSema): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5e40 */
	.set	noreorder
	.text
	.globl	iPollSema
	.ent	iPollSema
iPollSema:
	li	$3,-70				# $3 = syscall number -0x46
	syscall
	jr	$31
	nop
	.end	iPollSema

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x47 (ReferSemaStatus): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5e50 */
	.set	noreorder
	.text
	.globl	ReferSemaStatus
	.ent	ReferSemaStatus
ReferSemaStatus:
	li	$3,71				# $3 = syscall number 0x47
	syscall
	jr	$31
	nop
	.end	ReferSemaStatus

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x48 (iReferSemaStatus): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5e60 */
	.set	noreorder
	.text
	.globl	iReferSemaStatus
	.ent	iReferSemaStatus
iReferSemaStatus:
	li	$3,-72				# $3 = syscall number -0x48
	syscall
	jr	$31
	nop
	.end	iReferSemaStatus

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x49: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5e70 */
	.set	noreorder
	.text
	.globl	RFU073
	.ent	RFU073
RFU073:
	li	$3,73				# $3 = syscall number 0x49
	syscall
	jr	$31
	nop
	.end	RFU073

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x4a (SetOsdConfigParam): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5e80 */
	.set	noreorder
	.text
	.globl	SetOsdConfigParam
	.ent	SetOsdConfigParam
SetOsdConfigParam:
	li	$3,74				# $3 = syscall number 0x4a
	syscall
	jr	$31
	nop
	.end	SetOsdConfigParam

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x4b (GetOsdConfigParam): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5e90 */
	.set	noreorder
	.text
	.globl	GetOsdConfigParam
	.ent	GetOsdConfigParam
GetOsdConfigParam:
	li	$3,75				# $3 = syscall number 0x4b
	syscall
	jr	$31
	nop
	.end	GetOsdConfigParam

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x4c (GetGsHParam): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5ea0 */
	.set	noreorder
	.text
	.globl	GetGsHParam
	.ent	GetGsHParam
GetGsHParam:
	li	$3,76				# $3 = syscall number 0x4c
	syscall
	jr	$31
	nop
	.end	GetGsHParam

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x4d (GetGsVParam): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5eb0 */
	.set	noreorder
	.text
	.globl	GetGsVParam
	.ent	GetGsVParam
GetGsVParam:
	li	$3,77				# $3 = syscall number 0x4d
	syscall
	jr	$31
	nop
	.end	GetGsVParam

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x4e (SetGsHParam): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5ec0 */
	.set	noreorder
	.text
	.globl	SetGsHParam
	.ent	SetGsHParam
SetGsHParam:
	li	$3,78				# $3 = syscall number 0x4e
	syscall
	jr	$31
	nop
	.end	SetGsHParam

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x4f (SetGsVParam): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5ed0 */
	.set	noreorder
	.text
	.globl	SetGsVParam
	.ent	SetGsVParam
SetGsVParam:
	li	$3,79				# $3 = syscall number 0x4f
	syscall
	jr	$31
	nop
	.end	SetGsVParam

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x50: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5ee0 */
	.set	noreorder
	.text
	.globl	RFU080_CreateEventFlag
	.ent	RFU080_CreateEventFlag
RFU080_CreateEventFlag:
	li	$3,80				# $3 = syscall number 0x50
	syscall
	jr	$31
	nop
	.end	RFU080_CreateEventFlag

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x51: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5ef0 */
	.set	noreorder
	.text
	.globl	RFU081_DeleteEventFlag
	.ent	RFU081_DeleteEventFlag
RFU081_DeleteEventFlag:
	li	$3,81				# $3 = syscall number 0x51
	syscall
	jr	$31
	nop
	.end	RFU081_DeleteEventFlag

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x52: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5f00 */
	.set	noreorder
	.text
	.globl	RFU082_SetEventFlag
	.ent	RFU082_SetEventFlag
RFU082_SetEventFlag:
	li	$3,82				# $3 = syscall number 0x52
	syscall
	jr	$31
	nop
	.end	RFU082_SetEventFlag

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x53: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5f10 */
	.set	noreorder
	.text
	.globl	RFU083_iSetEventFlag
	.ent	RFU083_iSetEventFlag
RFU083_iSetEventFlag:
	li	$3,-83				# $3 = syscall number -0x53
	syscall
	jr	$31
	nop
	.end	RFU083_iSetEventFlag

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x54: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5f20 */
	.set	noreorder
	.text
	.globl	RFU084_ClearEventFlag
	.ent	RFU084_ClearEventFlag
RFU084_ClearEventFlag:
	li	$3,84				# $3 = syscall number 0x54
	syscall
	jr	$31
	nop
	.end	RFU084_ClearEventFlag

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x55: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5f30 */
	.set	noreorder
	.text
	.globl	RFU085_iClearEventFlag
	.ent	RFU085_iClearEventFlag
RFU085_iClearEventFlag:
	li	$3,-85				# $3 = syscall number -0x55
	syscall
	jr	$31
	nop
	.end	RFU085_iClearEventFlag

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x56: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5f40 */
	.set	noreorder
	.text
	.globl	RFU086_WaitEvnetFlag
	.ent	RFU086_WaitEvnetFlag
RFU086_WaitEvnetFlag:
	li	$3,86				# $3 = syscall number 0x56
	syscall
	jr	$31
	nop
	.end	RFU086_WaitEvnetFlag

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x57: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5f50 */
	.set	noreorder
	.text
	.globl	RFU087_PollEvnetFlag
	.ent	RFU087_PollEvnetFlag
RFU087_PollEvnetFlag:
	li	$3,87				# $3 = syscall number 0x57
	syscall
	jr	$31
	nop
	.end	RFU087_PollEvnetFlag

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x58: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5f60 */
	.set	noreorder
	.text
	.globl	RFU088_iPollEvnetFlag
	.ent	RFU088_iPollEvnetFlag
RFU088_iPollEvnetFlag:
	li	$3,-88				# $3 = syscall number -0x58
	syscall
	jr	$31
	nop
	.end	RFU088_iPollEvnetFlag

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x59: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5f70 */
	.set	noreorder
	.text
	.globl	RFU089_ReferEventFlagStatus
	.ent	RFU089_ReferEventFlagStatus
RFU089_ReferEventFlagStatus:
	li	$3,89				# $3 = syscall number 0x59
	syscall
	jr	$31
	nop
	.end	RFU089_ReferEventFlagStatus

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x5a: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5f80 */
	.set	noreorder
	.text
	.globl	RFU090_iReferEventFlagStatus
	.ent	RFU090_iReferEventFlagStatus
RFU090_iReferEventFlagStatus:
	li	$3,-90				# $3 = syscall number -0x5a
	syscall
	jr	$31
	nop
	.end	RFU090_iReferEventFlagStatus

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x5b: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5f90 */
	.set	noreorder
	.text
	.globl	GetEntryAddress
	.ent	GetEntryAddress
GetEntryAddress:
	li	$3,91				# $3 = syscall number 0x5b
	syscall
	jr	$31
	nop
	.end	GetEntryAddress

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x5c (EnableIntcHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5fa0 */
	.set	noreorder
	.text
	.globl	EnableIntcHandler
	.ent	EnableIntcHandler
EnableIntcHandler:
	li	$3,92				# $3 = syscall number 0x5c
	syscall
	jr	$31
	nop
	.end	EnableIntcHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x5c (iEnableIntcHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5fb0 */
	.set	noreorder
	.text
	.globl	iEnableIntcHandler
	.ent	iEnableIntcHandler
iEnableIntcHandler:
	li	$3,-92				# $3 = syscall number -0x5c
	syscall
	jr	$31
	nop
	.end	iEnableIntcHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x5d (DisableIntcHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5fc0 */
	.set	noreorder
	.text
	.globl	DisableIntcHandler
	.ent	DisableIntcHandler
DisableIntcHandler:
	li	$3,93				# $3 = syscall number 0x5d
	syscall
	jr	$31
	nop
	.end	DisableIntcHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x5d (iDisableIntcHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5fd0 */
	.set	noreorder
	.text
	.globl	iDisableIntcHandler
	.ent	iDisableIntcHandler
iDisableIntcHandler:
	li	$3,-93				# $3 = syscall number -0x5d
	syscall
	jr	$31
	nop
	.end	iDisableIntcHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x5e (EnableDmacHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5fe0 */
	.set	noreorder
	.text
	.globl	EnableDmacHandler
	.ent	EnableDmacHandler
EnableDmacHandler:
	li	$3,94				# $3 = syscall number 0x5e
	syscall
	jr	$31
	nop
	.end	EnableDmacHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x5e (iEnableDmacHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f5ff0 */
	.set	noreorder
	.text
	.globl	iEnableDmacHandler
	.ent	iEnableDmacHandler
iEnableDmacHandler:
	li	$3,-94				# $3 = syscall number -0x5e
	syscall
	jr	$31
	nop
	.end	iEnableDmacHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x5f (DisableDmacHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6000 */
	.set	noreorder
	.text
	.globl	DisableDmacHandler
	.ent	DisableDmacHandler
DisableDmacHandler:
	li	$3,95				# $3 = syscall number 0x5f
	syscall
	jr	$31
	nop
	.end	DisableDmacHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x5f (iDisableDmacHandler): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6010 */
	.set	noreorder
	.text
	.globl	iDisableDmacHandler
	.ent	iDisableDmacHandler
iDisableDmacHandler:
	li	$3,-95				# $3 = syscall number -0x5f
	syscall
	jr	$31
	nop
	.end	iDisableDmacHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x60: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6020 */
	.set	noreorder
	.text
	.globl	KSeg0
	.ent	KSeg0
KSeg0:
	li	$3,96				# $3 = syscall number 0x60
	syscall
	jr	$31
	nop
	.end	KSeg0

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x61: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6030 */
	.set	noreorder
	.text
	.globl	EnableCache
	.ent	EnableCache
EnableCache:
	li	$3,97				# $3 = syscall number 0x61
	syscall
	jr	$31
	nop
	.end	EnableCache

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x62: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6040 */
	.set	noreorder
	.text
	.globl	DisableCache
	.ent	DisableCache
DisableCache:
	li	$3,98				# $3 = syscall number 0x62
	syscall
	jr	$31
	nop
	.end	DisableCache

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x63: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6050 */
	.set	noreorder
	.text
	.globl	GetCop0
	.ent	GetCop0
GetCop0:
	li	$3,99				# $3 = syscall number 0x63
	syscall
	jr	$31
	nop
	.end	GetCop0

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x64 (FlushCache): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6060 */
	.set	noreorder
	.text
	.globl	FlushCache
	.ent	FlushCache
FlushCache:
	li	$3,100				# $3 = syscall number 0x64
	syscall
	jr	$31
	nop
	.end	FlushCache

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x66: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6070 */
	.set	noreorder
	.text
	.globl	CpuConfig
	.ent	CpuConfig
CpuConfig:
	li	$3,102				# $3 = syscall number 0x66
	syscall
	jr	$31
	nop
	.end	CpuConfig

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x67: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6080 */
	.set	noreorder
	.text
	.globl	iGetCop0
	.ent	iGetCop0
iGetCop0:
	li	$3,-103				# $3 = syscall number -0x67
	syscall
	jr	$31
	nop
	.end	iGetCop0

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x68 (iFlushCache): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6090 */
	.set	noreorder
	.text
	.globl	iFlushCache
	.ent	iFlushCache
iFlushCache:
	li	$3,-104				# $3 = syscall number -0x68
	syscall
	jr	$31
	nop
	.end	iFlushCache

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x6a: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f60a0 */
	.set	noreorder
	.text
	.globl	iCpuConfig
	.ent	iCpuConfig
iCpuConfig:
	li	$3,-106				# $3 = syscall number -0x6a
	syscall
	jr	$31
	nop
	.end	iCpuConfig

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x6b (sceSifStopDma): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f60b0 */
	.set	noreorder
	.text
	.globl	sceSifStopDma
	.ent	sceSifStopDma
sceSifStopDma:
	li	$3,107				# $3 = syscall number 0x6b
	syscall
	jr	$31
	nop
	.end	sceSifStopDma

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x6c: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f60c0 */
	.set	noreorder
	.text
	.globl	SetCPUTimerHandler
	.ent	SetCPUTimerHandler
SetCPUTimerHandler:
	li	$3,108				# $3 = syscall number 0x6c
	syscall
	jr	$31
	nop
	.end	SetCPUTimerHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x6d: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f60d0 */
	.set	noreorder
	.text
	.globl	SetCPUTimer
	.ent	SetCPUTimer
SetCPUTimer:
	li	$3,109				# $3 = syscall number 0x6d
	syscall
	jr	$31
	nop
	.end	SetCPUTimer

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x6e: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f60e0 */
	.set	noreorder
	.text
	.globl	SetOsdConfigParam2
	.ent	SetOsdConfigParam2
SetOsdConfigParam2:
	li	$3,110				# $3 = syscall number 0x6e
	syscall
	jr	$31
	nop
	.end	SetOsdConfigParam2

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x6f (GetOsdConfigParam2): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f60f0 */
	.set	noreorder
	.text
	.globl	GetOsdConfigParam2
	.ent	GetOsdConfigParam2
GetOsdConfigParam2:
	li	$3,111				# $3 = syscall number 0x6f
	syscall
	jr	$31
	nop
	.end	GetOsdConfigParam2

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x70 (GsGetIMR): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6100 */
	.set	noreorder
	.text
	.globl	GsGetIMR
	.ent	GsGetIMR
GsGetIMR:
	li	$3,112				# $3 = syscall number 0x70
	syscall
	jr	$31
	nop
	.end	GsGetIMR

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x70 (iGsGetIMR): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6110 */
	.set	noreorder
	.text
	.globl	iGsGetIMR
	.ent	iGsGetIMR
iGsGetIMR:
	li	$3,-112				# $3 = syscall number -0x70
	syscall
	jr	$31
	nop
	.end	iGsGetIMR

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x71 (GsPutIMR): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6120 */
	.set	noreorder
	.text
	.globl	GsPutIMR
	.ent	GsPutIMR
GsPutIMR:
	li	$3,113				# $3 = syscall number 0x71
	syscall
	jr	$31
	nop
	.end	GsPutIMR

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x71 (iGsPutIMR): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6130 */
	.set	noreorder
	.text
	.globl	iGsPutIMR
	.ent	iGsPutIMR
iGsPutIMR:
	li	$3,-113				# $3 = syscall number -0x71
	syscall
	jr	$31
	nop
	.end	iGsPutIMR

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x72: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6140 */
	.set	noreorder
	.text
	.globl	SetPgifHandler
	.ent	SetPgifHandler
SetPgifHandler:
	li	$3,114				# $3 = syscall number 0x72
	syscall
	jr	$31
	nop
	.end	SetPgifHandler

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x73 (SetVSyncFlag): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6150 */
	.set	noreorder
	.text
	.globl	SetVSyncFlag
	.ent	SetVSyncFlag
SetVSyncFlag:
	li	$3,115				# $3 = syscall number 0x73
	syscall
	jr	$31
	nop
	.end	SetVSyncFlag

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x74: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6160 */
	.set	noreorder
	.text
	.globl	SetSyscall
	.ent	SetSyscall
SetSyscall:
	li	$3,116				# $3 = syscall number 0x74
	syscall
	jr	$31
	nop
	.end	SetSyscall

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x75: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6170 */
	.set	noreorder
	.text
	.globl	_print
	.ent	_print
_print:
	li	$3,117				# $3 = syscall number 0x75
	syscall
	jr	$31
	nop
	.end	_print

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x76 (sceSifDmaStat): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6180 */
	.set	noreorder
	.text
	.globl	sceSifDmaStat
	.ent	sceSifDmaStat
sceSifDmaStat:
	li	$3,118				# $3 = syscall number 0x76
	syscall
	jr	$31
	nop
	.end	sceSifDmaStat

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x76 (isceSifDmaStat): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6190 */
	.set	noreorder
	.text
	.globl	isceSifDmaStat
	.ent	isceSifDmaStat
isceSifDmaStat:
	li	$3,-118				# $3 = syscall number -0x76
	syscall
	jr	$31
	nop
	.end	isceSifDmaStat

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x77 (sceSifSetDma): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f61a0 */
	.set	noreorder
	.text
	.globl	sceSifSetDma
	.ent	sceSifSetDma
sceSifSetDma:
	li	$3,119				# $3 = syscall number 0x77
	syscall
	jr	$31
	nop
	.end	sceSifSetDma

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x77 (isceSifSetDma): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f61b0 */
	.set	noreorder
	.text
	.globl	isceSifSetDma
	.ent	isceSifSetDma
isceSifSetDma:
	li	$3,-119				# $3 = syscall number -0x77
	syscall
	jr	$31
	nop
	.end	isceSifSetDma

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x78 (sceSifSetDChain): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f61c0 */
	.set	noreorder
	.text
	.globl	sceSifSetDChain
	.ent	sceSifSetDChain
sceSifSetDChain:
	li	$3,120				# $3 = syscall number 0x78
	syscall
	jr	$31
	nop
	.end	sceSifSetDChain

/*
 * Sony libkernel syscall stubs. Kernel call stub for interrupt-context (negative) syscall -0x78 (isceSifSetDChain): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f61d0 */
	.set	noreorder
	.text
	.globl	isceSifSetDChain
	.ent	isceSifSetDChain
isceSifSetDChain:
	li	$3,-120				# $3 = syscall number -0x78
	syscall
	jr	$31
	nop
	.end	isceSifSetDChain

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x79 (sceSifSetReg): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f61e0 */
	.set	noreorder
	.text
	.globl	sceSifSetReg
	.ent	sceSifSetReg
sceSifSetReg:
	li	$3,121				# $3 = syscall number 0x79
	syscall
	jr	$31
	nop
	.end	sceSifSetReg

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x7a (sceSifGetReg): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f61f0 */
	.set	noreorder
	.text
	.globl	sceSifGetReg
	.ent	sceSifGetReg
sceSifGetReg:
	li	$3,122				# $3 = syscall number 0x7a
	syscall
	jr	$31
	nop
	.end	sceSifGetReg

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x7b (ExecOSD): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6200 */
	.set	noreorder
	.text
	.globl	ExecOSD
	.ent	ExecOSD
ExecOSD:
	li	$3,123				# $3 = syscall number 0x7b
	syscall
	jr	$31
	nop
	.end	ExecOSD

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x7c (Deci2Call): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6210 */
	.set	noreorder
	.text
	.globl	Deci2Call
	.ent	Deci2Call
Deci2Call:
	li	$3,124				# $3 = syscall number 0x7c
	syscall
	jr	$31
	nop
	.end	Deci2Call

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x7d: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6220 */
	.set	noreorder
	.text
	.globl	PSMode
	.ent	PSMode
PSMode:
	li	$3,125				# $3 = syscall number 0x7d
	syscall
	jr	$31
	nop
	.end	PSMode

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x7e (MachineType): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6230 */
	.set	noreorder
	.text
	.globl	MachineType
	.ent	MachineType
MachineType:
	li	$3,126				# $3 = syscall number 0x7e
	syscall
	jr	$31
	nop
	.end	MachineType

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x7f (GetMemorySize): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6240 */
	.set	noreorder
	.text
	.globl	GetMemorySize
	.ent	GetMemorySize
GetMemorySize:
	li	$3,127				# $3 = syscall number 0x7f
	syscall
	jr	$31
	nop
	.end	GetMemorySize
