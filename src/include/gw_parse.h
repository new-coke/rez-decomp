/* Debug GUI number parsing, bit-string formatting. */

#ifndef REZ_GW_PARSE_H
#define REZ_GW_PARSE_H

char *gw_format_bits(int val, int nbits, char *buf, char *ones, char *zeros);
float gw_parse_float(char *s);
int gw_parse_int(char *s);

#endif /* REZ_GW_PARSE_H */
