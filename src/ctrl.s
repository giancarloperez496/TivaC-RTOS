	.global setAsp
	.global setPsp
	.global getPsp
	.global getMsp
	.global setCtrl
	.global clrCtrl
	.global pushR4_R11
	.global popR11_R4
	.global getR0

.thumb
.const

setAsp:
		MRS R0, CONTROL
        ORR R0, R0, #0x2
        MSR CONTROL, R0
        ISB
        BX LR

setPsp:
		MSR PSP, R0
		ISB
		BX LR

getPsp:
		MRS R0, PSP
		BX LR

getMsp:
		MRS R0, MSP
		BX LR

setCtrl:
		MRS R1, CONTROL
		ORR R1, R1, R0
		MSR CONTROL, R1
		DSB
		ISB
		BX LR

clrCtrl:
		MRS R1, CONTROL
		EOR R0, R0, #0 ;; fix this
		AND R1, R1, R0
		MSR CONTROL, R1
		ISB
		BX LR

pushR4_R11:
		MRS R0, PSP
		STR R4, [R0, #-4]
		STR R5, [R0, #-8]
		STR R6, [R0, #-12]
		STR R7, [R0, #-16]
		STR R8, [R0, #-20]
		STR R9, [R0, #-24]
		STR R10, [R0, #-28]
		STR R11, [R0, #-32]
		SUB R0, R0, #32
		MSR PSP, R0
		BX LR

popR11_R4:
		MRS R0, PSP
		LDR R11, [R0]
		LDR R10, [R0, #4]
		LDR R9, [R0, #8]
		LDR R8, [R0, #12]
		LDR R7, [R0, #16]
		LDR R6, [R0, #20]
		LDR R5, [R0, #24]
		LDR R4, [R0, #28]
		ADD R0, R0, #32
		MSR PSP, R0
		BX LR

getR0:
	BX LR

