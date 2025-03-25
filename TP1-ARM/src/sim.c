#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "shell.h"

#define BIT(n) (1UL << (n))
#define MASK_FROM_TO(high, low) (((~0U) << (31 - (high))) & ~((1U << (low)) - 1))

#define MASK_31_24 MASK_FROM_TO(31, 24)
#define MASK_31_21 MASK_FROM_TO(31, 21)
#define MASK_31_26 MASK_FROM_TO(31, 26)
#define MASK_31_24 MASK_FROM_TO(31, 24)
#define MASK_31_22 MASK_FROM_TO(31, 22)

void exec_halt(CPU_State* cpu, uint32_t instr_bits);
void exec_add_immediate(CPU_State* cpu, uint32_t instr_bits);
void exec_adds_immediate(CPU_State* cpu, uint32_t instr_bits);
void exec_sub_immediate(CPU_State* cpu, uint32_t instr_bits);
void exec_subs_immediate(CPU_State* cpu, uint32_t instr_bits);
void exec_add_extended_reg(CPU_State* cpu, uint32_t instr_bits);
void exec_adds_extended_reg(CPU_State* cpu, uint32_t instr_bits);
void exec_sub_extended_reg(CPU_State* cpu, uint32_t instr_bits);
void exec_subs_extended_reg(CPU_State* cpu, uint32_t instr_bits);
void exec_orrs_shifted_reg(CPU_State* cpu, uint32_t instr_bits);
void exec_eors_shifted_reg(CPU_State* cpu, uint32_t instr_bits);
void exec_ands_shifted_reg(CPU_State* cpu, uint32_t instr_bits);
void exec_b(CPU_State* cpu, uint32_t instr_bits);
void exec_br(CPU_State* cpu, uint32_t instr_bits);
void exec_bcond(CPU_State* cpu, uint32_t instr_bits);
void exec_cbz(CPU_State* cpu, uint32_t instr_bits);
void exec_cbnz(CPU_State* cpu, uint32_t instr_bits);
void exec_lsr_lsl_immediate(CPU_State* cpu, uint32_t instr_bits);
void exec_stur(CPU_State* cpu, uint32_t instr_bits);
void exec_sturb(CPU_State* cpu, uint32_t instr_bits);
void exec_sturh(CPU_State* cpu, uint32_t instr_bits);
void exec_ldur(CPU_State* cpu, uint32_t instr_bits);
void exec_ldurb(CPU_State* cpu, uint32_t instr_bits);
void exec_ldurh(CPU_State* cpu, uint32_t instr_bits);
void exec_movz(CPU_State* cpu, uint32_t instr_bits);
void exec_mul(CPU_State* cpu, uint32_t instr_bits);

typedef void (*instr_func_t)(CPU_State*, uint32_t);

instr_func_t decode_opcode(uint32_t instruction);

typedef struct {
    uint32_t mask;
    uint32_t opcode;
    instr_func_t func;
} opcode_entry_t;

opcode_entry_t opcode_table[] = {

    {MASK_31_21, (0x6A2 << 21), exec_halt },

    {MASK_31_24, (0x91 << 24), exec_add_immediate },
    {MASK_31_24, (0xB1 << 24), exec_adds_immediate },
    {MASK_31_24, (0xD1 << 24), exec_sub_immediate },
    {MASK_31_24, (0xF1 << 24), exec_subs_immediate },

    {MASK_31_21, (0x458 << 21), exec_add_extended_reg },
    {MASK_31_21, (0x558 << 21), exec_adds_extended_reg },
    {MASK_31_21, (0x658 << 21), exec_sub_extended_reg },
    {MASK_31_21, (0x758 << 21), exec_subs_extended_reg },

    {MASK_31_21, (0x550 << 21), exec_orrs_shifted_reg },
    {MASK_31_21, (0x650 << 21), exec_eors_shifted_reg},
    {MASK_31_21, (0x750 << 21), exec_ands_shifted_reg},

    {MASK_31_26, (0x05 << 26), exec_b },
    {MASK_31_21, (0x5B0 << 21), exec_br },

    {MASK_31_24, (0x54 << 24), exec_bcond},
    {MASK_31_24, (0xB4 << 24), exec_cbz},
    {MASK_31_24, (0xB5 << 24), exec_cbnz},

    {MASK_31_22, (0x34D << 22), exec_lsr_lsl_immediate},

    {MASK_31_21, (0x7C0 << 21), exec_stur},
    {MASK_31_21, (0x1C0 << 21), exec_sturb},
    {MASK_31_21, (0x3C0 << 21), exec_sturh},

    {MASK_31_21, (0x7C2 << 21), exec_ldur},
    {MASK_31_21, (0x1C2 << 21), exec_ldurb},
    {MASK_31_21, (0x3C2 << 21), exec_ldurh},

    {MASK_31_21, (0x694 << 21), exec_movz},

    {MASK_31_21, (0x4D8 << 21), exec_mul},

};

