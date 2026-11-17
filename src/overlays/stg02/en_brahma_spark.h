/* Brahma's sparks, Area 2. */

#ifndef REZ_STG02_EN_BRAHMA_SPARK_H
#define REZ_STG02_EN_BRAHMA_SPARK_H

#include "types_common.h"

struct EnBrahmaSparkWork;

void en_brahma_spark_vars_register(void);
void en_brahma_spark_vars_unregister(void);
void en_brahma_spark_start_strand(struct EnBrahmaSparkWork *w, int i);
int en_brahma_spark_can_emit(int *w);
void en_brahma_spark_move_strand(char *wp, int i);
void en_brahma_spark_exec(char *w);
void en_brahma_spark_disp(struct EnBrahmaSparkWork *w);
void en_brahma_spark_init(void *wp, Vec3 *pos, void *src, unsigned int rate);
void en_brahma_spark_release(void);
void *en_brahma_spark_create(void *parent, Vec3 *pos, void *src, unsigned int rate);

#endif /* REZ_STG02_EN_BRAHMA_SPARK_H */
