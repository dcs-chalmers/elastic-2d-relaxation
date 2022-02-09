/*
 *   File: main_test_loop.h
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description:
 *   main_test_loop.h is part of ASCYLIB
 *
 * Copyright (c) 2014 Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>,
 * 	     	      Tudor David <tudor.david@epfl.ch>
 *	      	      Distributed Programming Lab (LPD), EPFL
 *
 * ASCYLIB is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#ifndef _MAIN_TEST_LOOP_H_

	#define _MAIN_TEST_LOOP_H_

	#if OPS_PER_THREAD == 1
		#define PRINT_OPS_PER_THREAD() \
			printf("%-3lu  %-8zu %-8zu %-8zu\n",t, getting_count[t], putting_count[t], removing_count[t]);
	#else
		#define PRINT_OPS_PER_THREAD()
	#endif

	#if WORKLOAD == 2 //skewed workload
		#define THREAD_INIT(id)													\
			if (!ID) { printf("- Creating zipf random numbers array\n"); }		\
			__zipf_arr = zipf_get_rand_array(ZIPF_ALPHA, 0, rand_max + 1, id); 	\
			barrier_cross(&barrier);											\
			if (!ID) { printf("- Done\n"); }

		#define THREAD_END(id)							\
			ZIPF_STATS_DO								\
			(											\
				if (id == 0)							\
				{										\
					zipf_print_stats(__zipf_arr);		\
				}										\
				free(__zipf_arr->stats);				\
			);											\
			free(__zipf_arr);
	#elif WORKLOAD == 4 //Precomputed workload.


		#define THREAD_INIT(id)													\
			unsigned char buffer[100];											\
			FILE *ptr;															\
			char name[100];														\
			char number[3];														\
																				\
			strcpy(name, PRECOMP_DIR);											\
			strcat(name, "/");													\
			sprintf(number, "%d", id);											\
			strcat(name, number);												\
																				\
			ptr = fopen(name, "rb");											\
			int bytes;															\
			uint64_t allocated_bytes = 128;										\
			precomputed_bytes = calloc(allocated_bytes, 1);						\
																				\
			do {																\
				bytes = fread(buffer, 1, sizeof(buffer), ptr);					\
				for (int i = 0; i < bytes; i++) {								\
					if (nbr_precomputed_bytes >= allocated_bytes) {				\
						allocated_bytes *= 2;									\
						precomputed_bytes = realloc(precomputed_bytes, allocated_bytes);		\
					}															\
					precomputed_bytes[nbr_precomputed_bytes++] = buffer[i];		\
				}																\
			} while(bytes > 0);													\
																				\
			fclose(ptr);

		#define THREAD_END(id)		free(precomputed_bytes);
	#else
		#define THREAD_INIT(id)
		#define THREAD_END()
	#endif

	#define TEST_VARS_GLOBAL												\
		volatile int phase_put = 0;											\
		volatile uint32_t phase_put_threshold_start = 0.99999 * UINT_MAX;	\
		volatile uint32_t phase_put_threshold_stop  = 0.9999999 * UINT_MAX;	\
		__thread volatile ticks phase_start, phase_stop;					\
		ZIPF_RAND_DECLARATIONS();											\
		__thread char* precomputed_bytes;									\
		__thread uint32_t nbr_precomputed_bytes, byte_at, bit_at;
	#ifndef WORKLOAD
		#define WORKLOAD 0		/*normal workload*/
	#endif

	#if WORKLOAD == 1		/* with phases */
		#define TEST_LOOP(algo_type)															\
			c = (uint32_t)(my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])));						\
			key = (c & rand_max) + rand_min;													\
			if (!phase_put && c > phase_put_threshold_start)									\
			{																					\
				phase_start = getticks();														\
				phase_put = 1;																	\
				MEM_BARRIER;																	\
				}																					\
			else if (phase_put && c > phase_put_threshold_stop)									\
			{																					\
				phase_stop = getticks();														\
				if (!ID) printf("[%2u]phase dur = %f\n", ID, (phase_stop-phase_start)/2.8e9); 	\
				phase_put = 0;																	\
			}																					\
																								\
			if (phase_put || unlikely(c <= scale_put))											\
			{																					\
				int res;																		\
				START_TS(1);																	\
				res = DS_ADD(set, key, algo_type);												\
				if(res)																			\
				{																				\
					END_TS(1, my_putting_count_succ);											\
					ADD_DUR(my_putting_succ);													\
					my_putting_count_succ++;													\
				}																				\
				END_TS_ELSE(4, my_putting_count - my_putting_count_succ, my_putting_fail);		\
				my_putting_count++;																\
			}																					\
			else if(unlikely(c <= scale_rem))													\
			{																					\
				int removed;																	\
				START_TS(2);																	\
				removed = DS_REMOVE(set, key, algo_type);										\
				if(removed != 0)																\
				{																				\
					END_TS(2, my_removing_count_succ);											\
					ADD_DUR(my_removing_succ);													\
					my_removing_count_succ++;													\
				}																				\
				END_TS_ELSE(5, my_removing_count - my_removing_count_succ, my_removing_fail);	\
				my_removing_count++;															\
			}																					\
			else																				\
			{																					\
				int res;																		\
				START_TS(0);																	\
				res = (sval_t) DS_CONTAINS(set, key, algo_type);								\
				if(res != 0)																	\
				{																				\
					END_TS(0, my_getting_count_succ);											\
					ADD_DUR(my_getting_succ);													\
					my_getting_count_succ++;													\
				}																				\
				END_TS_ELSE(3, my_getting_count - my_getting_count_succ, my_getting_fail);		\
				my_getting_count++;																\
			}

	#elif WORKLOAD == 4	/* precomputed workload */

		static inline char get_precomputed()
		{
			extern __thread uint32_t bit_at, byte_at, nbr_precomputed_bytes;
			extern __thread char* precomputed_bytes;

			assert(nbr_precomputed_bytes > 0);

			if (unlikely(++bit_at == 8)) {
				bit_at = 0;

				if (unlikely(++byte_at == nbr_precomputed_bytes)) {
					byte_at = 0;
				}
			}

			char byte = precomputed_bytes[byte_at];
			return byte & (1 << bit_at);
		}


		#define TEST_LOOP(algo_type)	error("ERROR: Precomputed work load is only update/no update\n");

		#define TEST_LOOP_ONLY_UPDATES()														\
			if (get_precomputed())																\
			{																					\
				key = 1;	/* This should not be needed, and we don't gen rng anymore*/ 		\
				int res;																		\
				START_TS(1);																	\
				res = DS_ADD(set, key, key);													\
				if(res)																			\
				{																				\
					END_TS(1, my_putting_count_succ);											\
					ADD_DUR(my_putting_succ);													\
					my_putting_count_succ++;													\
				}																				\
			  END_TS_ELSE(4, my_putting_count - my_putting_count_succ, my_putting_fail);		\
			  my_putting_count++;																\
			}																					\
			else																				\
			{																					\
				int removed;																	\
				START_TS(2);																	\
				removed = DS_REMOVE(set);														\
				if(removed != 0)																\
				{																				\
					END_TS(2, my_removing_count_succ);											\
					ADD_DUR(my_removing_succ);													\
					my_removing_count_succ++;													\
				}																				\
				END_TS_ELSE(5, my_removing_count - my_removing_count_succ, my_removing_fail);	\
				my_removing_count++;															\
			} 																					\
			if(side_work>0)																		\
				cpause(my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (side_work));			\

		#define COUNTER_LOOP_ONLY_UPDATES()														\
			if (get_precomputed())																\
			{																					\
				int res;																		\
				START_TS(1);																	\
				res = DS_ADD(set,keye,key);														\
				if(res)																			\
				{																				\
					END_TS(1, my_putting_count_succ);											\
					ADD_DUR(my_putting_succ);													\
					my_putting_count_succ++;													\
				}																				\
			  END_TS_ELSE(4, my_putting_count - my_putting_count_succ, my_putting_fail);		\
			  my_putting_count++;																\
			}																					\
			else																				\
			{																					\
				int removed;																	\
				START_TS(2);																	\
				removed = DS_REMOVE(set);														\
				if(removed != 0)																\
				{																				\
					END_TS(2, my_removing_count_succ);											\
					ADD_DUR(my_removing_succ);													\
					my_removing_count_succ++;													\
				}																				\
				END_TS_ELSE(5, my_removing_count - my_removing_count_succ, my_removing_fail);	\
				my_removing_count++;															\
			} 																					\
			if(side_work>0)																		\
				cpause(my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (side_work));			\

		#define TEST_LOOP_ONLY_4UPDATES()	error("TEST_LOOP_ONLY_4UPDATES() not implemented\n");

	#elif WORKLOAD == 0	/* normal uniform workload */
		#define TEST_LOOP(algo_type)														\
			c = (uint32_t)(my_random(&(seeds[0]),&(seeds[1]),&(seeds[2]))); /*toss dise to get operation to carry out*/ \
			key = (c & rand_max) + rand_min; /*get key to use in the operation */			\
			if (unlikely(c <= scale_put))													\
			{																				\
				int res;																	\
				START_TS(1);																\
				res = DS_ADD(set, key, algo_type);											\
				if(res)																		\
				{																			\
					END_TS(1, my_putting_count_succ);										\
					ADD_DUR(my_putting_succ);												\
					my_putting_count_succ++;												\
				}																			\
				END_TS_ELSE(4, my_putting_count - my_putting_count_succ, my_putting_fail);	\
				my_putting_count++;															\
			}																				\
			else if(unlikely(c <= scale_rem))												\
			{																				\
				int removed;																\
				START_TS(2);																\
				removed = DS_REMOVE(set, key, algo_type);									\
				if(removed != 0)															\
				{																			\
					END_TS(2, my_removing_count_succ);										\
					ADD_DUR(my_removing_succ);												\
					my_removing_count_succ++;												\
				}																			\
				END_TS_ELSE(5, my_removing_count - my_removing_count_succ, my_removing_fail);\
				my_removing_count++;														\
			}																				\
			else																			\
			{																				\
				int res;																	\
				START_TS(0);																\
				res = (sval_t) DS_CONTAINS(set, key, algo_type);							\
				if(res != 0)																\
				{																			\
					END_TS(0, my_getting_count_succ);										\
					ADD_DUR(my_getting_succ);												\
					my_getting_count_succ++;												\
				}																			\
				END_TS_ELSE(3, my_getting_count - my_getting_count_succ, my_getting_fail);	\
				my_getting_count++;															\
			}																				\
			if(side_work>0)																		\
				cpause(my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (side_work));			\

		#define TEST_LOOP_ONLY_UPDATES()														\
			c = (uint32_t)(my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])));						\
			if (unlikely(c < scale_put))														\
			{																					\
				key = (c & rand_max) + rand_min;												\
				int res;																		\
				START_TS(1);																	\
				res = DS_ADD(set, key, key);													\
				if(res)																			\
				{																				\
					END_TS(1, my_putting_count_succ);											\
					ADD_DUR(my_putting_succ);													\
					my_putting_count_succ++;													\
				}																				\
			  END_TS_ELSE(4, my_putting_count - my_putting_count_succ, my_putting_fail);		\
			  my_putting_count++;																\
			}																					\
			else if(unlikely(c <= scale_rem))													\
			{																					\
				int removed;																	\
				START_TS(2);																	\
				removed = DS_REMOVE(set);														\
				if(removed != 0)																\
				{																				\
					END_TS(2, my_removing_count_succ);											\
					ADD_DUR(my_removing_succ);													\
					my_removing_count_succ++;													\
				}																				\
				END_TS_ELSE(5, my_removing_count - my_removing_count_succ, my_removing_fail);	\
				my_removing_count++;															\
			} 																					\
			if(side_work>0)																		\
				cpause(my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (side_work));			\

		#define COUNTER_LOOP_ONLY_UPDATES()														\
			c = (uint32_t)(my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])));						\
			if (unlikely(c < scale_put))														\
			{																					\
				int res;																		\
				START_TS(1);																	\
				res = DS_ADD(set,keye,key);														\
				if(res)																			\
				{																				\
					END_TS(1, my_putting_count_succ);											\
					ADD_DUR(my_putting_succ);													\
					my_putting_count_succ++;													\
				}																				\
			  END_TS_ELSE(4, my_putting_count - my_putting_count_succ, my_putting_fail);		\
			  my_putting_count++;																\
			}																					\
			else if(unlikely(c <= scale_rem))													\
			{																					\
				int removed;																	\
				START_TS(2);																	\
				removed = DS_REMOVE(set);														\
				if(removed != 0)																\
				{																				\
					END_TS(2, my_removing_count_succ);											\
					ADD_DUR(my_removing_succ);													\
					my_removing_count_succ++;													\
				}																				\
				END_TS_ELSE(5, my_removing_count - my_removing_count_succ, my_removing_fail);	\
				my_removing_count++;															\
			} 																					\
			if(side_work>0)																		\
				cpause(my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (side_work));			\

		#define TEST_LOOP_ONLY_4UPDATES()														\
			c = (uint32_t)(my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])));						\
			tail = (uint32_t)(my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])));					\
			if (unlikely(c < scale_put))														\
			{																					\
				key = (c & rand_max) + rand_min;												\
				int res;																		\
				START_TS(1);																	\
				if (unlikely(tail < 0.5)) res = DS_ADD_R(set, key, key);						\
				else res = DS_ADD_L(set, key, key);												\
				if(res)																			\
				{																				\
					END_TS(1, my_putting_count_succ);											\
					ADD_DUR(my_putting_succ);													\
					my_putting_count_succ++;													\
				}																				\
			  END_TS_ELSE(4, my_putting_count - my_putting_count_succ, my_putting_fail);		\
			  my_putting_count++;																\
			}																					\
			else if(unlikely(c <= scale_rem))													\
			{																					\
				int removed;																	\
				START_TS(2);																	\
				if (unlikely(tail < 0.5)) removed = DS_REMOVE_R(set);							\
				else removed = DS_REMOVE_L(set);												\
				if(removed != 0)																\
				{																				\
					END_TS(2, my_removing_count_succ);											\
					ADD_DUR(my_removing_succ);													\
					my_removing_count_succ++;													\
				}																				\
				END_TS_ELSE(5, my_removing_count - my_removing_count_succ, my_removing_fail);	\
				my_removing_count++;															\
			} 																					\
			if(side_work>0)																		\
				cpause(my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % (side_work));			\

	#elif WORKLOAD == 3	/* Producer consumer workload */
		#define TEST_LOOP_ONLY_UPDATES()														\
			c = (uint32_t)(my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])));                     \
			if (ID < (num_threads*put_rate))													\
			{																					\
				key = (c & rand_max) + rand_min;												\
				int res;																		\
				START_TS(1);																	\
				res = DS_ADD(set, key, key);													\
				if(res)																			\
				{																				\
					END_TS(1, my_putting_count_succ);											\
					ADD_DUR(my_putting_succ);													\
					my_putting_count_succ++;													\
				}																				\
			  END_TS_ELSE(4, my_putting_count - my_putting_count_succ, my_putting_fail);		\
			  my_putting_count++;			 													\
			}																					\
			else																				\
			{																					\
				int removed;																	\
				START_TS(2);																	\
				removed = DS_REMOVE(set);														\
				if(removed != 0)																\
				{																				\
					END_TS(2, my_removing_count_succ);											\
					ADD_DUR(my_removing_succ);													\
					my_removing_count_succ++;													\
				}																				\
				END_TS_ELSE(5, my_removing_count - my_removing_count_succ, my_removing_fail);	\
				my_removing_count++;															\
			} 																					\
			cpause(side_work);

	#elif WORKLOAD == 2	/* zipf workload */
		#  define TEST_LOOP(algo_type)														\
			c = (uint32_t)(my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])));					\
			key = rand_max - zipf_get_next(__zipf_arr) + rand_min;							\
			ZIPF_STATS_DO(__zipf_arr->stats[key]++); 										\
																							\
			if (unlikely(c <= scale_put))  													\
			{																				\
				int res;																	\
				START_TS(1);																\
				res = DS_ADD(set, key, algo_type); 											\
				if(res)																		\
				{																			\
					END_TS(1, my_putting_count_succ); 										\
					ADD_DUR(my_putting_succ); 												\
					my_putting_count_succ++; 												\
				}																			\
				END_TS_ELSE(4, my_putting_count - my_putting_count_succ, my_putting_fail); 	\
				my_putting_count++;															\
			}																				\
			else if(unlikely(c <= scale_rem)) 												\
			{																				\
				int removed;																\
				START_TS(2);																\
				removed = DS_REMOVE(set, key, algo_type); 									\
				if(removed != 0)															\
				{																			\
					END_TS(2, my_removing_count_succ);										\
					ADD_DUR(my_removing_succ);												\
					my_removing_count_succ++; 												\
				}																			\
				END_TS_ELSE(5, my_removing_count - my_removing_count_succ,my_removing_fail);\
				my_removing_count++;														\
			}																				\
			else																			\
			{																				\
				int res;																	\
				START_TS(0);																\
				res = (sval_t) DS_CONTAINS(set, key, algo_type); 							\
				if(res != 0)																\
				{																			\
					END_TS(0, my_getting_count_succ); 										\
					ADD_DUR(my_getting_succ); 												\
					my_getting_count_succ++; 												\
				}																			\
				END_TS_ELSE(3, my_getting_count - my_getting_count_succ,my_getting_fail); 	\
				my_getting_count++;															\
			}
	#endif	/* WORKLOAD */

	#define POW_CORRECTED 0
	// double pow_tot_correction = (throughput * eng_per_test_iter_nj[num_threads-1][0]) / 1e9;
	//  printf("#Duration: %f, %f, %f\n", s.duration[0], s.duration[1], s.duration[2]);
	#if RAPL_READ_ENABLE == 1
		#if POW_CORRECTED == 1
			#define RR_PRINT_CORRECTED()																		\
				rapl_stats_t s;																					\
				RR_STATS(&s);																					\
				if (num_threads > (CORES_PER_SOCKET*NUMBER_OF_SOCKETS))											\
				{																								\
					num_threads = (CORES_PER_SOCKET*NUMBER_OF_SOCKETS);											\
				}																								\
				double static_pow = 0;																			\
				int si;																							\
				for (si = 0; si < NUMBER_OF_SOCKETS; si++)														\
				{																								\
					if (s.power_total[si] > 0)																	\
					{																							\
						static_pow += static_power[si + 1];														\
					}																							\
				}																								\
				double pow_tot_corrected = s.power_total[NUMBER_OF_SOCKETS] - static_pow; 						\
				printf("#Total Power Corrected : %11f (correction= %10f) W\n", pow_tot_corrected, static_pow);	\
				double eop = (1e6 * s.power_total[NUMBER_OF_SOCKETS]) / throughput;								\
				double eop_corrected = (1e6 * pow_tot_corrected) / throughput;									\
				printf("#Energy per Operation : %11f (corrected = %10f) uJ\n", eop, eop_corrected);
		#else	 /* not corrected */
			#define RR_PRINT_CORRECTED()																	\
				rapl_stats_t s;																				\
				RR_STATS(&s);																				\
				if (num_threads > (CORES_PER_SOCKET*NUMBER_OF_SOCKETS))										\
				{																							\
					num_threads = (CORES_PER_SOCKET*NUMBER_OF_SOCKETS);										\
				}																							\
				double pow_tot = s.power_total[NUMBER_OF_SOCKETS];								\
				/*printf("#Total Power Corrected : %11f (correction= %10f) W\n",  pow_tot_corrected, 0.0);*/ 	\
				double eop = (1e6 * s.power_total[NUMBER_OF_SOCKETS]) / throughput;							\
				/*double eop_corrected = eop;*/																\
				/*printf("#Energy per Operation : %11f (corrected = %10f) uJ\n", eop, eop_corrected);*/		\
				/** adon **/																				\
				printf("Energy per Operation (uJ), %11f \n", eop);											\
				printf("Total Power  (W), %11f \n",  pow_tot);												\
				printf("Rest Power  (W), %11f \n",s.power_rest[NUMBER_OF_SOCKETS]);							\
				printf("PPO Power  (W), %11f \n",s.power_pp0[NUMBER_OF_SOCKETS]);							\
				printf("Package Power  (W), %11f \n",s.power_package[NUMBER_OF_SOCKETS]);					\
				printf("DRAM Power  (W), %11f \n",s.power_dram[NUMBER_OF_SOCKETS]);
		#endif
	/* double pow_tot_corrected = s.power_total[NUMBER_OF_SOCKETS] - pow_tot_correction - static_pow; \ */
	#else
		#define RR_PRINT_CORRECTED()
	#endif
#endif	/* _MAIN_TEST_LOOP_H_ */