#define NUM_OPCODES (sizeof(opcode_table) / sizeof(opcode_entry_t))


static inline int32_t sign_extend_9(uint32_t val) {
    if (val & 0x100) {   
        val |= 0xFFFFFE00; 
    }
    return (int32_t)val;
}

static inline int32_t sign_extend_19(uint32_t val) {
    if (val & 0x40000) {      
        val |= 0xFFF80000;
    }
    return (int32_t)val;
}

static inline int32_t sign_extend_26(uint32_t val) {
    if (val & 0x2000000) {
        val |= 0xFC000000;
    }
    return (int32_t)val;
}


void print_binary(uint32_t num) {
    for (int i = 31; i >= 0; i--) {
        printf("%u", (num >> i) & 1);
        if(i % 8 == 0) {
            printf(" ");
        }
    }
    printf("\n");
}

void update_flags(CPU_State* cpu, uint64_t result) {
    cpu->FLAG_Z = (result == 0);
    cpu->FLAG_N = ((int64_t)result < 0);
}

void increment_pc(CPU_State* cpu, uint64_t increment) {
    cpu->PC += increment;
}

void process_instruction() {
    uint32_t instruction = mem_read_32(CURRENT_STATE.PC);
    NEXT_STATE = CURRENT_STATE;

    print_binary(instruction);
    instr_func_t instr_func = decode_opcode(instruction);

    if (instr_func) {
        instr_func(&NEXT_STATE, instruction);
    } else {
        printf("Instrucción desconocida\n");
        print_binary(instruction);
    }
}

instr_func_t decode_opcode(uint32_t instruction) {
    for (int i = 0; i < NUM_OPCODES; i++) {
        if ((instruction & opcode_table[i].mask) == opcode_table[i].opcode) {
            return opcode_table[i].func;
        }
    }
    
    return NULL; // Instrucción desconocida
}

void exec_halt(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_halt called\n");
    RUN_BIT = 0;
}

void exec_add_immediate(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_add_immediate called\n");
    uint32_t shift = (instr_bits >> 22) & 0x1;
    uint32_t imm12 = (instr_bits >> 10) & 0xFFF;
    uint32_t Rn    = (instr_bits >> 5)  & 0x1F;
    uint32_t Rd    =  instr_bits        & 0x1F;

    uint64_t valRn = (Rn == 31) ? 0ULL : (uint64_t)cpu->REGS[Rn];

    if (shift == 1) {
        imm12 = imm12 << 12;
    }

    uint64_t result = valRn + imm12;
    if (Rd != 31) {
        cpu->REGS[Rd] = (int64_t)result;
    }
    increment_pc(cpu, 4);
}

void exec_adds_immediate(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_adds_immediate called\n");
    uint32_t shift = (instr_bits >> 22) & 0x1;
    uint32_t imm12 = (instr_bits >> 10) & 0xFFF;
    uint32_t Rn    = (instr_bits >> 5)  & 0x1F;
    uint32_t Rd    = instr_bits & 0x1F;

    uint64_t valRn = (Rn == 31) ? 0ULL : (uint64_t)cpu->REGS[Rn];

    if (shift == 1) {
        imm12 = imm12 << 12;
    }
    
    uint64_t result = valRn + imm12;
    if (Rd != 31) {
        cpu->REGS[Rd] = (int64_t)result;
    }
    update_flags(cpu, result);
    increment_pc(cpu, 4);
}

