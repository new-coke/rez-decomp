/* Float bit tricks: mantissa rounding, depth nudging. */
#ifndef REZ_FLOAT_BITS_H
#define REZ_FLOAT_BITS_H

float float_round_mantissa(float x);
float float_add_bits16(int n, float x);

#endif /* REZ_FLOAT_BITS_H */