void exec_sub_immediate(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_sub_immediate called\n");
    uint32_t shift = (instr_bits >> 22) & 0x1;
    uint32_t imm12 = (instr_bits >> 10) & 0xFFF;
    uint32_t Rn    = (instr_bits >> 5) & 0x1F;
    uint32_t Rd    = instr_bits & 0x1F;

    uint64_t valRn = (Rn == 31) ? 0ULL : (uint64_t)cpu->REGS[Rn];

    if (shift == 1) {
        imm12 = imm12 << 12;
    }

    uint64_t result = valRn - imm12;
    if (Rd != 31) {
        cpu->REGS[Rd] = (int64_t)result;
    }
    increment_pc(cpu, 4);
}

void exec_subs_immediate(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_subs_immediate called\n");
    uint32_t shift = (instr_bits >> 22) & 0x1;
    uint32_t imm12 = (instr_bits >> 10) & 0xFFF;
    uint32_t Rn    = (instr_bits >> 5) & 0x1F;
    uint32_t Rd    = instr_bits & 0x1F;

    uint64_t valRn = (Rn == 31) ? 0ULL : (uint64_t)cpu->REGS[Rn];

    if (shift == 1) {
        imm12 = imm12 << 12;
    }

    uint64_t result = valRn - imm12;
    if (Rd != 31) {
        cpu->REGS[Rd] = (int64_t)result;
    }
    update_flags(cpu, result);
    increment_pc(cpu, 4);
}

void exec_add_extended_reg(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_add_extended_reg called\n");
    uint32_t Rm = (instr_bits >> 16) & 0x1F;
    uint32_t Rn = (instr_bits >> 5)  & 0x1F;
    uint32_t Rd = instr_bits & 0x1F;
    
    uint64_t valRn = (Rn == 31) ? 0ULL : (uint64_t)cpu->REGS[Rn];
    uint64_t valRm = (Rm == 31) ? 0ULL : (uint64_t)cpu->REGS[Rm];
    
    uint64_t result = valRn + valRm;
    if (Rd != 31) {
        NEXT_STATE.REGS[Rd] = (int64_t)result;
    }
    increment_pc(cpu, 4);
}

void exec_adds_extended_reg(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_adds_extended_reg called\n");
    uint32_t Rm = (instr_bits >> 16) & 0x1F;
    uint32_t Rn = (instr_bits >> 5)  & 0x1F;
    uint32_t Rd = instr_bits & 0x1F;
    
    uint64_t valRn = (Rn == 31) ? 0ULL : (uint64_t)cpu->REGS[Rn];
    uint64_t valRm = (Rm == 31) ? 0ULL : (uint64_t)cpu->REGS[Rm];
    
    uint64_t result = valRn + valRm;
    if (Rd != 31) {
        NEXT_STATE.REGS[Rd] = (int64_t)result;
    }
    update_flags(cpu, result);
    increment_pc(cpu, 4);
}

void exec_sub_extended_reg(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_sub_extended_reg called\n");
    uint32_t Rm = (instr_bits >> 16) & 0x1F;
    uint32_t Rn = (instr_bits >> 5)  & 0x1F;
    uint32_t Rd = instr_bits & 0x1F;
    
    uint64_t valRn = (Rn == 31) ? 0ULL : (uint64_t)cpu->REGS[Rn];
    uint64_t valRm = (Rm == 31) ? 0ULL : (uint64_t)cpu->REGS[Rm];
    
    uint64_t result = valRn - valRm;
    if (Rd != 31) {
        NEXT_STATE.REGS[Rd] = (int64_t)result;
    }
    increment_pc(cpu, 4);
}

void exec_subs_extended_reg(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_subs_extended_reg called\n");
    uint32_t Rm = (instr_bits >> 16) & 0x1F;
    uint32_t Rn = (instr_bits >> 5)  & 0x1F;
    uint32_t Rd = instr_bits & 0x1F;
    
    uint64_t valRn = (Rn == 31) ? 0ULL : (uint64_t)cpu->REGS[Rn];
    uint64_t valRm = (Rm == 31) ? 0ULL : (uint64_t)cpu->REGS[Rm];
    
    uint64_t result = valRn - valRm;
    if (Rd != 31) {
        NEXT_STATE.REGS[Rd] = (int64_t)result;
    }
    update_flags(cpu, result);
    increment_pc(cpu, 4);
}

void exec_orrs_shifted_reg(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_orrs_shifted_reg called\n");
    uint32_t Rm = (instr_bits >> 16) & 0x1F;
    uint32_t Rn = (instr_bits >> 5) & 0x1F;
    uint32_t Rd = instr_bits & 0x1F;

    uint64_t valRn = (uint64_t)cpu->REGS[Rn];
    uint64_t valRm = (uint64_t)cpu->REGS[Rm];

    uint64_t result = valRn | valRm;

    if (Rd != 31) {
        cpu->REGS[Rd] = (int64_t)result;
    }

    update_flags(cpu, result);
    increment_pc(cpu, 4);
}

void exec_eors_shifted_reg(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_eors_shifted_reg called\n");
    uint32_t Rm = (instr_bits >> 16) & 0x1F;
    uint32_t Rn = (instr_bits >> 5) & 0x1F;
    uint32_t Rd = instr_bits & 0x1F;

    uint64_t valRn = (uint64_t)cpu->REGS[Rn];
    uint64_t valRm = (uint64_t)cpu->REGS[Rm];

    uint64_t result = valRn ^ valRm;

    if (Rd != 31) {
        cpu->REGS[Rd] = (int64_t)result;
    }

    update_flags(cpu, result);
    increment_pc(cpu, 4);
}
void exec_ands_shifted_reg(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_ands_shifted_reg called\n");
    uint32_t Rm = (instr_bits >> 16) & 0x1F;
    uint32_t Rn = (instr_bits >> 5) & 0x1F;
    uint32_t Rd = instr_bits & 0x1F;
    
    uint64_t valRn = (uint64_t)cpu->REGS[Rn];
    uint64_t valRm = (uint64_t)cpu->REGS[Rm];

    uint64_t result = valRn & valRm;

    if (Rd != 31) {
        cpu->REGS[Rd] = (int64_t)result;
    }

    update_flags(cpu, result);
    increment_pc(cpu, 4);
}
void exec_b(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_b called\n");
    uint32_t imm26 = instr_bits & 0x03FFFFFF; 
    int32_t offset = sign_extend_26(imm26);
    offset <<= 2;

    increment_pc(cpu, offset);
}
void exec_br(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_br called\n");
    uint32_t Rn = (instr_bits >> 5) & 0x1F;
    increment_pc(cpu, cpu->REGS[Rn]);
}

void exec_bcond(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_bcond called\n");
    uint32_t cond = instr_bits & 0xF;
    uint32_t imm19 = (instr_bits >> 5) & 0x7FFFF;
    int32_t offset = sign_extend_19(imm19);
    offset <<= 2;

    int branch_taken = 0;
    
    switch (cond) {
        case 0x0: // BEQ
            printf("BEQ\n");
            if (cpu->FLAG_Z) {
                increment_pc(cpu, offset);
                branch_taken = 1;
            }
            break;
        case 0x1: // BNE
            printf("BNE\n");
            if (!cpu->FLAG_Z) {
                increment_pc(cpu, offset);
                branch_taken = 1;
            }
            break;
        case 0xA: // BGE
            printf("BGE\n");
            if (cpu->FLAG_N == 0) {
                increment_pc(cpu, offset);
                branch_taken = 1;
            }
            break;
        case 0xB: // BLT
            printf("BLT\n");
            if (cpu->FLAG_N != 0) {
                increment_pc(cpu, offset);
                branch_taken = 1;
            }
            break;
        case 0xC: // BGT
            printf("BGT\n");
            if (!cpu->FLAG_Z && (cpu->FLAG_N == 0)) {
                increment_pc(cpu, offset);
                branch_taken = 1;
            }
            break;
        case 0xD: // BLE
            printf("BLE\n");
            if (cpu->FLAG_Z || (cpu->FLAG_N != 0)) {
                increment_pc(cpu, offset);
                branch_taken = 1;
            }
            break;
        default:
            printf("Condición no implementada\n");
            break;
    }
    if (!branch_taken) {
        increment_pc(cpu, 4);
    }
}

void exec_cbz(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_cbz called\n");
    uint32_t imm19 = (instr_bits >> 5) & 0x7FFFF;
    int32_t offset = sign_extend_19(imm19);
    offset <<= 2;

    uint32_t Rt = instr_bits & 0x1F;
    if (cpu->REGS[Rt] == 0) {
        cpu->PC = cpu->PC + offset;
    }
}
void exec_cbnz(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_cbnz called\n");
    uint32_t imm19 = (instr_bits >> 5) & 0x7FFFF;
    int32_t offset = sign_extend_19(imm19);
    offset <<= 2;

    uint32_t Rt = instr_bits & 0x1F;
    if (cpu->REGS[Rt] != 0) {
        cpu->PC = cpu->PC + offset;
    }
}
void exec_lsr_lsl_immediate(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_lsr_lsl_immediate called\n");

    // Extraemos immr e imms (cada uno 6 bits), Rn y Rd
    uint32_t immr = (instr_bits >> 16) & 0x3F; 
    uint32_t imms = (instr_bits >> 10) & 0x3F; 
    uint32_t Rn   = (instr_bits >>  5) & 0x1F; 
    uint32_t Rd   =  instr_bits        & 0x1F;

    // Tomamos el valor de Rn (o 0 si Rn == 31)
    uint64_t val = (Rn == 31) ? 0 : cpu->REGS[Rn];
    uint64_t result;

    printf("immr=%u imms=%u\n", immr, imms);

    if (imms == 63) {
        printf("LSR, shifting right by %u\n", immr);
        result = val >> immr;
    } else if (imms == immr - 1) {
        printf("LSL, shifting left by %u\n", 64 - immr);
        result = val << (64 - immr);
    } else {
        printf("error");
        result = val >> immr;
    }


    if (Rd != 31) {
        cpu->REGS[Rd] = result;
    }

    increment_pc(cpu, 4);
}



void exec_stur(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_stur called\n");
    uint32_t offset9 = (instr_bits >> 12) & 0x1FF;
    int32_t offset = sign_extend_9(offset9);
    uint32_t Rn = (instr_bits >> 5) & 0x1F;
    uint32_t Rt = instr_bits & 0x1F;
    uint64_t base = cpu->REGS[Rn];
    uint64_t address = (uint64_t)(((int64_t)base) + offset);
    uint32_t data = (uint32_t)(cpu->REGS[Rt] & 0xFFFFFFFF);
    mem_write_32(address, data);

    increment_pc(cpu, 4);
}
void exec_sturb(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_sturb called\n");
    uint32_t offset9 = (instr_bits >> 12) & 0x1FF;
    int32_t  offset  = sign_extend_9(offset9);
    uint32_t Rn      = (instr_bits >> 5) & 0x1F;
    uint32_t Rt      =  instr_bits       & 0x1F;

    uint64_t base    = (Rn == 31) ? 0 : cpu->REGS[Rn];
    uint64_t address = (uint64_t)(((int64_t)base) + offset);

    // Se alinea la dirección a 4 bytes
    uint64_t aligned_addr = address & ~0x3ULL;
    // Lee la palabra de 32 bits
    uint32_t word = mem_read_32(aligned_addr);

    // Desplazamiento dentro de la palabra
    uint32_t shift = (address & 0x3) * 8;
    // Toma el byte menos significativo de Xn
    uint8_t byte = (uint8_t)(cpu->REGS[Rt] & 0xFF);

    // Limpia el byte en esa posición y sobreescribe
    word &= ~(0xFF << shift);
    word |= (byte << shift);

    // Escribe la palabra resultante
    mem_write_32(aligned_addr, word);

    increment_pc(cpu, 4);
}
void exec_sturh(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_sturh called\n");
    uint32_t offset9 = (instr_bits >> 12) & 0x1FF;
    int32_t  offset  = sign_extend_9(offset9);
    uint32_t Rn      = (instr_bits >> 5) & 0x1F;
    uint32_t Rt      =  instr_bits       & 0x1F;

    uint64_t base    = (Rn == 31) ? 0 : cpu->REGS[Rn];
    uint64_t address = (uint64_t)(((int64_t)base) + offset);

    // Se alinea la dirección a 4 bytes
    uint64_t aligned_addr = address & ~0x3ULL;
    // Lee la palabra de 32 bits
    uint32_t word = mem_read_32(aligned_addr);

    // Desplazamiento dentro de la palabra
    uint32_t shift = (address & 0x3) * 8;
    // Toma los 16 bits menos significativos de Xn
    uint16_t half = (uint16_t)(cpu->REGS[Rt] & 0xFFFF);

    // Limpia los 16 bits en esa posición y sobreescribe
    word &= ~(0xFFFF << shift);
    word |= ((uint32_t)half << shift);

    // Escribe la palabra resultante
    mem_write_32(aligned_addr, word);

    increment_pc(cpu, 4);
}

void exec_ldur(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_ldur called\n");
    uint32_t offset9 = (instr_bits >> 12) & 0x1FF;
    int32_t offset = sign_extend_9(offset9);
    uint32_t Rn = (instr_bits >> 5) & 0x1F;
    uint32_t Rt = instr_bits & 0x1F;
    uint64_t base = (Rn == 31) ? 0 : cpu->REGS[Rn];
    uint64_t address = (uint64_t)(((int64_t)base) + offset);
    uint32_t word = mem_read_32(address);
    cpu->REGS[Rt] = (uint64_t)word;  // Zero extension

    increment_pc(cpu, 4);
}

void exec_ldurb(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_ldurb called\n");
    uint32_t offset9 = (instr_bits >> 12) & 0x1FF;
    int32_t  offset  = sign_extend_9(offset9);
    uint32_t Rn      = (instr_bits >> 5) & 0x1F;
    uint32_t Rt      =  instr_bits       & 0x1F;

    uint64_t base    = (Rn == 31) ? 0 : cpu->REGS[Rn];
    uint64_t address = (uint64_t)(((int64_t)base) + offset);

    // Se alinea la dirección a 4 bytes
    uint64_t aligned_addr = address & ~0x3ULL;
    // Lee la palabra de 32 bits
    uint32_t word = mem_read_32(aligned_addr);

    // Desplazamiento dentro de la palabra
    uint32_t shift = (address & 0x3) * 8;
    // Extrae el byte
    uint8_t byte = (word >> shift) & 0xFF;

    // Zero extension a 64 bits
    cpu->REGS[Rt] = (uint64_t)byte;

    increment_pc(cpu, 4);
}

void exec_ldurh(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_ldurh called\n");
    printf("exec_ldurh called\n");
    uint32_t offset9 = (instr_bits >> 12) & 0x1FF;
    int32_t  offset  = sign_extend_9(offset9);
    uint32_t Rn      = (instr_bits >> 5) & 0x1F;
    uint32_t Rt      =  instr_bits       & 0x1F;

    uint64_t base    = (Rn == 31) ? 0 : cpu->REGS[Rn];
    uint64_t address = (uint64_t)(((int64_t)base) + offset);

    // Se alinea la dirección a 4 bytes
    uint64_t aligned_addr = address & ~0x3ULL;
    // Lee la palabra de 32 bits
    uint32_t word = mem_read_32(aligned_addr);

    // Desplazamiento dentro de la palabra
    uint32_t shift = (address & 0x3) * 8;
    // Extrae los 16 bits (halfword)
    uint16_t half = (word >> shift) & 0xFFFF;

    // Zero extension a 64 bits
    cpu->REGS[Rt] = (uint64_t)half;

    increment_pc(cpu, 4);
}

void exec_movz(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_movz called\n");
    uint32_t imm16 = (instr_bits >> 5) & 0xFFFF;
    uint32_t Rd = instr_bits & 0x1F;
    cpu->REGS[Rd] = (uint64_t)imm16;

    increment_pc(cpu, 4);
}

void exec_mul(CPU_State* cpu, uint32_t instr_bits) {
    printf("exec_mul called\n");
    uint32_t Rm = (instr_bits >> 16) & 0x1F;
    uint32_t Rn = (instr_bits >> 5) & 0x1F;
    uint32_t Rd = instr_bits & 0x1F;

    uint64_t valRn = cpu->REGS[Rn];
    uint64_t valRm = cpu->REGS[Rm];

    uint64_t result = valRn * valRm;

    if (Rd != 31) {
        cpu->REGS[Rd] = (int64_t)result;
    }

    increment_pc(cpu, 4);
}